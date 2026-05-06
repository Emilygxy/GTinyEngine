#pragma once

#include "framework/VulkanGeometryPass.h"
#include "framework/VulkanLightingPass.h"

namespace te {

struct VulkanDeferredPipelineCreateInfo {
    VulkanGeometryPassCreateInfo geometry{};
    VulkanLightingPassCreateInfo lighting{};
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

private:
    VulkanGeometryPass geometryPass_{};
    VulkanLightingPass lightingPass_{};
    VkExtent2D extent_{ 0, 0 };
};

} // namespace te

