#pragma once
#include "framework/RenderPass.h"
#include "framework/RenderGraph.h"
#include "framework/VulkanDeferredPipeline.h"
#include <memory>
#include <utility>

enum class RendererBackend;

namespace te
{
class RenderGraphVisualizer;

// Render Pass Manager
class RenderPassManager
{
public:
    enum class ActiveBackend
    {
        OpenGL,
        Vulkan
    };

    static RenderPassManager& GetInstance();
    
    // ========== Legacy Interface (for backward compatibility) ==========
    // Pass Management
    bool AddPass(const std::shared_ptr<RenderPass>& pass);
    void RemovePass(const std::string& name);
    std::shared_ptr<RenderPass> GetPass(const std::string& name) const;
    // Execute All Passes
    void ExecuteAll(const std::vector<RenderCommand>& commands);
    void ExecuteAll() { ExecuteAll({}); }
    // Dependency Sorting
    void SortPassesByDependencies();
    // Dirty Management
    void MarkDirty() { mDirty = true; }
    bool IsDirty() const { return mDirty; }
    // Cleanup
    void Clear();
    
    // ========== RenderGraph Interface ==========
    // enable/disable RenderGraph
    void EnableRenderGraph(bool enable) { mUseRenderGraph = enable; }
    bool IsRenderGraphEnabled() const { return mUseRenderGraph; }
    
    // get RenderGraph Builder
    RenderGraphBuilder& GetGraphBuilder() { return mGraphBuilder; }
    
    // compile RenderGraph
    bool CompileRenderGraph();
    
    // execute with RenderGraph
    void ExecuteWithRenderGraph(const std::vector<RenderCommand>& commands);
    
    // get RenderGraph Executor (for getting resource handles, etc.)
    RenderGraphExecutor* GetGraphExecutor() const { return mExecutor.get(); }
    
    // set resource size (for RenderGraph compilation)
    void SetResourceSize(uint32_t width, uint32_t height) 
    { 
        mResourceWidth = width; 
        mResourceHeight = height; 
    }
    
    // generate visualization file (.dot format)
    bool GenerateVisualization(const std::string& filename);

    // ========== Vulkan Graph Interface (M2) ==========
    void EnableVulkanGraph(bool enable) { mUseVulkanGraph = enable; }
    bool IsVulkanGraphEnabled() const { return mUseVulkanGraph; }
    bool BuildVulkanDeferredGraph(VulkanDeferredPipeline* pipeline);
    void ExecuteVulkanGraph(VkCommandBuffer commandBuffer, const std::vector<RenderCommand>& commands);
    uint64_t GetVulkanResourceHandle(const std::string& name) const;
    void SetVulkanPostProcessCallback(std::function<void(VkCommandBuffer)> callback) { mVulkanPostProcessCallback = std::move(callback); }
    void SetVulkanPresentCallback(std::function<void(VkCommandBuffer)> callback) { mVulkanPresentCallback = std::move(callback); }
    void SetActiveBackend(ActiveBackend backend) { mActiveBackend = backend; }
    ActiveBackend GetActiveBackend() const { return mActiveBackend; }
    void SyncActiveBackend(RendererBackend backend);
    void SetVulkanCommandBuffer(VkCommandBuffer commandBuffer) { mVulkanCommandBuffer = commandBuffer; }
    
private:
    RenderPassManager() = default;
    ~RenderPassManager() = default;
    
    // Legacy members
    std::vector<std::shared_ptr<RenderPass>> mPasses;
    std::unordered_map<std::string, size_t> mPassIndexMap;
    bool mDirty = true;  // Track if dependency graph needs re-sorting
    
    // RenderGraph members
    bool mUseRenderGraph = false;
    RenderGraphBuilder mGraphBuilder;
    std::unique_ptr<CompiledGraph> mCompiledGraph;
    std::unique_ptr<RenderGraphExecutor> mExecutor;
    uint32_t mResourceWidth = 1920;   // default resource width
    uint32_t mResourceHeight = 1080;  // default resource height

    struct VulkanPassNode
    {
        std::string name;
        std::vector<std::string> dependencies;
        std::function<void(VkCommandBuffer, const std::vector<RenderCommand>&)> execute;
    };
    bool mUseVulkanGraph = false;
    VulkanDeferredPipeline* mpVulkanDeferredPipeline = nullptr;
    std::vector<VulkanPassNode> mVulkanPassNodes;
    std::unordered_map<std::string, uint64_t> mVulkanResourceHandles;
    std::function<void(VkCommandBuffer)> mVulkanPostProcessCallback;
    std::function<void(VkCommandBuffer)> mVulkanPresentCallback;
    ActiveBackend mActiveBackend = ActiveBackend::OpenGL;
    VkCommandBuffer mVulkanCommandBuffer = VK_NULL_HANDLE;
};
}