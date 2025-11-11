# GTinyEngine 着色器预处理器使用指南

## 概述

GTinyEngine 的着色器预处理器是一个强大的工具，用于在编译着色器之前处理GLSL源代码。它支持头文件包含、宏定义、条件编译等高级功能，让着色器代码更加模块化和可维护。

## 主要特性

- ✅ **头文件包含** - 支持 `#include` 指令
- ✅ **宏定义** - 支持简单宏和函数宏
- ✅ **条件编译** - 支持 `#ifdef`、`#ifndef` 等
- ✅ **循环包含检测** - 防止无限递归包含
- ✅ **调试支持** - 可选的调试输出
- ✅ **配置灵活** - 可自定义处理选项

## 快速开始

### 1. 基本使用

```cpp
#include "shader.h"
#include "shader/ShaderPreprocessor.h"

// 使用默认配置创建着色器
Shader shader("vertex.glsl", "fragment.glsl");

// 检查着色器是否创建成功
if (shader.IsValid()) {
    shader.use();
    // 设置uniform变量...
} else {
    std::cout << "着色器创建失败!" << std::endl;
}
```

### 2. 使用自定义配置

```cpp
#include "shader/ShaderPreprocessor.h"

// 创建自定义配置
te::ShaderPreprocessorConfig config;
config.shaderDirectory = "resources/shaders/";
config.includeDirectory = "resources/shaders/includes/";
config.enableMacroExpansion = true;
config.enableIncludeProcessing = true;
config.enableLineDirectives = false; // 建议保持false，避免OpenGL兼容性问题

// 使用配置创建着色器
Shader shader("vertex.glsl", "fragment.glsl", config);
```

### 3. 使用 ShaderBuilder（推荐）

```cpp
#include "shader/ShaderPreprocessor.h"

// 使用 ShaderBuilder 进行链式操作
auto shader = te::ShaderBuilder()
    .Define("MAX_LIGHTS", "4")
    .Define("USE_NORMAL_MAPPING", "1")
    .Define("PI", "3.14159265359")
    .BuildShader("vertex.glsl", "fragment.glsl");

if (shader && shader->IsValid()) {
    shader->use();
    // 使用着色器...
}
```

## 头文件包含

### 1. 创建头文件

在 `resources/shaders/includes/` 目录下创建头文件：

**math_common.glsl**
```glsl
// 数学常量和函数
#define PI 3.14159265359
#define TWO_PI 6.28318530718
#define HALF_PI 1.57079632679

// 色调映射函数
vec3 ReinhardToneMapping(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 ACESToneMapping(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// 颜色空间转换
vec3 LinearToSRGB(vec3 linear) {
    return mix(1.055 * pow(linear, vec3(1.0/2.4)) - 0.055, 
               12.92 * linear, 
               step(linear, vec3(0.0031308)));
}
```

**lighting_common.glsl**
```glsl
// 光照计算函数
vec3 CalculateAmbient(vec3 lightColor, float strength) {
    return strength * lightColor;
}

vec3 CalculateDiffuse(vec3 lightDir, vec3 normal, vec3 lightColor, float strength) {
    float diff = max(dot(normal, lightDir), 0.0);
    return strength * diff * lightColor;
}

vec3 CalculateSpecular(vec3 viewDir, vec3 lightDir, vec3 normal, vec3 lightColor, float strength, float shininess) {
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    return strength * spec * lightColor;
}
```

### 2. 在着色器中使用头文件

**fragment.glsl**
```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

// 包含头文件
#include <includes/math_common.glsl>
#include <includes/lighting_common.glsl>

uniform sampler2D u_albedoMap;
uniform float u_exposure;

void main()
{
    vec3 color = texture(u_albedoMap, TexCoords).rgb;
    color *= u_exposure;
    
    // 使用头文件中定义的函数
    vec3 mapped = ReinhardToneMapping(color);
    mapped = LinearToSRGB(mapped);
    
    FragColor = vec4(mapped, 1.0);
}
```

## 宏定义

### 1. 简单宏

```cpp
// 使用 ShaderBuilder 定义宏
auto shader = te::ShaderBuilder()
    .Define("MAX_LIGHTS", "4")
    .Define("USE_SHADOWS", "1")
    .Define("PI", "3.14159265359")
    .BuildShader("vertex.glsl", "fragment.glsl");
```

在着色器中使用：
```glsl
#version 330 core

// 这些宏会被预处理器展开
uniform vec3 u_lightPositions[MAX_LIGHTS];
uniform vec3 u_lightColors[MAX_LIGHTS];

void main()
{
    vec3 result = vec3(0.0);
    
    #ifdef USE_SHADOWS
    // 阴影计算代码
    #endif
    
    for (int i = 0; i < MAX_LIGHTS; i++) {
        // 光照计算...
    }
}
```

### 2. 函数宏

```cpp
// 定义函数宏
auto shader = te::ShaderBuilder()
    .Define("LERP", "mix($1, $2, $3)", true)  // 参数化宏
    .Define("SQUARE", "($1) * ($1)", true)    // 平方函数
    .BuildShader("vertex.glsl", "fragment.glsl");
```

在着色器中使用：
```glsl
#version 330 core

void main()
{
    float a = 0.5;
    float b = 1.0;
    float t = 0.3;
    
    // 这些会被展开为：
    // float result1 = mix(a, b, t);
    // float result2 = (a) * (a);
    float result1 = LERP(a, b, t);
    float result2 = SQUARE(a);
}
```

## 高级用法

### 1. 条件编译

```glsl
#version 330 core

// 根据宏定义选择不同的实现
#ifdef USE_PHONG_LIGHTING
    vec3 CalculateLighting(vec3 normal, vec3 lightDir, vec3 viewDir) {
        // Phong光照模型
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        return spec;
    }
#else
    vec3 CalculateLighting(vec3 normal, vec3 lightDir, vec3 viewDir) {
        // Blinn-Phong光照模型
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
        return spec;
    }
#endif
```

### 2. 动态宏管理

```cpp
// 创建预处理器实例
te::ShaderPreprocessor preprocessor;

// 动态添加宏
preprocessor.DefineMacro("FEATURE_A", "1");
preprocessor.DefineMacro("FEATURE_B", "0");

// 检查宏是否定义
if (preprocessor.IsMacroDefined("FEATURE_A")) {
    std::cout << "Feature A is enabled" << std::endl;
}

// 取消定义宏
preprocessor.UndefineMacro("FEATURE_B");

// 清除所有宏
preprocessor.ClearMacros();
```

### 3. 调试预处理结果

```cpp
// 启用调试输出
#ifdef SHADER_PREPROCESSOR_DEBUG
// 在编译时定义此宏以启用调试输出
#endif

// 或者手动获取处理后的内容
te::ShaderPreprocessor preprocessor;
std::string processedContent = preprocessor.ProcessShader("fragment.glsl");
std::cout << "Processed shader content:" << std::endl;
std::cout << processedContent << std::endl;
```

## 配置选项

### ShaderPreprocessorConfig 结构

```cpp
struct ShaderPreprocessorConfig
{
    std::string shaderDirectory = "resources/shaders/";        // 着色器根目录
    std::string includeDirectory = "resources/shaders/includes/"; // 头文件目录
    bool enableMacroExpansion = true;                          // 启用宏展开
    bool enableIncludeProcessing = true;                       // 启用头文件包含处理
    bool enableLineDirectives = false;                         // 启用行指令（建议保持false）
};
```

### 自定义配置示例

```cpp
te::ShaderPreprocessorConfig config;
config.shaderDirectory = "my_shaders/";
config.includeDirectory = "my_shaders/headers/";
config.enableMacroExpansion = true;
config.enableIncludeProcessing = true;
config.enableLineDirectives = false; // 避免OpenGL兼容性问题

Shader shader("vertex.glsl", "fragment.glsl", config);
```

## 最佳实践

### 1. 目录结构

```
resources/shaders/
├── includes/           # 头文件目录
│   ├── math_common.glsl
│   ├── lighting_common.glsl
│   └── postprocess_common.glsl
├── vertex/            # 顶点着色器
│   ├── basic.vs
│   └── skinned.vs
├── fragment/          # 片段着色器
│   ├── phong.fs
│   ├── pbr.fs
│   └── postprocess/
│       ├── tone_mapping.fs
│       └── edge_detection.fs
└── geometry/          # 几何着色器
    └── normal_visualization.gs
```

### 2. 头文件组织

- **按功能分类**：将相关函数放在同一个头文件中
- **避免循环依赖**：确保头文件之间没有循环包含
- **使用命名空间**：在头文件中使用有意义的函数名
- **添加注释**：为每个函数添加详细的注释

### 3. 宏定义规范

```cpp
// 使用大写字母和下划线
.Define("MAX_LIGHTS", "8")
.Define("USE_SHADOWS", "1")
.Define("ENABLE_DEBUG", "0")

// 函数宏使用描述性名称
.Define("LERP", "mix($1, $2, $3)", true)
.Define("CLAMP", "clamp($1, $2, $3)", true)
```

### 4. 错误处理

```cpp
// 总是检查着色器创建是否成功
auto shader = te::ShaderBuilder()
    .Define("MAX_LIGHTS", "4")
    .BuildShader("vertex.glsl", "fragment.glsl");

if (!shader || !shader->IsValid()) {
    std::cerr << "Failed to create shader!" << std::endl;
    return false;
}

// 使用着色器
shader->use();
```

## 常见问题

### 1. 循环包含问题

**问题**：头文件A包含头文件B，头文件B又包含头文件A

**解决方案**：
- 重新设计头文件结构，避免循环依赖
- 将共同依赖的代码提取到第三个头文件中

### 2. 宏展开问题

**问题**：宏没有按预期展开

**解决方案**：
- 检查宏定义语法是否正确
- 确保宏名称没有拼写错误
- 使用调试输出查看展开结果

### 3. 头文件找不到

**问题**：`#include` 指令找不到文件

**解决方案**：
- 检查文件路径是否正确
- 确认 `includeDirectory` 配置是否正确
- 使用相对路径或绝对路径

### 4. OpenGL兼容性问题

**问题**：预处理后的着色器编译失败

**解决方案**：
- 禁用 `enableLineDirectives`（设置为false）
- 检查宏展开是否产生了无效的GLSL代码
- 使用调试输出检查预处理结果

## 示例项目

查看 `Examples/ShaderPreprocessorDemo/` 目录中的完整示例，了解如何在实际项目中使用着色器预处理器。

## API 参考

### ShaderPreprocessor 类

```cpp
class ShaderPreprocessor {
public:
    // 构造函数
    ShaderPreprocessor();
    explicit ShaderPreprocessor(const ShaderPreprocessorConfig& config);
    
    // 处理着色器
    std::string ProcessShader(const std::string& shaderPath);
    std::string ProcessShaderContent(const std::string& content, const std::string& basePath = "");
    
    // 宏管理
    void DefineMacro(const std::string& name, const std::string& value = "", bool isFunction = false);
    void UndefineMacro(const std::string& name);
    void ClearMacros();
    bool IsMacroDefined(const std::string& name) const;
    
    // 配置管理
    void SetConfig(const ShaderPreprocessorConfig& config);
    const ShaderPreprocessorConfig& GetConfig() const;
};
```

### ShaderBuilder 类

```cpp
class ShaderBuilder {
public:
    // 构造函数
    ShaderBuilder();
    explicit ShaderBuilder(const ShaderPreprocessorConfig& config);
    
    // 构建着色器
    std::shared_ptr<Shader> BuildShader(const std::string& vertexPath, const std::string& fragmentPath);
    
    // 链式操作
    ShaderBuilder& Define(const std::string& name, const std::string& value = "", bool isFunction = false);
    ShaderBuilder& Undefine(const std::string& name);
    ShaderBuilder& SetConfig(const ShaderPreprocessorConfig& config);
};
```

---

*本指南涵盖了GTinyEngine着色器预处理器的主要功能和使用方法。如有问题，请参考示例代码或联系开发团队。*
