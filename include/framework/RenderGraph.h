#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <map>
#include <tuple>
#include "framework/RenderPass.h"
#include "framework/FrameBuffer.h"

namespace te
{
    // Forward declarations
    class RenderPass;

    // Resource Access Mode
    enum class ResourceAccess
    {
        Read,           // read only
        Write,          // write only
        ReadWrite       // read write
    };

    // Resource State
    enum class ResourceState
    {
        Undefined,      // undefined
        RenderTarget,   // render target
        ShaderResource, // shader resource
        Present         // present
    };

    // Resource Description
    struct ResourceDesc
    {
        std::string name;
        RenderTargetFormat format;
        uint32_t width;
        uint32_t height;
        ResourceAccess access;
        ResourceState initialState;
        ResourceState finalState;
        bool allowAliasing = false;  // allow aliasing
    };

    // Resource Usage
    struct ResourceUsage
    {
        std::string resourceName;
        ResourceAccess access;
        ResourceState requiredState;
        ResourceState outputState;
        size_t passIndex;  // pass index that uses this resource
    };

    // Resource Lifetime
    struct ResourceLifetime
    {
        size_t firstUse = SIZE_MAX;   // first use
        size_t lastUse = 0;            // last use
        bool isAliasable = false;      // allow aliasing
    };

    // Pass Node
    struct PassNode
    {
        std::string name;
        std::shared_ptr<RenderPass> pass;
        
        // resource usage
        std::vector<ResourceUsage> reads;   // reads
        std::vector<ResourceUsage> writes;  // writes
        
        // dependencies
        std::vector<std::string> dependencies;
        
        // execution information
        std::function<void(const std::vector<RenderCommand>&)> executeFunc;
    };

    // Sync Point
    struct SyncPoint
    {
        size_t passIndex;
        std::vector<std::string> resources;
        ResourceState fromState;
        ResourceState toState;
    };

    // Resource Allocation
    struct ResourceAllocation
    {
        std::string resourceName;
        std::string aliasName;  // if empty, use own resource
        size_t createPassIndex;  // create resource pass index
        size_t destroyPassIndex; // destroy resource pass index
    };

    // Compiled Graph
    struct CompiledGraph
    {
        // execution order
        std::vector<size_t> executionOrder;
        
        // Pass nodes (in execution order)
        std::vector<PassNode> passes;
        
        // resource allocations
        std::vector<ResourceAllocation> allocations;
        
        // sync points
        std::vector<SyncPoint> syncPoints;
        
        // resource alias mapping
        std::unordered_map<std::string, std::string> aliases;
        
        // resource descriptions
        std::unordered_map<std::string, ResourceDesc> resources;
        
        // resource lifetimes
        std::unordered_map<std::string, ResourceLifetime> lifetimes;
    };

    // RenderGraph Builder
    class RenderGraphBuilder
    {
    public:
        RenderGraphBuilder();
        ~RenderGraphBuilder() = default;

        // add Pass
        RenderGraphBuilder& AddPass(const std::string& name, 
                                    std::shared_ptr<RenderPass> pass);
        
        // declare resource usage (for current pass being built)
        RenderGraphBuilder& Read(const std::string& resourceName);
        RenderGraphBuilder& Write(const std::string& resourceName);
        RenderGraphBuilder& ReadWrite(const std::string& resourceName);
        
        // set resource description
        RenderGraphBuilder& DeclareResource(const ResourceDesc& desc);
        
        // compile graph
        std::unique_ptr<CompiledGraph> Compile();
        
        // clear builder
        void Clear();
        
        // get resources (for checking if resources are declared)
        const std::unordered_map<std::string, ResourceDesc>& GetResources() const { return mResources; }

    private:
        std::vector<PassNode> mPasses;
        std::unordered_map<std::string, ResourceDesc> mResources;
        PassNode* mCurrentPass;  // current pass being built
    };

    // RenderGraph Compiler
    class RenderGraphCompiler
    {
    public:
        static std::unique_ptr<CompiledGraph> Compile(
            const std::vector<PassNode>& passes,
            const std::unordered_map<std::string, ResourceDesc>& resources);
    
    private:
        // build dependency graph
        static void BuildDependencyGraph(
            const std::vector<PassNode>& passes,
            std::vector<std::vector<size_t>>& adjacencyList);
        
        // topological sort
        static std::vector<size_t> TopologicalSort(
            const std::vector<std::vector<size_t>>& adjacencyList);
        
        // analyze resource lifetimes
        static void AnalyzeResourceLifetimes(
            const std::vector<PassNode>& passes,
            const std::vector<size_t>& executionOrder,
            std::unordered_map<std::string, ResourceLifetime>& lifetimes);
        
        // analyze resource aliasing
        static void AnalyzeResourceAliasing(
            const std::unordered_map<std::string, ResourceDesc>& resources,
            const std::unordered_map<std::string, ResourceLifetime>& lifetimes,
            std::unordered_map<std::string, std::string>& aliases);
        
        // generate sync points
        static std::vector<SyncPoint> GenerateSyncPoints(
            const std::vector<PassNode>& passes,
            const std::vector<size_t>& executionOrder);
        
        // generate resource allocations
        static std::vector<ResourceAllocation> GenerateResourceAllocations(
            const std::unordered_map<std::string, ResourceLifetime>& lifetimes,
            const std::unordered_map<std::string, std::string>& aliases);
    };

    // RenderGraph Executor
    class RenderGraphExecutor
    {
    public:
        RenderGraphExecutor(std::unique_ptr<CompiledGraph> compiledGraph);
        ~RenderGraphExecutor();
        
        void Execute(const std::vector<RenderCommand>& commands);
        
        // get resource handle
        GLuint GetResourceHandle(const std::string& resourceName) const;
        
        // get compiled graph (for visualization, etc.)
        const CompiledGraph* GetCompiledGraph() const { return mCompiledGraph.get(); }
        
        // clear all resources
        void Clear();
    
    private:
        // resource management
        void CreateResource(const std::string& name, const ResourceDesc& desc);
        void DestroyResource(const std::string& name);
        
        // sync
        void InsertSyncPoint(const SyncPoint& sync);
        
        // state transition (OpenGL mainly deals with layout transitions, here simplified)
        void TransitionResource(const std::string& name, 
                               ResourceState from, ResourceState to);
        
        std::unique_ptr<CompiledGraph> mCompiledGraph;
        std::unordered_map<std::string, std::shared_ptr<RenderTarget>> mResources;
        std::unordered_map<std::string, GLuint> mResourceHandles;
        std::unordered_map<std::string, ResourceState> mResourceStates;
    };
}
