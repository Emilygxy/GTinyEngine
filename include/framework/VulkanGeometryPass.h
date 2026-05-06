#pragma once

#include "framework/Renderer.h"
#include "GTVulkan/VK_Deferred.h"
#include <unordered_map>

namespace te {

struct VulkanGeometryPassCreateInfo {
    VkExtent2D extent{ 0, 0 };
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
};

class VulkanGeometryPass {
public:
    VulkanGeometryPass() = default;
    ~VulkanGeometryPass() = default;

    bool Initialize(const VulkanGeometryPassCreateInfo& createInfo);
    void Shutdown();
    bool Resize(VkExtent2D extent);

    void SetPipeline(VkPipeline pipeline, VkPipelineLayout layout);
    void SetRenderPass(VkRenderPass renderPass);

    void Record(VkCommandBuffer commandBuffer, const std::vector<RenderCommand>& commands);

    const vk::VulkanGBuffer& GetGBuffer() const { return gbuffer_; }
    VkRenderPass GetRenderPass() const { return renderPass_; }
    vk::VulkanGBuffer& GetGBuffer() { return gbuffer_; }
    static VkVertexInputBindingDescription VertexBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> VertexAttributeDescriptions();

private:
    struct VulkanMeshBuffer {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
        uint32_t vertexCount = 0;
    };

    static size_t HashMeshData(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    bool GetOrCreateMeshBuffer(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, VulkanMeshBuffer& outBuffer);
    void DestroyMeshBuffers();
    static void DestroyMeshBuffer(VulkanMeshBuffer& meshBuffer);
    static bool CreateHostVisibleBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuffer, VkDeviceMemory& outMemory);
    static uint32_t FindHostVisibleMemoryType(uint32_t memoryTypeBits);

    bool RebuildFramebuffer();
    std::vector<VkClearValue> BuildClearValues() const;

private:
    vk::VulkanGBuffer gbuffer_{};
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkExtent2D extent_{ 0, 0 };
    VkFormat depthFormat_ = VK_FORMAT_D32_SFLOAT;
    std::unordered_map<size_t, VulkanMeshBuffer> meshBuffers_{};
};

} // namespace te

