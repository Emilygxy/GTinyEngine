#include "framework/VulkanGeometryPass.h"

#include "GTVulkan/EasyVulkan.h"
#include "materials/PBRMaterial.h"
#include "textures/Texture.h"
#include "filesystem.h"
#include "mesh/Vertex.h"
#include <cstring>
#include <stb_image.h>

namespace te {

bool VulkanGeometryPass::Initialize(const VulkanGeometryPassCreateInfo& createInfo)
{
    Shutdown();

    extent_ = createInfo.extent;
    renderPass_ = createInfo.renderPass;
    pipeline_ = createInfo.pipeline;
    pipelineLayout_ = createInfo.pipelineLayout;
    depthFormat_ = createInfo.depthFormat;

    if (extent_.width == 0 || extent_.height == 0) {
        return false;
    }

    vk::GBufferCreateInfo gbufferCi{};
    gbufferCi.extent = extent_;
    gbufferCi.depthFormat = depthFormat_;
    if (!gbuffer_.Initialize(gbufferCi)) {
        return false;
    }
    if (!CreatePerObjectDescriptorResources()) {
        return false;
    }
    if (!CreateTextureDescriptorResources()) {
        return false;
    }

    if (renderPass_ != VK_NULL_HANDLE && !gbuffer_.BuildFramebuffer(renderPass_)) {
        return false;
    }

    return true;
}

void VulkanGeometryPass::Shutdown()
{
    DestroyMeshBuffers();
    DestroyPerObjectDescriptorResources();
    DestroyTextureDescriptorResources();
    gbuffer_.Destroy();
    renderPass_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    extent_ = { 0, 0 };
}

bool VulkanGeometryPass::Resize(VkExtent2D extent)
{
    extent_ = extent;
    if (!gbuffer_.Resize(extent_)) {
        return false;
    }
    return RebuildFramebuffer();
}

void VulkanGeometryPass::SetPipeline(VkPipeline pipeline, VkPipelineLayout layout)
{
    pipeline_ = pipeline;
    pipelineLayout_ = layout;
}

void VulkanGeometryPass::SetRenderPass(VkRenderPass renderPass)
{
    renderPass_ = renderPass;
    RebuildFramebuffer();
}

void VulkanGeometryPass::Record(VkCommandBuffer commandBuffer, const std::vector<RenderCommand>& commands)
{
    if (commandBuffer == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE || gbuffer_.Framebuffer() == VK_NULL_HANDLE) {
        return;
    }

    // M1 skeleton:
    // 1) Transition gbuffer images to attachment layouts.
    // 2) Begin geometry render pass and bind pipeline.
    // 3) Iterate render commands (draw path will be filled in later M1 tasks).
    gbuffer_.CmdTransitionForGeometryWrite(commandBuffer);
    UpdateCameraUbo();

    std::vector<VkClearValue> clearValues = BuildClearValues();
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass_;
    beginInfo.framebuffer = gbuffer_.Framebuffer();
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = extent_;
    beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (pipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        // Minimal M1 draw path:
        // Upload each fragment mesh into transient host-visible buffers and draw with
        // a fixed Vertex layout { vec3 position, vec3 normal, vec2 texCoord }.
        for (const auto& command : commands) {
            if (!command.fragmentsSource) {
                continue;
            }

            const auto& fragments = command.fragmentsSource->GetFragments();
            for (const auto& fragment : fragments) {
                if (fragment.mpGeometry == nullptr) {
                    continue;
                }
                const auto vertices = fragment.mpGeometry->GetVertices();
                const auto indices = fragment.mpGeometry->GetIndices();
                if (vertices.empty() || indices.empty()) {
                    continue;
                }

                VulkanMeshBuffer meshBuffer{};
                if (!GetOrCreateMeshBuffer(vertices, indices, meshBuffer)) {
                    continue;
                }
                UpdateModelUbo(fragment.mpGeometry->GetWorldTransform());

                VkDeviceSize vertexOffset = 0;
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshBuffer.vertexBuffer, &vertexOffset);
                vkCmdBindIndexBuffer(commandBuffer, meshBuffer.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                if (pipelineLayout_ != VK_NULL_HANDLE && descriptorSet_ != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
                }
                VkDescriptorSet materialSet = VK_NULL_HANDLE;
                if (GetOrCreateMaterialTextureSet(command.fragmentsSource->GetMaterial(), materialSet) &&
                    pipelineLayout_ != VK_NULL_HANDLE && materialSet != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &materialSet, 0, nullptr);
                }
                vkCmdDrawIndexed(commandBuffer, meshBuffer.indexCount, 1, 0, 0, 0);
            }
        }
    } else {
        (void)commands;
    }

    vkCmdEndRenderPass(commandBuffer);
}

bool VulkanGeometryPass::RebuildFramebuffer()
{
    if (renderPass_ == VK_NULL_HANDLE) {
        return false;
    }
    return gbuffer_.BuildFramebuffer(renderPass_);
}

std::vector<VkClearValue> VulkanGeometryPass::BuildClearValues() const
{
    std::vector<VkClearValue> clearValues;
    clearValues.resize(static_cast<size_t>(vk::GBufferSlot::Count) + 1);
    for (size_t i = 0; i < static_cast<size_t>(vk::GBufferSlot::Count); ++i) {
        clearValues[i].color = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    clearValues.back().depthStencil = { 1.0f, 0 };
    return clearValues;
}

size_t VulkanGeometryPass::HashMeshData(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
    constexpr size_t kFNVOffsetBasis = sizeof(size_t) == 8 ? 1469598103934665603ull : 2166136261u;
    constexpr size_t kFNVPrime = sizeof(size_t) == 8 ? 1099511628211ull : 16777619u;
    size_t hash = kFNVOffsetBasis;

    auto hashBytes = [&hash, kFNVPrime](const uint8_t* bytes, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            hash ^= static_cast<size_t>(bytes[i]);
            hash *= kFNVPrime;
        }
    };
    hashBytes(reinterpret_cast<const uint8_t*>(vertices.data()), vertices.size() * sizeof(Vertex));
    hashBytes(reinterpret_cast<const uint8_t*>(indices.data()), indices.size() * sizeof(unsigned int));
    return hash;
}

bool VulkanGeometryPass::GetOrCreateMeshBuffer(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, VulkanMeshBuffer& outBuffer)
{
    const size_t meshHash = HashMeshData(vertices, indices);
    const auto it = meshBuffers_.find(meshHash);
    if (it != meshBuffers_.end()) {
        outBuffer = it->second;
        return true;
    }

    VulkanMeshBuffer meshBuffer{};
    const VkDeviceSize vertexDataSize = static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
    const VkDeviceSize indexDataSize = static_cast<VkDeviceSize>(indices.size() * sizeof(uint32_t));
    if (!UploadDeviceLocalBuffer(vertices.data(), vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, meshBuffer.vertexBuffer, meshBuffer.vertexMemory) ||
        !UploadDeviceLocalBuffer(indices.data(), indexDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, meshBuffer.indexBuffer, meshBuffer.indexMemory)) {
        DestroyMeshBuffer(meshBuffer);
        return false;
    }

    meshBuffer.vertexCount = static_cast<uint32_t>(vertices.size());
    meshBuffer.indexCount = static_cast<uint32_t>(indices.size());
    meshBuffers_.emplace(meshHash, meshBuffer);
    outBuffer = meshBuffer;
    return true;
}

void VulkanGeometryPass::DestroyMeshBuffers()
{
    for (auto& [_, meshBuffer] : meshBuffers_) {
        DestroyMeshBuffer(meshBuffer);
    }
    meshBuffers_.clear();
}

void VulkanGeometryPass::DestroyMeshBuffer(VulkanMeshBuffer& meshBuffer)
{
    if (meshBuffer.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), meshBuffer.vertexBuffer, nullptr);
        meshBuffer.vertexBuffer = VK_NULL_HANDLE;
    }
    if (meshBuffer.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), meshBuffer.vertexMemory, nullptr);
        meshBuffer.vertexMemory = VK_NULL_HANDLE;
    }
    if (meshBuffer.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), meshBuffer.indexBuffer, nullptr);
        meshBuffer.indexBuffer = VK_NULL_HANDLE;
    }
    if (meshBuffer.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), meshBuffer.indexMemory, nullptr);
        meshBuffer.indexMemory = VK_NULL_HANDLE;
    }
    meshBuffer.indexCount = 0;
    meshBuffer.vertexCount = 0;
}

bool VulkanGeometryPass::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& outBuffer, VkDeviceMemory& outMemory)
{
    VkBufferCreateInfo bufferCi{};
    bufferCi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCi.size = size;
    bufferCi.usage = usage;
    bufferCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk::GraphicsBase::Base().Device(), &bufferCi, nullptr, &outBuffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(vk::GraphicsBase::Base().Device(), outBuffer, &memoryRequirements);
    const uint32_t memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);
    if (memoryTypeIndex == UINT32_MAX) {
        return false;
    }

    VkMemoryAllocateInfo allocCi{};
    allocCi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocCi.allocationSize = memoryRequirements.size;
    allocCi.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(vk::GraphicsBase::Base().Device(), &allocCi, nullptr, &outMemory) != VK_SUCCESS) {
        return false;
    }
    return vkBindBufferMemory(vk::GraphicsBase::Base().Device(), outBuffer, outMemory, 0) == VK_SUCCESS;
}

uint32_t VulkanGeometryPass::FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties)
{
    const auto& memProps = vk::GraphicsBase::Base().PhysicalDeviceMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool memorySupported = (memoryTypeBits & (1u << i)) != 0;
        const bool propertySupported = (memProps.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties;
        if (memorySupported && propertySupported) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool VulkanGeometryPass::UploadDeviceLocalBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuffer, VkDeviceMemory& outMemory)
{
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory)) {
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), stagingMemory, 0, size, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), stagingBuffer, nullptr);
        vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);
        return false;
    }
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(vk::GraphicsBase::Base().Device(), stagingMemory);

    if (!CreateBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outBuffer, outMemory)) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), stagingBuffer, nullptr);
        vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);
        return false;
    }

    auto& transferCmd = vk::GraphicsBase::Plus().CommandBuffer_Transfer();
    transferCmd.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VkBufferCopy copyRegion{ 0, 0, size };
    vkCmdCopyBuffer(transferCmd, stagingBuffer, outBuffer, 1, &copyRegion);
    transferCmd.End();
    vk::GraphicsBase::Plus().ExecuteCommandBuffer_Graphics(transferCmd);

    vkDestroyBuffer(vk::GraphicsBase::Base().Device(), stagingBuffer, nullptr);
    vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);
    return true;
}

VkVertexInputBindingDescription VulkanGeometryPass::VertexBindingDescription()
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = static_cast<uint32_t>(sizeof(Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 3> VulkanGeometryPass::VertexAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, position)) };
    attributes[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal)) };
    attributes[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, texCoords)) };
    return attributes;
}

void VulkanGeometryPass::SetViewProjection(const glm::mat4& view, const glm::mat4& proj)
{
    view_ = view;
    proj_ = proj;
}

bool VulkanGeometryPass::CreatePerObjectDescriptorResources()
{
    if (!CreateBuffer(sizeof(CameraUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      cameraUboBuffer_, cameraUboMemory_)) {
        return false;
    }
    if (!CreateBuffer(sizeof(ModelUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      modelUboBuffer_, modelUboMemory_)) {
        return false;
    }

    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 2;
    layoutCi.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(vk::GraphicsBase::Base().Device(), &layoutCi, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    if (descriptorPool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolCreateInfo poolCi{};
        poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCi.maxSets = 2;
        poolCi.poolSizeCount = 2;
        poolCi.pPoolSizes = poolSizes;
        if (vkCreateDescriptorPool(vk::GraphicsBase::Base().Device(), &poolCi, nullptr, &descriptorPool_) != VK_SUCCESS) {
            return false;
        }
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout_;
    if (vkAllocateDescriptorSets(vk::GraphicsBase::Base().Device(), &allocInfo, &descriptorSet_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorBufferInfo cameraInfo{ cameraUboBuffer_, 0, sizeof(CameraUbo) };
    VkDescriptorBufferInfo modelInfo{ modelUboBuffer_, 0, sizeof(ModelUbo) };
    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet_;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &cameraInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet_;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &modelInfo;
    vkUpdateDescriptorSets(vk::GraphicsBase::Base().Device(), 2, writes, 0, nullptr);
    return true;
}

void VulkanGeometryPass::DestroyPerObjectDescriptorResources()
{
    if (cameraUboBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), cameraUboBuffer_, nullptr);
        cameraUboBuffer_ = VK_NULL_HANDLE;
    }
    if (cameraUboMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), cameraUboMemory_, nullptr);
        cameraUboMemory_ = VK_NULL_HANDLE;
    }
    if (modelUboBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), modelUboBuffer_, nullptr);
        modelUboBuffer_ = VK_NULL_HANDLE;
    }
    if (modelUboMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), modelUboMemory_, nullptr);
        modelUboMemory_ = VK_NULL_HANDLE;
    }
    descriptorSet_ = VK_NULL_HANDLE;
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::GraphicsBase::Base().Device(), descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

bool VulkanGeometryPass::CreateTextureDescriptorResources()
{
    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 1;
    layoutCi.pBindings = &textureBinding;
    if (vkCreateDescriptorSetLayout(vk::GraphicsBase::Base().Device(), &layoutCi, nullptr, &textureDescriptorSetLayout_) != VK_SUCCESS) {
        return false;
    }

    if (descriptorPool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 2;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 256;

        VkDescriptorPoolCreateInfo poolCi{};
        poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCi.maxSets = 256;
        poolCi.poolSizeCount = 2;
        poolCi.pPoolSizes = poolSizes;
        if (vkCreateDescriptorPool(vk::GraphicsBase::Base().Device(), &poolCi, nullptr, &descriptorPool_) != VK_SUCCESS) {
            return false;
        }
    }

    VkSamplerCreateInfo samplerCi{};
    samplerCi.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCi.magFilter = VK_FILTER_LINEAR;
    samplerCi.minFilter = VK_FILTER_LINEAR;
    samplerCi.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCi.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCi.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCi.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCi.maxAnisotropy = 1.0f;
    if (vkCreateSampler(vk::GraphicsBase::Base().Device(), &samplerCi, nullptr, &albedoTextureSampler_) != VK_SUCCESS) {
        return false;
    }

    // Create one default material entry so we always have a valid fallback texture set.
    VkDescriptorSet defaultSet = VK_NULL_HANDLE;
    return GetOrCreateMaterialTextureSet(nullptr, defaultSet);
}

bool VulkanGeometryPass::CreateTextureFromPixels(const uint8_t* rgbaPixels, uint32_t width, uint32_t height, VkImage& outImage, VkDeviceMemory& outMemory, VkImageView& outView)
{
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingMemory)) {
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), stagingMemory, 0, imageSize, 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(mapped, rgbaPixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(vk::GraphicsBase::Base().Device(), stagingMemory);

    VkImageCreateInfo imageCi{};
    imageCi.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCi.imageType = VK_IMAGE_TYPE_2D;
    imageCi.extent = { width, height, 1 };
    imageCi.mipLevels = 1;
    imageCi.arrayLayers = 1;
    imageCi.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCi.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCi.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCi.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(vk::GraphicsBase::Base().Device(), &imageCi, nullptr, &outImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(vk::GraphicsBase::Base().Device(), outImage, &memReq);
    VkMemoryAllocateInfo allocCi{};
    allocCi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocCi.allocationSize = memReq.size;
    allocCi.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(vk::GraphicsBase::Base().Device(), &allocCi, nullptr, &outMemory) != VK_SUCCESS) {
        return false;
    }
    if (vkBindImageMemory(vk::GraphicsBase::Base().Device(), outImage, outMemory, 0) != VK_SUCCESS) {
        return false;
    }

    auto& transferCmd = vk::GraphicsBase::Plus().CommandBuffer_Transfer();
    transferCmd.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = outImage;
    toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(transferCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = { width, height, 1 };
    vkCmdCopyBufferToImage(transferCmd, stagingBuffer, outImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    VkImageMemoryBarrier toShaderRead{};
    toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.image = outImage;
    toShaderRead.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(transferCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &toShaderRead);

    transferCmd.End();
    vk::GraphicsBase::Plus().ExecuteCommandBuffer_Graphics(transferCmd);
    vkDestroyBuffer(vk::GraphicsBase::Base().Device(), stagingBuffer, nullptr);
    vkFreeMemory(vk::GraphicsBase::Base().Device(), stagingMemory, nullptr);

    VkImageViewCreateInfo viewCi{};
    viewCi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCi.image = outImage;
    viewCi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCi.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewCi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return vkCreateImageView(vk::GraphicsBase::Base().Device(), &viewCi, nullptr, &outView) == VK_SUCCESS;
}

bool VulkanGeometryPass::GetOrCreateMaterialTextureSet(const std::shared_ptr<MaterialBase>& material, VkDescriptorSet& outDescriptorSet)
{
    MaterialBase* key = material.get();
    if (auto it = materialTextures_.find(key); it != materialTextures_.end()) {
        outDescriptorSet = it->second.descriptorSet;
        return true;
    }

    std::array<uint32_t, 4> fallbackPixels = {
        0xFF7F7FFFu, 0xFF5F5FFFu,
        0xFF5F5FFFu, 0xFF7F7FFFu
    };
    std::vector<uint8_t> materialPixels;
    uint32_t texWidth = 2;
    uint32_t texHeight = 2;

    auto tryLoadTexturePath = [&](const std::string& path) -> bool {
        if (path.empty()) return false;
        const std::string fullPath = FileSystem::getPath(path);
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* data = stbi_load(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!data || width <= 0 || height <= 0) {
            if (data) stbi_image_free(data);
            return false;
        }
        texWidth = static_cast<uint32_t>(width);
        texHeight = static_cast<uint32_t>(height);
        materialPixels.resize(static_cast<size_t>(texWidth) * static_cast<size_t>(texHeight) * 4);
        std::memcpy(materialPixels.data(), data, materialPixels.size());
        stbi_image_free(data);
        return true;
    };

    bool loadedFromTexture = false;
    if (auto pbr = std::dynamic_pointer_cast<PBRMaterial>(material)) {
        if (auto tex = pbr->GetAlbedoTexture()) {
            loadedFromTexture = tryLoadTexturePath(tex->GetPrimaryTexturePath());
        }
        if (!loadedFromTexture) {
            const glm::vec3 albedo = pbr->GetAlbedo();
            const uint8_t r = static_cast<uint8_t>(glm::clamp(albedo.r, 0.0f, 1.0f) * 255.0f);
            const uint8_t g = static_cast<uint8_t>(glm::clamp(albedo.g, 0.0f, 1.0f) * 255.0f);
            const uint8_t b = static_cast<uint8_t>(glm::clamp(albedo.b, 0.0f, 1.0f) * 255.0f);
            const uint32_t rgba = uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | 0xFF000000u;
            fallbackPixels = { rgba, rgba, rgba, rgba };
        }
    } else if (auto phong = std::dynamic_pointer_cast<PhongMaterial>(material)) {
        if (auto tex = phong->GetDiffuseTexture()) {
            loadedFromTexture = tryLoadTexturePath(tex->GetPrimaryTexturePath());
        }
    }

    MaterialTextureEntry entry{};
    if (loadedFromTexture) {
        if (!CreateTextureFromPixels(materialPixels.data(), texWidth, texHeight, entry.image, entry.memory, entry.view)) {
            return false;
        }
    } else {
        if (!CreateTextureFromPixels(reinterpret_cast<const uint8_t*>(fallbackPixels.data()), 2, 2, entry.image, entry.memory, entry.view)) {
            return false;
        }
    }
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureDescriptorSetLayout_;
    if (vkAllocateDescriptorSets(vk::GraphicsBase::Base().Device(), &allocInfo, &entry.descriptorSet) != VK_SUCCESS) {
        return false;
    }
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = albedoTextureSampler_;
    imageInfo.imageView = entry.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = entry.descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(vk::GraphicsBase::Base().Device(), 1, &write, 0, nullptr);

    materialTextures_.emplace(key, entry);
    outDescriptorSet = entry.descriptorSet;
    return true;
}

void VulkanGeometryPass::DestroyTextureDescriptorResources()
{
    DestroyMaterialTextures();
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vk::GraphicsBase::Base().Device(), descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (textureDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::GraphicsBase::Base().Device(), textureDescriptorSetLayout_, nullptr);
        textureDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

void VulkanGeometryPass::DestroyMaterialTextures()
{
    for (auto& [_, entry] : materialTextures_) {
        if (entry.view != VK_NULL_HANDLE) {
            vkDestroyImageView(vk::GraphicsBase::Base().Device(), entry.view, nullptr);
        }
        if (entry.image != VK_NULL_HANDLE) {
            vkDestroyImage(vk::GraphicsBase::Base().Device(), entry.image, nullptr);
        }
        if (entry.memory != VK_NULL_HANDLE) {
            vkFreeMemory(vk::GraphicsBase::Base().Device(), entry.memory, nullptr);
        }
    }
    materialTextures_.clear();
    if (albedoTextureSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(vk::GraphicsBase::Base().Device(), albedoTextureSampler_, nullptr);
        albedoTextureSampler_ = VK_NULL_HANDLE;
    }
}

bool VulkanGeometryPass::UpdateCameraUbo() const
{
    if (cameraUboMemory_ == VK_NULL_HANDLE) {
        return false;
    }
    CameraUbo camera{};
    camera.view = view_;
    camera.proj = proj_;
    void* mapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), cameraUboMemory_, 0, sizeof(CameraUbo), 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(mapped, &camera, sizeof(CameraUbo));
    vkUnmapMemory(vk::GraphicsBase::Base().Device(), cameraUboMemory_);
    return true;
}

bool VulkanGeometryPass::UpdateModelUbo(const glm::mat4& model) const
{
    if (modelUboMemory_ == VK_NULL_HANDLE) {
        return false;
    }
    ModelUbo modelUbo{};
    modelUbo.model = model;
    void* mapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), modelUboMemory_, 0, sizeof(ModelUbo), 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(mapped, &modelUbo, sizeof(ModelUbo));
    vkUnmapMemory(vk::GraphicsBase::Base().Device(), modelUboMemory_);
    return true;
}

} // namespace te

