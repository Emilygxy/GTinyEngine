#pragma once

#include "framework/Renderer.h"
#include "GTVulkan/VK_Deferred.h"
#include <glm/glm.hpp>

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
    void SetLightingParams(const glm::vec3& lightDir, const glm::vec3& lightColor, const glm::vec3& cameraPos);

    void Record(VkCommandBuffer commandBuffer, const vk::VulkanGBuffer& gbuffer);

private:
    bool CreateDescriptorResources();
    void DestroyDescriptorResources();
    bool UpdateGBufferDescriptors(const vk::VulkanGBuffer& gbuffer);
    bool UpdateLightingUbo() const;

    std::vector<VkClearValue> BuildClearValues() const;

private:
    struct LightingUbo {
        glm::vec4 lightDirection{ 0.0f, 1.0f, 0.0f, 0.0f };
        glm::vec4 lightColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 cameraPos{ 0.0f, 0.0f, 2.0f, 1.0f };
    };

    VkExtent2D extent_{ 0, 0 };
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkSampler gbufferSampler_ = VK_NULL_HANDLE;
    mutable VkBuffer lightingUboBuffer_ = VK_NULL_HANDLE;
    mutable VkDeviceMemory lightingUboMemory_ = VK_NULL_HANDLE;
    LightingUbo lightingParams_{};
};

} // namespace te

