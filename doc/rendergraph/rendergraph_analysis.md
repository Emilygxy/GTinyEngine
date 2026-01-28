# RenderGraph 原理分析与 GTinyEngine 实现方案

## 目录
1. [RenderGraph 核心原理](#1-rendergraph-核心原理)
2. [当前 RenderPassManager 实现分析](#2-当前-renderpassmanager-实现分析)
3. [与标准 RenderGraph 的对比](#3-与标准-rendergraph-的对比)
4. [GTinyEngine RenderGraph 设计方案](#4-gtinyengine-rendergraph-设计方案)
5. [实现路线图](#5-实现路线图)

---

## 1. RenderGraph 核心原理

### 1.1 什么是 RenderGraph？
Render Graph是近几年流行起来的一种组织渲染管线的架构，最早由Frostbite提出，Filament、Unreal和Unity的SRP都实现了Render Graph。这是一种针对渲染管线的基于图的调度系统，使渲染管线的代码更容易扩展和维护，提供可视化的调试工具，收集整帧的渲染任务，基于整帧的信息验证渲染管线的正确性、剔除冗余的工作、充分利用现代图形API来优化性能。

Render Graph不会立即执行渲染命令，而是记录完整帧的命令后，进行编译、执行，一般有三个步骤：
- **Setup** 
定义使用的Pass和每个Pass的输入、输出资源，每个Pass通过输入、输出的资源连接起来。资源使用Handle来表示，定义时并没有分配内存，Render Graph内部负责资源的分配、释放；Pass用lambda定义，好处是可以保证线性的代码流，最小化现有代码的修改。
- **Compilation** 
剔除没用的Pass，计算资源的生命周期，计算Graphic和Compute的同步点。
- **Execution** 
插入Resource Barrier，执行剔除后的所有Pass，Pass的执行顺序和定义时一致，Render Graph不会对Pass重新排序，所以Setup时需要考虑Pass的顺序来使性能最优。

RenderGraph（渲染图）是现代游戏引擎中用于管理渲染管线的高级抽象系统。它通过声明式的方式描述渲染流程，自动管理资源生命周期，优化渲染性能。

### 1.2 核心概念

#### 1.2.1 声明式资源管理（Declarative Resource Management）

**传统方式（命令式）：**
```cpp
// 传统方式：直接创建和管理资源
auto texture = CreateTexture(width, height);
RenderToTexture(texture);
UseTexture(texture);
DestroyTexture(texture);
```

**RenderGraph 方式（声明式）：**
```cpp
// RenderGraph：声明资源的使用意图
graph.AddPass("GeometryPass")
    .Write("GBuffer_Albedo")
    .Write("GBuffer_Normal")
    .Write("GBuffer_Depth");

graph.AddPass("LightingPass")
    .Read("GBuffer_Albedo")
    .Read("GBuffer_Normal")
    .Read("GBuffer_Depth")
    .Write("FinalColor");
```

#### 1.2.2 资源生命周期管理

RenderGraph 自动管理资源的创建和销毁：

- **资源创建时机**：在第一个使用该资源的 Pass 执行前创建
- **资源销毁时机**：在所有使用该资源的 Pass 执行完毕后销毁
- **资源复用**：如果两个 Pass 使用相同格式的资源但时间不重叠，可以复用同一块内存

#### 1.2.3 资源别名（Resource Aliasing）

RenderGraph 可以识别资源的生命周期，将不重叠的资源映射到同一块内存：

```
Pass1: Write ResourceA (生命周期: 0-100ms)
Pass2: Write ResourceB (生命周期: 200-300ms)
Pass3: Write ResourceC (生命周期: 400-500ms)

优化后：ResourceA、ResourceB、ResourceC 可以共享同一块内存
```

#### 1.2.4 屏障和同步（Barriers and Synchronization）

RenderGraph 自动插入必要的同步点：

- **布局转换**：从渲染目标布局转换为采样器布局
- **内存屏障**：确保写入完成后再读取
- **子资源同步**：Mipmap 生成、深度预处理的同步

#### 1.2.5 两阶段执行

**阶段1：编译阶段（Compile Phase）**
- 构建依赖图
- 分析资源使用模式
- 优化资源分配
- 生成执行计划

**阶段2：执行阶段（Execute Phase）**
- 按照编译好的计划执行
- 自动管理资源创建/销毁
- 自动插入同步点

### 1.3 现代引擎中的 RenderGraph

#### Unreal Engine 的 RenderGraph
- 使用 `FRDGBuilder` 构建渲染图
- 支持资源别名和自动优化
- 自动管理资源生命周期

#### Unity 的 RenderGraph
- 使用 `ScriptableRenderContext` 和 `RenderGraph`
- 支持多线程渲染
- 自动优化渲染顺序

#### Frostbite 的 RenderGraph
- 最早提出 RenderGraph 概念的引擎之一
- 支持复杂的多线程渲染
- 自动资源管理和优化

---

## 2. 当前 RenderPassManager 实现分析

### 2.1 当前实现概览

GTinyEngine 的 `RenderPassManager` 提供了基本的 Pass 管理功能：

```cpp
class RenderPassManager {
    bool AddPass(const std::shared_ptr<RenderPass>& pass);
    void RemovePass(const std::string& name);
    std::shared_ptr<RenderPass> GetPass(const std::string& name) const;
    void ExecuteAll(const std::vector<RenderCommand>& commands);
    void SortPassesByDependencies();
};
```

### 2.2 优点

1. **依赖排序**：实现了基本的拓扑排序，确保 Pass 按依赖顺序执行
2. **输入/输出管理**：通过 `RenderPassInput` 和 `RenderPassOutput` 声明资源依赖
3. **状态管理**：支持 Pass 的启用/禁用
4. **脏标记机制**：使用 `mDirty` 标志避免不必要的重新排序

### 2.3 存在的问题

#### 2.3.1 缺少声明式资源管理

**当前实现：**
```cpp
// RenderPassManager::ExecuteAll()
for (const auto& input : pass->GetConfig().inputs)
{
    auto sourcePass = GetPass(input.sourcePass);
    if (sourcePass)
    {
        auto outputTarget = sourcePass->GetOutput(input.sourceTarget);
        if (outputTarget)
        {
            pass->SetInput(input.sourceTarget, outputTarget->GetTextureHandle());
        }
    }
}
```

**问题：**
- 资源是在 Pass 执行时直接获取的，而不是在编译阶段分析
- 无法提前知道所有资源的生命周期
- 无法进行资源优化

#### 2.3.2 资源生命周期管理缺失

**当前实现：**
```cpp
// RenderPass::SetupFrameBuffer()
void RenderPass::SetupFrameBuffer()
{
    mFrameBuffer = std::make_shared<MultiRenderTarget>();
    // 资源在 Pass 初始化时创建，在 Pass 销毁时销毁
}
```

**问题：**
- 资源在 Pass 初始化时创建，而不是在需要时创建
- 资源在 Pass 销毁时销毁，而不是在所有使用完成后销毁
- 无法实现资源复用和别名

#### 2.3.3 没有编译阶段

**当前实现：**
```cpp
void RenderPassManager::ExecuteAll(const std::vector<RenderCommand>& commands)
{
    if (mDirty)
    {
        SortPassesByDependencies();  // 只有简单的排序
        mDirty = false;
    }
    
    // 直接执行，没有编译阶段
    for (const auto& pass : mPasses)
    {
        pass->Execute(commands);
    }
}
```

**问题：**
- 没有编译阶段来分析资源使用模式
- 无法进行优化（资源别名、Pass 合并等）
- 每次执行都需要重新设置输入

#### 2.3.4 缺少资源别名机制

**当前实现：**
- 每个 Pass 的每个输出都创建独立的资源
- 即使两个资源生命周期不重叠，也无法复用内存

#### 2.3.5 缺少同步机制

**当前实现：**
- 没有显式的同步点
- 依赖 OpenGL 的隐式同步（在某些情况下可能不够）

#### 2.3.6 资源描述不完整

**当前实现：**
```cpp
struct RenderPassOutput
{
    std::string name;
    std::string targetName;
    RenderTargetFormat format;
    bool clearOnStart = true;
};
```

**问题：**
- 缺少资源的使用模式（Read/Write/ReadWrite）
- 缺少资源的生命周期信息
- 缺少资源的尺寸信息（依赖 RenderView，不够灵活）

---

## 3. 与标准 RenderGraph 的对比

| 特性 | 标准 RenderGraph | GTinyEngine 当前实现 | 差距 |
|------|-----------------|---------------------|------|
| 声明式资源管理 | ✅ | ❌ | 需要实现 |
| 资源生命周期管理 | ✅ | ❌ | 需要实现 |
| 资源别名 | ✅ | ❌ | 需要实现 |
| 编译阶段 | ✅ | ❌ | 需要实现 |
| 执行阶段优化 | ✅ | ❌ | 需要实现 |
| 依赖排序 | ✅ | ✅ | 已有基础 |
| 输入/输出声明 | ✅ | ✅ | 已有基础 |
| 同步机制 | ✅ | ❌ | 需要实现 |
| Pass 状态管理 | ✅ | ✅ | 已有基础 |

---

## 4. GTinyEngine RenderGraph 设计方案

### 4.1 架构设计

```
┌─────────────────────────────────────────┐
│         RenderGraph Builder             │  ← 用户接口
│  (声明式 API，构建渲染图)                │
└─────────────────┬───────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────┐
│         RenderGraph Compiler             │  ← 编译阶段
│  (分析资源使用，优化分配)                 │
└─────────────────┬───────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────┐
│         RenderGraph Executor             │  ← 执行阶段
│  (执行渲染，管理资源)                     │
└─────────────────────────────────────────┘
```

### 4.2 核心数据结构

#### 4.2.1 资源描述（Resource Description）

```cpp
enum class ResourceAccess
{
    Read,           // 只读
    Write,          // 只写
    ReadWrite       // 读写
};

enum class ResourceState
{
    Undefined,      // 未定义
    RenderTarget,   // 渲染目标
    ShaderResource, // 着色器资源
    Present         // 呈现
};

struct ResourceDesc
{
    std::string name;
    RenderTargetFormat format;
    uint32_t width;
    uint32_t height;
    ResourceAccess access;
    ResourceState initialState;
    ResourceState finalState;
    bool allowAliasing = false;  // 是否允许别名
};
```

#### 4.2.2 Pass 节点（Pass Node）

```cpp
struct PassNode
{
    std::string name;
    std::shared_ptr<RenderPass> pass;
    
    // 资源使用
    std::vector<ResourceUsage> reads;   // 读取的资源
    std::vector<ResourceUsage> writes;  // 写入的资源
    
    // 依赖关系
    std::vector<std::string> dependencies;
    
    // 执行信息
    std::function<void()> executeFunc;
};
```

#### 4.2.3 资源使用（Resource Usage）

```cpp
struct ResourceUsage
{
    std::string resourceName;
    ResourceAccess access;
    ResourceState requiredState;
    ResourceState outputState;
    size_t passIndex;  // 使用该资源的 Pass 索引
};
```

#### 4.2.4 编译后的图（Compiled Graph）

```cpp
struct CompiledGraph
{
    // 执行顺序
    std::vector<size_t> executionOrder;
    
    // 资源分配
    std::vector<ResourceAllocation> allocations;
    
    // 同步点
    std::vector<SyncPoint> syncPoints;
    
    // 资源别名映射
    std::unordered_map<std::string, std::string> aliases;
};
```

### 4.3 核心类设计

#### 4.3.1 RenderGraphBuilder（构建器）

```cpp
class RenderGraphBuilder
{
public:
    // 添加 Pass
    RenderGraphBuilder& AddPass(const std::string& name, 
                                std::shared_ptr<RenderPass> pass);
    
    // 声明资源使用
    RenderGraphBuilder& Read(const std::string& resourceName);
    RenderGraphBuilder& Write(const std::string& resourceName);
    RenderGraphBuilder& ReadWrite(const std::string& resourceName);
    
    // 设置资源描述
    RenderGraphBuilder& DeclareResource(const ResourceDesc& desc);
    
    // 编译图
    std::unique_ptr<CompiledGraph> Compile();
    
private:
    std::vector<PassNode> mPasses;
    std::unordered_map<std::string, ResourceDesc> mResources;
    std::vector<ResourceUsage> mResourceUsages;
};
```

#### 4.3.2 RenderGraphCompiler（编译器）

```cpp
class RenderGraphCompiler
{
public:
    static std::unique_ptr<CompiledGraph> Compile(
        const std::vector<PassNode>& passes,
        const std::unordered_map<std::string, ResourceDesc>& resources);
    
private:
    // 构建依赖图
    static void BuildDependencyGraph(
        const std::vector<PassNode>& passes,
        std::vector<std::vector<size_t>>& adjacencyList);
    
    // 拓扑排序
    static std::vector<size_t> TopologicalSort(
        const std::vector<std::vector<size_t>>& adjacencyList);
    
    // 分析资源生命周期
    static void AnalyzeResourceLifetimes(
        const std::vector<PassNode>& passes,
        std::unordered_map<std::string, ResourceLifetime>& lifetimes);
    
    // 资源别名分析
    static void AnalyzeResourceAliasing(
        const std::unordered_map<std::string, ResourceLifetime>& lifetimes,
        std::unordered_map<std::string, std::string>& aliases);
    
    // 生成同步点
    static std::vector<SyncPoint> GenerateSyncPoints(
        const std::vector<PassNode>& passes,
        const std::vector<size_t>& executionOrder);
};
```

#### 4.3.3 RenderGraphExecutor（执行器）

```cpp
class RenderGraphExecutor
{
public:
    RenderGraphExecutor(std::unique_ptr<CompiledGraph> compiledGraph);
    
    void Execute(const std::vector<RenderCommand>& commands);
    
private:
    // 资源管理
    void CreateResource(const std::string& name, const ResourceDesc& desc);
    void DestroyResource(const std::string& name);
    GLuint GetResourceHandle(const std::string& name);
    
    // 同步
    void InsertSyncPoint(const SyncPoint& sync);
    
    // 状态转换
    void TransitionResource(const std::string& name, 
                           ResourceState from, ResourceState to);
    
    std::unique_ptr<CompiledGraph> mCompiledGraph;
    std::unordered_map<std::string, std::shared_ptr<RenderTarget>> mResources;
    std::unordered_map<std::string, GLuint> mResourceHandles;
};
```

### 4.4 使用示例

#### 4.4.1 构建渲染图

```cpp
// 创建构建器
RenderGraphBuilder builder;

// 声明资源
builder.DeclareResource({
    "GBuffer_Albedo", 
    RenderTargetFormat::RGBA8, 
    1920, 1080,
    ResourceAccess::Write,
    ResourceState::RenderTarget,
    ResourceState::ShaderResource
});

builder.DeclareResource({
    "GBuffer_Normal",
    RenderTargetFormat::RGB16F,
    1920, 1080,
    ResourceAccess::Write,
    ResourceState::RenderTarget,
    ResourceState::ShaderResource
});

// 添加 Geometry Pass
auto geometryPass = std::make_shared<GeometryPass>();
builder.AddPass("GeometryPass", geometryPass)
    .Write("GBuffer_Albedo")
    .Write("GBuffer_Normal")
    .Write("GBuffer_Depth");

// 添加 Lighting Pass
auto lightingPass = std::make_shared<BasePass>();
builder.AddPass("LightingPass", lightingPass)
    .Read("GBuffer_Albedo")
    .Read("GBuffer_Normal")
    .Read("GBuffer_Depth")
    .Write("FinalColor");

// 编译图
auto compiledGraph = builder.Compile();
```

#### 4.4.2 执行渲染图

```cpp
// 创建执行器
RenderGraphExecutor executor(std::move(compiledGraph));

// 执行
executor.Execute(renderCommands);
```

### 4.5 资源生命周期管理

#### 4.5.1 生命周期分析

```cpp
struct ResourceLifetime
{
    size_t firstUse;   // 首次使用
    size_t lastUse;    // 最后使用
    bool isAliasable;  // 是否可别名
};

void RenderGraphCompiler::AnalyzeResourceLifetimes(
    const std::vector<PassNode>& passes,
    std::unordered_map<std::string, ResourceLifetime>& lifetimes)
{
    for (size_t i = 0; i < passes.size(); ++i)
    {
        const auto& pass = passes[i];
        
        // 分析读取的资源
        for (const auto& read : pass.reads)
        {
            auto& lifetime = lifetimes[read.resourceName];
            if (lifetime.firstUse == SIZE_MAX)
                lifetime.firstUse = i;
            lifetime.lastUse = i;
        }
        
        // 分析写入的资源
        for (const auto& write : pass.writes)
        {
            auto& lifetime = lifetimes[write.resourceName];
            if (lifetime.firstUse == SIZE_MAX)
                lifetime.firstUse = i;
            lifetime.lastUse = i;
        }
    }
}
```

#### 4.5.2 资源别名分析

```cpp
void RenderGraphCompiler::AnalyzeResourceAliasing(
    const std::unordered_map<std::string, ResourceLifetime>& lifetimes,
    std::unordered_map<std::string, std::string>& aliases)
{
    // 按格式和尺寸分组资源
    std::map<std::tuple<RenderTargetFormat, uint32_t, uint32_t>, 
             std::vector<std::string>> resourceGroups;
    
    for (const auto& [name, lifetime] : lifetimes)
    {
        // 根据资源描述获取格式和尺寸
        auto key = std::make_tuple(format, width, height);
        resourceGroups[key].push_back(name);
    }
    
    // 对每组资源，检查是否可以别名
    for (auto& [key, resources] : resourceGroups)
    {
        if (resources.size() < 2) continue;
        
        // 检查生命周期是否重叠
        for (size_t i = 0; i < resources.size(); ++i)
        {
            for (size_t j = i + 1; j < resources.size(); ++j)
            {
                const auto& lifetime1 = lifetimes.at(resources[i]);
                const auto& lifetime2 = lifetimes.at(resources[j]);
                
                // 如果生命周期不重叠，可以别名
                if (lifetime1.lastUse < lifetime2.firstUse || 
                    lifetime2.lastUse < lifetime1.firstUse)
                {
                    aliases[resources[j]] = resources[i];
                }
            }
        }
    }
}
```

### 4.6 同步机制

#### 4.6.1 同步点生成

```cpp
struct SyncPoint
{
    size_t passIndex;
    std::vector<std::string> resources;
    ResourceState fromState;
    ResourceState toState;
};

std::vector<SyncPoint> RenderGraphCompiler::GenerateSyncPoints(
    const std::vector<PassNode>& passes,
    const std::vector<size_t>& executionOrder)
{
    std::vector<SyncPoint> syncPoints;
    
    // 跟踪每个资源的当前状态
    std::unordered_map<std::string, ResourceState> resourceStates;
    
    for (size_t passIdx : executionOrder)
    {
        const auto& pass = passes[passIdx];
        SyncPoint sync;
        sync.passIndex = passIdx;
        
        // 检查读取的资源是否需要状态转换
        for (const auto& read : pass.reads)
        {
            auto currentState = resourceStates[read.resourceName];
            if (currentState != read.requiredState)
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
```

### 4.7 与现有系统的集成

#### 4.7.1 保持向后兼容

```cpp
class RenderPassManager
{
public:
    // 保留原有接口
    bool AddPass(const std::shared_ptr<RenderPass>& pass);
    void ExecuteAll(const std::vector<RenderCommand>& commands);
    
    // 新增 RenderGraph 接口
    void EnableRenderGraph(bool enable) { mUseRenderGraph = enable; }
    RenderGraphBuilder& GetGraphBuilder() { return mGraphBuilder; }
    
private:
    bool mUseRenderGraph = false;
    RenderGraphBuilder mGraphBuilder;
    std::unique_ptr<CompiledGraph> mCompiledGraph;
    std::unique_ptr<RenderGraphExecutor> mExecutor;
};
```

#### 4.7.2 渐进式迁移

1. **第一阶段**：实现 RenderGraph 框架，与现有系统并行运行
2. **第二阶段**：逐步将 Pass 迁移到 RenderGraph
3. **第三阶段**：完全切换到 RenderGraph，移除旧代码

---

## 5. 实现路线图

### 5.1 阶段一：基础框架（1-2周）

**目标**：实现基本的 RenderGraph 框架

**任务**：
1. 实现 `ResourceDesc`、`PassNode`、`ResourceUsage` 等基础数据结构
2. 实现 `RenderGraphBuilder` 类
3. 实现基本的依赖排序（复用现有代码）
4. 实现简单的执行器

**验收标准**：
- 能够构建简单的渲染图
- 能够按依赖顺序执行 Pass
- 与现有系统兼容

### 5.2 阶段二：资源管理（2-3周）

**目标**：实现资源生命周期管理

**任务**：
1. 实现资源生命周期分析
2. 实现资源的自动创建和销毁
3. 实现资源状态跟踪
4. 实现基本的同步机制

**验收标准**：
- 资源在需要时创建，使用完毕后销毁
- 资源状态正确转换
- 无资源泄漏

### 5.3 阶段三：优化（2-3周）

**目标**：实现资源别名和性能优化

**任务**：
1. 实现资源别名分析
2. 实现资源复用
3. 优化执行顺序
4. 性能分析和优化

**验收标准**：
- 资源内存使用减少 20-30%
- 执行性能提升 10-15%

### 5.4 阶段四：高级特性（可选，2-3周）

**目标**：实现高级特性

**任务**：
1. 实现 Pass 合并优化
2. 实现多线程渲染支持
3. 实现动态资源调整
4. 实现渲染图可视化工具

**验收标准**：
- 支持复杂的渲染场景
- 性能进一步提升

---

## 6. 总结

### 6.1 当前状态

GTinyEngine 的 `RenderPassManager` 提供了基本的 Pass 管理功能，但缺少现代 RenderGraph 的核心特性：
- ❌ 声明式资源管理
- ❌ 资源生命周期管理
- ❌ 资源别名
- ❌ 编译阶段优化
- ❌ 同步机制

### 6.2 改进方向

通过引入 RenderGraph 系统，可以实现：
- ✅ 更清晰的渲染流程描述
- ✅ 自动资源管理
- ✅ 内存优化（资源别名）
- ✅ 性能优化（编译时优化）
- ✅ 更好的可维护性

### 6.3 实施建议

1. **渐进式迁移**：保持向后兼容，逐步迁移
2. **充分测试**：每个阶段都要充分测试
3. **性能监控**：监控内存使用和性能指标
4. **文档完善**：及时更新文档和示例

---

## 7. 参考资料

1. **Frostbite Rendering Architecture** - DICE/EA
2. **Unreal Engine RenderGraph** - Epic Games
3. **Unity RenderGraph** - Unity Technologies
4. **Modern Graphics Rendering Techniques** - Various GDC Talks
5. **Vulkan Render Passes** - Khronos Group

---

*文档创建日期：2024年*
*最后更新日期：2024年*
