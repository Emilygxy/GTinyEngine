#pragma once

#include "GTVulkan/VK_Base.h"

namespace te {

struct VulkanPresentPassCreateInfo {
    VkExtent2D extent{ 0, 0 };
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

class VulkanPresentPass {
public:
    bool Initialize(const VulkanPresentPassCreateInfo& createInfo);
    void Shutdown();
    void Resize(VkExtent2D extent);

    void SetRenderTargets(VkRenderPass renderPass, VkFramebuffer framebuffer);
    void SetPipeline(VkPipeline pipeline, VkPipelineLayout layout);
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return descriptorSetLayout_; }
    void Record(VkCommandBuffer commandBuffer, VkImageView inputView);

private:
    bool CreateDescriptorResources();
    void DestroyDescriptorResources();
    bool UpdateDescriptors(VkImageView inputView);

    VkExtent2D extent_{ 0, 0 };
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkSampler inputSampler_ = VK_NULL_HANDLE;
};

} // namespace te

