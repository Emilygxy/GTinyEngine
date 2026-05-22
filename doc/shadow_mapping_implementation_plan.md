# GTinyEngine 阴影映射（Shadow Mapping）实现方案

> 参考：[LearnOpenGL - Shadow Mapping](https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping)  
> 目标：在现有 OpenGL 多 Pass 管线（TinyRenderer / `RenderPassManager`）中落地方向光阴影，并以 `Sandbox_ShadowRenderingDemo` 作为验证场景。

---

## 目录

1. [原理摘要（对照博客）](#原理摘要对照博客)
2. [与 GTinyEngine 现状的对照](#与-gtinyengine-现状的对照)
3. [总体架构](#总体架构)
4. [分阶段实现计划](#分阶段实现计划)
5. [模块级设计](#模块级设计)
6. [着色器设计](#着色器设计)
7. [与 Sandbox / Demo 的集成](#与-sandbox--demo-的集成)
8. [质量优化（博客后续章节）](#质量优化博客后续章节)
9. [测试与验收标准](#测试与验收标准)
10. [后续扩展（不在本期范围）](#后续扩展不在本期范围)

---

## 原理摘要（对照博客）

阴影映射的核心思想：**从光源视角渲染一次场景，把“最近可见深度”存进纹理（Shadow Map / Depth Map）；在主渲染 Pass 中，把片元变换到光源空间，比较当前深度与 Shadow Map 采样深度，判断是否在阴影中。**

### 两 Pass 流程

| Pass | 目的 | 输出 |
|------|------|------|
| **Pass 1：深度图** | 以光源为“相机”，只写深度 | `GL_DEPTH_COMPONENT` 纹理 |
| **Pass 2：主光照** | 正常 PBR/Phong 光照，片元着色器做阴影测试 | 带阴影的最终颜色 |

### 光源空间矩阵

对**方向光**（平行光）使用：

- **投影**：正交投影 `glm::ortho(left, right, bottom, top, near, far)`
- **视图**：`glm::lookAt(lightPos, sceneCenter, up)`
- **lightSpaceMatrix** = `lightProjection * lightView`

每个世界空间点 `P` 经 `lightSpaceMatrix` 变换后，其 `z` 即为“从光源看到的深度”；`xy` 经 NDC → `[0,1]` 后用于采样 Shadow Map。

### 片元侧阴影判定（伪代码）

```glsl
vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
projCoords = projCoords * 0.5 + 0.5;           // NDC -> [0,1]
float closestDepth = texture(shadowMap, projCoords.xy).r;
float currentDepth = projCoords.z;
float shadow = (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
// 光照：ambient 不受阴影影响；diffuse/specular 乘以 (1.0 - shadow)
```

### 博客中的常见改进

| 问题 | 手段 |
|------|------|
| Shadow acne（条纹） | 深度偏移 `bias`（可随 `dot(N, L)` 变化） |
| Peter panning（阴影漂浮） | 生成 Shadow Map 时 `glCullFace(GL_FRONT)` |
| 视锥外错误采样 | `GL_CLAMP_TO_BORDER` + border color = 1.0；`projCoords.z > 1.0` 时 shadow = 0 |
| 硬边锯齿 | PCF：3×3 邻域采样平均 |

---

## 与 GTinyEngine 现状的对照

### 已有能力（可直接复用）

| 组件 | 现状 | 阴影实现中的用途 |
|------|------|------------------|
| `te::MultiRenderTarget` / `RenderTarget` | `include/framework/FrameBuffer.h` | Pass 1 的深度专用 FBO |
| `te::RenderPass` 基类 | `RenderPass.cpp` | 新增 `ShadowPass` |
| `RenderPassFlag::Shadowing` | `RenderPassFlag.h` | 标记参与 Shadow Pass 的 `RenderCommand` |
| `RenderPassType::Shadow` | `RenderPass.h` | Pass 类型枚举已预留 |
| `RenderPassManager` + `RenderGraph` | 依赖排序、输入绑定 | ShadowPass → BasePass 依赖链 |
| `Light` | 位置、颜色、方向 | 计算 `lightSpaceMatrix` |
| `RenderContext::GetDefaultLight()` | BasePass 已挂接光源 | Shadow / PBR 共用 |
| `PBRMaterial` + `pbr.fs` | 前向 PBR | 扩展阴影采样与 `(1-shadow)` 调制 |
| `Sandbox_ShadowRenderingDemo` | 球体 + 地面 Plane | 验收场景（`Examples/ShadowRenderingDemo` 仍为占位） |

### 当前缺口

1. **无 `ShadowPass` 实现**：`RenderPass.cpp` 中仅有 `GeometryPass`、`BasePass`、`PostProcessPass`、`SkyboxPass`。
2. **`RenderPassFlag::Shadowing` 未被使用**：`Sandbox_ShadowRenderingDemo` 仅设置 `BaseColor | Geometry`。
3. **PBR 着色器无 Shadow Map**：`common.vs` 未输出 `FragPosLightSpace`；`pbr.fs` 无 `shadowMap` / `ShadowCalculation`。
4. **`Light` 模型偏简单**：无显式 LightType（方向光/点光/聚光），方向光需约定用 `GetDirection()` 或 position→target 推导。
5. **多 Pass 默认管线未包含 Shadow**：`RenderAgent::SetupMultiPassRendering()` 为 Skybox → Geometry → Base → PostProcess。

---

## 总体架构

### 渲染管线（目标）

在 **不改变** 现有 G-Buffer 几何 Pass 的前提下，在 **BasePass 之前** 插入 Shadow Pass，把 Shadow Map 作为 BasePass 的输入纹理。

```
RenderCommand（来自 Sandbox / Scene）
        │
        ▼
┌──────────────┐
│  SkyboxPass  │  （背景，可选）
└──────┬───────┘
       ▼
┌──────────────┐     ShadowMap (Depth32F)
│  ShadowPass  │ ─────────────────────────┐
│  flag:       │                          │
│  Shadowing   │                          │
└──────┬───────┘                          │
       ▼                                  │
┌──────────────┐                          │
│ GeometryPass │  G-Buffer（与阴影并行）    │
│ flag:Geometry│                          │
└──────┬───────┘                          │
       ▼                                  ▼
┌──────────────┐     读取 ShadowMap ──────┘
│   BasePass   │  PBR + ShadowCalculation
│ flag:BaseColor
└──────┬───────┘
       ▼
┌──────────────┐
│PostProcessPass│ Blit 到屏幕
└──────────────┘
```

### 数据流

```
Light + Scene AABB
       → LightSpaceMatrixProvider（CPU）
       → ShadowPass（写 depth texture）
       → BasePass.BindInputs("ShadowMap", texture)
       → PBRMaterial / pbr_shadow.fs（读 shadowMap，调制光照）
```

### 设计原则

1. **最小侵入**：优先扩展 `RenderPass` + 着色器，不重构整个 deferred 路径。
2. **与 `RenderCommand` 对齐**：Shadow Pass 只绘制 `renderpassflag & Shadowing` 的命令（与 `GeometryPass` / `BasePass` 同一套命令列表）。
3. **资源由 Pass 拥有**：Shadow Map 分辨率独立于窗口（如 1024×1024），Pass 内切换 viewport。
4. **一期只做方向光 + 单张 Shadow Map**；点光/级联阴影列入后续扩展。

---

## 分阶段实现计划

### Phase 0：基础设施（1–2 天）

| 任务 | 说明 |
|------|------|
| 新增 `ShadowPass` 类 | 在 `RenderPass.h` 声明，在 `RenderPass.cpp` 实现 |
| Shadow FBO | 单 `Depth32F` 纹理；`glDrawBuffer(GL_NONE)`、`glReadBuffer(GL_NONE)` |
| `ShadowDepthMaterial` | 仅 `model` + `lightSpaceMatrix` 的深度 Pass 材质 |
| `LightSpaceMatrix` 工具 | 根据场景包围盒 + 光方向计算 ortho + lookAt |
| 注册 Pass | `RenderAgent::SetupMultiPassRendering()` 与 `MultiPassWithBackgroundDemo` 等入口 |

**Phase 0 验收**：Debug 全屏显示 Shadow Map（灰度深度），无阴影着色亦可。

### Phase 1：主 Pass 阴影（2–3 天）

| 任务 | 说明 |
|------|------|
| 扩展顶点着色器 | `FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0)` |
| 扩展 `pbr.fs` 或新增 `pbr_shadow.fs` | `ShadowCalculation` + `(1-shadow)` 乘 diffuse/specular |
| `BasePass` 输入 | `{"ShadowMap", "ShadowPass", "depth", ...}` |
| `PBRMaterial` | 绑定 shadow 纹理单元；`setMat4("lightSpaceMatrix", ...)` |
| `Sandbox_ShadowRenderingDemo` | 命令 flag 增加 `Shadowing`；调整球/地平面位置便于观察阴影 |

**Phase 1 验收**：球体在地面 Plane 上投下清晰阴影（允许轻微 acne）。

### Phase 2：质量与稳定性（1–2 天）

按博客顺序实现：

1. 固定 + 斜面自适应 `bias`
2. Shadow Pass 内 `GL_CULL_FACE` + `GL_FRONT`
3. `CLAMP_TO_BORDER` + `projCoords.z > 1.0` 分支
4. 3×3 PCF（可选 UI 开关）

**Phase 2 验收**：无明显条纹、视锥外无大块错误黑影、边缘略柔和。

### Phase 3：工程化（可选）

- ImGui：Shadow Map 分辨率、ortho 范围、bias、PCF 开关
- `RenderGraph` 资源声明：`ShadowMap` 显式 `Read/Write`
- 单元/截图对比测试清单

---

## 模块级设计

### 1. `te::ShadowPass`（新建）

**职责**：从光源视角渲染深度图。

**配置要点**（`OnInitialize`）：

```cpp
mConfig.name = "ShadowPass";
mConfig.type = RenderPassType::Shadow;
mRenderPassFlag = RenderPassFlag::Shadowing;

mConfig.outputs = {
    {"Depth", "depth", RenderTargetFormat::Depth32F}
};
mConfig.useCustomViewport = true;
mConfig.viewport = {0, 0, SHADOW_WIDTH, SHADOW_HEIGHT};  // 如 1024, 1024
mConfig.clearDepth = true;
mConfig.clearColor = false;
```

**Execute 流程**：

1. `ApplyRenderCommand(commands)` — 筛选 `Shadowing` 命令  
2. 计算/更新 `lightSpaceMatrix`（见下节）  
3. `mFrameBuffer->Bind()`，设置 viewport 为 Shadow 分辨率  
4. `glClear(GL_DEPTH_BUFFER_BIT)`  
5. 可选：`glCullFace(GL_FRONT)`（Phase 2）  
6. 对每个 candidate：用 `ShadowDepthMaterial`，`setMat4("lightSpaceMatrix", ...)`，`setMat4("model", transform)`，绘制网格  
7. `Unbind`，恢复 viewport 与 cull 状态  

**与博客对应**：即 “1. first render to depth map” 整段逻辑，复用引擎 `MultiRenderTarget` 而非手写裸 `glGenFramebuffers`（内部仍会是 FBO + depth texture）。

### 2. `LightSpaceMatrix` 计算（新建工具类或 `RenderContext` 方法）

博客示例使用固定 ortho 范围；引擎中建议 **用场景 AABB 拟合**，避免阴影贴图浪费或裁切。

**输入**：

- `Light::GetDirection()`（方向光，需归一化）
- 场景包围盒：遍历当前帧 `Shadowing` 命令中所有 `worldTransform * mesh AABB`
- 可调参数：`shadowDistance`、`padding`、`nearPlane`、`farPlane`

**步骤（概念）**：

1. 以场景中心为焦点，沿 `-lightDir` 放置虚拟光源位置  
2. `lightView = lookAt(lightPos, sceneCenter, up)`  
3. 将 AABB 8 顶点变换到 light view space，取 `min/max` 得到 ortho 六面体  
4. `lightProjection = ortho(minX, maxX, minY, maxY, near, far)`  
5. `lightSpaceMatrix = lightProjection * lightView`  

**输出**：每帧写入 `RenderContext` 或 `ShadowPass` 成员，供 Shadow Pass 与 BasePass 共用。

### 3. `ShadowDepthMaterial`（新建）

| 属性 | 值 |
|------|-----|
| VS | `resources/shaders/shadow/shadow_depth.vs` |
| FS | `resources/shaders/shadow/shadow_depth.fs`（空 main，仅写深度） |
| Uniforms | `model`, `lightSpaceMatrix` |

不执行光照；与博客 `simpleDepthShader` 一致。

### 4. 扩展 `BasePass`

**依赖**：

```cpp
mConfig.dependencies.push_back({"ShadowPass", true, []{ return true; }});
mConfig.inputs.push_back({"ShadowMap", "ShadowPass", "depth", 0, true});
```

**Execute 中**（在 `OnBind` / `UpdateUniform` 之前）：

- `pMaterial->GetShader()->setMat4("lightSpaceMatrix", lightSpaceMatrix);`
- 将 `GetInput("ShadowMap")` 绑定到约定纹理单元（如 **unit 5**，避免与 PBR 0–4 冲突）

### 5. 扩展 `PBRMaterial`

| 变更 | 说明 |
|------|------|
| 可选 FS 路径 | `pbr_shadow.fs` 或在 `pbr.fs` 用 `#ifdef ENABLE_SHADOW`（需与 shader preprocessor 对齐） |
| `OnBind` | `glActiveTexture(GL_TEXTURE5); glBindTexture(shadowMap)` |
| `UpdateUniform` | `lightSpaceMatrix`、`u_shadowBias`、`u_enablePCF` |
| 光照公式 | `vec3 Lo = (diffuse + specular) * (1.0 - shadow);`，`ambient` 不乘 shadow（与博客一致） |

当前 `pbr.fs` 使用 Cook-Torrance 直接光项；阴影应乘在 **直接光结果** 上，而非整个 `ambient + direct`。

### 6. `Light` 小扩展（建议）

```cpp
enum class LightType { Directional, Point, Spot };
void SetType(LightType t);
void SetDirection(const glm::vec3& dir);  // 已有 GetDirection，补 setter
```

一期 Shadow Pass **仅处理 Directional**；Point/Spot 使用透视 Shadow Map（博客 “Orthographic vs perspective”）留作后续。

### 7. `RenderPassFlag` 使用约定

| Pass | Flag |
|------|------|
| ShadowPass | `Shadowing` |
| GeometryPass | `Geometry` |
| BasePass | `BaseColor` |

**同一物体**通常同时参与 Shadow 与光照：

```cpp
cmd.renderpassflag = RenderPassFlag::Shadowing
                   | RenderPassFlag::Geometry
                   | RenderPassFlag::BaseColor;
```

仅接收阴影、不投射阴影的地面可只设 `Geometry | BaseColor`（博客中 floor 只接收）；若 floor 也写入 Shadow Map，则加上 `Shadowing`。

---

## 着色器设计

### `shadow_depth.vs`

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
```

### `shadow_depth.fs`

```glsl
#version 330 core
void main() { }
```

### 扩展 `common.vs`（或 `pbr_shadow.vs`）

在现有 `FragPos/Normal/TexCoords` 基础上增加：

```glsl
out vec4 FragPosLightSpace;

uniform mat4 lightSpaceMatrix;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    // ... 原有 view/projection
}
```

### `shadow_common.glsl`（建议放在 `resources/shaders/includes/`）

```glsl
float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap,
                        vec3 normal, vec3 lightDir, float bias, bool usePCF)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float slopeBias = max(0.05 * (1.0 - dot(normal, lightDir)), bias);

    // PCF 3x3 或单次采样
    ...
}
```

在 `pbr.fs` 的 `#include` 之后、计算 `Lo` 处调用，并 `Lo *= (1.0 - shadow)`。

### 纹理单元分配（建议）

| Unit | 用途 |
|------|------|
| 0–4 | PBR 现有贴图 |
| 5 | `shadowMap` |

---

## 与 Sandbox / Demo 的集成

### `Sandbox_ShadowRenderingDemo`

**场景布局（建议，便于观察阴影）**：

- 球体：`( -1.5, 0.5, -2.0 )` 略高于地面  
- 平面：绕 X 轴 -90°，作为接收面，`y = -1`  
- 方向光：例如 `direction = normalize((1, -1, 1))`，颜色 `(1,1,1)`  

**`GetRenderCommands` 修改**：

```cpp
sphereCommand.renderpassflag = RenderPassFlag::Shadowing
                             | RenderPassFlag::Geometry
                             | RenderPassFlag::BaseColor;

planeCommand.renderpassflag = RenderPassFlag::Geometry
                            | RenderPassFlag::BaseColor;  // 仅接收，可不写 Shadow Map
```

### `RenderAgent::SetupMultiPassRendering`

在 `GeometryPass` **之前** 注册并 `AddPass(shadowPass)`：

```cpp
auto shadowPass = std::make_shared<te::ShadowPass>();
shadowPass->Initialize(mpRenderView, mpRenderContext);
te::RenderPassManager::GetInstance().AddPass(shadowPass);  // 依赖：无
te::RenderPassManager::GetInstance().AddPass(skyboxPass);
te::RenderPassManager::GetInstance().AddPass(geometryPass);
// BasePass 依赖 ShadowPass + GeometryPass + SkyboxPass
```

`RenderGraph` 启用时，在 builder 中显式：

- `ShadowPass` **Write** `ShadowMap`  
- `BasePass` **Read** `ShadowMap`  

### `Examples/ShadowRenderingDemo`

可将 `main.cpp` 改为调用与 TinyRenderer 相同的 Pass 链，或仅作为无 UI 的自动化截图测试；**主验收场景以 TinyRenderer Sandbox 为准**。

---

## 质量优化（博客后续章节）

实现顺序建议与博客一致，便于逐项对比截图：

### 1. Shadow bias

```glsl
float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
shadow = (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
```

暴露为 ImGui / 配置项，按场景微调。

### 2. 正面剔除（Peter panning）

仅在 `ShadowPass::Execute` 内：

```cpp
glEnable(GL_CULL_FACE);
glCullFace(GL_FRONT);
// ... render ...
glCullFace(GL_BACK);
```

对**单平面地面**无效（会被完全剔除）；地面依赖 bias，实体用 front cull。

### 3. 视锥外 Over-sampling

创建 Shadow 深度纹理时：

- `GL_CLAMP_TO_BORDER`  
- `glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white)`  
- 片元中 `projCoords.z > 1.0` → `shadow = 0`  

需在 `FrameBuffer.cpp` / `RenderTarget` 创建路径确认支持 border color（若无，Shadow Pass 内单独创建纹理）。

### 4. PCF（Percentage-Closer Filtering）

3×3 邻域平均，参考博客双重循环；注意 `textureSize(shadowMap, 0)` 与 `texelSize`。

---

## 测试与验收标准

| 阶段 | 检查项 |
|------|--------|
| Phase 0 | Shadow Map 调试视图可见；物体轮廓与深度渐变正确 |
| Phase 1 | 球体在地面有阴影；移动相机阴影相对稳定；环境光区域不被错误压黑 |
| Phase 2 | 斜面无条纹；视锥边缘无大面积假阴影；PCF 开启后边缘变柔 |
| 回归 | `MultiPassWithBackgroundDemo`、默认 Sandbox 无 Shadow 时行为不变（`ShadowPass` 可 Conditional 禁用） |

**已知问题（与阴影无关）**：`doc/开发过程问题记录.md` 中 ASWD 移动时球与平面遮挡关系异常，属相机/深度排序问题，应在阴影实现前或并行排查，避免误判为 Shadow acne。

---

## 后续扩展（不在本期范围）

| 方向 | 说明 |
|------|------|
| Point / Spot 阴影 | 透视投影 Shadow Map（博客 Orthographic vs perspective） |
| Omnidirectional | 立方体 Shadow Map（博客 Point Shadows） |
| CSM | 级联阴影，适配大场景 |
| 与 Deferred 结合 | 在 `deferred/lighting.fs` 中采样 Shadow Map，替代前向 BasePass |
| Vulkan | `GTVulkan` / `VK_Deferred` 路径单独设计 `VkRenderPass` + depth attachment |
| 多光源 | 多张 Shadow Map 或 atlas |

---

## 文件清单（预计新增/修改）

### 新增

| 路径 | 用途 |
|------|------|
| `include/framework/ShadowPass.h` | Shadow Pass 声明（或合入 `RenderPass.h`） |
| `source/framework/ShadowPass.cpp` | 实现（或合入 `RenderPass.cpp`） |
| `include/materials/ShadowDepthMaterial.h` | 深度 Pass 材质 |
| `source/materials/ShadowDepthMaterial.cpp` | |
| `include/framework/LightSpaceMatrix.h` | 矩阵计算工具 |
| `source/framework/LightSpaceMatrix.cpp` | |
| `resources/shaders/shadow/shadow_depth.vs` | |
| `resources/shaders/shadow/shadow_depth.fs` | |
| `resources/shaders/includes/shadow_common.glsl` | |

### 修改

| 路径 | 用途 |
|------|------|
| `include/framework/RenderPass.h` | 声明 `ShadowPass`（若独立头文件则仅 include） |
| `source/framework/RenderPass.cpp` 或 `ShadowPass.cpp` | Pass 实现 |
| `resources/shaders/common/common.vs` 或 `pbr_shadow.vs` | `FragPosLightSpace` |
| `resources/shaders/common/pbr.fs` | 阴影调制 |
| `source/materials/PBRMaterial.cpp` | Shadow 纹理与 uniform |
| `include/Light.h` / `source/Light.cpp` | LightType、SetDirection |
| `TinyRenderer/source/RenderAgent.cpp` | 注册 ShadowPass |
| `source/sandbox/Sandbox_ShadowRenderingDemo.cpp` | RenderPassFlag |
| `CMakeLists.txt`（相关 target） | 新源文件与 shader 安装 |

---

## 小结

GTinyEngine 实现阴影映射的推荐路径是：**在现有 `RenderPass` 框架中新增 `ShadowPass` 生成深度贴图，通过 `RenderPassFlag::Shadowing` 筛选绘制对象，在 `BasePass` 的 PBR 着色阶段用 `lightSpaceMatrix` 与 `ShadowCalculation` 调制直接光**。这与 LearnOpenGL 的两 Pass 流程一一对应，且与当前 `GeometryPass` + `BasePass` 多 Pass 架构兼容。实现按 **深度图 → 基础阴影 → bias/PCF/边界修复** 三阶段推进，以 `Sandbox_ShadowRenderingDemo` 作为端到端验收场景。
