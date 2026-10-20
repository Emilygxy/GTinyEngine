#pragma once

#include "framework/VulkanGeometryPass.h"
#include "framework/VulkanLightingPass.h"
#include "framework/VulkanSkyboxPass.h"

namespace te {

struct VulkanDeferredPipelineCreateInfo {
    VulkanGeometryPassCreateInfo geometry{};
    VulkanLightingPassCreateInfo lighting{};
    VulkanSkyboxPassCreateInfo skybox{};
};

class VulkanDeferredPipeline {
public:
    VulkanDeferredPipeline() = default;
    ~VulkanDeferredPipeline() = default;

    bool Initialize(const VulkanDeferredPipelineCreateInfo& createInfo);
    void Shutdown();
    bool Resize(VkExtent2D extent);

    void RecordFrame(VkCommandBuffer commandBuffer, const std::vector<RenderCommand>& commands);

    VulkanGeometryPass& GeometryPass() { return geometryPass_; }
    VulkanLightingPass& LightingPass() { return lightingPass_; }
    VulkanSkyboxPass& SkyboxPass() { return skyboxPass_; }

private:
    VulkanGeometryPass geometryPass_{};
    VulkanLightingPass lightingPass_{};
    VulkanSkyboxPass skyboxPass_{};
    VkExtent2D extent_{ 0, 0 };
};

} // namespace te

