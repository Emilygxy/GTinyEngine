#pragma once

#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "GaussianSplat/GSComputeSubsystem.h"
#include "GaussianSplat/GSRenderTypes.h"

class VulkanRenderer;

/**
 * Prototype wiring: GS preprocess before the deferred graph, GS sort/render + depth-aware compositor after lighting.
 * Expects `VulkanRenderer::SetHybrid*` callbacks to be registered before `VulkanRenderer::Initialize()`.
 */
class HybridGSIntegration {
public:
    HybridGSIntegration(VulkanRenderer& renderer, std::vector<GSVertex> vertices);
    ~HybridGSIntegration();

    HybridGSIntegration(const HybridGSIntegration&) = delete;
    HybridGSIntegration& operator=(const HybridGSIntegration&) = delete;

    bool attachCamera(const std::shared_ptr<Camera>& camera);

    /** Call before `VulkanRenderer::Shutdown()` so GS/compositor GPU objects are released while the device is valid. */
    void shutdownGpu();

    void onPreprocess();
    void onAfterLighting(VkCommandBuffer cmd, uint32_t swapchainImageIndex);

    /** Call once after `VulkanRenderer::Initialize()` builds the HDR post render pass. */
    bool createCompositorAfterRendererInit();

private:
    bool createCompositorInternal();
    void destroyCompositor();

    bool released_ = false;
    VulkanRenderer& renderer_;
    gt::gs::GSComputeSubsystem gs_;
    std::vector<GSVertex> vertices_;

    struct Compositor;
    std::unique_ptr<Compositor> compositor_;
    gt::gs::GSPreprocessResult lastPre_{};
};
