#include "framework/VulkanLightingPass.h"

namespace te {

bool VulkanLightingPass::Initialize(const VulkanLightingPassCreateInfo& createInfo)
{
    Shutdown();
    extent_ = createInfo.extent;
    renderPass_ = createInfo.renderPass;
    framebuffer_ = createInfo.framebuffer;
    pipeline_ = createInfo.pipeline;
    pipelineLayout_ = createInfo.pipelineLayout;
    if (extent_.width == 0 || extent_.height == 0) {
        return false;
    }
    return CreateDescriptorResources();
}

void VulkanLightingPass::Shutdown()
{
    DestroyDescriptorResources();
    extent_ = { 0, 0 };
    renderPass_ = VK_NULL_HANDLE;
    framebuffer_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
}

void VulkanLightingPass::Resize(VkExtent2D extent)
{
    extent_ = extent;
}

void VulkanLightingPass::SetRenderTargets(VkRenderPass renderPass, VkFramebuffer framebuffer)
{
    renderPass_ = renderPass;
    framebuffer_ = framebuffer;
}

void VulkanLightingPass::SetPipeline(VkPipeline pipeline, VkPipelineLayout layout)
{
    pipeline_ = pipeline;
    pipelineLayout_ = layout;
}

void VulkanLightingPass::Record(VkCommandBuffer commandBuffer, const vk::VulkanGBuffer& gbuffer)
{
    if (commandBuffer == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE || framebuffer_ == VK_NULL_HANDLE) {
        return;
    }

    // Make GBuffer readable for fragment shader sampling/input attachment.
    gbuffer.CmdTransitionForLightingRead(commandBuffer);

    std::vector<VkClearValue> clearValues = BuildClearValues();
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass_;
    beginInfo.framebuffer = framebuffer_;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = extent_;
    beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (pipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        if (descriptorSet_ != VK_NULL_HANDLE && pipelineLayout_ != VK_NULL_HANDLE && UpdateGBufferDescriptors(gbuffer)) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
        }
        // M1 skeleton: fullscreen triangle for deferred lighting.
        // Descriptor and material bindings are added in next iteration.
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);
}

std::vector<VkClearValue> VulkanLightingPass::BuildClearValues() const
{
    std::vector<VkClearValue> clearValues(1);
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    return clearValues;
}

bool VulkanLightingPass::CreateDescriptorResources()
{
    VkDescriptorSetLayoutBinding bindings[4]{};
    for (uint32_t i = 0; i < 4; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 4;
    layoutCi.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(vk::GraphicsBase::Base().Device(), &layoutCi, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 4;

    VkDescriptorPoolCreateInfo poolCi{};
    poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCi.maxSets = 1;
    poolCi.poolSizeCount = 1;
    poolCi.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(vk::GraphicsBase::Base().Device(), &poolCi, nullptr, &descriptorPool_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout_;
    if (vkAllocateDescriptorSets(vk::GraphicsBase::Base().Device(), &allocInfo, &descriptorSet_) != VK_SUCCESS) {
        return false;
    }

    VkSamplerCreateInfo samplerCi{};
    samplerCi.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCi.magFilter = VK_FILTER_LINEAR;
    samplerCi.minFilter = VK_FILTER_LINEAR;
    samplerCi.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCi.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.maxAnisotropy = 1.0f;
    if (vkCreateSampler(vk::GraphicsBase::Base().Device(), &samplerCi, nullptr, &gbufferSampler_) != VK_SUCCESS) {
        return false;
    }

    return true;
}

void VulkanLightingPass::DestroyDescriptorResources()
{
    if (gbufferSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(vk::GraphicsBase::Base().Device(), gbufferSampler_, nullptr);
        gbufferSampler_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vk::GraphicsBase::Base().Device(), descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    descriptorSet_ = VK_NULL_HANDLE;
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::GraphicsBase::Base().Device(), descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

bool VulkanLightingPass::UpdateGBufferDescriptors(const vk::VulkanGBuffer& gbuffer)
{
    if (descriptorSet_ == VK_NULL_HANDLE || gbufferSampler_ == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorImageInfo imageInfos[4]{};
    imageInfos[0].sampler = gbufferSampler_;
    imageInfos[0].imageView = gbuffer.ColorView(vk::GBufferSlot::Albedo);
    imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageInfos[1].sampler = gbufferSampler_;
    imageInfos[1].imageView = gbuffer.ColorView(vk::GBufferSlot::Normal);
    imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageInfos[2].sampler = gbufferSampler_;
    imageInfos[2].imageView = gbuffer.ColorView(vk::GBufferSlot::Material);
    imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageInfos[3].sampler = gbufferSampler_;
    imageInfos[3].imageView = gbuffer.DepthView();
    imageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[4]{};
    for (uint32_t i = 0; i < 4; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptorSet_;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &imageInfos[i];
    }
    vkUpdateDescriptorSets(vk::GraphicsBase::Base().Device(), 4, writes, 0, nullptr);
    return true;
}

} // namespace te

