#include "framework/VulkanLightingPass.h"

namespace te {

bool VulkanLightingPass::Initialize(const VulkanLightingPassCreateInfo& createInfo)
{
    extent_ = createInfo.extent;
    renderPass_ = createInfo.renderPass;
    framebuffer_ = createInfo.framebuffer;
    pipeline_ = createInfo.pipeline;
    pipelineLayout_ = createInfo.pipelineLayout;
    return extent_.width > 0 && extent_.height > 0;
}

void VulkanLightingPass::Shutdown()
{
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

void VulkanLightingPass::Record(VkCommandBuffer commandBuffer, const vk::VulkanGBuffer& gbuffer) const
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

} // namespace te

