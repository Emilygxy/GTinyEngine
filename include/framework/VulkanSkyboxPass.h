#pragma once

#include "GTVulkan/VK_Base.h"
#include <glm/glm.hpp>

namespace te {

struct VulkanSkyboxPassCreateInfo {
    VkExtent2D extent{ 0, 0 };
    VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
};

// Vulkan counterpart of the OpenGL SkyboxPass: a camera-centered cube, depth
// pinned to the far plane, depth writes disabled, sampled from the same cubemap.
class VulkanSkyboxPass {
public:
    bool Initialize(const VulkanSkyboxPassCreateInfo& createInfo);
    void Shutdown();
    void Resize(VkExtent2D extent);

    void SetPipeline(VkPipeline pipeline, VkPipelineLayout layout);
    void SetAttachments(VkImageView colorView, VkImageView depthView);
    VkRenderPass GetRenderPass() const { return renderPass_; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return descriptorSetLayout_; }

    void SetViewProjection(const glm::mat4& view, const glm::mat4& proj);
    void Record(VkCommandBuffer commandBuffer);

    static VkVertexInputBindingDescription VertexBindingDescription();
    static VkVertexInputAttributeDescription VertexAttributeDescription();

private:
    bool CreateRenderPass();
    bool CreateCubeBuffers();
    bool CreateDescriptorResources();
    bool CreateSkyboxCubemap();
    bool RebuildFramebuffer();
    void DestroyFramebuffer();
    void DestroyCubeBuffers();
    void DestroyDescriptorResources();
    void DestroySkyboxCubemap();
    bool UpdateSkyUbo() const;

    struct SkyUbo {
        glm::mat4 view{ 1.0f };
        glm::mat4 proj{ 1.0f };
    };

    VkExtent2D extent_{ 0, 0 };
    VkFormat colorFormat_ = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depthFormat_ = VK_FORMAT_D32_SFLOAT;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkImageView colorView_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;

    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory_ = VK_NULL_HANDLE;
    uint32_t indexCount_ = 0;

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkSampler cubemapSampler_ = VK_NULL_HANDLE;
    VkImage skyboxImage_ = VK_NULL_HANDLE;
    VkDeviceMemory skyboxMemory_ = VK_NULL_HANDLE;
    VkImageView skyboxView_ = VK_NULL_HANDLE;
    mutable VkBuffer skyUboBuffer_ = VK_NULL_HANDLE;
    mutable VkDeviceMemory skyUboMemory_ = VK_NULL_HANDLE;

    glm::mat4 view_{ 1.0f };
    glm::mat4 proj_{ 1.0f };
};

} // namespace te
