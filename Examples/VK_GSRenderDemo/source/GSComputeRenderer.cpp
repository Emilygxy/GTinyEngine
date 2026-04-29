#include "GSComputeRenderer.h"

#include "GTVulkan/EasyVulkan.h"
#include "GTVulkan/GlfwGeneral.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

using namespace easy_vk;
using namespace vk;

namespace {

constexpr uint32_t kTileWidth = 16;
constexpr uint32_t kTileHeight = 16;
constexpr uint32_t kSortBlocksPerWorkgroup = 1;

struct PreprocessResult {
    uint32_t numInstances = 0;
    bool prefixInPing = true;
};

struct StorageImage {
    image imageHandle;
    deviceMemory memory;
    VkImageView view = VK_NULL_HANDLE;

    void create(VkExtent2D extent, VkFormat format) {
        VkImageCreateInfo imageInfo{
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {extent.width, extent.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        if (imageHandle.Create(imageInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create storage image");
        }
        auto allocInfo = imageHandle.MemoryAllocateInfo(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory.Allocate(allocInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate storage image memory");
        }
        if (imageHandle.BindMemory(memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind storage image memory");
        }

        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = imageHandle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        if (vkCreateImageView(GraphicsBase::Base().Device(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create storage image view");
        }
    }

    void destroy() {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(GraphicsBase::Base().Device(), view, nullptr);
            view = VK_NULL_HANDLE;
        }
    }
};

uint32_t ceilDiv(uint32_t x, uint32_t y) {
    return (x + y - 1) / y;
}

void checkVk(VkResult result, const char* message) {
    if (result != VK_SUCCESS) {
        std::cerr << message << " VkResult=" << static_cast<int>(result) << std::endl;
        std::abort();
    }
}

VkDescriptorSetLayout createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    checkVk(vkCreateDescriptorSetLayout(GraphicsBase::Base().Device(), &info, nullptr, &layout),
            "Failed to create descriptor set layout");
    return layout;
}

VkDescriptorPool createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 3> sizes{
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 320},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 96},
    };
    VkDescriptorPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 192,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    };
    VkDescriptorPool pool = VK_NULL_HANDLE;
    checkVk(vkCreateDescriptorPool(GraphicsBase::Base().Device(), &info, nullptr, &pool),
            "Failed to create descriptor pool");
    return pool;
}

VkDescriptorSet allocateDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    VkDescriptorSet set = VK_NULL_HANDLE;
    checkVk(vkAllocateDescriptorSets(GraphicsBase::Base().Device(), &info, &set),
            "Failed to allocate descriptor set");
    return set;
}

class GSComputeRendererImpl {
public:
    bool initialize(const std::vector<GSVertex>& inputVertices) {
        if (!InitializeWindow({1280, 720}, false, true, false)) {
            return false;
        }
        vertices = inputVertices;
        fitCameraToScene();
        lastExtent = windowSize;
        createCommandResources();
        createSyncResources();
        createBuffers();
        createDescriptorResources();
        createPipelines();
        precomputeCov3D();
        createOutputImagesAndRenderSets();
        return true;
    }

    void run() {
        while (!glfwWindowShouldClose(pWindow)) {
            while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)) {
                glfwWaitEvents();
            }
            drawFrame();
            glfwPollEvents();
            TitleFps();
        }
        GraphicsBase::Base().WaitIdle();
    }

    void shutdown() {
        auto device = GraphicsBase::Base().Device();
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
        for (auto& img : depthOutputs) {
            img.destroy();
        }
        for (auto& img : normalOutputs) {
            img.destroy();
        }
        if (descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        for (auto layout : descriptorSetLayouts) {
            if (layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
        if (pipeline_precomp != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline_precomp, nullptr);
        if (pipeline_preprocess != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline_preprocess, nullptr);
        if (pipeline_prefixSum != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline_prefixSum, nullptr);
        if (pipeline_preprocessSort != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline_preprocessSort, nullptr);
        if (pipeline_hist != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline_hist, nullptr);
        if (pipeline_sort != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline_sort, nullptr);
        if (pipeline_tileBoundary != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline_tileBoundary, nullptr);
        if (pipeline_render != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline_render, nullptr);
        if (layout_precomp != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout_precomp, nullptr);
        if (layout_preprocess != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout_preprocess, nullptr);
        if (layout_prefixSum != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout_prefixSum, nullptr);
        if (layout_preprocessSort != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout_preprocessSort, nullptr);
        if (layout_hist != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout_hist, nullptr);
        if (layout_sort != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout_sort, nullptr);
        if (layout_tileBoundary != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout_tileBoundary, nullptr);
        if (layout_render != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout_render, nullptr);
        renderFinishedSemaphores.clear();
        imageAvailableSemaphores.clear();
        fenceRender.reset();
        fenceCompute.reset();
        TerminateWindow();
    }

private:
    std::vector<GSVertex> vertices;
    GSCameraState camera;
    VkExtent2D lastExtent{};

    commandPool commandPoolGraphics;
    commandBuffer cmdBuffers[2];
    std::unique_ptr<fence> fenceCompute;
    std::unique_ptr<fence> fenceRender;
    std::vector<semaphore> imageAvailableSemaphores;
    std::vector<semaphore> renderFinishedSemaphores;
    uint32_t frameSemaphoreIndex = 0;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    VkPipelineLayout layout_precomp = VK_NULL_HANDLE;
    VkPipelineLayout layout_preprocess = VK_NULL_HANDLE;
    VkPipelineLayout layout_prefixSum = VK_NULL_HANDLE;
    VkPipelineLayout layout_preprocessSort = VK_NULL_HANDLE;
    VkPipelineLayout layout_hist = VK_NULL_HANDLE;
    VkPipelineLayout layout_sort = VK_NULL_HANDLE;
    VkPipelineLayout layout_tileBoundary = VK_NULL_HANDLE;
    VkPipelineLayout layout_render = VK_NULL_HANDLE;
    VkPipeline pipeline_precomp = VK_NULL_HANDLE;
    VkPipeline pipeline_preprocess = VK_NULL_HANDLE;
    VkPipeline pipeline_prefixSum = VK_NULL_HANDLE;
    VkPipeline pipeline_preprocessSort = VK_NULL_HANDLE;
    VkPipeline pipeline_hist = VK_NULL_HANDLE;
    VkPipeline pipeline_sort = VK_NULL_HANDLE;
    VkPipeline pipeline_tileBoundary = VK_NULL_HANDLE;
    VkPipeline pipeline_render = VK_NULL_HANDLE;

    deviceLocalBuffer vertexBuffer;
    deviceLocalBuffer cov3DBuffer;
    deviceLocalBuffer uniformBuffer;
    deviceLocalBuffer vertexAttributeBuffer;
    deviceLocalBuffer tileOverlapBuffer;
    deviceLocalBuffer prefixSumPingBuffer;
    deviceLocalBuffer prefixSumPongBuffer;
    bufferMemory totalSumBufferHost;
    deviceLocalBuffer sortKBufferEven;
    deviceLocalBuffer sortKBufferOdd;
    deviceLocalBuffer sortVBufferEven;
    deviceLocalBuffer sortVBufferOdd;
    deviceLocalBuffer sortHistBuffer;
    deviceLocalBuffer tileBoundaryBuffer;

    std::vector<StorageImage> depthOutputs;
    std::vector<StorageImage> normalOutputs;
    uint32_t sortBufferSizeMultiplier = 1;

    VkDescriptorSet set_precomp = VK_NULL_HANDLE;
    VkDescriptorSet set_preprocess0 = VK_NULL_HANDLE;
    VkDescriptorSet set_preprocess1 = VK_NULL_HANDLE;
    VkDescriptorSet set_prefix = VK_NULL_HANDLE;
    VkDescriptorSet set_preprocessSortPing = VK_NULL_HANDLE;
    VkDescriptorSet set_preprocessSortPong = VK_NULL_HANDLE;
    VkDescriptorSet set_hist_even = VK_NULL_HANDLE;
    VkDescriptorSet set_hist_odd = VK_NULL_HANDLE;
    VkDescriptorSet set_sort_even = VK_NULL_HANDLE;
    VkDescriptorSet set_sort_odd = VK_NULL_HANDLE;
    VkDescriptorSet set_tileBoundary = VK_NULL_HANDLE;
    VkDescriptorSet set_render0 = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> set_render1;

    static VkBufferMemoryBarrier bufferBarrier(VkBuffer buf, VkAccessFlags src, VkAccessFlags dst) {
        return VkBufferMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = src,
            .dstAccessMask = dst,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buf,
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };
    }

    void writeBuffer(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkBuffer buf, VkDeviceSize range) {
        VkDescriptorBufferInfo bi{buf, 0, range};
        VkWriteDescriptorSet wr{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set,
            .dstBinding = binding,
            .descriptorCount = 1,
            .descriptorType = type,
            .pBufferInfo = &bi
        };
        vkUpdateDescriptorSets(GraphicsBase::Base().Device(), 1, &wr, 0, nullptr);
    }

    void fitCameraToScene() {
        if (vertices.empty()) {
            return;
        }
        glm::vec3 minP = glm::vec3(vertices[0].position);
        glm::vec3 maxP = glm::vec3(vertices[0].position);
        for (const auto& v : vertices) {
            const glm::vec3 p = glm::vec3(v.position);
            minP.x = (std::min)(minP.x, p.x);
            minP.y = (std::min)(minP.y, p.y);
            minP.z = (std::min)(minP.z, p.z);
            maxP.x = (std::max)(maxP.x, p.x);
            maxP.y = (std::max)(maxP.y, p.y);
            maxP.z = (std::max)(maxP.z, p.z);
        }
        const glm::vec3 center = (minP + maxP) * 0.5f;
        const float radius = glm::length(maxP - minP) * 0.5f;
        const float safeRadius = (std::max)(radius, 0.1f);
        const float fovRad = glm::radians(camera.fov);
        const float distance = safeRadius / std::tan(fovRad * 0.5f) + safeRadius;
        camera.position = center + glm::vec3(0.0f, 0.0f, distance);
        camera.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        camera.nearPlane = (std::max)(0.01f, distance - safeRadius * 4.0f);
        camera.farPlane = distance + safeRadius * 4.0f;
    }

    void createCommandResources() {
        commandPoolGraphics.Create(GraphicsBase::Base().QueueFamilyIndex_Graphics(),
                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        commandPoolGraphics.AllocateBuffers(cmdBuffers);
    }

    void createSyncResources() {
        fenceCompute = std::make_unique<fence>(VK_FENCE_CREATE_SIGNALED_BIT);
        fenceRender = std::make_unique<fence>(VK_FENCE_CREATE_SIGNALED_BIT);
        imageAvailableSemaphores.clear();
        renderFinishedSemaphores.clear();
        const uint32_t count = GraphicsBase::Base().SwapchainImageCount();
        imageAvailableSemaphores.reserve(count);
        renderFinishedSemaphores.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            imageAvailableSemaphores.emplace_back();
            renderFinishedSemaphores.emplace_back();
        }
    }

    void createBuffers() {
        const uint32_t n = static_cast<uint32_t>(vertices.size());
        const auto extent = windowSize;
        const uint32_t tileX = ceilDiv(extent.width, kTileWidth);
        const uint32_t tileY = ceilDiv(extent.height, kTileHeight);

        vertexBuffer.Create(sizeof(GSVertex) * n, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        cov3DBuffer.Create(sizeof(float) * 6 * n, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        uniformBuffer.Create(sizeof(GSUniformBufferCPU), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        vertexAttributeBuffer.Create(sizeof(GSVertexAttributeCPU) * n, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        tileOverlapBuffer.Create(sizeof(uint32_t) * n, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        prefixSumPingBuffer.Create(sizeof(uint32_t) * n, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        prefixSumPongBuffer.Create(sizeof(uint32_t) * n, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

        VkBufferCreateInfo hostInfo{
            .size = sizeof(uint32_t),
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        };
        checkVk(totalSumBufferHost.Create(hostInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
                "Failed to create host total-sum buffer");

        const uint32_t maxInstances = n * sortBufferSizeMultiplier;
        sortKBufferEven.Create(sizeof(uint64_t) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        sortKBufferOdd.Create(sizeof(uint64_t) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        sortVBufferEven.Create(sizeof(uint32_t) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        sortVBufferOdd.Create(sizeof(uint32_t) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        const uint32_t globalInvocation = ceilDiv(maxInstances, kSortBlocksPerWorkgroup);
        const uint32_t numWorkgroups = ceilDiv(globalInvocation, 256u);
        sortHistBuffer.Create(sizeof(uint32_t) * 256u * numWorkgroups, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        tileBoundaryBuffer.Create(sizeof(uint32_t) * tileX * tileY * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        vertexBuffer.TransferData(vertices.data(), sizeof(GSVertex) * n);
    }

    VkPipeline createComputePipeline(const char* spvName, VkPipelineLayout layout) {
        std::string shaderPath = std::string("resources/compiled_shaders/") + spvName;
        shaderModule shader(shaderPath.c_str());
        VkComputePipelineCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = shader.StageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT),
            .layout = layout
        };
        VkPipeline p = VK_NULL_HANDLE;
        checkVk(vkCreateComputePipelines(GraphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &info, nullptr, &p),
                "Failed to create compute pipeline");
        return p;
    }

    void createDescriptorResources();
    void createPipelines();
    void precomputeCov3D();
    void createOutputImagesAndRenderSets();
    void updateUniforms();
    PreprocessResult runPreprocessPass();
    void ensureSortCapacity(uint32_t numInstances);
    void rebuildResizeDependentResources();
    void drawFrame();
};

void GSComputeRendererImpl::createDescriptorResources() {
    descriptorPool = createDescriptorPool();
    descriptorSetLayouts.resize(10, VK_NULL_HANDLE);
    descriptorSetLayouts[0] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[1] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[2] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[3] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[4] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[5] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[6] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[7] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[8] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});
    descriptorSetLayouts[9] = createDescriptorSetLayout({{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}});

    set_precomp = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[0]);
    set_preprocess0 = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[1]);
    set_preprocess1 = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[2]);
    set_prefix = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[3]);
    set_preprocessSortPing = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[4]);
    set_preprocessSortPong = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[4]);
    set_hist_even = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[5]);
    set_hist_odd = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[5]);
    set_sort_even = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[6]);
    set_sort_odd = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[6]);
    set_tileBoundary = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[7]);
    set_render0 = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[8]);

    writeBuffer(set_precomp, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertexBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_precomp, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, cov3DBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocess0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertexBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocess0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, cov3DBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocess1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffer, sizeof(GSUniformBufferCPU));
    writeBuffer(set_preprocess1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertexAttributeBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocess1, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tileOverlapBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_prefix, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, prefixSumPingBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_prefix, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, prefixSumPongBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPing, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertexAttributeBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPing, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, prefixSumPingBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPing, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPing, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPong, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertexAttributeBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPong, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, prefixSumPongBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPong, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPong, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_hist_even, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_hist_even, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortHistBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_hist_odd, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_hist_odd, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortHistBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortHistBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortHistBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_tileBoundary, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_tileBoundary, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tileBoundaryBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_render0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertexAttributeBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_render0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tileBoundaryBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_render0, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
}

void GSComputeRendererImpl::createPipelines() {
    auto createLayout = [&](const std::vector<VkDescriptorSetLayout>& sets, uint32_t pushSize, VkPipelineLayout& outLayout) {
        VkPushConstantRange pushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, pushSize};
        VkPipelineLayoutCreateInfo info{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
            static_cast<uint32_t>(sets.size()), sets.data(),
            pushSize > 0 ? 1u : 0u, pushSize > 0 ? &pushRange : nullptr
        };
        checkVk(vkCreatePipelineLayout(GraphicsBase::Base().Device(), &info, nullptr, &outLayout), "Failed to create pipeline layout");
    };

    createLayout({descriptorSetLayouts[0]}, sizeof(float), layout_precomp);
    createLayout({descriptorSetLayouts[1], descriptorSetLayouts[2]}, 0, layout_preprocess);
    createLayout({descriptorSetLayouts[3]}, sizeof(uint32_t), layout_prefixSum);
    createLayout({descriptorSetLayouts[4]}, sizeof(uint32_t), layout_preprocessSort);
    createLayout({descriptorSetLayouts[5]}, sizeof(GSRadixSortPushConstants), layout_hist);
    createLayout({descriptorSetLayouts[6]}, sizeof(GSRadixSortPushConstants), layout_sort);
    createLayout({descriptorSetLayouts[7]}, sizeof(uint32_t), layout_tileBoundary);
    createLayout({descriptorSetLayouts[8], descriptorSetLayouts[9]}, sizeof(GSRenderPushConstants), layout_render);

    pipeline_precomp = createComputePipeline("gs_precomp_cov3d_comp.spv", layout_precomp);
    pipeline_preprocess = createComputePipeline("gs_preprocess_comp.spv", layout_preprocess);
    pipeline_prefixSum = createComputePipeline("gs_prefix_sum_comp.spv", layout_prefixSum);
    pipeline_preprocessSort = createComputePipeline("gs_preprocess_sort_comp.spv", layout_preprocessSort);
    pipeline_hist = createComputePipeline("gs_hist_comp.spv", layout_hist);
    pipeline_sort = createComputePipeline("gs_sort_comp.spv", layout_sort);
    pipeline_tileBoundary = createComputePipeline("gs_tile_boundary_comp.spv", layout_tileBoundary);
    pipeline_render = createComputePipeline("gs_render_comp.spv", layout_render);
}

void GSComputeRendererImpl::precomputeCov3D() {
    auto& cmd = cmdBuffers[0];
    cmd.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_precomp);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_precomp, 0, 1, &set_precomp, 0, nullptr);
    float scale = 1.0f;
    vkCmdPushConstants(cmd, layout_precomp, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &scale);
    vkCmdDispatch(cmd, ceilDiv(static_cast<uint32_t>(vertices.size()), 256u), 1, 1);
    cmd.End();
    fenceCompute->Reset();
    GraphicsBase::Base().SubmitCommandBuffer_Graphics(cmd, *fenceCompute);
    fenceCompute->WaitAndReset();
}

void GSComputeRendererImpl::createOutputImagesAndRenderSets() {
    const auto count = GraphicsBase::Base().SwapchainImageCount();
    depthOutputs.resize(count);
    normalOutputs.resize(count);
    set_render1.resize(count, VK_NULL_HANDLE);
    const auto extent = windowSize;
    const auto fmt = GraphicsBase::Base().SwapchainCreateInfo().imageFormat;
    for (uint32_t i = 0; i < count; ++i) {
        depthOutputs[i].create(extent, fmt);
        normalOutputs[i].create(extent, fmt);
        set_render1[i] = allocateDescriptorSet(descriptorPool, descriptorSetLayouts[9]);
        VkDescriptorImageInfo colorInfo{VK_NULL_HANDLE, GraphicsBase::Base().SwapchainImageView(i), VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo depthInfo{VK_NULL_HANDLE, depthOutputs[i].view, VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo normalInfo{VK_NULL_HANDLE, normalOutputs[i].view, VK_IMAGE_LAYOUT_GENERAL};
        std::array<VkWriteDescriptorSet, 3> writes{
            VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set_render1[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &colorInfo},
            VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set_render1[i], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &depthInfo},
            VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set_render1[i], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &normalInfo}
        };
        vkUpdateDescriptorSets(GraphicsBase::Base().Device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void GSComputeRendererImpl::updateUniforms() {
    GSUniformBufferCPU u{};
    auto extent = windowSize;
    u.width = extent.width;
    u.height = extent.height;
    u.camera_position = glm::vec4(camera.position, 1.0f);
    auto rotation = glm::mat4_cast(camera.rotation);
    auto translation = glm::translate(glm::mat4(1.0f), camera.position);
    auto view = glm::inverse(translation * rotation);
    const float tanFovX = std::tan(glm::radians(camera.fov) * 0.5f);
    const float tanFovY = tanFovX * (static_cast<float>(extent.height) / static_cast<float>(extent.width));
    u.view_mat = view;
    u.proj_mat = glm::perspective(std::atan(tanFovY) * 2.0f, static_cast<float>(extent.width) / static_cast<float>(extent.height), camera.nearPlane, camera.farPlane) * view;
    u.view_mat[0][1] *= -1.0f; u.view_mat[1][1] *= -1.0f; u.view_mat[2][1] *= -1.0f; u.view_mat[3][1] *= -1.0f;
    u.view_mat[0][2] *= -1.0f; u.view_mat[1][2] *= -1.0f; u.view_mat[2][2] *= -1.0f; u.view_mat[3][2] *= -1.0f;
    u.proj_mat[0][1] *= -1.0f; u.proj_mat[1][1] *= -1.0f; u.proj_mat[2][1] *= -1.0f; u.proj_mat[3][1] *= -1.0f;
    u.tan_fovx = tanFovX;
    u.tan_fovy = tanFovY;
    uniformBuffer.TransferData(u);
}

PreprocessResult GSComputeRendererImpl::runPreprocessPass() {
    auto& cmd = cmdBuffers[0];
    const uint32_t n = static_cast<uint32_t>(vertices.size());
    const uint32_t groups = ceilDiv(n, 256u);
    const uint32_t iters = static_cast<uint32_t>(std::ceil(std::log2(static_cast<float>(n))));
    cmd.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_preprocess);
    std::array<VkDescriptorSet, 2> sets{set_preprocess0, set_preprocess1};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_preprocess, 0, 2, sets.data(), 0, nullptr);
    vkCmdDispatch(cmd, groups, 1, 1);
    auto b0 = bufferBarrier(tileOverlapBuffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &b0, 0, nullptr);
    VkBufferCopy copyRegion{0, 0, sizeof(uint32_t) * n};
    vkCmdCopyBuffer(cmd, tileOverlapBuffer, prefixSumPingBuffer, 1, &copyRegion);
    auto b1 = bufferBarrier(prefixSumPingBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b1, 0, nullptr);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_prefixSum);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_prefixSum, 0, 1, &set_prefix, 0, nullptr);
    for (uint32_t t = 0; t <= iters; ++t) {
        vkCmdPushConstants(cmd, layout_prefixSum, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &t);
        vkCmdDispatch(cmd, groups, 1, 1);
        VkBuffer srcBuf = (t % 2 == 0) ? static_cast<VkBuffer>(prefixSumPongBuffer) : static_cast<VkBuffer>(prefixSumPingBuffer);
        auto bb = bufferBarrier(srcBuf, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bb, 0, nullptr);
    }
    const bool prefixInPing = (iters % 2 == 0);
    VkBuffer srcPrefix = prefixInPing ? static_cast<VkBuffer>(prefixSumPingBuffer) : static_cast<VkBuffer>(prefixSumPongBuffer);
    VkBufferCopy tailCopy{(n - 1) * sizeof(uint32_t), 0, sizeof(uint32_t)};
    vkCmdCopyBuffer(cmd, srcPrefix, totalSumBufferHost.Buffer(), 1, &tailCopy);
    cmd.End();
    fenceCompute->Reset();
    GraphicsBase::Base().SubmitCommandBuffer_Graphics(cmd, *fenceCompute);
    fenceCompute->WaitAndReset();
    uint32_t total = 0;
    totalSumBufferHost.RetrieveData(&total, sizeof(uint32_t), 0);
    return PreprocessResult{total, prefixInPing};
}

void GSComputeRendererImpl::ensureSortCapacity(uint32_t numInstances) {
    const uint32_t n = static_cast<uint32_t>(vertices.size());
    if (numInstances <= n * sortBufferSizeMultiplier) return;
    while (numInstances > n * sortBufferSizeMultiplier) sortBufferSizeMultiplier++;
    const uint32_t maxInstances = n * sortBufferSizeMultiplier;
    sortKBufferEven.Recreate(sizeof(uint64_t) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    sortKBufferOdd.Recreate(sizeof(uint64_t) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    sortVBufferEven.Recreate(sizeof(uint32_t) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    sortVBufferOdd.Recreate(sizeof(uint32_t) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const uint32_t globalInvocation = ceilDiv(maxInstances, kSortBlocksPerWorkgroup);
    const uint32_t numWorkgroups = ceilDiv(globalInvocation, 256u);
    sortHistBuffer.Recreate(sizeof(uint32_t) * 256u * numWorkgroups, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    writeBuffer(set_preprocessSortPing, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPing, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPong, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_preprocessSortPong, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_hist_even, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_hist_odd, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_hist_even, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortHistBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_hist_odd, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortHistBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_sort_even, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortHistBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferOdd, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_sort_odd, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortHistBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_tileBoundary, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortKBufferEven, VK_WHOLE_SIZE);
    writeBuffer(set_render0, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sortVBufferEven, VK_WHOLE_SIZE);
}

void GSComputeRendererImpl::rebuildResizeDependentResources() {
    auto extent = windowSize;
    if (extent.width == lastExtent.width && extent.height == lastExtent.height) return;
    GraphicsBase::Base().WaitIdle();
    lastExtent = extent;
    for (auto& img : depthOutputs) img.destroy();
    for (auto& img : normalOutputs) img.destroy();
    const uint32_t tileX = ceilDiv(extent.width, kTileWidth);
    const uint32_t tileY = ceilDiv(extent.height, kTileHeight);
    tileBoundaryBuffer.Recreate(sizeof(uint32_t) * tileX * tileY * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    writeBuffer(set_tileBoundary, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tileBoundaryBuffer, VK_WHOLE_SIZE);
    writeBuffer(set_render0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tileBoundaryBuffer, VK_WHOLE_SIZE);
    createOutputImagesAndRenderSets();
}

void GSComputeRendererImpl::drawFrame() {
    rebuildResizeDependentResources();
    updateUniforms();
    VkSemaphore acquireSemaphore = imageAvailableSemaphores[frameSemaphoreIndex];
    GraphicsBase::Base().SwapImage(acquireSemaphore);
    const uint32_t imageIndex = GraphicsBase::Base().CurrentImageIndex();
    VkSemaphore renderFinishedSemaphore = renderFinishedSemaphores[imageIndex];
    auto prep = runPreprocessPass();
    uint32_t numInstances = prep.numInstances == 0 ? 1 : prep.numInstances;
    ensureSortCapacity(numInstances);
    const uint32_t n = static_cast<uint32_t>(vertices.size());
    const uint32_t groups = ceilDiv(n, 256u);
    const uint32_t sortInvocation = ceilDiv(ceilDiv(numInstances, kSortBlocksPerWorkgroup), 256u);
    auto& cmd = cmdBuffers[1];
    cmd.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_preprocessSort);
    VkDescriptorSet preSortSet = prep.prefixInPing ? set_preprocessSortPing : set_preprocessSortPong;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_preprocessSort, 0, 1, &preSortSet, 0, nullptr);
    uint32_t tileX = ceilDiv(windowSize.width, kTileWidth);
    vkCmdPushConstants(cmd, layout_preprocessSort, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &tileX);
    vkCmdDispatch(cmd, groups, 1, 1);
    auto bps = bufferBarrier(sortKBufferEven, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bps, 0, nullptr);
    for (uint32_t i = 0; i < 8; ++i) {
        GSRadixSortPushConstants pc{numInstances, i * 8, sortInvocation, kSortBlocksPerWorkgroup};
        VkDescriptorSet histSet = (i % 2 == 0) ? set_hist_even : set_hist_odd;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_hist);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_hist, 0, 1, &histSet, 0, nullptr);
        vkCmdPushConstants(cmd, layout_hist, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GSRadixSortPushConstants), &pc);
        vkCmdDispatch(cmd, sortInvocation, 1, 1);
        auto bh = bufferBarrier(sortHistBuffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bh, 0, nullptr);
        VkDescriptorSet sortSet = (i % 2 == 0) ? set_sort_even : set_sort_odd;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_sort);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_sort, 0, 1, &sortSet, 0, nullptr);
        vkCmdPushConstants(cmd, layout_sort, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GSRadixSortPushConstants), &pc);
        vkCmdDispatch(cmd, sortInvocation, 1, 1);
        VkBuffer outKey = (i % 2 == 0) ? static_cast<VkBuffer>(sortKBufferOdd) : static_cast<VkBuffer>(sortKBufferEven);
        auto bo = bufferBarrier(outKey, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bo, 0, nullptr);
    }
    vkCmdFillBuffer(cmd, tileBoundaryBuffer, 0, VK_WHOLE_SIZE, 0);
    auto tbFill = bufferBarrier(tileBoundaryBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &tbFill, 0, nullptr);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_tileBoundary);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_tileBoundary, 0, 1, &set_tileBoundary, 0, nullptr);
    vkCmdPushConstants(cmd, layout_tileBoundary, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &numInstances);
    vkCmdDispatch(cmd, ceilDiv(numInstances, 256u), 1, 1);
    auto tbRead = bufferBarrier(tileBoundaryBuffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &tbRead, 0, nullptr);
    VkImageMemoryBarrier colorBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    colorBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = GraphicsBase::Base().SwapchainImage(imageIndex);
    colorBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &colorBarrier);
    auto transitionStorageImage = [&](VkImage img) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    };
    transitionStorageImage(depthOutputs[imageIndex].imageHandle);
    transitionStorageImage(normalOutputs[imageIndex].imageHandle);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_render);
    std::array<VkDescriptorSet, 2> renderSets{set_render0, set_render1[imageIndex]};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_render, 0, 2, renderSets.data(), 0, nullptr);
    GSRenderPushConstants renderPC{windowSize.width, windowSize.height, camera.nearPlane, camera.farPlane};
    vkCmdPushConstants(cmd, layout_render, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GSRenderPushConstants), &renderPC);
    vkCmdDispatch(cmd, ceilDiv(windowSize.width, kTileWidth), ceilDiv(windowSize.height, kTileHeight), 1);
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    colorBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &colorBarrier);
    cmd.End();
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkSubmitInfo submitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &acquireSemaphore, &waitStage, 1, cmd.Address(), 1, &renderFinishedSemaphore
    };
    fenceRender->Reset();
    GraphicsBase::Base().SubmitCommandBuffer_Graphics(submitInfo, *fenceRender);
    GraphicsBase::Base().PresentImage(renderFinishedSemaphore);
    fenceRender->WaitAndReset();
    frameSemaphoreIndex = (frameSemaphoreIndex + 1) % static_cast<uint32_t>(imageAvailableSemaphores.size());
}

GSComputeRendererImpl* g_renderer = nullptr;

} // namespace

bool GSComputeRenderer::initialize(const std::vector<GSVertex>& vertices) {
    if (!g_renderer) {
        g_renderer = new GSComputeRendererImpl();
    }
    return g_renderer->initialize(vertices);
}

void GSComputeRenderer::run() {
    if (g_renderer) {
        g_renderer->run();
    }
}

void GSComputeRenderer::shutdown() {
    if (g_renderer) {
        g_renderer->shutdown();
        delete g_renderer;
        g_renderer = nullptr;
    }
}

