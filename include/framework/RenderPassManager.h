#pragma once
#include "framework/RenderPass.h"
#include "framework/RenderGraph.h"
#include <memory>

namespace te
{
class RenderGraphVisualizer;

// Render Pass Manager
class RenderPassManager
{
public:
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
};
}