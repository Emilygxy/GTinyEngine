#pragma once

#include "VK_Base.h"
#include <array>

namespace vk {

enum class GBufferSlot : uint32_t {
    Albedo = 0,
    Normal = 1,
    Material = 2,
    Count = 3
};

struct GBufferCreateInfo {
    VkExtent2D extent{ 0, 0 };
    VkFormat albedoFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat normalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat materialFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    VkImageUsageFlags lightingUsage = VK_IMAGE_USAGE_SAMPLED_BIT;
};

class VulkanGBuffer {
public:
    VulkanGBuffer() = default;
    ~VulkanGBuffer();

    VulkanGBuffer(const VulkanGBuffer&) = delete;
    VulkanGBuffer& operator=(const VulkanGBuffer&) = delete;

    bool Initialize(const GBufferCreateInfo& createInfo);
    void Destroy();
    bool Resize(VkExtent2D extent);

    bool BuildFramebuffer(VkRenderPass renderPass);
    void DestroyFramebuffer();

    void CmdTransitionForGeometryWrite(VkCommandBuffer commandBuffer) const;
    void CmdTransitionForLightingRead(VkCommandBuffer commandBuffer) const;

    VkExtent2D Extent() const { return extent_; }
    VkFramebuffer Framebuffer() const { return framebuffer_; }
    VkImageView ColorView(GBufferSlot slot) const { return colorAttachments_[static_cast<uint32_t>(slot)].view; }
    VkImageView DepthView() const { return depthAttachment_.view; }
    VkImage DepthImage() const { return depthAttachment_.image; }
    VkFormat ColorFormat(GBufferSlot slot) const { return colorAttachments_[static_cast<uint32_t>(slot)].format; }
    VkFormat DepthFormat() const { return depthAttachment_.format; }

private:
    struct Attachment {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    bool CreateAttachment(Attachment& attachment, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask);
    void DestroyAttachment(Attachment& attachment);
    uint32_t FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags desiredProperties) const;

private:
    GBufferCreateInfo createInfo_{};
    VkExtent2D extent_{ 0, 0 };
    std::array<Attachment, static_cast<size_t>(GBufferSlot::Count)> colorAttachments_{};
    Attachment depthAttachment_{};
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkRenderPass boundRenderPass_ = VK_NULL_HANDLE;
};

} // namespace vk

