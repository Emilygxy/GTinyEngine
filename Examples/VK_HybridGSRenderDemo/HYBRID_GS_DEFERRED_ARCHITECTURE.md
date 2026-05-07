# VK_HybridGSRenderDemo：3D Gaussian Splatting 与 MRT 延迟渲染（传统 Mesh）混合架构设计

本文基于仓库内当前实现，对 **在 `VK_HybridGSRenderDemo` 中集成 `.ply` 3DGS 与 Vulkan 延迟渲染管线** 给出可落地的架构结论与实施顺序。涉及代码位置：

- GS 数据与类型：`GTVulkan/include/GaussianSplat/GSRenderTypes.h`，加载：`GTVulkan/source/GaussianSplat/GSSceneLoader.cpp`
- GS 计算与呈现：`GTVulkan/source/GaussianSplat/GSComputeRenderer.cpp`（内部 `GSComputeRendererImpl`），Shader：`resources/shaders/vk/gs_*.comp`（构建产物见各 Demo README）
- 延迟 GBuffer：`GTVulkan/include/GTVulkan/VK_Deferred.h`、`GTVulkan/source/VK_Deferred.cpp`（`VulkanGBuffer`，MRT：Albedo / Normal / Material + Depth）
- 帧编排：`source/framework/VulkanDeferredPipeline.cpp`（`GeometryPass` → `LightingPass`）、`source/framework/Renderer.cpp`（`VulkanRenderer`：离屏 lighting → post → present）

当前 `Examples/VK_HybridGSRenderDemo/source/main.cpp` 仍走独立 `GSRenderDemoApp`，与「延迟 + Mesh」目标不一致；下列设计以 **单一 `GraphicsBase` 窗口与单条主 command buffer 帧** 为前提。

**与 `GaussianSplat_Godot_Analysis_And_GW_Minimal_Checklist.md` 的关系**：该文档总结了 Godot `gdgs` 插件的「离屏 GS + 深度感知 Compositor」思路，以及 GW 侧的最小改造清单。本文将其中 **已验证有效的稳定性手段** 映射到本仓库的 Vulkan 延迟管线，并把 **策略 B（合成阶段 GBuffer 深度 + GS 深度遮挡）** 作为 **首选目标架构**（策略 A 仍可作为阶段性垫脚石）。

---

## 1. 现状与根本矛盾

| 维度 | 当前 `GSComputeRenderer` | 当前 `VulkanRenderer` 延迟路径 |
|------|-------------------------|--------------------------------|
| 窗口 / 设备 | `initialize()` 内 `InitializeWindow`，自管生命周期 | 依赖已存在的 `easy_vk` 窗口与 `GraphicsBase` |
| 每帧输出 | Compute 直接 `imageStore` 到 **Swapchain**（+ 自管 depth/normal storage image） | GBuffer → **离屏 HDR**（`lightingTarget`）→ Post → Present |
| 相机矩阵 | `updateUniforms()` 内 3DGS 约定：`lookAt` + `tan_fovx/tan_fovy` + 手工 `proj_mat` 分量翻转 | `Camera::GetViewMatrix()` / `GetProjectionMatrix()`，且 Vulkan 下 `proj[1][1] *= -1` |
| 与 Mesh 关系 | 无 GBuffer、无光照 | Mesh 仅写入 GBuffer，光照为全屏 Pass |

**结论**：不能把现成 `GSComputeRenderer::initialize/run` 原封不动嵌进 Hybrid Demo。必须先做 **职责拆分（资源/Pass 与 App/窗口解耦）**，再在 **统一 command buffer** 上插入 GS 的 compute 段与 **深度感知合成 Pass**。

---

## 2. 集成目标（建议写进 Demo 需求）

1. **同一视口、同一相机**：Mesh 与 GS 对齐，避免「两层画面各算各的投影」（与 Godot 插件「统一相机语义」一致）。  
2. **同一 Vulkan 实例与 Swapchain**：仅一条提交链路与一致的 resize 语义。  
3. **首选混合语义 — 策略 B**：在 **光照之后、后处理之前（或并入后处理链）** 增加全屏 **Compositor**，读取 **已光照 HDR**、**GBuffer 深度**（或等价重建深度）、**GS 颜色（含 alpha）**、**GS 深度**，在同一深度域内做深度测试与 alpha 门控混合。  
4. **可调参稳定性**：暴露与 Godot / GW 清单同类的参数（`depth_capture_alpha`、`depth_bias`、`depth_test_min_alpha`、`alpha_cutoff` 等），便于在轨道/推拉/平移相机下压边界闪烁。

---

## 3. 推荐总体架构：三层 + Compositor（对齐 Godot 管线形态）

Godot `gdgs` 的总体形态可概括为：**离屏 GS（颜色 + 深度）→ Compositor 与场景颜色/深度合并**。本 Hybrid 方案与之同构，仅将「场景」替换为 **延迟渲染输出的 HDR + GBuffer 深度**。

```text
┌─────────────────────────────────────────────────────────────┐
│  HybridApplication（唯一入口：窗口、主循环、相机、CLI）      │
└─────────────────────────────────────────────────────────────┘
         │                              │
         v                              v
┌─────────────────────┐      ┌──────────────────────────────────┐
│ GSSceneLoader       │      │ VulkanRenderer（已有）             │
│ .ply → vector<GSVertex>    │ Geometry → GBuffer → Lighting … │
└─────────────────────┘      └──────────────────────────────────┘
         │                              │
         │         ┌────────────────────┴────────────────────┐
         │         │  GBuffer 深度在 Lighting 后需保持可采样：   │
         │         │  barrier 到 SHADER_READ_ONLY，或拷贝到     │
         │         │  专用 depth copy（若 lifetime 与 pass 冲突）│
         │         └──────────────────────────────────────────┘
         v
┌─────────────────────────────────────────────────────────────┐
│ GSComputeSubsystem（从 GSComputeRendererImpl 抽取）            │
│  - 投影 / radix sort / tile boundary / tile render（现有链）   │
│  - 输出：GS rgba（预乘或 straight 与合成端一致）+ GS depth     │
│  - 深度写出语义：见 §4.2「首层命中」策略（对齐 Godot）          │
└─────────────────────────────────────────────────────────────┘
         │
         v
┌─────────────────────────────────────────────────────────────┐
│ HybridCompositorPass（全屏 Fragment，新建或扩展 Post）         │
│  输入：lightingTarget（HDR）、GBuffer Depth、GS color、GS depth │
│  输出：进入现有 Post 链的 RT（如写入 post 前中间 RT）            │
│  逻辑：view-depth 同域比较 + bias + min-alpha 门控（§4.3）      │
└─────────────────────────────────────────────────────────────┘
```

**结论**：Hybrid 的「App」层只做编排；**GS 计算子系统**与 Godot 一样 **不进入 GBuffer 几何 Pass**；**Compositor** 承担深度感知合并，这是策略 B 的架构落点。

---

## 4. 帧内混合策略：以 B 为目标，A 为过渡

### 4.1 策略 A — 光照后 HDR 叠加（过渡 / 调试用）

**帧序**：Geometry → GBuffer → Lighting →（GS Compute →）**无深度** `over` / `mix` → Post → Present。

**用途**：快速验证「相机统一 + barrier + 分辨率对齐」；不作为最终画质目标。

---

### 4.2 策略 B — 深度感知合成（**首选目标**）

#### 4.2.1 帧序（推荐）

1. **GeometryPass**：Mesh 写入 `VulkanGBuffer`（含 `D32` 深度）。  
2. **LightingPass**：采样 GBuffer，写入 `lightingTarget`（HDR）。  
3. **Barrier**：  
   - 使 **GBuffer 深度** 对 fragment **可读**（`DEPTH_STENCIL_READ_ONLY_OPTIMAL` 或已从 `VulkanGBuffer::CmdTransitionForLightingRead` 过渡；若 lighting pass 结束布局不满足 Compositor，再补一道 barrier）。  
   - `lightingTarget` 若需同时作为采样输入，需从 `COLOR_ATTACHMENT_OPTIMAL` 转到 `SHADER_READ_ONLY_OPTIMAL`（或与 Compositor 子 pass 的 input attachment 设计一致）。  
4. **GSComputeSubsystem**：完整 compute 链，输出 **离屏** `gsColor`（`STORAGE`→`SAMPLED`）与 `gsDepth`（同上）。  
5. **HybridCompositorPass**（全屏三角）：  
   - 采样 **场景 HDR**（已光照颜色）、**场景深度**（GBuffer）、**GS 颜色/alpha**、**GS 深度**。  
   - 在同一深度域比较后决定每个像素取场景、取 GS 或混合。  
6. **VulkanPostProcessPass → Present**：保持现有 tone mapping / 呈现路径；Compositor 输出应接到 Post 的输入纹理链上。

#### 4.2.2 GS 深度写出语义（对齐 Godot，补齐 GW 清单 §3.1）

Godot / GW 文档指出：**用于遮挡的 GS 深度不宜采用重叠 splat 的加权平均**，否则相机运动下易漂移；应改为 **front-to-back 累积透明度达到阈值时，记录「首个有效命中」深度**（推送常量 `depth_capture_alpha`，建议初值约 `0.4 ~ 0.6`，可先 `0.5`）。

**本仓库落地**：在 `gs_render.comp`（或等价的 tile render compute）中：

- 增加 `depth_capture_alpha`（uniform 或 push constant）。  
- 将当前若存在的「平均式」深度累积改为：**当 `(1 - T) >= depth_capture_alpha` 时写入当前 splat 的深度并停止更新深度**（与文档中 Godot 描述一致）；空像素用 **远平面一致** 的哨兵值（与合成 shader 约定一致，例如 `1.0` 或 `far` 映射值）。  

**结论**：策略 B 的稳定性 **强依赖** 这一步；仅做合成侧比较而不改 GS 深度语义，收益会打折扣（GW 对照表 §2 已说明差距）。

#### 4.2.3 Compositor 内深度比较（对齐 Godot，补齐 GW 清单 §3.2）

参考 `gaussian_composite.glsl` 的思路，在本项目新建 **合成 fragment**（名称可如 `hybrid_gs_composite.frag`）：

1. **同域深度**：将 **GBuffer 深度** 与 **GS 深度** 都变换到 **view-space 线性深度**（或严格同一非线性编码 + 同一投影/near/far）；**禁止**一侧线性一侧未说明的 `gl_FragCoord.z` 混搭。  
   - 可利用与 `VulkanLightingPass::SetDeferredFrameMatrices` 已上传的 **`inverseViewProj` + `clipInfo`（zNear/zFar/分辨率）** 一致的一套反投影，避免与延迟光照「看世界坐标」的语义分叉。  
2. **拒绝与门控**（建议与 GW 清单参数对齐）：  
   - `depth_bias`：初始约 `0.01 ~ 0.05`（按场景尺度调）。  
   - `depth_test_min_alpha`：仅当 `gs.a >= depth_test_min_alpha` 才参与深度比较，抑制薄雾/低 alpha 区域的抖动（初始约 `0.03 ~ 0.08`）。  
   - `alpha_cutoff`：丢弃极低 alpha 贡献（初始约 `0.005 ~ 0.02`）。  
   - 在统一深度域内：**若 `gsDepth > sceneDepth + depth_bias`**，则 **拒绝 GS**（场景更近）；否则按预乘/straight 与引擎约定混合。  
3. **Alpha 语义**：全链路固定 **straight 或 premultiplied** 之一，与 `gs_render` 输出及 Compositor `mix` 一致（GW 清单 §3.2 强调与 `GS_COLOR_PREMULTIPLIED` 一致）。

#### 4.2.4 与 Godot「PRE_TRANSPARENT」阶段的对应关系

Godot 在 `EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT` 合成，以便与透明队列协调。本仓库延迟路径 **无独立透明子 pass** 时，**逻辑等价位** 为：**Lighting 完成之后、Post/Tone mapping 之前**。若后续引入 forward transparent，可将 Compositor 再前移或拆成两阶段（先处理 opaque lighting 结果，再与透明合成），此处以当前 `VulkanRenderer` 链为准。

---

### 4.3 策略 C — 光照阶段融合 GS（长期 / 研究向）

扩展 `VulkanLightingPass` 的 fragment，在 deferred 方程中叠加 GS 辐射项。画质潜力高，但改动面大；**在策略 B 与 Godot 参数模型稳定前不建议并行推进**。

---

## 5. Godot / GW 最小清单 → GTiny 工程映射表

下表将 `GaussianSplat_Godot_Analysis_And_GW_Minimal_Checklist.md` 中的条目映射到本仓库预期修改点（路径随实现微调）。

| 清单项（Godot / GW） | 目的 | GTiny 建议落点 |
|---------------------|------|----------------|
| `depth_capture_alpha`，首层命中深度 | GS 深度稳定、少漂移 | `resources/shaders/vk/gs_render.comp` + `GSRenderPushConstants` 或 UBO 扩展 |
| `depth_bias` / `depth_test_min_alpha` / `alpha_cutoff` | 合成边界稳定、少翻转 | 新建 `hybrid_gs_composite.frag` + 小 UBO / push constants |
| 深度比较统一为 **view 线性深度** | 避免误判 | Compositor fragment；CPU 侧与 `Camera` near/far、Vulkan reverse-Z 约定一致 |
| CPU 上传调参 | 调优与回归 | `HybridApplication` 或 Compositor 封装类；Debug UI 可选 |
| 相机/投影与场景深度同源 | 对齐几何与 GS | §6；与 `inverseViewProj` 同源推导 |
| 验证场景与相机运动 | 防回归 | Demo 内固定 mesh 切 GS + 轨道/推拉/平移测试（清单 §3.5） |

---

## 6. 相机与 Uniform：必须统一的工程结论

当前 GS 与延迟路径对 **投影矩阵的构造与 Y 约定** 不一致。若不做统一，会出现整体错位、比例错误、「仅 GS 炸毛」或 **深度比较系统性错误**。

**结论**：

1. **单一真源**：以 `RenderContext` 中的 `Camera` 为唯一输入（eye / target / up / near / far / aspect / FOV）。  
2. **双路径输出**：  
   - **Mesh / GBuffer**：继续使用现有 `GetViewMatrix()` + `GetProjectionMatrix()`（含 Vulkan `proj[1][1]` 处理）。  
   - **GS UBO**：在 CPU 侧从同一相机显式构造与现 `GSComputeRendererImpl::updateUniforms()` 等价的 `GSUniformBufferCPU`，并 **显式验证** 与用于反投影 GBuffer 深度的矩阵 **属于同一套 near/far、NDC Z 约定**（清单 §3.4：reverse-z / 非 reverse-z、重映射不可混用）。  
3. **沿用 GS README 中的链路纪律**：`uv`/tile/sort 全链路不在 preprocess 做 Y 翻转；仅保留在 `gs_render.comp` 写输出前翻转；若 Compositor 采样的 **场景深度** 与 **GS 纹理** 的像素坐标有 Y 翻转差异，必须在 **同一空间** 校正（避免深度对上了颜色没对上）。  

---

## 7. 资源、图像布局与 Swapchain

当前 GS 在 `drawFrame` 末尾将 swapchain 迁到 `PRESENT_SRC`。在 Hybrid 中若 GS **不再写 swapchain**，而写离屏 RT：

**结论**：

- 为 GS 的 color / depth 分配 **STORAGE | SAMPLED**（compute 写后 barrier 到 **SHADER_READ_ONLY** 再进 Compositor）。  
- **GBuffer 深度**：Geometry/Lighting 后需 **可读**；若与 `VulkanLightingPass::Record` 内 `CmdTransitionForLightingRead` 冲突，Compositor 前应再查询实际 layout 并补 barrier（或引入 **depth 的只读 copy** 到 `R32_SFLOAT` 纹理以降低 layout 耦合）。  
- **禁止**在 GS compute、Lighting、Compositor 之间留下未定义 layout。  
- **Resize**：GS 附件与 `impl.extent`、GBuffer、`lightingTarget` 同步重建。  

---

## 8. 与 `RenderPassManager` / 图编排的衔接方式

策略 B 要求顺序：**Lighting 完成 →（可选 barrier）→ GS Compute → Compositor → Post → Present**。

1. **侵入式**：扩展 `VulkanDeferredPipeline::RecordFrame` 或在 `VulkanRenderer::EndFrame` 提交前，于 `deferredPipeline.RecordFrame` 之后追加 `RecordHybridGSAndComposite(...)`。  
2. **非侵入式**：使用 `SetVulkanPostProcessCallback` 的 **前置**链式包装：先执行 GS + Compositor，再调用原 Post；需小心 **command buffer 内顺序** 与 **中间 RT** 是否已分配。

**结论**：Compositor 必须拿到 **lighting HDR + GBuffer depth + GS textures**；优先在 **单条 `commandBuffer`** 内顺序录制，便于 barrier 与调试。

---

## 9. 渐进落地路线（与 Godot/GW 清单对齐的里程碑）

| 阶段 | 交付物 | 验收 |
|------|--------|------|
| M0 | 抽出无窗口 `GSComputeSubsystem` + `Record`；GS 输出离屏 RT | 无 Mesh 亦可验证 GS 输出纹理 |
| M1 | `VulkanRenderer` + Mesh + **策略 A** 叠加 | 同屏可见；相机统一无相对漂移 |
| M2 | **`depth_capture_alpha` 首层深度**写入 `gs_render.comp` | 单测 GS：运动相机下 GS 深度噪声较「平均深度」明显减小 |
| M3 | **HybridCompositorPass**：view-depth 比较 + `depth_bias` / `depth_test_min_alpha` | Mesh 横切 GS：遮挡关系稳定；边缘少翻转 |
| M4 | CPU 调参、固定验证场景、参数扫值固化基线 | 完成清单 §3.5 验证项 |
| M5 | （可选）策略 C、性能优化 | 视需求 |

**推荐最小落地顺序**（与清单 §4 一致，映射到本仓库）：**M2 → M3 → 深度域统一（M3 内）→ CPU 参数（M4）→ 相机运动验证**。

---

## 10. 风险与已知维护点

- **排序缓冲区扩容**：保持 **sortK/sortV 对称 barrier**（混合后帧结构更复杂，更忌遗漏）。  
- **同帧同步**：GS 与 deferred 共用 command buffer 顺序时，避免 buffer/image 竞争。  
- **深度哨兵与 clear**：GBuffer 深度 clear 与 GS 空像素哨兵须在 Compositor 中 **分支一致**，避免整屏误拒或误通过。  
- **调试开关**：「仅场景 / 仅 GS / 合成 / 深度可视化」四态便于对照 Godot 行为。  

---

## 11. 总结性结论（架构层面）

1. **首选集成形态** 与 Godot `gdgs` 对齐：**离屏 GS（颜色 + 稳定深度）+ 后阶段 Compositor**，其中 Compositor 对应本项目的 **策略 B**：读 **GBuffer 深度** 与 **GS 深度**，在 **view 线性深度域** 比较并配合 **bias / min-alpha / cutoff**。  
2. **稳定性关键** 不在「多写一个 if」，而在 **GS 深度语义（首层命中）** 与 **合成侧同域比较** 两件套；二者来自 `GaussianSplat_Godot_Analysis_And_GW_Minimal_Checklist.md` 的核心结论，应写入实现规范而非可选优化。  
3. **策略 A** 仅作管线打通与矩阵/barrier 验证；**产品级 Hybrid 以策略 B 为完成定义**。  
4. **与现有引擎对齐**：复用 `VulkanGBuffer`、`VulkanDeferredPipeline`、HDR/Post 链；避免第二窗口与第二套 `InitializeWindow`。

按上述分层、帧序与 Godot/GW 参数模型扩展 `VK_HybridGSRenderDemo`，可在延迟 Mesh 与 3DGS 之间获得 **相机运动下更稳定** 的遮挡与合成效果。
