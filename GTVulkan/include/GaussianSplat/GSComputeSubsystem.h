#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "GaussianSplat/GSRenderTypes.h"

class Camera;

namespace gt {
namespace gs {

class GaussianSplatComputeEngine;

/** Result of GS preprocess (prefix sum + instance count); needed before sort/render dispatch. */
struct GSPreprocessResult {
    uint32_t numInstances = 0;
    bool prefixInPing = true;
};

/**
 * Embedded Gaussian splat compute path: no window creation, no swapchain color target.
 * Intended to be recorded into the host's command buffer after deferred lighting (Strategy B).
 */
class GSComputeSubsystem {
public:
    GSComputeSubsystem();
    ~GSComputeSubsystem();

    GSComputeSubsystem(const GSComputeSubsystem&) = delete;
    GSComputeSubsystem& operator=(const GSComputeSubsystem&) = delete;

    bool initialize(const std::vector<GSVertex>& vertices, const std::shared_ptr<Camera>& camera);
    void shutdown();

    void updateUniforms();
    /** Submit preprocess on internal command buffer; CPU wait (reads instance count). Call once per frame before sort/render. */
    GSPreprocessResult runPreprocessSubmitWait();

    /** Record radix sort + tile render into an already-started command buffer. */
    void recordSortAndRender(VkCommandBuffer cmd, uint32_t swapchainImageIndex, const GSPreprocessResult& prep);

    VkImageView gsColorView(uint32_t index) const;
    VkImageView gsDepthView(uint32_t index) const;

private:
    std::unique_ptr<GaussianSplatComputeEngine> engine_;
};

} // namespace gs
} // namespace gt
