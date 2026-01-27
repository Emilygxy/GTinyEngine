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
        
        // 从 RenderPass 配置中提取依赖关系
        const auto& config = pass->GetConfig();
        for (const auto& dep : config.dependencies)
        {
            node.dependencies.push_back(dep.passName);
        }
        
        // 从 RenderPass 配置中提取输入（作为 reads）
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
        
        // 从 RenderPass 配置中提取输出（作为 writes）
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
        
        // 设置执行函数
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

        // 构建依赖图
        std::vector<std::vector<size_t>> adjacencyList(passes.size());
        BuildDependencyGraph(passes, adjacencyList);

        // 拓扑排序
        compiledGraph->executionOrder = TopologicalSort(adjacencyList);
        
        if (compiledGraph->executionOrder.size() != passes.size())
        {
            std::cout << "RenderGraphCompiler::Compile: Circular dependency detected!" << std::endl;
            return nullptr;
        }

        // 按执行顺序重新排列 Pass
        compiledGraph->passes.resize(passes.size());
        for (size_t i = 0; i < compiledGraph->executionOrder.size(); ++i)
        {
            size_t originalIndex = compiledGraph->executionOrder[i];
            compiledGraph->passes[i] = passes[originalIndex];
            // 更新 Pass 索引
            compiledGraph->passes[i].reads.clear();
            compiledGraph->passes[i].writes.clear();
            
            // 重新添加 reads 和 writes，更新索引
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

        // 分析资源生命周期
        AnalyzeResourceLifetimes(compiledGraph->passes, 
                                 compiledGraph->executionOrder,
                                 compiledGraph->lifetimes);

        // 资源别名分析
        AnalyzeResourceAliasing(resources, 
                                compiledGraph->lifetimes,
                                compiledGraph->aliases);

        // 生成资源分配
        compiledGraph->allocations = GenerateResourceAllocations(
            compiledGraph->lifetimes,
            compiledGraph->aliases);

        // 生成同步点
        compiledGraph->syncPoints = GenerateSyncPoints(
            compiledGraph->passes,
            compiledGraph->executionOrder);

        return compiledGraph;
    }

    void RenderGraphCompiler::BuildDependencyGraph(
        const std::vector<PassNode>& passes,
        std::vector<std::vector<size_t>>& adjacencyList)
    {
        // 创建 Pass 名称到索引的映射
        std::unordered_map<std::string, size_t> nameToIndex;
        for (size_t i = 0; i < passes.size(); ++i)
        {
            nameToIndex[passes[i].name] = i;
        }

        // 构建依赖图
        for (size_t i = 0; i < passes.size(); ++i)
        {
            const auto& pass = passes[i];
            
            // 处理显式依赖
            for (const auto& depName : pass.dependencies)
            {
                auto it = nameToIndex.find(depName);
                if (it != nameToIndex.end())
                {
                    adjacencyList[it->second].push_back(i);
                }
            }
            
            // 处理资源依赖（读取的资源必须来自写入该资源的 Pass）
            for (const auto& read : pass.reads)
            {
                // 查找写入该资源的 Pass
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
        
        // 计算入度
        for (const auto& adjList : adjacencyList)
        {
            for (size_t v : adjList)
            {
                inDegree[v]++;
            }
        }
        
        // 使用队列进行拓扑排序
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
        // 初始化所有资源的生命周期
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

        // 分析每个 Pass 的资源使用
        for (size_t orderIdx = 0; orderIdx < executionOrder.size(); ++orderIdx)
        {
            size_t passIdx = executionOrder[orderIdx];
            const auto& pass = passes[passIdx];
            
            // 分析读取的资源
            for (const auto& read : pass.reads)
            {
                auto& lifetime = lifetimes[read.resourceName];
                if (lifetime.firstUse == SIZE_MAX)
                    lifetime.firstUse = orderIdx;
                lifetime.lastUse = orderIdx;
            }
            
            // 分析写入的资源
            for (const auto& write : pass.writes)
            {
                auto& lifetime = lifetimes[write.resourceName];
                if (lifetime.firstUse == SIZE_MAX)
                    lifetime.firstUse = orderIdx;
                lifetime.lastUse = orderIdx;
            }
        }

        // 标记可别名的资源
        for (auto& [name, lifetime] : lifetimes)
        {
            lifetime.isAliasable = true;  // 默认可以别名，后续在别名分析中会进一步判断
        }
    }

    void RenderGraphCompiler::AnalyzeResourceAliasing(
        const std::unordered_map<std::string, ResourceDesc>& resources,
        const std::unordered_map<std::string, ResourceLifetime>& lifetimes,
        std::unordered_map<std::string, std::string>& aliases)
    {
        // 按格式和尺寸分组资源
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
        
        // 对每组资源，检查是否可以别名
        for (auto& [key, resourceNames] : resourceGroups)
        {
            if (resourceNames.size() < 2) continue;
            
            // 检查生命周期是否重叠
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
                    
                    // 如果生命周期不重叠，可以别名
                    if (lifetime1.lastUse < lifetime2.firstUse || 
                        lifetime2.lastUse < lifetime1.firstUse)
                    {
                        // 将 name2 别名到 name1
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
        
        // 跟踪每个资源的当前状态
        std::unordered_map<std::string, ResourceState> resourceStates;
        
        // 初始化资源状态
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
            
            // 检查读取的资源是否需要状态转换
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
            
            // 更新写入资源的状态
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
            
            // 检查是否有别名
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
            allocation.destroyPassIndex = lifetime.lastUse + 1;  // 在最后使用后销毁
            
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

        // 初始化资源状态
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

        // 跟踪每个 Pass 的资源创建/销毁
        std::unordered_map<std::string, bool> resourceCreated;
        
        for (size_t i = 0; i < mCompiledGraph->executionOrder.size(); ++i)
        {
            size_t passIdx = mCompiledGraph->executionOrder[i];
            const auto& pass = mCompiledGraph->passes[passIdx];
            
            // 创建需要的资源
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
            
            // 插入同步点
            for (const auto& sync : mCompiledGraph->syncPoints)
            {
                if (sync.passIndex == i)
                {
                    InsertSyncPoint(sync);
                }
            }
            
            // 设置 Pass 的输入资源
            if (pass.pass)
            {
                for (const auto& read : pass.reads)
                {
                    GLuint handle = GetResourceHandle(read.resourceName);
                    if (handle != 0)
                    {
                        // 从配置中找到对应的输入名称
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
            
            // 准备 Pass
            if (pass.pass)
            {
                pass.pass->Prepare();
            }
            
            // 执行 Pass
            if (pass.executeFunc)
            {
                pass.executeFunc(commands);
            }
            
            // 销毁不再需要的资源
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
        // 检查是否有别名
        auto aliasIt = mCompiledGraph->aliases.find(resourceName);
        std::string actualName = (aliasIt != mCompiledGraph->aliases.end()) 
                                ? aliasIt->second 
                                : resourceName;
        
        auto it = mResourceHandles.find(actualName);
        return (it != mResourceHandles.end()) ? it->second : 0;
    }

    void RenderGraphExecutor::CreateResource(const std::string& name, const ResourceDesc& desc)
    {
        // 检查是否已经有别名资源
        auto aliasIt = mCompiledGraph->aliases.find(name);
        std::string actualName = (aliasIt != mCompiledGraph->aliases.end()) 
                                ? aliasIt->second 
                                : name;
        
        // 如果别名资源已存在，直接使用
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
        
        // 创建新资源
        RenderTargetDesc targetDesc;
        targetDesc.name = actualName;
        targetDesc.width = desc.width;
        targetDesc.height = desc.height;
        targetDesc.format = desc.format;
        
        // 根据格式确定类型
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
        // 检查是否有别名
        auto aliasIt = mCompiledGraph->aliases.find(name);
        std::string actualName = (aliasIt != mCompiledGraph->aliases.end()) 
                                ? aliasIt->second 
                                : name;
        
        // 如果资源被别名，不销毁（由主资源管理）
        if (aliasIt != mCompiledGraph->aliases.end())
        {
            mResourceHandles.erase(name);
            return;
        }
        
        // 检查是否还有其他资源使用这个实际资源
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
        // OpenGL 中，同步主要是通过 glFinish 或 glMemoryBarrier
        // 这里简化处理，在实际应用中可能需要更精细的同步
        for (const auto& resourceName : sync.resources)
        {
            TransitionResource(resourceName, sync.fromState, sync.toState);
        }
        
        // 插入内存屏障（如果需要）
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void RenderGraphExecutor::TransitionResource(const std::string& name, 
                                                  ResourceState from, ResourceState to)
    {
        // OpenGL 中资源状态转换主要是概念性的
        // 实际的状态转换在绑定资源时自动处理
        // 这里主要是更新内部状态跟踪
        mResourceStates[name] = to;
    }

    void RenderGraphExecutor::Clear()
    {
        // 销毁所有资源
        for (auto& [name, target] : mResources)
        {
            target->Shutdown();
        }
        
        mResources.clear();
        mResourceHandles.clear();
        mResourceStates.clear();
    }
}
