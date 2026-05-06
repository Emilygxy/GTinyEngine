#include "framework/VulkanGeometryPass.h"

#include "mesh/Vertex.h"
#include <cstring>

namespace te {

bool VulkanGeometryPass::Initialize(const VulkanGeometryPassCreateInfo& createInfo)
{
    Shutdown();

    extent_ = createInfo.extent;
    renderPass_ = createInfo.renderPass;
    pipeline_ = createInfo.pipeline;
    pipelineLayout_ = createInfo.pipelineLayout;
    depthFormat_ = createInfo.depthFormat;

    if (extent_.width == 0 || extent_.height == 0) {
        return false;
    }

    vk::GBufferCreateInfo gbufferCi{};
    gbufferCi.extent = extent_;
    gbufferCi.depthFormat = depthFormat_;
    if (!gbuffer_.Initialize(gbufferCi)) {
        return false;
    }

    if (renderPass_ != VK_NULL_HANDLE && !gbuffer_.BuildFramebuffer(renderPass_)) {
        return false;
    }

    return true;
}

void VulkanGeometryPass::Shutdown()
{
    gbuffer_.Destroy();
    renderPass_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    extent_ = { 0, 0 };
}

bool VulkanGeometryPass::Resize(VkExtent2D extent)
{
    extent_ = extent;
    if (!gbuffer_.Resize(extent_)) {
        return false;
    }
    return RebuildFramebuffer();
}

void VulkanGeometryPass::SetPipeline(VkPipeline pipeline, VkPipelineLayout layout)
{
    pipeline_ = pipeline;
    pipelineLayout_ = layout;
}

void VulkanGeometryPass::SetRenderPass(VkRenderPass renderPass)
{
    renderPass_ = renderPass;
    RebuildFramebuffer();
}

void VulkanGeometryPass::Record(VkCommandBuffer commandBuffer, const std::vector<RenderCommand>& commands)
{
    if (commandBuffer == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE || gbuffer_.Framebuffer() == VK_NULL_HANDLE) {
        return;
    }

    // M1 skeleton:
    // 1) Transition gbuffer images to attachment layouts.
    // 2) Begin geometry render pass and bind pipeline.
    // 3) Iterate render commands (draw path will be filled in later M1 tasks).
    gbuffer_.CmdTransitionForGeometryWrite(commandBuffer);

    std::vector<VkClearValue> clearValues = BuildClearValues();
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass_;
    beginInfo.framebuffer = gbuffer_.Framebuffer();
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = extent_;
    beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (pipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        // Minimal M1 draw path:
        // Upload each fragment mesh into transient host-visible buffers and draw with
        // a fixed Vertex layout { vec3 position, vec3 normal, vec2 texCoord }.
        for (const auto& command : commands) {
            if (!command.fragmentsSource) {
                continue;
            }

            const auto& fragments = command.fragmentsSource->GetFragments();
            for (const auto& fragment : fragments) {
                if (!fragment.IsReady() || fragment.mpGeometry == nullptr) {
                    continue;
                }
                const auto vertices = fragment.mpGeometry->GetVertices();
                const auto indices = fragment.mpGeometry->GetIndices();
                if (vertices.empty() || indices.empty()) {
                    continue;
                }

                VkBuffer vertexBuffer = VK_NULL_HANDLE;
                VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
                VkBuffer indexBuffer = VK_NULL_HANDLE;
                VkDeviceMemory indexMemory = VK_NULL_HANDLE;

                const auto createUploadBuffer = [](VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuffer, VkDeviceMemory& outMemory) -> bool {
                    VkBufferCreateInfo bufferCi{};
                    bufferCi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    bufferCi.size = size;
                    bufferCi.usage = usage;
                    bufferCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                    if (vkCreateBuffer(vk::GraphicsBase::Base().Device(), &bufferCi, nullptr, &outBuffer) != VK_SUCCESS) {
                        return false;
                    }

                    VkMemoryRequirements memoryRequirements{};
                    vkGetBufferMemoryRequirements(vk::GraphicsBase::Base().Device(), outBuffer, &memoryRequirements);

                    uint32_t memoryTypeIndex = UINT32_MAX;
                    const auto& memProps = vk::GraphicsBase::Base().PhysicalDeviceMemoryProperties();
                    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
                        const bool memorySupported = (memoryRequirements.memoryTypeBits & (1u << i)) != 0;
                        const bool propertySupported =
                            (memProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                        if (memorySupported && propertySupported) {
                            memoryTypeIndex = i;
                            break;
                        }
                    }
                    if (memoryTypeIndex == UINT32_MAX) {
                        return false;
                    }

                    VkMemoryAllocateInfo allocCi{};
                    allocCi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                    allocCi.allocationSize = memoryRequirements.size;
                    allocCi.memoryTypeIndex = memoryTypeIndex;
                    if (vkAllocateMemory(vk::GraphicsBase::Base().Device(), &allocCi, nullptr, &outMemory) != VK_SUCCESS) {
                        return false;
                    }
                    return vkBindBufferMemory(vk::GraphicsBase::Base().Device(), outBuffer, outMemory, 0) == VK_SUCCESS;
                };

                const VkDeviceSize vertexDataSize = static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
                const VkDeviceSize indexDataSize = static_cast<VkDeviceSize>(indices.size() * sizeof(uint32_t));
                if (!createUploadBuffer(vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexMemory) ||
                    !createUploadBuffer(indexDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexMemory)) {
                    if (vertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(vk::GraphicsBase::Base().Device(), vertexBuffer, nullptr);
                    if (vertexMemory != VK_NULL_HANDLE) vkFreeMemory(vk::GraphicsBase::Base().Device(), vertexMemory, nullptr);
                    if (indexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(vk::GraphicsBase::Base().Device(), indexBuffer, nullptr);
                    if (indexMemory != VK_NULL_HANDLE) vkFreeMemory(vk::GraphicsBase::Base().Device(), indexMemory, nullptr);
                    continue;
                }

                void* vertexMapped = nullptr;
                void* indexMapped = nullptr;
                if (vkMapMemory(vk::GraphicsBase::Base().Device(), vertexMemory, 0, vertexDataSize, 0, &vertexMapped) == VK_SUCCESS) {
                    std::memcpy(vertexMapped, vertices.data(), static_cast<size_t>(vertexDataSize));
                    vkUnmapMemory(vk::GraphicsBase::Base().Device(), vertexMemory);
                }
                if (vkMapMemory(vk::GraphicsBase::Base().Device(), indexMemory, 0, indexDataSize, 0, &indexMapped) == VK_SUCCESS) {
                    std::memcpy(indexMapped, indices.data(), static_cast<size_t>(indexDataSize));
                    vkUnmapMemory(vk::GraphicsBase::Base().Device(), indexMemory);
                }

                VkDeviceSize vertexOffset = 0;
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

                vkDestroyBuffer(vk::GraphicsBase::Base().Device(), vertexBuffer, nullptr);
                vkFreeMemory(vk::GraphicsBase::Base().Device(), vertexMemory, nullptr);
                vkDestroyBuffer(vk::GraphicsBase::Base().Device(), indexBuffer, nullptr);
                vkFreeMemory(vk::GraphicsBase::Base().Device(), indexMemory, nullptr);
            }
        }
    } else {
        (void)commands;
    }

    vkCmdEndRenderPass(commandBuffer);
}

bool VulkanGeometryPass::RebuildFramebuffer()
{
    if (renderPass_ == VK_NULL_HANDLE) {
        return false;
    }
    return gbuffer_.BuildFramebuffer(renderPass_);
}

std::vector<VkClearValue> VulkanGeometryPass::BuildClearValues() const
{
    std::vector<VkClearValue> clearValues;
    clearValues.resize(static_cast<size_t>(vk::GBufferSlot::Count) + 1);
    for (size_t i = 0; i < static_cast<size_t>(vk::GBufferSlot::Count); ++i) {
        clearValues[i].color = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    clearValues.back().depthStencil = { 1.0f, 0 };
    return clearValues;
}

} // namespace te

