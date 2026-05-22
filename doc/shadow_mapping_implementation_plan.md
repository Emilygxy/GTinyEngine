# GTinyEngine 阴影映射（Shadow Mapping）实现方案

> 参考：[LearnOpenGL - Shadow Mapping](https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping)  
> 目标：在现有 OpenGL 多 Pass 管线（TinyRenderer / `RenderPassManager`）中落地方向光阴影，并以 `Sandbox_ShadowRenderingDemo` 作为验证场景。  
> **文档状态**：已与当前代码同步（Phase 0–1 完成，Phase 2 部分完成，Phase 3 未开始）。

---

## 目录

1. [实现进度总览](#实现进度总览)
2. [原理摘要（对照博客）](#原理摘要对照博客)
3. [当前实现对照](#当前实现对照)
4. [总体架构](#总体架构)
5. [分阶段实现计划](#分阶段实现计划)
6. [模块级设计（已实现）](#模块级设计已实现)
7. [着色器设计](#着色器设计)
8. [与 Sandbox / Demo 的集成](#与-sandbox--demo-的集成)
9. [质量优化与已知取舍](#质量优化与已知取舍)
10. [测试与验收标准](#测试与验收标准)
11. [后续扩展](#后续扩展)
12. [文件清单](#文件清单)

---

## 实现进度总览

| 阶段 | 状态 | 完成度 | 说明 |
|------|------|--------|------|
| **Phase 0** 基础设施 | ✅ 已完成 | 100% | ShadowPass、FBO、深度材质、矩阵工具、TinyRenderer 注册 |
| **Phase 1** 主 Pass 阴影 | ✅ 已完成 | ~95% | PBR/Phong 阴影采样；与文档在 BasePass 输入方式上略有差异 |
| **Phase 2** 质量优化 | ⚠️ 部分完成 | ~70% | bias、border、PCF、远/近平面已实现；正面剔除未启用 |
| **Phase 3** 工程化 | ❌ 未开始 | 0% | 无 ImGui、无 RenderGraph 显式资源、无自动化测试 |
| **后续扩展** | ❌ 未开始 | — | 点光、CSM、Vulkan 等 |

---

## 原理摘要（对照博客）

阴影映射的核心思想：**从光源视角渲染一次场景，把“最近可见深度”存进纹理（Shadow Map / Depth Map）；在主渲染 Pass 中，把片元变换到光源空间，比较当前深度与 Shadow Map 采样深度，判断是否在阴影中。**

### 两 Pass 流程

| Pass | 目的 | 输出 |
|------|------|------|
| **Pass 1：深度图** | 以光源为“相机”，只写深度 | `GL_DEPTH_COMPONENT` 纹理（`shadowmap`） |
| **Pass 2：主光照** | PBR/Phong 光照 + 片元阴影测试 | 带阴影的最终颜色 |

### 光源空间矩阵

对**方向光**（平行光）使用：

- **投影**：正交投影 `glm::ortho(left, right, bottom, top, near, far)`
- **视图**：`glm::lookAt(lightPos, sceneCenter, up)`
- **lightSpaceMatrix** = `lightProjection * lightView`

每个世界空间点 `P` 经 `lightSpaceMatrix` 变换后，其 `z` 即为“从光源看到的深度”；`xy` 经 NDC → `[0,1]` 后用于采样 Shadow Map。

### 片元侧阴影判定（当前实现）

见 `resources/shaders/includes/shadow_common.glsl`：`ShadowCalculation` 含透视除法、NDC→[0,1]、斜面 bias、可选 3×3 PCF、越界处理。

```glsl
// 光照：ambient 不受阴影影响；直接光 Lo 乘以 (1.0 - shadow)
Lo *= (1.0 - shadow);
```

### 博客中的常见改进

| 问题 | 手段 | 实现状态 |
|------|------|----------|
| Shadow acne（条纹） | 深度偏移 `bias`（随 `dot(N, L)` 变化） | ✅ |
| Peter panning（阴影漂浮） | Shadow Map 阶段 `glCullFace(GL_FRONT)` | ❌ 未启用（见[已知取舍](#质量优化与已知取舍)） |
| 视锥外错误采样 | `GL_CLAMP_TO_BORDER` + border=1.0；`z>1` / `xy` 越界 | ✅ |
| 硬边锯齿 | 3×3 PCF | ✅ 默认开启，无 ImGui |

---

## 当前实现对照

### 已实现

| 组件 | 路径 / 说明 |
|------|-------------|
| `te::ShadowPass` | `include/framework/RenderPass.h`，`source/framework/ShadowPass.cpp` |
| Shadow FBO | 1024×1024，`Depth32F`，`GL_CLAMP_TO_BORDER`，`glDrawBuffer/readBuffer(GL_NONE)` |
| `ShadowDepthMaterial` | `include/materials/ShadowDepthMaterial.h`，`shadow/shadow_depth.{vs,fs}` |
| `LightSpaceMatrix` | `include/framework/LightSpaceMatrix.h`，AABB 拟合 ortho + 正确 near/far 距离 |
| `RenderPassFlag::Shadowing` | Shadow Pass 筛选；Sandbox 球体已使用 |
| `common.vs` | `FragPosLightSpace`，`uniform mat4 u_lightSpaceMatrix` |
| `pbr.fs` / `phong.fs` | `#include shadow_common.glsl`，纹理 unit 5 |
| `PBRMaterial` / `PhongMaterial` | `SetShadowEnabled/Map/LightSpaceMatrix/Bias/PCFEnabled` |
| `BasePass` | 依赖 `ShadowPass`；运行时 `GetPass("ShadowPass")` 取矩阵与纹理 |
| `RenderAgent` | `SetupMultiPassRendering()` 最先注册 `ShadowPass` |
| `Light::SetDirection` | `include/Light.h`，`source/Light.cpp` |
| `Sandbox_ShadowRenderingDemo` | 球投射 + 地面仅接收阴影 |

### 待办 / 与初版方案差异

| 项 | 状态 |
|----|------|
| `BasePass` 经 `mConfig.inputs` 绑定 `ShadowMap` | 未做；改用 `RenderPassManager::GetPass` |
| 独立 `pbr_shadow.fs` | 未做；在 `pbr.fs` 内扩展 |
| `Light::LightType` 枚举 | 未做 |
| `MultiPassWithBackgroundDemo` 等注册 ShadowPass | 未做 |
| Shadow Pass `GL_FRONT` 剔除 | 未做（球体/平面会出问题） |
| Phase 3：ImGui、RenderGraph 资源、测试清单 | 未做 |
| `Examples/ShadowRenderingDemo` | 仍为占位 `main` |
| `ShadowPass` 条件禁用（`RenderPassState::Conditional`） | 未做 |

---

## 总体架构

### 渲染管线（当前 TinyRenderer）

```
RenderCommand（Sandbox / Scene）
        │
        ▼
┌──────────────┐     shadowmap (1024², Depth32F)
│  ShadowPass  │ ─────────────────────────────┐
│  Shadowing   │                               │
└──────┬───────┘                               │
       ▼                                       │
┌──────────────┐                               │
│  SkyboxPass  │                               │
└──────┬───────┘                               │
       ▼                                       │
┌──────────────┐                               │
│ GeometryPass │  G-Buffer                     │
│  Geometry    │                               │
└──────┬───────┘                               │
       ▼                                       ▼
┌──────────────┐     GetPass("ShadowPass") ────┘
│   BasePass   │     PBR/Phong + ShadowCalculation
│  BaseColor   │
└──────┬───────┘
       ▼
┌──────────────┐
│PostProcessPass│
└──────────────┘
```

### 数据流（实际）

```
Light::GetDirection + 场景 AABB（投射体 + 接收体，见下）
       → ComputeDirectionalLightSpaceMatrix()
       → ShadowPass 写入 shadowmap
       → BasePass 从 ShadowPass 取 GetLightSpaceMatrix() / GetShadowMapTexture()
       → PBRMaterial / PhongMaterial（unit 5，u_lightSpaceMatrix）
```

### 设计原则

1. **最小侵入**：扩展 `RenderPass` + 着色器，不重构 deferred 路径。
2. **`RenderCommand` 对齐**：Shadow Pass 仅绘制 `Shadowing`；BasePass 绘制 `BaseColor`。
3. **Shadow Map 分辨率独立**：`ShadowPass::kShadowMapSize = 1024`，`useCustomViewport = true`。
4. **一期仅方向光 + 单张 Shadow Map**。

### 光源视锥拟合（实现细节）

`ShadowPass::ComputeSceneBounds` **区别于初版文档**：

1. 分别收集 **投射体**（`Shadowing`）与 **接收体**（`BaseColor`）的 AABB。
2. 以投射体为基础，在 **XZ** 上扩展并纳入接收体范围（`kGroundMargin = 4`），避免整块 20×20 地面把 Shadow Map 拉得过大导致阴影只有几个像素。
3. **Y** 方向合并投射体与接收体高度。

`LightSpaceMatrix` 中 **near/far** 使用 GLM 惯例的**正值距离**（非直接把负的 view-Z 传入 ortho）：

```cpp
nearPlaneDist = max(-lightSpaceMax.z + pad, params.nearPlane);
farPlaneDist  = max(-lightSpaceMin.z + pad, nearPlaneDist + 0.1f);
glm::ortho(..., nearPlaneDist, farPlaneDist);
```

---

## 分阶段实现计划

### Phase 0：基础设施 ✅ 已完成

| 任务 | 状态 | 实际位置 |
|------|------|----------|
| `ShadowPass` 类 | ✅ | `RenderPass.h` + `ShadowPass.cpp` |
| Shadow FBO | ✅ | `Depth32F`，draw/read `GL_NONE` |
| `ShadowDepthMaterial` | ✅ | `ShadowDepthMaterial.{h,cpp}` |
| `LightSpaceMatrix` | ✅ | `LightSpaceMatrix.{h,cpp}` |
| 注册 Pass | ⚠️ | ✅ `TinyRenderer/RenderAgent.cpp`；❌ 其他 Demo 未统一注册 |

**验收**：RenderDoc 可查看 ShadowPass 深度附件（灰度轮廓）。

### Phase 1：主 Pass 阴影 ✅ 已完成

| 任务 | 状态 | 说明 |
|------|------|------|
| `common.vs` → `FragPosLightSpace` | ✅ | `u_lightSpaceMatrix` |
| `pbr.fs` + `ShadowCalculation` | ✅ | 直接光 × `(1-shadow)` |
| `phong.fs` 阴影 | ✅ | 文档外额外支持 |
| `BasePass` 读 ShadowMap | ✅ | `GetPass("ShadowPass")`，非 `mConfig.inputs` |
| `PBRMaterial` unit 5 | ✅ | |
| `Sandbox_ShadowRenderingDemo` | ✅ | 球 `Shadowing\|Geometry\|BaseColor`；地面仅 `Geometry\|BaseColor` |

**验收**：球体在 Plane（`y=-1`）上应可见近圆形阴影（球心约 `(-1.5, 0.5, -2)`）。

### Phase 2：质量与稳定性 ⚠️ 部分完成

| 项 | 状态 |
|----|------|
| 斜面自适应 bias | ✅ `shadow_common.glsl` |
| `GL_CLAMP_TO_BORDER` + `borderColor` | ✅ `FrameBuffer` + `ShadowPass` |
| `projCoords.z > 1.0` / `xy` 越界 | ✅ |
| 3×3 PCF | ✅ 默认开；`SetShadowPCFEnabled(bool)`，无 UI |
| `GL_FRONT` 正面剔除 | ❌ 未启用 |
| bias / PCF ImGui | ❌ 属 Phase 3 |

### Phase 3：工程化 ❌ 未开始

- ImGui：Shadow Map 分辨率、ortho、bias、PCF
- `RenderGraph` 显式 `shadowmap` Read/Write
- 自动化截图 / 单元测试清单
- 内置 Shadow Map 全屏调试 Blit

---

## 模块级设计（已实现）

### 1. `te::ShadowPass`

| 配置项 | 当前值 |
|--------|--------|
| `mConfig.name` | `"ShadowPass"` |
| `mConfig.type` | `RenderPassType::Shadow` |
| `mRenderPassFlag` | `RenderPassFlag::Shadowing` |
| 输出 | `{"ShadowMap", "shadowmap", Depth32F}` |
| Viewport | `1024 × 1024`（`kShadowMapSize`） |
| 深度纹理 | `GL_NEAREST`，`GL_CLAMP_TO_BORDER`，`borderColor = (1,1,1,1)` |

**Execute 流程**：

1. `ApplyRenderCommand` — 仅 `Shadowing` 命令  
2. `ComputeSceneBounds` — 投射体 + 接收体（见上）  
3. `ComputeDirectionalLightSpaceMatrix` → `mLightSpaceMatrix`  
4. 绑定 FBO，`glDrawBuffer(GL_NONE)`，`glClear(DEPTH)`  
5. `ShadowDepthMaterial` 绘制所有 candidate  
6. **不**使用 `glCullFace(GL_FRONT)`（当前取舍）  

**对外接口**：

- `GetLightSpaceMatrix()`
- `GetShadowMapTexture()` → `GetOutput("shadowmap")`

### 2. `LightSpaceMatrix`

- 输入：`Light::GetDirection()`（或 position→scene 回退）、场景 `AaBB`、`LightSpaceMatrixParams`
- 输出：`lightSpace`、`lightProjection`、`lightView`
- 默认参数（`ShadowPass`）：`orthoPadding=1.5`，`shadowDistance=15`，`nearPlane=0.1`，`farPlane=40`

### 3. `ShadowDepthMaterial`

| 属性 | 值 |
|------|-----|
| VS | `resources/shaders/shadow/shadow_depth.vs` |
| FS | `resources/shaders/shadow/shadow_depth.fs`（空） |
| Uniforms | `model`，`lightSpaceMatrix` |

### 4. `BasePass`（阴影相关）

```cpp
mConfig.dependencies = {
    {"ShadowPass", true, ...},
    {"SkyboxPass", true, ...},
    {"GeometryPass", true, ...},
};
```

Execute 内对 `PBRMaterial` / `PhongMaterial`：

```cpp
shadow->GetLightSpaceMatrix();
shadow->GetShadowMapTexture();
pMaterial->SetShadowEnabled(shadowAvailable);
pMaterial->SetLightSpaceMatrix(...);
pMaterial->SetShadowMap(...);
```

### 5. `PBRMaterial` / `PhongMaterial`

| API / Uniform | 说明 |
|---------------|------|
| `kShadowTextureUnit = 5` | `u_shadowMap` |
| `u_lightSpaceMatrix` | 与 `common.vs` 一致 |
| `u_enableShadow` | 0/1 |
| `u_shadowBias` | 默认 `0.005` |
| `u_enablePCF` | 默认 `1`（3×3 PCF） |

光照：Cook-Torrance / Phong 的**直接光**乘 `(1-shadow)`，环境光不乘 shadow。

### 6. `Light`

- ✅ `SetDirection` / `GetDirection`
- ❌ `LightType` 枚举（仍为方向光约定）

### 7. `RenderPassFlag` 约定（当前 Sandbox）

| 物体 | Flag |
|------|------|
| 球体（投射 + 接收光照） | `Shadowing \| Geometry \| BaseColor` |
| 地面（仅接收） | `Geometry \| BaseColor` |

---

## 着色器设计

### `shadow_depth.vs` / `shadow_depth.fs`

已实现，见 `resources/shaders/shadow/`。

### `common.vs`（已实现）

```glsl
out vec4 FragPosLightSpace;
uniform mat4 u_lightSpaceMatrix;
// FragPosLightSpace = u_lightSpaceMatrix * vec4(FragPos, 1.0);
```

### `shadow_common.glsl`（已实现）

- `ShadowCompare` / `ShadowCalculation(..., bias, enablePCF)`
- 越界：`z ∉ [0,1]` 或 `xy ∉ [0,1]` → 无阴影
- PCF：`enablePCF >= 0.5` 时 3×3 邻域平均

### 纹理单元

| Unit | 用途 |
|------|------|
| 0–4 | PBR 贴图（albedo / normal / metallic / roughness / ao） |
| 5 | `u_shadowMap` |

---

## 与 Sandbox / Demo 的集成

### `Sandbox_ShadowRenderingDemo`（当前）

| 对象 | 变换 / 材质 | RenderPassFlag |
|------|-------------|----------------|
| 球体 `Sphere` | 位置 `(-1.5, 0.5, -2)`，PBR + 贴图 | `Shadowing \| Geometry \| BaseColor` |
| 地面 `Plane` 4×4，scale 5，绕 X -90°，`y=-1` | 白色 PBR | `Geometry \| BaseColor`（无 `Shadowing`） |

### `RenderAgent::SetupMultiPassRendering`（TinyRenderer）

```cpp
auto shadowPass = std::make_shared<te::ShadowPass>();
shadowPass->Initialize(mpRenderView, mpRenderContext);
RenderPassManager::GetInstance().AddPass(shadowPass);   // 第一个
RenderPassManager::GetInstance().AddPass(skyboxPass);
RenderPassManager::GetInstance().AddPass(geometryPass);
RenderPassManager::GetInstance().AddPass(basePass);
RenderPassManager::GetInstance().AddPass(postProcessPass);
```

默认光源（`RenderAgent` 初始化）：

```cpp
pLight->SetPosition(glm::vec3(2.0f, 2.0f, 2.0f));
pLight->SetDirection(glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f)));
```

> 注意：PBR 仍用 `u_lightPos` 做点光衰减，阴影用 `GetDirection()` 做平行光，二者尚未完全统一。

### `Examples/ShadowRenderingDemo`

仍为占位；**主验收以 TinyRenderer → Shadow Rendering Sandbox 为准**。

---

## 质量优化与已知取舍

### 已实现

1. **Shadow bias**：`max(0.05 * (1.0 - dot(N,L)), bias)`，`bias` 默认 `0.005`。  
2. **Over-sampling**：`GL_CLAMP_TO_BORDER` + 白边；片元越界返回 0。  
3. **PCF**：3×3，默认开启。

### 正面剔除（未启用）

LearnOpenGL 建议在 Shadow Pass 使用 `glCullFace(GL_FRONT)` 减轻 Peter panning。当前**未启用**，原因：

| 网格类型 | 问题 |
|----------|------|
| **球体** | 剔正面后深度图只剩远壳，地面阴影呈月牙/细环或消失 |
| **单平面地面** | 剔正面后平面不进 Shadow Map（故地面不设 `Shadowing`） |

后续可对 **封闭非球体**（如 Cube）按类型可选开启正面剔除。

### 实现过程中遇到的问题（供排查）

| 现象 | 原因 | 处理 |
|------|------|------|
| 月牙状窄阴影 | 视锥只 fit 球体；地面 UV 大部分在 map 外 | 视锥纳入接收体 + XZ margin |
| 完全无阴影 | `glm::ortho` 的 near/far 误用负 view-Z | 改为正值距离 `-maxZ`/`-minZ` |
| 正面剔除后球影异常 | 球体深度轮廓错误 | 关闭 `GL_FRONT` cull |

**其他**：`doc/开发过程问题记录.md` 中 ASWD 移动时遮挡关系异常，与 Shadow acne 无关，属相机/深度排序。

---

## 测试与验收标准

| 阶段 | 检查项 | 状态 |
|------|--------|------|
| Phase 0 | RenderDoc 中 ShadowPass 深度附件可见 | 可验证 |
| Phase 1 | 球在地面上有清晰阴影；环境光不被误压黑 | 可验证 |
| Phase 2 | 斜面无明显条纹；视锥外无大块假影；边缘略柔和 | 依赖 PCF/border |
| 回归 | 无 Shadow 的 Sandbox 行为不变 | 未系统化测试 |

**RenderDoc 建议**：捕获帧 → 查看 `ShadowPass` → `shadowmap` 深度；再查看 `BasePass` 最终颜色。

---

## 后续扩展

| 方向 | 说明 |
|------|------|
| Phase 3 工程化 | ImGui、RenderGraph 资源、Shadow Map 调试视图 |
| 按网格类型正面剔除 | Cube 开启 `GL_FRONT`，Sphere/Plane 关闭 |
| 光照与阴影方向统一 | 点光/方向光一致，或纯方向光 PBR |
| `Light::LightType` | Directional / Point / Spot |
| Point / Spot / CSM / Vulkan | 见 LearnOpenGL 后续章节 |
| `MultiPassDemo` 等注册 ShadowPass | 与其他 Demo 管线一致 |
| `Examples/ShadowRenderingDemo` | 无 UI 自动化截图 |

---

## 文件清单

### 已新增

| 路径 | 状态 |
|------|------|
| `source/framework/ShadowPass.cpp` | ✅ |
| `include/materials/ShadowDepthMaterial.h` | ✅ |
| `source/materials/ShadowDepthMaterial.cpp` | ✅ |
| `include/framework/LightSpaceMatrix.h` | ✅ |
| `source/framework/LightSpaceMatrix.cpp` | ✅ |
| `resources/shaders/shadow/shadow_depth.vs` | ✅ |
| `resources/shaders/shadow/shadow_depth.fs` | ✅ |
| `resources/shaders/includes/shadow_common.glsl` | ✅ |

> `ShadowPass` 类声明在 `include/framework/RenderPass.h`（未单独 `ShadowPass.h`）。

### 已修改

| 路径 | 变更摘要 |
|------|----------|
| `include/framework/RenderPass.h` | 声明 `ShadowPass` |
| `source/framework/RenderPass.cpp` | `BasePass` 阴影注入、依赖 `ShadowPass` |
| `include/framework/FrameBuffer.h` | `RenderTargetDesc::borderColor` |
| `source/framework/FrameBuffer.cpp` | `GL_CLAMP_TO_BORDER` 时设置 border |
| `resources/shaders/common/common.vs` | `FragPosLightSpace` |
| `resources/shaders/common/pbr.fs` | 阴影采样 |
| `resources/shaders/common/phong.fs` | 阴影采样 |
| `include/materials/PBRMaterial.h` | Shadow API |
| `source/materials/PBRMaterial.cpp` | |
| `include/materials/BaseMaterial.h` | `PhongMaterial` Shadow API |
| `source/materials/BaseMaterial.cpp` | |
| `include/Light.h` / `source/Light.cpp` | `SetDirection` |
| `TinyRenderer/source/RenderAgent.cpp` | 注册 ShadowPass、默认光方向 |
| `source/sandbox/Sandbox_ShadowRenderingDemo.cpp` | Flag 与场景布局 |

> 新源文件由 CMake `GLOB` 自动纳入 `GTinyEngine`；`resources` 由 POST_BUILD 复制到输出目录。

---

## 小结

当前 GTinyEngine 已实现 **OpenGL 方向光 Shadow Mapping** 主路径：`ShadowPass` 写 `shadowmap`，`BasePass` 中 **PBR/Phong** 通过 `shadow_common.glsl` 调制直接光。与 LearnOpenGL 两 Pass 流程一致，实现上采用 **AABB 拟合视锥**、**正值 near/far**、**投射/接收分离的 bounds**，并因 **球体/平面** 禁用正面剔除。

**下一步建议**：Phase 3（ImGui + 调试视图）；可选按物体类型启用正面剔除；统一 `u_lightPos` 与阴影方向。
