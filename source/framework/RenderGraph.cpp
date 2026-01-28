#include "framework/RenderGraph.h"
#include "framework/RenderPass.h"
#include "glad/glad.h"
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <queue>

namespace te
{
    // ============================================================================
    // RenderGraphBuilder Implementation
    // ============================================================================

    RenderGraphBuilder::RenderGraphBuilder()
        : mCurrentPass(nullptr)
    {
    }

    RenderGraphBuilder& RenderGraphBuilder::AddPass(const std::string& name, 
                                                     std::shared_ptr<RenderPass> pass)
    {
        if (!pass)
        {
            std::cout << "RenderGraphBuilder::AddPass: Pass is null" << std::endl;
            return *this;
        }

        PassNode node;
        node.name = name;
        node.pass = pass;
        
        // Get dependencies from RenderPass configuration
        const auto& config = pass->GetConfig();
        for (const auto& dep : config.dependencies)
        {
            node.dependencies.push_back(dep.passName);
        }
        
        // Get inputs from RenderPass configuration (as reads)
        for (const auto& input : config.inputs)
        {
            ResourceUsage usage;
            usage.resourceName = input.sourceTarget;
            usage.access = ResourceAccess::Read;
            usage.requiredState = ResourceState::ShaderResource;
            usage.outputState = ResourceState::ShaderResource;
            usage.passIndex = mPasses.size();
            node.reads.push_back(usage);
        }
        
        // Get outputs from RenderPass configuration (as writes)
        for (const auto& output : config.outputs)
        {
            ResourceUsage usage;
            usage.resourceName = output.targetName;
            usage.access = ResourceAccess::Write;
            usage.requiredState = ResourceState::RenderTarget;
            usage.outputState = ResourceState::ShaderResource;
            usage.passIndex = mPasses.size();
            node.writes.push_back(usage);
        }
        
        // set execution function
        node.executeFunc = [pass](const std::vector<RenderCommand>& commands) {
            if (pass->IsEnabled())
            {
                pass->Execute(commands);
            }
        };
        
        mPasses.push_back(node);
        mCurrentPass = &mPasses.back();
        
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::Read(const std::string& resourceName)
    {
        if (!mCurrentPass)
        {
            std::cout << "RenderGraphBuilder::Read: No current pass" << std::endl;
            return *this;
        }

        ResourceUsage usage;
        usage.resourceName = resourceName;
        usage.access = ResourceAccess::Read;
        usage.requiredState = ResourceState::ShaderResource;
        usage.outputState = ResourceState::ShaderResource;
        usage.passIndex = mPasses.size() - 1;
        
        mCurrentPass->reads.push_back(usage);
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::Write(const std::string& resourceName)
    {
        if (!mCurrentPass)
        {
            std::cout << "RenderGraphBuilder::Write: No current pass" << std::endl;
            return *this;
        }

        ResourceUsage usage;
        usage.resourceName = resourceName;
        usage.access = ResourceAccess::Write;
        usage.requiredState = ResourceState::RenderTarget;
        usage.outputState = ResourceState::ShaderResource;
        usage.passIndex = mPasses.size() - 1;
        
        mCurrentPass->writes.push_back(usage);
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::ReadWrite(const std::string& resourceName)
    {
        if (!mCurrentPass)
        {
            std::cout << "RenderGraphBuilder::ReadWrite: No current pass" << std::endl;
            return *this;
        }

        ResourceUsage usage;
        usage.resourceName = resourceName;
        usage.access = ResourceAccess::ReadWrite;
        usage.requiredState = ResourceState::RenderTarget;
        usage.outputState = ResourceState::RenderTarget;
        usage.passIndex = mPasses.size() - 1;
        
        mCurrentPass->reads.push_back(usage);
        mCurrentPass->writes.push_back(usage);
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::DeclareResource(const ResourceDesc& desc)
    {
        mResources[desc.name] = desc;
        return *this;
    }

    std::unique_ptr<CompiledGraph> RenderGraphBuilder::Compile()
    {
        return RenderGraphCompiler::Compile(mPasses, mResources);
    }

    void RenderGraphBuilder::Clear()
    {
        mPasses.clear();
        mResources.clear();
        mCurrentPass = nullptr;
    }

    // ============================================================================
    // RenderGraphCompiler Implementation
    // ============================================================================

    std::unique_ptr<CompiledGraph> RenderGraphCompiler::Compile(
        const std::vector<PassNode>& passes,
        const std::unordered_map<std::string, ResourceDesc>& resources)
    {
        auto compiledGraph = std::make_unique<CompiledGraph>();
        compiledGraph->resources = resources;

        // build dependency graph
        std::vector<std::vector<size_t>> adjacencyList(passes.size());
        BuildDependencyGraph(passes, adjacencyList);

        // topological sort
        compiledGraph->executionOrder = TopologicalSort(adjacencyList);
        
        if (compiledGraph->executionOrder.size() != passes.size())
        {
            std::cout << "RenderGraphCompiler::Compile: Circular dependency detected!" << std::endl;
            return nullptr;
        }

        // reorder Passes by execution order
        compiledGraph->passes.resize(passes.size());
        for (size_t i = 0; i < compiledGraph->executionOrder.size(); ++i)
        {
            size_t originalIndex = compiledGraph->executionOrder[i];
            compiledGraph->passes[i] = passes[originalIndex];
            // update Pass indices
            compiledGraph->passes[i].reads.clear();
            compiledGraph->passes[i].writes.clear();
            
            // re-add reads and writes, update indices
            for (const auto& read : passes[originalIndex].reads)
            {
                ResourceUsage usage = read;
                usage.passIndex = i;
                compiledGraph->passes[i].reads.push_back(usage);
            }
            for (const auto& write : passes[originalIndex].writes)
            {
                ResourceUsage usage = write;
                usage.passIndex = i;
                compiledGraph->passes[i].writes.push_back(usage);
            }
        }

        // analyze resource lifetimes
        AnalyzeResourceLifetimes(compiledGraph->passes, 
                                 compiledGraph->executionOrder,
                                 compiledGraph->lifetimes);

        // analyze resource aliasing
        AnalyzeResourceAliasing(resources, 
                                compiledGraph->lifetimes,
                                compiledGraph->aliases);

        // generate resource allocations
        compiledGraph->allocations = GenerateResourceAllocations(
            compiledGraph->lifetimes,
            compiledGraph->aliases);

        // generate sync points
        compiledGraph->syncPoints = GenerateSyncPoints(
            compiledGraph->passes,
            compiledGraph->executionOrder);

        return compiledGraph;
    }

    void RenderGraphCompiler::BuildDependencyGraph(
        const std::vector<PassNode>& passes,
        std::vector<std::vector<size_t>>& adjacencyList)
    {
        // create Pass name to index mapping
        std::unordered_map<std::string, size_t> nameToIndex;
        for (size_t i = 0; i < passes.size(); ++i)
        {
            nameToIndex[passes[i].name] = i;
        }

        // build dependency graph
        for (size_t i = 0; i < passes.size(); ++i)
        {
            const auto& pass = passes[i];
            
            // handle explicit dependencies
            for (const auto& depName : pass.dependencies)
            {
                auto it = nameToIndex.find(depName);
                if (it != nameToIndex.end())
                {
                    adjacencyList[it->second].push_back(i);
                }
            }
            
            // handle resource dependencies (read resources must come from Pass that writes to them)
            for (const auto& read : pass.reads)
            {
                // find Pass that writes to this resource
                for (size_t j = 0; j < passes.size(); ++j)
                {
                    if (i == j) continue;
                    
                    for (const auto& write : passes[j].writes)
                    {
                        if (write.resourceName == read.resourceName)
                        {
                            adjacencyList[j].push_back(i);
                            break;
                        }
                    }
                }
            }
        }
    }

    std::vector<size_t> RenderGraphCompiler::TopologicalSort(
        const std::vector<std::vector<size_t>>& adjacencyList)
    {
        size_t n = adjacencyList.size();
        std::vector<int> inDegree(n, 0);
        
        // calculate in-degree
        for (const auto& adjList : adjacencyList)
        {
            for (size_t v : adjList)
            {
                inDegree[v]++;
            }
        }
        
        // use queue for topological sort
        std::queue<size_t> q;
        for (size_t i = 0; i < n; ++i)
        {
            if (inDegree[i] == 0)
            {
                q.push(i);
            }
        }
        
        std::vector<size_t> result;
        while (!q.empty())
        {
            size_t u = q.front();
            q.pop();
            result.push_back(u);
            
            for (size_t v : adjacencyList[u])
            {
                inDegree[v]--;
                if (inDegree[v] == 0)
                {
                    q.push(v);
                }
            }
        }
        
        return result;
    }

    void RenderGraphCompiler::AnalyzeResourceLifetimes(
        const std::vector<PassNode>& passes,
        const std::vector<size_t>& executionOrder,
        std::unordered_map<std::string, ResourceLifetime>& lifetimes)
    {
        // initialize all resource lifetimes
        for (const auto& pass : passes)
        {
            for (const auto& read : pass.reads)
            {
                if (lifetimes.find(read.resourceName) == lifetimes.end())
                {
                    lifetimes[read.resourceName] = ResourceLifetime();
                }
            }
            for (const auto& write : pass.writes)
            {
                if (lifetimes.find(write.resourceName) == lifetimes.end())
                {
                    lifetimes[write.resourceName] = ResourceLifetime();
                }
            }
        }

        // analyze resource usage for each Pass
        for (size_t orderIdx = 0; orderIdx < executionOrder.size(); ++orderIdx)
        {
            size_t passIdx = executionOrder[orderIdx];
            const auto& pass = passes[passIdx];
            
            // analyze read resources
            for (const auto& read : pass.reads)
            {
                auto& lifetime = lifetimes[read.resourceName];
                if (lifetime.firstUse == SIZE_MAX)
                    lifetime.firstUse = orderIdx;
                lifetime.lastUse = orderIdx;
            }
            
            // analyze written resources
            for (const auto& write : pass.writes)
            {
                auto& lifetime = lifetimes[write.resourceName];
                if (lifetime.firstUse == SIZE_MAX)
                    lifetime.firstUse = orderIdx;
                lifetime.lastUse = orderIdx;
            }
        }

        // mark resources that can be aliased
        for (auto& [name, lifetime] : lifetimes)
        {
            lifetime.isAliasable = true;  // default is aliasable, will be further checked in aliasing analysis
        }
    }

    void RenderGraphCompiler::AnalyzeResourceAliasing(
        const std::unordered_map<std::string, ResourceDesc>& resources,
        const std::unordered_map<std::string, ResourceLifetime>& lifetimes,
        std::unordered_map<std::string, std::string>& aliases)
    {
        // group resources by format and size
        std::map<std::tuple<RenderTargetFormat, uint32_t, uint32_t>, 
                 std::vector<std::string>> resourceGroups;
        
        for (const auto& [name, desc] : resources)
        {
            if (!desc.allowAliasing)
                continue;
                
            auto it = lifetimes.find(name);
            if (it == lifetimes.end())
                continue;
                
            auto key = std::make_tuple(desc.format, desc.width, desc.height);
            resourceGroups[key].push_back(name);
        }
        
        // for each group of resources, check if they can be aliased
        for (auto& [key, resourceNames] : resourceGroups)
        {
            if (resourceNames.size() < 2) continue;
            
            // check if lifetimes overlap
            for (size_t i = 0; i < resourceNames.size(); ++i)
            {
                for (size_t j = i + 1; j < resourceNames.size(); ++j)
                {
                    const std::string& name1 = resourceNames[i];
                    const std::string& name2 = resourceNames[j];
                    
                    auto it1 = lifetimes.find(name1);
                    auto it2 = lifetimes.find(name2);
                    
                    if (it1 == lifetimes.end() || it2 == lifetimes.end())
                        continue;
                    
                    const auto& lifetime1 = it1->second;
                    const auto& lifetime2 = it2->second;
                    
                    // if lifetimes do not overlap, can be aliased
                    if (lifetime1.lastUse < lifetime2.firstUse || 
                        lifetime2.lastUse < lifetime1.firstUse)
                    {
                        // alias name2 to name1
                        if (aliases.find(name2) == aliases.end())
                        {
                            aliases[name2] = name1;
                        }
                    }
                }
            }
        }
    }

    std::vector<SyncPoint> RenderGraphCompiler::GenerateSyncPoints(
        const std::vector<PassNode>& passes,
        const std::vector<size_t>& executionOrder)
    {
        std::vector<SyncPoint> syncPoints;
        
        // track current state of each resource
        std::unordered_map<std::string, ResourceState> resourceStates;
        
        // initialize resource states
        for (const auto& pass : passes)
        {
            for (const auto& write : pass.writes)
            {
                resourceStates[write.resourceName] = ResourceState::Undefined;
            }
        }
        
        for (size_t orderIdx = 0; orderIdx < executionOrder.size(); ++orderIdx)
        {
            size_t passIdx = executionOrder[orderIdx];
            const auto& pass = passes[passIdx];
            
            SyncPoint sync;
            sync.passIndex = orderIdx;
            
            // check if read resources need state transition
            for (const auto& read : pass.reads)
            {
                auto currentState = resourceStates[read.resourceName];
                if (currentState != read.requiredState && 
                    currentState != ResourceState::Undefined)
                {
                    sync.resources.push_back(read.resourceName);
                    sync.fromState = currentState;
                    sync.toState = read.requiredState;
                }
            }
            
            // update written resource states
            for (const auto& write : pass.writes)
            {
                resourceStates[write.resourceName] = write.outputState;
            }
            
            if (!sync.resources.empty())
            {
                syncPoints.push_back(sync);
            }
        }
        
        return syncPoints;
    }

    std::vector<ResourceAllocation> RenderGraphCompiler::GenerateResourceAllocations(
        const std::unordered_map<std::string, ResourceLifetime>& lifetimes,
        const std::unordered_map<std::string, std::string>& aliases)
    {
        std::vector<ResourceAllocation> allocations;
        
        for (const auto& [name, lifetime] : lifetimes)
        {
            ResourceAllocation allocation;
            allocation.resourceName = name;
            
            // check if there is an alias
            auto aliasIt = aliases.find(name);
            if (aliasIt != aliases.end())
            {
                allocation.aliasName = aliasIt->second;
            }
            else
            {
                allocation.aliasName = "";
            }
            
            allocation.createPassIndex = lifetime.firstUse;
            allocation.destroyPassIndex = lifetime.lastUse + 1;  // destroy after last use
            
            allocations.push_back(allocation);
        }
        
        return allocations;
    }

    // ============================================================================
    // RenderGraphExecutor Implementation
    // ============================================================================

    RenderGraphExecutor::RenderGraphExecutor(std::unique_ptr<CompiledGraph> compiledGraph)
        : mCompiledGraph(std::move(compiledGraph))
    {
        if (!mCompiledGraph)
        {
            std::cout << "RenderGraphExecutor: CompiledGraph is null" << std::endl;
            return;
        }

        // initialize resource states
        for (const auto& [name, desc] : mCompiledGraph->resources)
        {
            mResourceStates[name] = desc.initialState;
        }
    }

    RenderGraphExecutor::~RenderGraphExecutor()
    {
        Clear();
    }

    void RenderGraphExecutor::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!mCompiledGraph)
        {
            std::cout << "RenderGraphExecutor::Execute: CompiledGraph is null" << std::endl;
            return;
        }

        // track resource creation/destruction for each Pass
        std::unordered_map<std::string, bool> resourceCreated;
        
        for (size_t i = 0; i < mCompiledGraph->executionOrder.size(); ++i)
        {
            size_t passIdx = mCompiledGraph->executionOrder[i];
            const auto& pass = mCompiledGraph->passes[passIdx];
            
            // create required resources
            for (const auto& read : pass.reads)
            {
                if (!resourceCreated[read.resourceName])
                {
                    auto it = mCompiledGraph->resources.find(read.resourceName);
                    if (it != mCompiledGraph->resources.end())
                    {
                        CreateResource(read.resourceName, it->second);
                        resourceCreated[read.resourceName] = true;
                    }
                }
            }
            for (const auto& write : pass.writes)
            {
                if (!resourceCreated[write.resourceName])
                {
                    auto it = mCompiledGraph->resources.find(write.resourceName);
                    if (it != mCompiledGraph->resources.end())
                    {
                        CreateResource(write.resourceName, it->second);
                        resourceCreated[write.resourceName] = true;
                    }
                }
            }
            
            // insert sync points
            for (const auto& sync : mCompiledGraph->syncPoints)
            {
                if (sync.passIndex == i)
                {
                    InsertSyncPoint(sync);
                }
            }
            
            // set Pass's input resources
            if (pass.pass)
            {
                for (const auto& read : pass.reads)
                {
                    GLuint handle = GetResourceHandle(read.resourceName);
                    if (handle != 0)
                    {
                        // find corresponding input name in configuration
                        const auto& config = pass.pass->GetConfig();
                        for (const auto& input : config.inputs)
                        {
                            if (input.sourceTarget == read.resourceName)
                            {
                                pass.pass->SetInput(input.name, handle);
                                break;
                            }
                        }
                    }
                }
            }
            
            // prepare Pass
            if (pass.pass)
            {
                pass.pass->Prepare();
            }
            
            // execute Pass
            if (pass.executeFunc)
            {
                pass.executeFunc(commands);
            }
            
            // destroy resources that are no longer needed
            for (const auto& allocation : mCompiledGraph->allocations)
            {
                if (allocation.destroyPassIndex == i + 1)
                {
                    DestroyResource(allocation.resourceName);
                    resourceCreated[allocation.resourceName] = false;
                }
            }
        }
    }

    GLuint RenderGraphExecutor::GetResourceHandle(const std::string& resourceName) const
    {
        // check if there is an alias
        auto aliasIt = mCompiledGraph->aliases.find(resourceName);
        std::string actualName = (aliasIt != mCompiledGraph->aliases.end()) 
                                ? aliasIt->second 
                                : resourceName;
        
        auto it = mResourceHandles.find(actualName);
        return (it != mResourceHandles.end()) ? it->second : 0;
    }

    void RenderGraphExecutor::CreateResource(const std::string& name, const ResourceDesc& desc)
    {
        // check if there is an alias resource
        auto aliasIt = mCompiledGraph->aliases.find(name);
        std::string actualName = (aliasIt != mCompiledGraph->aliases.end()) 
                                ? aliasIt->second 
                                : name;
        
        // if alias resource exists, use it
        if (aliasIt != mCompiledGraph->aliases.end())
        {
            auto it = mResources.find(actualName);
            if (it != mResources.end())
            {
                mResourceHandles[name] = it->second->GetTextureHandle();
                mResourceStates[name] = desc.initialState;
                return;
            }
        }
        
        // create new resource
        RenderTargetDesc targetDesc;
        targetDesc.name = actualName;
        targetDesc.width = desc.width;
        targetDesc.height = desc.height;
        targetDesc.format = desc.format;
        
        // determine type based on format
        switch (desc.format)
        {
        case RenderTargetFormat::Depth24:
        case RenderTargetFormat::Depth32F:
            targetDesc.type = RenderTargetType::Depth;
            break;
        case RenderTargetFormat::Depth24Stencil8:
            targetDesc.type = RenderTargetType::ColorDepthStencil;
            break;
        default:
            targetDesc.type = RenderTargetType::Color;
            break;
        }
        
        auto renderTarget = std::make_shared<RenderTarget>();
        if (renderTarget->Initialize(targetDesc))
        {
            mResources[actualName] = renderTarget;
            mResourceHandles[name] = renderTarget->GetTextureHandle();
            mResourceStates[name] = desc.initialState;
        }
        else
        {
            std::cout << "RenderGraphExecutor::CreateResource: Failed to create resource " << name << std::endl;
        }
    }

    void RenderGraphExecutor::DestroyResource(const std::string& name)
    {
        // check if there is an alias
        auto aliasIt = mCompiledGraph->aliases.find(name);
        std::string actualName = (aliasIt != mCompiledGraph->aliases.end()) 
                                ? aliasIt->second 
                                : name;
        
        // if resource is aliased, do not destroy (managed by main resource)
        if (aliasIt != mCompiledGraph->aliases.end())
        {
            mResourceHandles.erase(name);
            return;
        }
        
        // check if there are other resources using this actual resource
        bool stillInUse = false;
        for (const auto& [aliasName, actual] : mCompiledGraph->aliases)
        {
            if (actual == actualName && aliasName != name)
            {
                stillInUse = true;
                break;
            }
        }
        
        if (!stillInUse)
        {
            auto it = mResources.find(actualName);
            if (it != mResources.end())
            {
                it->second->Shutdown();
                mResources.erase(it);
            }
        }
        
        mResourceHandles.erase(name);
        mResourceStates.erase(name);
    }

    void RenderGraphExecutor::InsertSyncPoint(const SyncPoint& sync)
    {
        // in OpenGL, synchronization is mainly through glFinish or glMemoryBarrier
        // here simplified, in actual application, more precise synchronization may be needed
        for (const auto& resourceName : sync.resources)
        {
            TransitionResource(resourceName, sync.fromState, sync.toState);
        }
        
        // insert memory barrier (if needed)
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void RenderGraphExecutor::TransitionResource(const std::string& name, 
                                                  ResourceState from, ResourceState to)
    {
        // in OpenGL, resource state transition is mainly conceptual
        // actual state transition is handled automatically when binding resources
        // here mainly update internal state tracking
        mResourceStates[name] = to;
    }

    void RenderGraphExecutor::Clear()
    {
        // destroy all resources
        for (auto& [name, target] : mResources)
        {
            target->Shutdown();
        }
        
        mResources.clear();
        mResourceHandles.clear();
        mResourceStates.clear();
    }
}
