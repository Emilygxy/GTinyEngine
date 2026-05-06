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
    DestroyMeshBuffers();
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
                if (fragment.mpGeometry == nullptr) {
                    continue;
                }
                const auto vertices = fragment.mpGeometry->GetVertices();
                const auto indices = fragment.mpGeometry->GetIndices();
                if (vertices.empty() || indices.empty()) {
                    continue;
                }

                VulkanMeshBuffer meshBuffer{};
                if (!GetOrCreateMeshBuffer(vertices, indices, meshBuffer)) {
                    continue;
                }
                VkDeviceSize vertexOffset = 0;
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshBuffer.vertexBuffer, &vertexOffset);
                vkCmdBindIndexBuffer(commandBuffer, meshBuffer.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandBuffer, meshBuffer.indexCount, 1, 0, 0, 0);
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

size_t VulkanGeometryPass::HashMeshData(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
    constexpr size_t kFNVOffsetBasis = sizeof(size_t) == 8 ? 1469598103934665603ull : 2166136261u;
    constexpr size_t kFNVPrime = sizeof(size_t) == 8 ? 1099511628211ull : 16777619u;
    size_t hash = kFNVOffsetBasis;

    auto hashBytes = [&hash, kFNVPrime](const uint8_t* bytes, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            hash ^= static_cast<size_t>(bytes[i]);
            hash *= kFNVPrime;
        }
    };
    hashBytes(reinterpret_cast<const uint8_t*>(vertices.data()), vertices.size() * sizeof(Vertex));
    hashBytes(reinterpret_cast<const uint8_t*>(indices.data()), indices.size() * sizeof(unsigned int));
    return hash;
}

bool VulkanGeometryPass::GetOrCreateMeshBuffer(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, VulkanMeshBuffer& outBuffer)
{
    const size_t meshHash = HashMeshData(vertices, indices);
    const auto it = meshBuffers_.find(meshHash);
    if (it != meshBuffers_.end()) {
        outBuffer = it->second;
        return true;
    }

    VulkanMeshBuffer meshBuffer{};
    const VkDeviceSize vertexDataSize = static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
    const VkDeviceSize indexDataSize = static_cast<VkDeviceSize>(indices.size() * sizeof(uint32_t));
    if (!CreateHostVisibleBuffer(vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, meshBuffer.vertexBuffer, meshBuffer.vertexMemory) ||
        !CreateHostVisibleBuffer(indexDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, meshBuffer.indexBuffer, meshBuffer.indexMemory)) {
        DestroyMeshBuffer(meshBuffer);
        return false;
    }

    void* vertexMapped = nullptr;
    void* indexMapped = nullptr;
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), meshBuffer.vertexMemory, 0, vertexDataSize, 0, &vertexMapped) == VK_SUCCESS) {
        std::memcpy(vertexMapped, vertices.data(), static_cast<size_t>(vertexDataSize));
        vkUnmapMemory(vk::GraphicsBase::Base().Device(), meshBuffer.vertexMemory);
    } else {
        DestroyMeshBuffer(meshBuffer);
        return false;
    }
    if (vkMapMemory(vk::GraphicsBase::Base().Device(), meshBuffer.indexMemory, 0, indexDataSize, 0, &indexMapped) == VK_SUCCESS) {
        std::memcpy(indexMapped, indices.data(), static_cast<size_t>(indexDataSize));
        vkUnmapMemory(vk::GraphicsBase::Base().Device(), meshBuffer.indexMemory);
    } else {
        DestroyMeshBuffer(meshBuffer);
        return false;
    }

    meshBuffer.vertexCount = static_cast<uint32_t>(vertices.size());
    meshBuffer.indexCount = static_cast<uint32_t>(indices.size());
    meshBuffers_.emplace(meshHash, meshBuffer);
    outBuffer = meshBuffer;
    return true;
}

void VulkanGeometryPass::DestroyMeshBuffers()
{
    for (auto& [_, meshBuffer] : meshBuffers_) {
        DestroyMeshBuffer(meshBuffer);
    }
    meshBuffers_.clear();
}

void VulkanGeometryPass::DestroyMeshBuffer(VulkanMeshBuffer& meshBuffer)
{
    if (meshBuffer.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), meshBuffer.vertexBuffer, nullptr);
        meshBuffer.vertexBuffer = VK_NULL_HANDLE;
    }
    if (meshBuffer.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), meshBuffer.vertexMemory, nullptr);
        meshBuffer.vertexMemory = VK_NULL_HANDLE;
    }
    if (meshBuffer.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk::GraphicsBase::Base().Device(), meshBuffer.indexBuffer, nullptr);
        meshBuffer.indexBuffer = VK_NULL_HANDLE;
    }
    if (meshBuffer.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), meshBuffer.indexMemory, nullptr);
        meshBuffer.indexMemory = VK_NULL_HANDLE;
    }
    meshBuffer.indexCount = 0;
    meshBuffer.vertexCount = 0;
}

bool VulkanGeometryPass::CreateHostVisibleBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuffer, VkDeviceMemory& outMemory)
{
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
    const uint32_t memoryTypeIndex = FindHostVisibleMemoryType(memoryRequirements.memoryTypeBits);
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
}

uint32_t VulkanGeometryPass::FindHostVisibleMemoryType(uint32_t memoryTypeBits)
{
    const auto& memProps = vk::GraphicsBase::Base().PhysicalDeviceMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool memorySupported = (memoryTypeBits & (1u << i)) != 0;
        const bool propertySupported =
            (memProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memorySupported && propertySupported) {
            return i;
        }
    }
    return UINT32_MAX;
}

VkVertexInputBindingDescription VulkanGeometryPass::VertexBindingDescription()
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = static_cast<uint32_t>(sizeof(Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 3> VulkanGeometryPass::VertexAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, position)) };
    attributes[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal)) };
    attributes[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, texCoords)) };
    return attributes;
}

} // namespace te

