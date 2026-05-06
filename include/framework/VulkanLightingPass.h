#pragma once

#include "framework/Renderer.h"
#include "GTVulkan/VK_Deferred.h"

namespace te {

struct VulkanLightingPassCreateInfo {
    VkExtent2D extent{ 0, 0 };
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

class VulkanLightingPass {
public:
    VulkanLightingPass() = default;
    ~VulkanLightingPass() = default;

    bool Initialize(const VulkanLightingPassCreateInfo& createInfo);
    void Shutdown();
    void Resize(VkExtent2D extent);

    void SetRenderTargets(VkRenderPass renderPass, VkFramebuffer framebuffer);
    void SetPipeline(VkPipeline pipeline, VkPipelineLayout layout);
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return descriptorSetLayout_; }

    void Record(VkCommandBuffer commandBuffer, const vk::VulkanGBuffer& gbuffer);

private:
    bool CreateDescriptorResources();
    void DestroyDescriptorResources();
    bool UpdateGBufferDescriptors(const vk::VulkanGBuffer& gbuffer);

    std::vector<VkClearValue> BuildClearValues() const;

private:
    VkExtent2D extent_{ 0, 0 };
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkSampler gbufferSampler_ = VK_NULL_HANDLE;
};

} // namespace te

