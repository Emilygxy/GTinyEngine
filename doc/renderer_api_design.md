# GTinyEngine 统一渲染 API 设计

## 概述

GTinyEngine 实现了一个统一的渲染 API 抽象，支持多种渲染后端（OpenGL、OpenGLES、Vulkan），并提供了简洁的 `DrawMesh` 接口。

## 核心组件

### 1. IRenderer 接口

统一的渲染器接口，定义了渲染器的核心功能：

```cpp
class IRenderer
{
public:
    virtual ~IRenderer() = default;
    
    // 初始化渲染器
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    
    // 帧开始/结束
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    
    // 核心渲染接口
    virtual void DrawMesh(const RenderCommand& command) = 0;
    virtual void DrawMesh(const std::vector<Vertex>& vertices, 
                         const std::vector<unsigned int>& indices,
                         const std::shared_ptr<MaterialBase>& material,
                         const glm::mat4& transform = glm::mat4(1.0f)) = 0;
    
    // 批量渲染
    virtual void DrawMeshes(const std::vector<RenderCommand>& commands) = 0;
    
    // 设置渲染状态
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void SetClearColor(float r, float g, float b, float a) = 0;
    virtual void Clear(uint32_t flags) = 0;
    
    // 获取渲染统计
    virtual const RenderStats& GetRenderStats() const = 0;
    virtual void ResetRenderStats() = 0;
    
    // 设置全局渲染参数
    virtual void SetCamera(const std::shared_ptr<Camera>& camera) = 0;
    virtual void SetLight(const std::shared_ptr<Light>& light) = 0;
};
```

### 2. RenderCommand 结构

封装渲染命令的数据结构：

```cpp
struct RenderCommand
{
    std::shared_ptr<MaterialBase> material;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::mat4 transform;
    RenderState state;
    bool hasUV;
};
```

### 3. RenderState 枚举

定义不同的渲染状态：

```cpp
enum class RenderState
{
    Opaque,      // 不透明
    Transparent, // 透明
    Wireframe,   // 线框
    Points,      // 点
    Lines        // 线
};
```

## 使用方法

### 1. 创建渲染器

```cpp
// 使用工厂模式创建渲染器
auto renderer = RendererFactory::CreateRenderer(RendererBackend::OpenGL);
renderer->Initialize();

// 设置全局参数
renderer->SetCamera(camera);
renderer->SetLight(light);
```

### 2. 渲染循环

```cpp
while (!window.ShouldClose())
{
    // 开始渲染帧
    renderer->BeginFrame();
    
    // 设置视口和清除颜色
    renderer->SetViewport(0, 0, width, height);
    renderer->SetClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    renderer->Clear(0x3); // 清除颜色和深度缓冲
    
    // 渲染网格
    renderer->DrawMesh(vertices, indices, material, transform);
    
    // 结束渲染帧
    renderer->EndFrame();
}
```

### 3. 批量渲染

```cpp
std::vector<RenderCommand> commands;
// 添加渲染命令...

renderer->DrawMeshes(commands);
```

## BlinnPhongMaterial 实现

### 1. 类定义

```cpp
class BlinnPhongMaterial : public PhongMaterial
{
public:
    BlinnPhongMaterial(const std::string& vs_path, const std::string& fs_path);
    
    // 设置是否使用 Blinn-Phong 算法
    void SetUseBlinnPhong(bool useBlinnPhong);
    bool GetUseBlinnPhong() const;
    
    // 重写 UpdateUniform 方法
    void UpdateUniform() override;

private:
    bool mUseBlinnPhong = true;
};
```

### 2. Shader 实现

在 fragment shader 中通过 uniform 切换算法：

```glsl
uniform float u_useBlinnPhong;

bool isBlinnPhong()
{
    return u_useBlinnPhong > 0.5;
}

vec3 CalculatePhongColor()
{
    // ... 其他计算 ...
    
    float spec = 0.0;
    if(isBlinnPhong())
    {
        // Blinn-Phong 算法
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    }
    else
    {
        // Phong 算法
        vec3 reflectDir = reflect(-lightDir, norm);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    }
    
    // ... 其他计算 ...
}
```

### 3. 使用示例

```cpp
// 创建材质
auto material = std::make_shared<BlinnPhongMaterial>();
material->AttachedCamera(camera);
material->AttachedLight(light);

// 切换算法
material->SetUseBlinnPhong(true);  // 使用 Blinn-Phong
material->SetUseBlinnPhong(false); // 使用 Phong

// 渲染
renderer->DrawMesh(vertices, indices, material, transform);
```

## 架构优势

### 1. 统一接口
- 屏蔽不同渲染后端的差异
- 提供一致的 API 接口
- 便于切换渲染后端

### 2. 可扩展性
- 支持添加新的渲染后端
- 支持新的材质类型
- 支持新的渲染状态

### 3. 性能优化
- 网格数据缓存
- 批量渲染支持
- 渲染统计信息

### 4. 易用性
- 简洁的 API 设计
- 自动资源管理
- 错误处理机制

## 扩展计划

### 1. 渲染后端
- [ ] OpenGLES 渲染器实现
- [ ] Vulkan 渲染器实现
- [ ] DirectX 渲染器实现

### 2. 渲染特性
- [ ] 延迟渲染
- [ ] 前向渲染
- [ ] 后处理效果
- [ ] 阴影映射

### 3. 材质系统
- [ ] PBR 材质
- [ ] 自定义材质
- [ ] 材质编辑器

### 4. 性能优化
- [ ] 实例化渲染
- [ ] 遮挡剔除
- [ ] LOD 系统 