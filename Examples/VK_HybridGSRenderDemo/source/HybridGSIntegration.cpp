#include "HybridGSIntegration.h"

#include "framework/Renderer.h"
#include "framework/RenderContext.h"
#include "framework/VulkanDeferredPipeline.h"
#include "framework/VulkanGeometryPass.h"
#include "GTVulkan/EasyVulkan.h"
#include "GTVulkan/VK_Base.h"
#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <iostream>

namespace {

void CmdTransitionColor(VkCommandBuffer cb, VkImage img, VkImageLayout oldL, VkImageLayout newL, VkAccessFlags srcA, VkAccessFlags dstA,
                        VkPipelineStageFlags srcS, VkPipelineStageFlags dstS)
{
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = oldL;
    b.newLayout = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b.srcAccessMask = srcA;
    b.dstAccessMask = dstA;
    vkCmdPipelineBarrier(cb, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
}

void CmdTransitionDepth(VkCommandBuffer cb, VkImage img, VkImageLayout oldL, VkImageLayout newL, VkAccessFlags srcA, VkAccessFlags dstA,
                        VkPipelineStageFlags srcS, VkPipelineStageFlags dstS)
{
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = oldL;
    b.newLayout = newL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    b.srcAccessMask = srcA;
    b.dstAccessMask = dstA;
    vkCmdPipelineBarrier(cb, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
}

struct alignas(256) HybridCompositeUboStd140 {
    glm::mat4 invViewProj{};
    glm::mat4 invProj{};
    glm::vec4 clipInfo{};
    glm::vec4 params{0.02f, 0.05f, 0.01f, 0.0f};
    glm::vec4 nearFar{};
};

} // namespace

struct HybridGSIntegration::Compositor {
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceMemory uboMem = VK_NULL_HANDLE;
    bool compositePrimed = false;

    ~Compositor() { destroy(); }

    void destroy()
    {
        if (device == VK_NULL_HANDLE) {
            return;
        }
        if (ubo != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, ubo, nullptr);
            ubo = VK_NULL_HANDLE;
        }
        if (uboMem != VK_NULL_HANDLE) {
            vkFreeMemory(device, uboMem, nullptr);
            uboMem = VK_NULL_HANDLE;
        }
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, sampler, nullptr);
            sampler = VK_NULL_HANDLE;
        }
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, pool, nullptr);
            pool = VK_NULL_HANDLE;
        }
        if (pipe != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipe, nullptr);
            pipe = VK_NULL_HANDLE;
        }
        if (pl != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pl, nullptr);
            pl = VK_NULL_HANDLE;
        }
        if (dsl != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, dsl, nullptr);
            dsl = VK_NULL_HANDLE;
        }
        device = VK_NULL_HANDLE;
        compositePrimed = false;
    }

    bool create(VkRenderPass renderPass, VkExtent2D extent)
    {
        destroy();
        device = vk::GraphicsBase::Base().Device();
        VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (vkCreateSampler(device, &sci, nullptr, &sampler) != VK_SUCCESS) {
            return false;
        }

        VkDescriptorSetLayoutBinding binds[5]{};
        for (uint32_t i = 0; i < 4; ++i) {
            binds[i].binding = i;
            binds[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binds[i].descriptorCount = 1;
            binds[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        binds[4].binding = 4;
        binds[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binds[4].descriptorCount = 1;
        binds[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslCi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dslCi.bindingCount = 5;
        dslCi.pBindings = binds;
        if (vkCreateDescriptorSetLayout(device, &dslCi, nullptr, &dsl) != VK_SUCCESS) {
            return false;
        }
        VkPipelineLayoutCreateInfo plCi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plCi.setLayoutCount = 1;
        plCi.pSetLayouts = &dsl;
        if (vkCreatePipelineLayout(device, &plCi, nullptr, &pl) != VK_SUCCESS) {
            return false;
        }

        vk::shaderModule vert("resources/compiled_shaders/hybrid_gs_composite_vert.spv");
        vk::shaderModule frag("resources/compiled_shaders/hybrid_gs_composite_frag.spv");
        VkPipelineShaderStageCreateInfo stages[2] = {
            vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
            frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };
        vk::graphicsPipelineCreateInfoPack pack;
        pack.createInfo.layout = pl;
        pack.createInfo.renderPass = renderPass;
        pack.createInfo.subpass = 0;
        pack.createInfo.stageCount = 2;
        pack.createInfo.pStages = stages;
        pack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pack.viewports.emplace_back(0.f, 0.f, float(extent.width), float(extent.height), 0.f, 1.f);
        pack.scissors.emplace_back(VkOffset2D{}, extent);
        pack.rasterizationStateCi.polygonMode = VK_POLYGON_MODE_FILL;
        pack.rasterizationStateCi.cullMode = VK_CULL_MODE_NONE;
        pack.rasterizationStateCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pack.rasterizationStateCi.lineWidth = 1.0f;
        pack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pack.colorBlendAttachmentStates.resize(1);
        pack.colorBlendAttachmentStates[0].colorWriteMask = 0b1111;
        pack.UpdateAllArrays();
        // Do not assign from temporary vk::pipeline(pack): its destructor would destroy the handle while we keep a raw VkPipeline copy.
        VkGraphicsPipelineCreateInfo& gci = pack;
        gci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gci, nullptr, &pipe) != VK_SUCCESS) {
            pipe = VK_NULL_HANDLE;
            return false;
        }

        VkDescriptorPoolSize sizes[2]{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        };
        VkDescriptorPoolCreateInfo poolCi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCi.maxSets = 1;
        poolCi.poolSizeCount = 2;
        poolCi.pPoolSizes = sizes;
        if (vkCreateDescriptorPool(device, &poolCi, nullptr, &pool) != VK_SUCCESS) {
            return false;
        }
        VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        alloc.descriptorPool = pool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &dsl;
        if (vkAllocateDescriptorSets(device, &alloc, &set) != VK_SUCCESS) {
            return false;
        }

        VkBufferCreateInfo bufCi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufCi.size = sizeof(HybridCompositeUboStd140);
        bufCi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (vkCreateBuffer(device, &bufCi, nullptr, &ubo) != VK_SUCCESS) {
            return false;
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, ubo, &req);
        VkPhysicalDeviceMemoryProperties memProps = vk::GraphicsBase::Base().PhysicalDeviceMemoryProperties();
        uint32_t memIndex = UINT32_MAX;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((req.memoryTypeBits & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags &
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                memIndex = i;
                break;
            }
        }
        if (memIndex == UINT32_MAX) {
            return false;
        }
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = memIndex;
        if (vkAllocateMemory(device, &mai, nullptr, &uboMem) != VK_SUCCESS) {
            return false;
        }
        vkBindBufferMemory(device, ubo, uboMem, 0);
        return true;
    }

    void record(VkCommandBuffer cmd, VulkanRenderer& renderer, uint32_t imageIndex, VkImageView gsColor, VkImageView gsDepth)
    {
        if (pipe == VK_NULL_HANDLE || set == VK_NULL_HANDLE) {
            return;
        }
        auto ctx = renderer.GetRenderContext();
        auto cam = ctx ? ctx->GetAttachedCamera() : nullptr;
        if (!cam) {
            return;
        }
        glm::mat4 view = cam->GetViewMatrix();
        glm::mat4 proj = cam->GetProjectionMatrix();
        proj[1][1] *= -1.0f;
        const glm::mat4 invVp = glm::inverse(proj * view);
        const glm::mat4 invP = glm::inverse(proj);

        HybridCompositeUboStd140 u{};
        u.invViewProj = invVp;
        u.invProj = invP;
        u.clipInfo = glm::vec4(cam->GetNearPlane(), cam->GetFarPlane(), float(renderer.GetFramebufferExtent().width),
                               float(renderer.GetFramebufferExtent().height));
        u.params = glm::vec4(0.02f, 0.05f, 0.01f, 0.0f);
        u.nearFar = glm::vec4(cam->GetNearPlane(), cam->GetFarPlane(), 0.0f, 0.0f);
        void* mapped = nullptr;
        vkMapMemory(device, uboMem, 0, sizeof(u), 0, &mapped);
        std::memcpy(mapped, &u, sizeof(u));
        vkUnmapMemory(device, uboMem);

        VkDescriptorImageInfo infos[4]{};
        infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        infos[0].imageView = renderer.GetLightingTargetView();
        infos[0].sampler = sampler;
        infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        infos[1].imageView = gsColor;
        infos[1].sampler = sampler;
        infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        infos[2].imageView = gsDepth;
        infos[2].sampler = sampler;

        VkImageView sceneDepthView = VK_NULL_HANDLE;
        VkImage sceneDepthImage = VK_NULL_HANDLE;
        if (auto* dp = static_cast<te::VulkanDeferredPipeline*>(renderer.GetDeferredPipelineOpaque())) {
            sceneDepthView = dp->GeometryPass().GetGBuffer().DepthView();
            sceneDepthImage = dp->GeometryPass().GetGBuffer().DepthImage();
        }
        infos[3].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        infos[3].imageView = sceneDepthView;
        infos[3].sampler = sampler;

        VkWriteDescriptorSet writes[5]{};
        for (uint32_t i = 0; i < 4; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].pImageInfo = &infos[i];
        }
        VkDescriptorBufferInfo binfo{ubo, 0, sizeof(HybridCompositeUboStd140)};
        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[4].dstSet = set;
        writes[4].dstBinding = 4;
        writes[4].descriptorCount = 1;
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[4].pBufferInfo = &binfo;
        vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);

        CmdTransitionColor(cmd, renderer.GetLightingTargetImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        if (sceneDepthImage != VK_NULL_HANDLE) {
            CmdTransitionDepth(cmd, sceneDepthImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                               VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }

        VkImage compositeImg = renderer.GetHybridCompositeTargetImage();
        if (compositePrimed) {
            CmdTransitionColor(cmd, compositeImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        }

        VkClearValue clear{};
        clear.color = {0.0f, 0.0f, 0.0f, 1.0f};
        VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp.renderPass = renderer.GetHdrPostRenderPass();
        rp.framebuffer = renderer.GetHybridCompositeFramebuffer();
        rp.renderArea.extent = renderer.GetFramebufferExtent();
        rp.clearValueCount = 1;
        rp.pClearValues = &clear;
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 0, 1, &set, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);

        compositePrimed = true;
        (void)imageIndex;
    }
};

HybridGSIntegration::HybridGSIntegration(VulkanRenderer& renderer, std::vector<GSVertex> vertices)
    : renderer_(renderer), vertices_(std::move(vertices)) {}

HybridGSIntegration::~HybridGSIntegration()
{
    shutdownGpu();
}

void HybridGSIntegration::shutdownGpu()
{
    if (released_) {
        return;
    }
    destroyCompositor();
    gs_.shutdown();
    released_ = true;
}

bool HybridGSIntegration::attachCamera(const std::shared_ptr<Camera>& camera)
{
    return gs_.initialize(vertices_, camera);
}

bool HybridGSIntegration::createCompositorAfterRendererInit()
{
    return createCompositorInternal();
}

bool HybridGSIntegration::createCompositorInternal()
{
    compositor_ = std::make_unique<Compositor>();
    return compositor_->create(renderer_.GetHdrPostRenderPass(), renderer_.GetFramebufferExtent());
}

void HybridGSIntegration::destroyCompositor()
{
    if (compositor_) {
        compositor_->destroy();
        compositor_.reset();
    }
}

void HybridGSIntegration::onPreprocess()
{
    lastPre_ = gs_.runPreprocessSubmitWait();
}

void HybridGSIntegration::onAfterLighting(VkCommandBuffer cmd, uint32_t swapchainImageIndex)
{
    gs_.updateUniforms();
    gs_.recordSortAndRender(cmd, swapchainImageIndex, lastPre_);
    if (compositor_) {
        compositor_->record(cmd, renderer_, swapchainImageIndex, gs_.gsColorView(swapchainImageIndex),
                            gs_.gsDepthView(swapchainImageIndex));
    }
}
