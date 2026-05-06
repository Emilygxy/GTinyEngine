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

    void Record(VkCommandBuffer commandBuffer, const vk::VulkanGBuffer& gbuffer) const;

private:
    std::vector<VkClearValue> BuildClearValues() const;

private:
    VkExtent2D extent_{ 0, 0 };
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
};

} // namespace te

