# RenderGraph 使用示例

本文档展示如何在 GTinyEngine 中使用 RenderGraph 系统。

## 基本使用

### 1. 启用 RenderGraph

```cpp
#include "framework/RenderPassManager.h"

auto& manager = te::RenderPassManager::GetInstance();

// 启用 RenderGraph
manager.EnableRenderGraph(true);
```

### 2. 构建渲染图

#### 方式一：使用 RenderGraphBuilder（推荐）

```cpp
auto& builder = manager.GetGraphBuilder();

// 声明资源
builder.DeclareResource({
    "GBuffer_Albedo",
    te::RenderTargetFormat::RGBA8,
    1920, 1080,
    te::ResourceAccess::Write,
    te::ResourceState::RenderTarget,
    te::ResourceState::ShaderResource,
    true  // allowAliasing
});

builder.DeclareResource({
    "GBuffer_Normal",
    te::RenderTargetFormat::RGB16F,
    1920, 1080,
    te::ResourceAccess::Write,
    te::ResourceState::RenderTarget,
    te::ResourceState::ShaderResource,
    true
});

builder.DeclareResource({
    "GBuffer_Depth",
    te::RenderTargetFormat::Depth24,
    1920, 1080,
    te::ResourceAccess::Write,
    te::ResourceState::RenderTarget,
    te::ResourceState::ShaderResource,
    true
});

builder.DeclareResource({
    "FinalColor",
    te::RenderTargetFormat::RGBA8,
    1920, 1080,
    te::ResourceAccess::Write,
    te::ResourceState::RenderTarget,
    te::ResourceState::Present,
    true
});

// 添加 Geometry Pass
auto geometryPass = std::make_shared<te::GeometryPass>();
builder.AddPass("GeometryPass", geometryPass)
    .Write("GBuffer_Albedo")
    .Write("GBuffer_Normal")
    .Write("GBuffer_Depth");

// 添加 Lighting Pass
auto lightingPass = std::make_shared<te::BasePass>();
builder.AddPass("LightingPass", lightingPass)
    .Read("GBuffer_Albedo")
    .Read("GBuffer_Normal")
    .Read("GBuffer_Depth")
    .Write("FinalColor");

// 编译图
if (!manager.CompileRenderGraph())
{
    std::cout << "Failed to compile RenderGraph" << std::endl;
    return;
}
```

#### 方式二：自动从现有 Pass 构建

```cpp
// 先添加 Pass（使用传统方式）
auto geometryPass = std::make_shared<te::GeometryPass>();
manager.AddPass(geometryPass);

auto lightingPass = std::make_shared<te::BasePass>();
manager.AddPass(lightingPass);

// 启用 RenderGraph 并编译
manager.EnableRenderGraph(true);
if (!manager.CompileRenderGraph())
{
    std::cout << "Failed to compile RenderGraph" << std::endl;
    return;
}
```

### 3. 执行渲染

```cpp
// 执行渲染（会自动使用 RenderGraph）
std::vector<te::RenderCommand> commands;
// ... 填充 commands ...

manager.ExecuteAll(commands);
```

## 高级用法

### 资源别名

RenderGraph 会自动分析资源的生命周期，将不重叠的资源映射到同一块内存：

```cpp
// 声明两个生命周期不重叠的资源
builder.DeclareResource({
    "TempBuffer1",
    te::RenderTargetFormat::RGBA8,
    1920, 1080,
    te::ResourceAccess::Write,
    te::ResourceState::RenderTarget,
    te::ResourceState::ShaderResource,
    true  // allowAliasing = true
});

builder.DeclareResource({
    "TempBuffer2",
    te::RenderTargetFormat::RGBA8,
    1920, 1080,
    te::ResourceAccess::Write,
    te::ResourceState::RenderTarget,
    te::ResourceState::ShaderResource,
    true  // allowAliasing = true
});

// Pass1 使用 TempBuffer1
builder.AddPass("Pass1", pass1)
    .Write("TempBuffer1");

// Pass2 使用 TempBuffer2（生命周期不重叠）
builder.AddPass("Pass2", pass2)
    .Write("TempBuffer2");

// RenderGraph 会自动将 TempBuffer1 和 TempBuffer2 别名到同一块内存
```

### 手动控制资源使用

```cpp
// 手动声明资源使用
builder.AddPass("CustomPass", customPass)
    .Read("InputTexture")
    .Write("OutputTexture")
    .ReadWrite("ReadWriteBuffer");
```

### 获取资源句柄

```cpp
auto& executor = manager.GetGraphExecutor();  // 注意：需要添加这个接口
GLuint textureHandle = executor->GetResourceHandle("GBuffer_Albedo");
```

## 与传统方式的对比

### 传统方式

```cpp
// 需要手动管理资源
auto gBuffer = CreateFrameBuffer(1920, 1080);
// ... 渲染到 gBuffer ...
auto texture = gBuffer->GetTexture();
// ... 使用 texture ...
DestroyFrameBuffer(gBuffer);
```

### RenderGraph 方式

```cpp
// 声明式，自动管理
builder.DeclareResource({...});
builder.AddPass("Pass", pass).Write("Resource");
// RenderGraph 自动管理资源的创建和销毁
```

## 注意事项

1. **资源尺寸**：目前资源尺寸需要手动指定，未来版本可能会从 RenderView 自动获取
2. **向后兼容**：默认使用传统方式，需要显式启用 RenderGraph
3. **资源别名**：只有 `allowAliasing = true` 的资源才会被考虑别名
4. **编译时机**：在第一次执行前需要调用 `CompileRenderGraph()`

## 迁移指南

### 从传统方式迁移到 RenderGraph

1. **保持现有代码不变**：传统接口仍然可用
2. **逐步迁移**：
   ```cpp
   // 第一步：启用 RenderGraph
   manager.EnableRenderGraph(true);
   
   // 第二步：编译（会自动从现有 Pass 构建）
   manager.CompileRenderGraph();
   
   // 第三步：执行（会自动使用 RenderGraph）
   manager.ExecuteAll(commands);
   ```
3. **优化**：逐步使用声明式 API 来获得更好的优化效果

## 性能优化建议

1. **启用资源别名**：对于临时资源，设置 `allowAliasing = true`
2. **合理设置资源格式**：使用合适的格式可以减少内存使用
3. **避免不必要的资源**：只声明实际使用的资源
4. **批量编译**：在渲染循环外编译 RenderGraph

---

*更多详细信息请参考 `doc/rendergraph_analysis.md`*
