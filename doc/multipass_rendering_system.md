# GTinyEngine 多Pass渲染系统

## 概述

GTinyEngine的多Pass渲染系统提供了一个灵活、可扩展的渲染管线架构，支持延迟渲染、后处理、阴影映射等高级渲染技术。该系统基于Pass的概念，每个Pass负责渲染管线中的一个特定阶段。

## 系统架构

### 核心组件

```
多Pass渲染系统
├── FrameBuffer系统
│   ├── RenderTarget (单个渲染目标)
│   ├── MultiRenderTarget (多重渲染目标)
│   └── FrameBufferManager (FrameBuffer管理器)
├── RenderPass系统
│   ├── RenderPass (Pass基类)
│   ├── GeometryPass (几何Pass)
│   ├── LightingPass (光照Pass)
│   ├── PostProcessPass (后处理Pass)
│   └── SkyboxPass (天空盒Pass)
└── 扩展的IRenderer接口
    ├── 多Pass管理
    ├── Pass执行
    └── 依赖排序
```

### 渲染管线流程图

```
场景数据
    ↓
┌─────────────────┐
│   RenderAgent   │
│  (收集渲染命令)  │
└─────────────────┘
    ↓
┌─────────────────┐
│   IRenderer     │
│ (多Pass管理器)   │
└─────────────────┘
    ↓
┌─────────────────┐
│  Pass依赖排序   │
└─────────────────┘
    ↓
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  GeometryPass   │───▶│  LightingPass   │───▶│ PostProcessPass │
│   (生成GBuffer) │    │   (光照计算)     │    │   (后处理效果)   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
    ↓                        ↓                        ↓
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Albedo RT     │    │   Lighting RT   │    │   Final RT      │
│   Normal RT     │    │                 │    │                 │
│  Position RT    │    │                 │    │                 │
│   Depth RT      │    │                 │    │                 │
└─────────────────┘    └─────────────────┘    └─────────────────┘
    ↓                        ↓                        ↓
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  FrameBuffer    │    │  FrameBuffer    │    │  FrameBuffer    │
│   (MRT)         │    │   (单目标)       │    │   (单目标)       │
└─────────────────┘    └─────────────────┘    └─────────────────┘
    ↓                        ↓                        ↓
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  纹理输入到      │    │  纹理输入到      │    │  最终输出到      │
│  LightingPass   │    │ PostProcessPass │    │    屏幕         │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### 数据流图

```
输入数据流:
场景几何体 → GeometryPass → GBuffer纹理
光源信息 → LightingPass → 光照结果纹理
后处理参数 → PostProcessPass → 最终图像

Pass依赖关系:
GeometryPass (无依赖)
    ↓
LightingPass (依赖: GeometryPass)
    ↓
PostProcessPass (依赖: LightingPass)
    ↓
SkyboxPass (无依赖，可并行执行)
```

## FrameBuffer系统

### RenderTarget

单个渲染目标，支持多种格式和类型：

```cpp
struct RenderTargetDesc
{
    std::string name;                    // 目标名称
    RenderTargetType type;               // 目标类型 (Color, Depth, Stencil等)
    RenderTargetFormat format;           // 数据格式 (RGB8, RGBA16F等)
    uint32_t width, height;              // 尺寸
    bool generateMipmaps;                // 是否生成Mipmap
    GLenum wrapMode, filterMode;         // 包装和过滤模式
};
```

### MultiRenderTarget

多重渲染目标（MRT），支持同时渲染到多个缓冲区：

```cpp
auto frameBuffer = std::make_shared<MultiRenderTarget>();
frameBuffer->Initialize(800, 600);

// 添加渲染目标
frameBuffer->AddRenderTarget(RenderTargetDesc{
    "Albedo", RenderTargetType::Color, RenderTargetFormat::RGBA8, 800, 600
});
frameBuffer->AddRenderTarget(RenderTargetDesc{
    "Normal", RenderTargetType::Color, RenderTargetFormat::RGB16F, 800, 600
});
```

## RenderPass系统

### RenderPass基类

所有Pass的基类，提供通用的Pass管理功能：

```cpp
class RenderPass
{
public:
    virtual void Execute(const std::vector<RenderCommand>& commands) = 0;
    
    // 配置管理
    const RenderPassConfig& GetConfig() const;
    void SetState(RenderPassState state);
    
    // 输入输出管理
    void SetInput(const std::string& name, GLuint textureHandle);
    std::shared_ptr<RenderTarget> GetOutput(const std::string& name) const;
    
    // 依赖检查
    bool CheckDependencies(const std::vector<std::shared_ptr<RenderPass>>& allPasses) const;
};
```

### RenderPassConfig

Pass配置结构，定义Pass的行为和依赖关系：

```cpp
struct RenderPassConfig
{
    std::string name;                    // Pass名称
    RenderPassType type;                 // Pass类型
    RenderPassState state;               // 状态 (Enabled/Disabled/Conditional)
    
    std::vector<RenderPassInput> inputs;     // 输入列表
    std::vector<RenderPassOutput> outputs;   // 输出列表
    std::vector<RenderPassDependency> dependencies; // 依赖列表
    
    // 渲染设置
    bool clearColor, clearDepth, clearStencil;
    glm::vec4 clearColorValue;
    bool enableDepthTest, enableBlend;
    // ... 更多设置
};
```

## 预定义Pass类型

### 1. GeometryPass (几何Pass)

负责生成GBuffer，用于延迟渲染：

```cpp
auto geometryPass = std::make_shared<te::GeometryPass>();
geometryPass->Initialize(RenderPassConfig{
    "GeometryPass",
    RenderPassType::Geometry,
    RenderPassState::Enabled,
    {}, // 无输入
    {
        {"Albedo", "albedo", RenderTargetFormat::RGBA8},
        {"Normal", "normal", RenderTargetFormat::RGB16F},
        {"Position", "position", RenderTargetFormat::RGB16F},
        {"Depth", "depth", RenderTargetFormat::Depth24}
    }
});
```

**输出缓冲区：**
- Albedo: 漫反射颜色
- Normal: 世界空间法线
- Position: 世界空间位置
- Depth: 深度信息

### 2. LightingPass (光照Pass)

基于GBuffer进行光照计算：

```cpp
auto lightingPass = std::make_shared<te::LightingPass>();
lightingPass->Initialize(RenderPassConfig{
    "LightingPass",
    RenderPassType::Lighting,
    RenderPassState::Enabled,
    {
        {"Albedo", "GeometryPass", "albedo", 0, true},
        {"Normal", "GeometryPass", "normal", 0, true},
        {"Position", "GeometryPass", "position", 0, true}
    },
    {
        {"Lighting", "lighting", RenderTargetFormat::RGBA8}
    },
    {
        {"GeometryPass", true, []() { return true; }}
    }
});
```

### 3. PostProcessPass (后处理Pass)

应用后处理效果：

```cpp
auto postProcessPass = std::make_shared<te::PostProcessPass>();
postProcessPass->Initialize(RenderPassConfig{
    "PostProcessPass",
    RenderPassType::PostProcess,
    RenderPassState::Enabled,
    {
        {"Color", "LightingPass", "lighting", 0, true}
    },
    {
        {"Final", "final", RenderTargetFormat::RGBA8}
    }
});

// 添加后处理效果
postProcessPass->AddEffect("Bloom", bloomMaterial);
postProcessPass->AddEffect("ToneMapping", toneMappingMaterial);
```

### 4. SkyboxPass (天空盒Pass)

渲染天空盒：

```cpp
auto skyboxPass = std::make_shared<te::SkyboxPass>();
skyboxPass->Initialize(RenderPassConfig{
    "SkyboxPass",
    RenderPassType::Skybox,
    RenderPassState::Enabled
});
skyboxPass->SetSkybox(skybox);
```

## 使用示例

### 基本设置

```cpp
// 创建渲染器
auto renderer = RendererFactory::CreateRenderer(RendererBackend::OpenGL);
renderer->Initialize();

// 启用多Pass渲染
renderer->SetMultiPassEnabled(true);

// 添加Pass
renderer->AddRenderPass(geometryPass);
renderer->AddRenderPass(lightingPass);
renderer->AddRenderPass(postProcessPass);
```

### 渲染循环

```cpp
while (!glfwWindowShouldClose(window))
{
    // 开始帧
    renderer->BeginFrame();
    
    // 设置视口和清除颜色
    renderer->SetViewport(0, 0, width, height);
    renderer->SetClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    renderer->Clear(0x3);
    
    // 执行多Pass渲染
    if (renderer->IsMultiPassEnabled())
    {
        renderer->ExecuteRenderPasses();
    }
    else
    {
        // 传统单Pass渲染
        renderer->DrawMesh(vertices, indices, material, transform);
    }
    
    // 结束帧
    renderer->EndFrame();
    
    glfwSwapBuffers(window);
    glfwPollEvents();
}
```

## 渲染管线示例

### 延迟渲染管线

```
1. GeometryPass
   ├── 输入: 场景几何体
   ├── 输出: GBuffer (Albedo, Normal, Position, Depth)
   └── 目的: 生成几何信息

2. LightingPass
   ├── 输入: GBuffer + 光源信息
   ├── 输出: 光照结果
   └── 目的: 计算光照

3. PostProcessPass
   ├── 输入: 光照结果
   ├── 输出: 最终图像
   └── 目的: 后处理效果

4. SkyboxPass
   ├── 输入: 相机信息
   ├── 输出: 天空盒
   └── 目的: 渲染背景
```

### 前向渲染管线

```
1. SkyboxPass
   ├── 输入: 相机信息
   ├── 输出: 天空盒
   └── 目的: 渲染背景

2. ForwardPass (自定义Pass)
   ├── 输入: 场景几何体 + 光源
   ├── 输出: 最终图像
   └── 目的: 前向渲染
```

## 扩展性

### 创建自定义Pass

```cpp
class CustomPass : public RenderPass
{
public:
    void Execute(const std::vector<RenderCommand>& commands) override
    {
        if (!IsEnabled()) return;
        
        OnPreExecute();
        
        // 绑定FrameBuffer
        if (mFrameBuffer)
            mFrameBuffer->Bind();
        
        // 应用渲染设置
        ApplyRenderSettings();
        
        // 自定义渲染逻辑
        // ...
        
        // 解绑FrameBuffer
        if (mFrameBuffer)
            mFrameBuffer->Unbind();
        
        // 恢复渲染设置
        RestoreRenderSettings();
        
        OnPostExecute();
    }
    
protected:
    void OnInitialize() override
    {
        // 自定义初始化逻辑
    }
};
```

### 动态Pass管理

```cpp
// 运行时添加/移除Pass
renderer->AddRenderPass(customPass);
renderer->RemoveRenderPass("OldPass");

// 启用/禁用Pass
auto pass = renderer->GetRenderPass("LightingPass");
if (pass)
    pass->SetState(RenderPassState::Disabled);
```

## 性能优化

### 1. Pass排序

系统自动按依赖关系排序Pass，确保正确的执行顺序：

```cpp
// 依赖关系示例
LightingPass -> 依赖 -> GeometryPass
PostProcessPass -> 依赖 -> LightingPass
```

### 2. 条件执行

Pass可以设置条件执行：

```cpp
RenderPassDependency{
    "GeometryPass", 
    true, 
    []() { return enableDeferredRendering; }
}
```

### 3. 资源复用

FrameBuffer和RenderTarget可以被多个Pass共享，减少内存使用。

## 调试和监控

### Pass状态监控

```cpp
// 检查Pass是否启用
bool isEnabled = renderer->GetRenderPass("LightingPass")->IsEnabled();

// 获取渲染统计
const auto& stats = renderer->GetRenderStats();
std::cout << "Draw Calls: " << stats.drawCalls << std::endl;
```

### FrameBuffer调试

```cpp
// 检查FrameBuffer完整性
auto frameBuffer = renderer->GetRenderPass("GeometryPass")->GetFrameBuffer();
if (frameBuffer && frameBuffer->IsComplete())
{
    std::cout << "FrameBuffer is complete" << std::endl;
}
```

## 总结

GTinyEngine的多Pass渲染系统提供了：

1. **灵活性**: 支持自定义Pass和渲染管线
2. **可扩展性**: 易于添加新的Pass类型和效果
3. **性能**: 优化的Pass排序和资源管理
4. **易用性**: 简洁的API和清晰的架构
5. **兼容性**: 与现有单Pass渲染系统兼容

该系统为现代渲染技术（如延迟渲染、PBR、后处理等）提供了坚实的基础，同时保持了良好的性能和可维护性。
