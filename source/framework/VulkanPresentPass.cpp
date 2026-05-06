#include "framework/VulkanPresentPass.h"

#include "GTVulkan/VK_Base.h"

namespace te {

bool VulkanPresentPass::Initialize(const VulkanPresentPassCreateInfo& createInfo)
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

void VulkanPresentPass::Shutdown()
{
    DestroyDescriptorResources();
    extent_ = { 0, 0 };
    renderPass_ = VK_NULL_HANDLE;
    framebuffer_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
}

void VulkanPresentPass::Resize(VkExtent2D extent)
{
    extent_ = extent;
}

void VulkanPresentPass::SetRenderTargets(VkRenderPass renderPass, VkFramebuffer framebuffer)
{
    renderPass_ = renderPass;
    framebuffer_ = framebuffer;
}

void VulkanPresentPass::SetPipeline(VkPipeline pipeline, VkPipelineLayout layout)
{
    pipeline_ = pipeline;
    pipelineLayout_ = layout;
}

void VulkanPresentPass::Record(VkCommandBuffer commandBuffer, VkImageView inputView)
{
    if (commandBuffer == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE || framebuffer_ == VK_NULL_HANDLE || inputView == VK_NULL_HANDLE) {
        return;
    }
    VkClearValue clear{};
    clear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass_;
    beginInfo.framebuffer = framebuffer_;
    beginInfo.renderArea.extent = extent_;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clear;
    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (pipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        if (pipelineLayout_ != VK_NULL_HANDLE && descriptorSet_ != VK_NULL_HANDLE && UpdateDescriptors(inputView)) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
        }
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);
}

bool VulkanPresentPass::CreateDescriptorResources()
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 1;
    layoutCi.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(vk::GraphicsBase::Base().Device(), &layoutCi, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
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
    if (vkCreateSampler(vk::GraphicsBase::Base().Device(), &samplerCi, nullptr, &inputSampler_) != VK_SUCCESS) {
        return false;
    }
    return true;
}

void VulkanPresentPass::DestroyDescriptorResources()
{
    if (inputSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(vk::GraphicsBase::Base().Device(), inputSampler_, nullptr);
        inputSampler_ = VK_NULL_HANDLE;
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

bool VulkanPresentPass::UpdateDescriptors(VkImageView inputView)
{
    if (descriptorSet_ == VK_NULL_HANDLE || inputSampler_ == VK_NULL_HANDLE || inputView == VK_NULL_HANDLE) {
        return false;
    }
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = inputSampler_;
    imageInfo.imageView = inputView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(vk::GraphicsBase::Base().Device(), 1, &write, 0, nullptr);
    return true;
}

} // namespace te

