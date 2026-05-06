#pragma once

#include "framework/Renderer.h"
#include "GTVulkan/VK_Deferred.h"
#include "materials/BaseMaterial.h"
#include <glm/glm.hpp>
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
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return descriptorSetLayout_; }
    VkDescriptorSetLayout GetTextureDescriptorSetLayout() const { return textureDescriptorSetLayout_; }
    static VkVertexInputBindingDescription VertexBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> VertexAttributeDescriptions();
    void SetViewProjection(const glm::mat4& view, const glm::mat4& proj);

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
    static bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& outBuffer, VkDeviceMemory& outMemory);
    static uint32_t FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties);
    static bool UploadDeviceLocalBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuffer, VkDeviceMemory& outMemory);

    bool CreatePerObjectDescriptorResources();
    void DestroyPerObjectDescriptorResources();
    bool CreateTextureDescriptorResources();
    void DestroyTextureDescriptorResources();
    bool CreateTextureFromPixels(const uint8_t* rgbaPixels, uint32_t width, uint32_t height, VkImage& outImage, VkDeviceMemory& outMemory, VkImageView& outView);
    bool GetOrCreateMaterialTextureSet(const std::shared_ptr<MaterialBase>& material, VkDescriptorSet& outDescriptorSet);
    void DestroyMaterialTextures();
    bool UpdateCameraUbo() const;
    bool UpdateModelUbo(const glm::mat4& model) const;

    bool RebuildFramebuffer();
    std::vector<VkClearValue> BuildClearValues() const;

private:
    struct CameraUbo {
        glm::mat4 view{ 1.0f };
        glm::mat4 proj{ 1.0f };
    };

    struct ModelUbo {
        glm::mat4 model{ 1.0f };
    };

    vk::VulkanGBuffer gbuffer_{};
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkExtent2D extent_{ 0, 0 };
    VkFormat depthFormat_ = VK_FORMAT_D32_SFLOAT;
    std::unordered_map<size_t, VulkanMeshBuffer> meshBuffers_{};

    glm::mat4 view_{ 1.0f };
    glm::mat4 proj_{ 1.0f };
    mutable VkBuffer cameraUboBuffer_ = VK_NULL_HANDLE;
    mutable VkDeviceMemory cameraUboMemory_ = VK_NULL_HANDLE;
    mutable VkBuffer modelUboBuffer_ = VK_NULL_HANDLE;
    mutable VkDeviceMemory modelUboMemory_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkSampler albedoTextureSampler_ = VK_NULL_HANDLE;

    struct MaterialTextureEntry {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };
    std::unordered_map<MaterialBase*, MaterialTextureEntry> materialTextures_{};
};

} // namespace te

