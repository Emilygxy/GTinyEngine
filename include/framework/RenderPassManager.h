#pragma once
#include "framework/RenderPass.h"

namespace te
{
// Render Pass Manager
class RenderPassManager
{
public:
    static RenderPassManager& GetInstance();
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
private:
    RenderPassManager() = default;
    ~RenderPassManager() = default;
    std::vector<std::shared_ptr<RenderPass>> mPasses;
    std::unordered_map<std::string, size_t> mPassIndexMap;
    bool mDirty = true;  // Track if dependency graph needs re-sorting
};
}