#include "GTVulkan/VK_Deferred.h"

#include <vector>

namespace vk {

namespace {
constexpr uint32_t kColorAttachmentCount = static_cast<uint32_t>(GBufferSlot::Count);
}

VulkanGBuffer::~VulkanGBuffer()
{
    Destroy();
}

bool VulkanGBuffer::Initialize(const GBufferCreateInfo& createInfo)
{
    Destroy();
    createInfo_ = createInfo;
    extent_ = createInfo.extent;

    if (extent_.width == 0 || extent_.height == 0) {
        outStream << "[ VulkanGBuffer ] ERROR\nInvalid extent.\n";
        return false;
    }

    const VkImageUsageFlags colorUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | createInfo_.lightingUsage;
    const VkImageUsageFlags depthUsage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | createInfo_.lightingUsage;

    return CreateAttachment(colorAttachments_[static_cast<uint32_t>(GBufferSlot::Albedo)],
                            createInfo_.albedoFormat, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT) &&
           CreateAttachment(colorAttachments_[static_cast<uint32_t>(GBufferSlot::Normal)],
                            createInfo_.normalFormat, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT) &&
           CreateAttachment(colorAttachments_[static_cast<uint32_t>(GBufferSlot::Material)],
                            createInfo_.materialFormat, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT) &&
           CreateAttachment(depthAttachment_, createInfo_.depthFormat, depthUsage, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanGBuffer::Destroy()
{
    DestroyFramebuffer();
    for (auto& attachment : colorAttachments_) {
        DestroyAttachment(attachment);
    }
    DestroyAttachment(depthAttachment_);
    extent_ = { 0, 0 };
}

bool VulkanGBuffer::Resize(VkExtent2D extent)
{
    if (extent.width == extent_.width && extent.height == extent_.height) {
        return true;
    }
    GBufferCreateInfo createInfo = createInfo_;
    createInfo.extent = extent;
    return Initialize(createInfo);
}

bool VulkanGBuffer::BuildFramebuffer(VkRenderPass renderPass)
{
    DestroyFramebuffer();
    boundRenderPass_ = renderPass;

    std::array<VkImageView, kColorAttachmentCount + 1> attachments = {
        colorAttachments_[static_cast<uint32_t>(GBufferSlot::Albedo)].view,
        colorAttachments_[static_cast<uint32_t>(GBufferSlot::Normal)].view,
        colorAttachments_[static_cast<uint32_t>(GBufferSlot::Material)].view,
        depthAttachment_.view
    };

    VkFramebufferCreateInfo framebufferCi = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = extent_.width,
        .height = extent_.height,
        .layers = 1
    };
    const VkResult result = vkCreateFramebuffer(GraphicsBase::Base().Device(), &framebufferCi, nullptr, &framebuffer_);
    if (result != VK_SUCCESS) {
        outStream << "[ VulkanGBuffer ] ERROR\nFailed to create framebuffer. Error code: " << int32_t(result) << '\n';
        framebuffer_ = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void VulkanGBuffer::DestroyFramebuffer()
{
    if (framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(GraphicsBase::Base().Device(), framebuffer_, nullptr);
        framebuffer_ = VK_NULL_HANDLE;
    }
    boundRenderPass_ = VK_NULL_HANDLE;
}

void VulkanGBuffer::CmdTransitionForGeometryWrite(VkCommandBuffer commandBuffer) const
{
    std::array<VkImageMemoryBarrier, kColorAttachmentCount + 1> barriers{};
    for (uint32_t i = 0; i < kColorAttachmentCount; ++i) {
        barriers[i] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = colorAttachments_[i].image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
    }
    barriers[kColorAttachmentCount] = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = depthAttachment_.image,
        .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data());
}

void VulkanGBuffer::CmdTransitionForLightingRead(VkCommandBuffer commandBuffer) const
{
    std::array<VkImageMemoryBarrier, kColorAttachmentCount + 1> barriers{};
    for (uint32_t i = 0; i < kColorAttachmentCount; ++i) {
        barriers[i] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = colorAttachments_[i].image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
    }
    barriers[kColorAttachmentCount] = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = depthAttachment_.image,
        .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data());
}

bool VulkanGBuffer::CreateAttachment(Attachment& attachment, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask)
{
    attachment.format = format;

    VkImageCreateInfo imageCi = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { extent_.width, extent_.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkResult result = vkCreateImage(GraphicsBase::Base().Device(), &imageCi, nullptr, &attachment.image);
    if (result != VK_SUCCESS) {
        outStream << "[ VulkanGBuffer ] ERROR\nFailed to create image. Error code: " << int32_t(result) << '\n';
        return false;
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(GraphicsBase::Base().Device(), attachment.image, &memReq);

    VkMemoryAllocateInfo allocCi = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (allocCi.memoryTypeIndex == UINT32_MAX) {
        outStream << "[ VulkanGBuffer ] ERROR\nNo suitable memory type for image attachment.\n";
        return false;
    }

    result = vkAllocateMemory(GraphicsBase::Base().Device(), &allocCi, nullptr, &attachment.memory);
    if (result != VK_SUCCESS) {
        outStream << "[ VulkanGBuffer ] ERROR\nFailed to allocate image memory. Error code: " << int32_t(result) << '\n';
        return false;
    }

    result = vkBindImageMemory(GraphicsBase::Base().Device(), attachment.image, attachment.memory, 0);
    if (result != VK_SUCCESS) {
        outStream << "[ VulkanGBuffer ] ERROR\nFailed to bind image memory. Error code: " << int32_t(result) << '\n';
        return false;
    }

    VkImageViewCreateInfo viewCi = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = attachment.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = { aspectMask, 0, 1, 0, 1 }
    };
    result = vkCreateImageView(GraphicsBase::Base().Device(), &viewCi, nullptr, &attachment.view);
    if (result != VK_SUCCESS) {
        outStream << "[ VulkanGBuffer ] ERROR\nFailed to create image view. Error code: " << int32_t(result) << '\n';
        return false;
    }
    return true;
}

void VulkanGBuffer::DestroyAttachment(Attachment& attachment)
{
    if (attachment.view != VK_NULL_HANDLE) {
        vkDestroyImageView(GraphicsBase::Base().Device(), attachment.view, nullptr);
        attachment.view = VK_NULL_HANDLE;
    }
    if (attachment.image != VK_NULL_HANDLE) {
        vkDestroyImage(GraphicsBase::Base().Device(), attachment.image, nullptr);
        attachment.image = VK_NULL_HANDLE;
    }
    if (attachment.memory != VK_NULL_HANDLE) {
        vkFreeMemory(GraphicsBase::Base().Device(), attachment.memory, nullptr);
        attachment.memory = VK_NULL_HANDLE;
    }
    attachment.format = VK_FORMAT_UNDEFINED;
}

uint32_t VulkanGBuffer::FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags desiredProperties) const
{
    const auto& memProps = GraphicsBase::Base().PhysicalDeviceMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool memorySupported = (memoryTypeBits & (1u << i)) != 0;
        const bool propertySupported = (memProps.memoryTypes[i].propertyFlags & desiredProperties) == desiredProperties;
        if (memorySupported && propertySupported) {
            return i;
        }
    }
    return UINT32_MAX;
}

} // namespace vk

