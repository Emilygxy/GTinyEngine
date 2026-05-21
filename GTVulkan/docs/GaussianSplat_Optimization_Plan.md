# GaussianSplat（GTVulkan）优化方案

> 目的：对照仓库内 **`3DGS/`** 参考实现的设计分层与资源策略，说明当前 **`GTVulkan/source/GaussianSplat/`** 路径下实现的不足，并给出**可选优化方向**与**实施阶段**，便于你按优先级取舍。  
> 背景现象：`VK_GSRenderDemo` / `VK_HybridGSRenderDemo` 在 **RenderDoc 抓帧重放** 时出现 `VK_ERROR_OUT_OF_DEVICE_MEMORY`，控制台可见 `deviceMemory` 分配失败（`-2`）；除工具开销外，**实现侧峰值显存与每帧同步成本偏高**会放大该问题。

---

## 1. 当前实现（GTVulkan）在做什么

| 方面 | 现状（概括） |
|------|----------------|
| 代码组织 | `GSComputeRenderer.cpp` 内 **单一大类** `GaussianSplatComputeEngine` 聚合：窗口/嵌入、命令池、**10 级**描述符布局、多 compute pipeline、排序缓冲、tile、多 swapchain 张 **StorageImage**、同步对象等。 |
| 与宿主关系 | `GSComputeSubsystem` 薄封装 + Hybrid 集成；**设备/队列/窗口** 与 `GraphicsBase` / `easy_vk` 强耦合。 |
| 内存 | GS 核心缓冲与 depth/normal/color **中间图** 已迁 **VMA**（`GaussianSplatGpu*`）；仍与 `GraphicsBase` / `easy_vk` 共用设备与 staging；排序缓冲按 `n * sortBufferSizeMultiplier` **扩容时 `WaitIdle` + Recreate**；预处理 **每帧独立 submit + fence 等待**。 |
| 配置 | 无统一 `RendererConfiguration`；PLY 体量、校验层、最大 splat 等主要靠应用侧零散处理（如 Hybrid 的 `HYBRID_GS_MAX_SPLATS`）。 |

结论：**功能完整但“编排 + 资源 + 平台”挤在同一层**，与 `3DGS` 文档中的分层目标不一致，维护和做显存/同步优化都偏难。

---

## 2. `3DGS/` 参考框架里值得对齐的设计点

依据 `3DGS/docs/3dgs_cpp_architecture.md` 与源码结构：

| 层次 | 3DGS 做法 | 对 GTVulkan 的启示 |
|------|-----------|---------------------|
| **门面** | `VulkanSplatting` + `RendererConfiguration`（CLI/env 合并） | 抽 **公共配置**：最大 splat、是否校验层、预处理策略、排序缓冲上限等，避免各 Demo 各写一套。 |
| **编排** | `Renderer` 只管流程：`initialize` 里按步骤建管线/录命令；`draw` 里更新 uniform + 提交 | 把 **“每帧干什么”** 从巨型类里拆成 **可测试的步骤**（或状态机），便于做“仅相机变化时跳过预处理”等优化。 |
| **设备与内存** | `VulkanContext` + **VMA**（`vk_mem_alloc`） | 大场景下 **子分配与重用** 更友好，峰值与碎片通常优于大量独立 `vkAllocateMemory`；对 RenderDoc 重放也更可预期。 |
| **场景数据** | `GSScene`：`Buffer` 抽象，职责是 PLY → GPU | 与 `GSSceneLoader` 对齐：**加载 / GPU 上传 / 生命周期** 与 **每帧调度** 分离。 |
| **管线** | `ComputePipeline` 一条管线一类对象 | 将 preprocess / prefix / sort / tile / render **拆成独立小类型**，描述符更新范围更小，便于按需创建/销毁与单元化排查。 |

---

## 3. 问题归因（为何“笨重”且易 OOM）

1. **峰值显存**  
   - 顶点数 `n` 大时：`tileOverlap`、`prefixSum` 双缓冲、`sort*×2`、`sortHist`、`tileBoundary`、多 swapchain 的 **depth/normal/color storage** 同时存在。  
   - `ensureSortCapacity` 在倍数增长时会 **整段 Recreate** 多块 buffer，且 **`Recreate` 内 `WaitIdle`**，短期峰值与驱动重排压力都偏大。

2. **每帧 GPU/CPU 同步过重**  
   - 预处理路径 **每帧** `Submit + WaitForFences`（与 3DGS 里“一帧一管线序列”类似，但若可与主 command buffer 合并或做脏标记，可减少 **独立队列压力** 和 **Capture 中的 submit 数量**）。  
   - RenderDoc 对 **多 submit、多同步** 的捕获更重，重放时再分配镜像资源，更容易顶满显存。

3. **架构耦合**  
   - 嵌入模式与独立窗口共用一大类，**条件分支多**，难单独为“仅嵌入、无 swapchain 颜色”做 **更小的资源子集**（例如 Hybrid 不需要 standalone 的 semaphore 链等）。

4. **工具链**  
   - 大 PLY + 校验层 + RenderDoc 本身 = 显存与系统内存三重压力；**与实现质量相关，但不等于实现 bug**。

---

## 4. 优化方向总览（请你决策组合）

下列按 **收益 / 风险 / 工作量** 大致排序，可只选前几项做 MVP。

### A. 显存与缓冲策略（高优先级，针对 OOM / RenderDoc）

| 子项 | 做法 | 优点 | 代价 |
|------|------|------|------|
| **A1. 引入 VMA**（或统一分配器） | 与 `3DGS` 一致，用 `VmaAllocator` 管理 buffer/image | 降低碎片、便于预算与统计、大场景更稳 | 引入依赖、初始化顺序要接好现有 `GraphicsBase` |
| **A2. 排序缓冲预算上限** | 配置 `maxSortInstances`（或按显存档位），**禁止无界 multiplier**；超出时 **降质/抽稀** 或拒绝加载 | 峰值可证、RenderDoc 友好 | 极大场景下需产品策略（提示用户） |
| **A3. 延迟创建 / 共享** | 无 Hybrid 时不要创建 **normal** 输出；嵌入模式与桌面模式 **资源模板** 分离 | 减少常驻 image | 代码分支与测试矩阵增加 |
| **A4. Recreate 策略** | 避免在热路径频繁 `WaitIdle`；能 `vkCmdPipelineBarrier` + 同尺寸 reuse 则不 Recreate；或 **双缓冲** 交换 | 减少卡顿与捕获噪声 | 实现复杂度上升 |

### B. 每帧调度与预处理（中高优先级，针对 CPU + Capture 体积）

| 子项 | 做法 | 优点 | 代价 |
|------|------|------|------|
| **B1. 脏标记** | 仅当相机视图/投影变化或 `numInstances` 变化时跑 **完整 preprocess**；静止帧复用上一帧 `prefix` 结果（需严格正确性证明） | 大幅减少每帧 compute 与 submit | 需定义“何时失效” |
| **B2. 合并提交** | 在 **单条** graphics/compute 可接受的 command buffer 中串联 preprocess→sort→render（或两提交：pre+main），减少 fence 次数 | RenderDoc 捕获更轻 | 与现有 `GraphicsBase::SubmitCommandBuffer_Graphics` 契约要对齐 |
| **B3. 异步读回** | `totalSum` 用 **延迟一帧读回** 或 **GPU-driven indirect**（长期） | 减少 CPU 等 GPU | 改动大 |

### C. 架构拆分（中优先级，对齐 3DGS 文档、利于长期维护）

| 子项 | 做法 | 优点 | 代价 |
|------|------|------|------|
| **C1. `GaussianSplatContext`** | 从 `GraphicsBase` 接 device/queue/family，持有 VMA、descriptor pool **分池**（如：静态场景一池、每帧动态一池） | 边界清晰 | 与 `easy_vk` 融合工作量 |
| **C2. `GaussianSplatPipelines`** | 每个 compute stage 一个 `ComputePipeline` 式对象（可仿 `3DGS/src/vulkan/pipelines/`） | 与文档一致、单测友好 | 初期重构量大 |
| **C3. `GaussianSplatScene`** | 仅持有 vertex/cov3D/attribute 上传与容量 | 与 `GSSceneLoader` 解耦 | 接口迁移 |

### D. 配置与产品化（低～中工作量）

- 统一 **`GaussianSplatConfig`**：`maxSplats`、`enableValidation`、`preprocessEveryFrame`、`sortBufferMultiplierCap` 等（CLI/env 可参考 `3DGS` 的 `RendererConfiguration` + env 前缀）。  
- Demo 默认使用 **小 PLY** 或文档说明 RenderDoc 场景下的推荐参数。

### E. 与 Hybrid 的边界（若继续双路径）

- **嵌入路径** 最小化：无 swapchain 时不创建 `imageAvailable`/`renderFinished` 等与 present 相关的对象。  
- 明确 **“GS 不负责窗口”** 时由宿主提供 **extent 与 swapchain image count**，避免重复探测。

---

## 5. 建议实施路线（供拍板）

| 阶段 | 内容 | 预期效果 |
|------|------|----------|
| **P0（1～3 天）** | 配置化：`maxSplats`、排序倍数上限、日志；默认降低 demo 峰值；文档化 RenderDoc 使用说明 | 不改架构即可缓解 OOM |
| **P1（约 1～2 周）** | B1 脏标记 + 减少无意义 `WaitIdle`；A4 审慎调整 `Recreate` | 降 CPU/GPU 同步与捕获重量 |
| **P2（约 2～4 周）** | 引入 VMA（A1）+ 缓冲子系统拆分；pipeline 分文件/分类型 | 显存与可维护性结构性改善；**进行中**：`GaussianSplatVma.*`、`GaussianSplatGpuBuffer.*`、`GaussianSplatGpuImage.*` + `GSComputeRenderer` 接线 |
| **P3（长期）** | 对齐 `3DGS`：`VulkanContext` 式设备层 + `Renderer` 式编排 + 可选 B3 | 与参考实现同构，便于合并上游改进 |

---

## 6. 决策清单（你可直接勾选）

**当前拍板（与实现同步）：**

- [x] **引入 VMA**（与现有 `GraphicsBase` / `easy_vk` 并存；GS 缓冲与中间纹理走 `VmaAllocator`，见 `GaussianSplatVma.*`、`GaussianSplatGpuBuffer.*`、`GaussianSplatGpuImage.*`）。CMake：`option(GSVULKAN_USE_VMA …)`，代码：`GSVULKAN_USE_VMA`。  
- [x] **严格实时全量 splat**：核心路径不对 splat 数做硬截断；工具链/低端机可在 Demo 应用层限流。预留宏 `GSVULKAN_GS_STRICT_FULL_SPLAT`（默认 1）与文档对齐。  
- [ ] 预处理 **每帧** vs **相机/实例变化才跑（B1）** — 当前仍为每帧；B1 未启用。  
- [x] **已启动 P2**：VMA（A1）+ 缓冲/图像子模块拆分先行；compute **管线分文件/分类型** 可渐进迁移。  
- [x] **单引擎 + 宏/配置**：Hybrid 与 Standalone 共用 `GaussianSplatComputeEngine`（`GSComputeRenderer` / `GSComputeSubsystem`），不拆两套独立引擎。

---

## 7. 参考阅读顺序（与 3DGS 文档一致）

1. `3DGS/docs/3dgs_cpp_architecture.md`  
2. `3DGS/src/Renderer.h` / `Renderer.cpp`（`initialize` 步骤与 `draw` 流程）  
3. `3DGS/src/vulkan/VulkanContext.*`、`Buffer.*`、`ComputePipeline.*`  
4. 本仓库：`GTVulkan/source/GaussianSplat/GSComputeRenderer.cpp`（重点：`createBuffers`、`ensureSortCapacity`、`runPreprocessPass`、`drawFrame` / 嵌入路径）

---

## 8. 免责声明

- RenderDoc **重放 OOM** 与 **本机可用显存、校验层、捕获选项** 强相关；优化实现可降低 **应用侧峰值**，但无法保证在任意捕获设置下 100% 成功。  
- 任何 **B1/B3** 类省略计算都需 **画面正确性验证**（建议对比帧哈希或目视 + validation）。

---

*文档版本：与仓库 `3DGS/docs/3dgs_cpp_architecture.md` 及当前 `GTVulkan` GaussianSplat 实现对齐整理；具体排期与接口命名可在立项后细化。*
