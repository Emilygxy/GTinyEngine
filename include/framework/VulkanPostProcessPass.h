#pragma once

#include "GTVulkan/VK_Base.h"
#include <glm/glm.hpp>

namespace te {

struct VulkanPostProcessPassCreateInfo {
    VkExtent2D extent{ 0, 0 };
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

class VulkanPostProcessPass {
public:
    bool Initialize(const VulkanPostProcessPassCreateInfo& createInfo);
    void Shutdown();
    void Resize(VkExtent2D extent);

    void SetRenderTargets(VkRenderPass renderPass, VkFramebuffer framebuffer);
    void SetPipeline(VkPipeline pipeline, VkPipelineLayout layout);
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return descriptorSetLayout_; }
    void SetToneMappingParams(float exposure, float gamma);
    void Record(VkCommandBuffer commandBuffer, VkImageView inputView);

private:
    struct ToneMappingUbo {
        glm::vec4 params{ 1.0f, 2.2f, 0.0f, 0.0f };
    };

    bool CreateDescriptorResources();
    void DestroyDescriptorResources();
    bool UpdateDescriptors(VkImageView inputView);
    bool UpdateUbo() const;

    VkExtent2D extent_{ 0, 0 };
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkSampler inputSampler_ = VK_NULL_HANDLE;
    mutable VkBuffer paramsUboBuffer_ = VK_NULL_HANDLE;
    mutable VkDeviceMemory paramsUboMemory_ = VK_NULL_HANDLE;
    ToneMappingUbo params_{};
};

} // namespace te

