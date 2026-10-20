#include "framework/VulkanDeferredPipeline.h"

namespace te {

bool VulkanDeferredPipeline::Initialize(const VulkanDeferredPipelineCreateInfo& createInfo)
{
    Shutdown();
    extent_ = createInfo.geometry.extent;

    if (!geometryPass_.Initialize(createInfo.geometry)) {
        return false;
    }
    if (!lightingPass_.Initialize(createInfo.lighting)) {
        geometryPass_.Shutdown();
        return false;
    }
    if (!skyboxPass_.Initialize(createInfo.skybox)) {
        lightingPass_.Shutdown();
        geometryPass_.Shutdown();
        return false;
    }
    return true;
}

void VulkanDeferredPipeline::Shutdown()
{
    skyboxPass_.Shutdown();
    lightingPass_.Shutdown();
    geometryPass_.Shutdown();
    extent_ = { 0, 0 };
}

bool VulkanDeferredPipeline::Resize(VkExtent2D extent)
{
    extent_ = extent;
    skyboxPass_.Resize(extent_);
    lightingPass_.Resize(extent_);
    return geometryPass_.Resize(extent_);
}

void VulkanDeferredPipeline::RecordFrame(VkCommandBuffer commandBuffer, const std::vector<RenderCommand>& commands)
{
    geometryPass_.Record(commandBuffer, commands);
    lightingPass_.Record(commandBuffer, geometryPass_.GetGBuffer());
    skyboxPass_.Record(commandBuffer);
}

} // namespace te

