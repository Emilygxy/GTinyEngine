# VK_GSRenderDemo

`VK_GSRenderDemo` 是一个基于 Vulkan Compute 的 3D Gaussian Splatting 演示程序，目标是作为 `3DGS` 目录移除后的独立实现。

当前代码结构已经拆分为职责清晰的三层：

- `GSSceneLoader`：负责场景数据加载与格式转换（PLY -> `GSVertex`）。
- `GSComputeRenderer`：负责 Vulkan 资源、compute pipeline、每帧调度与呈现。
- `GSRenderDemoApp`：负责组装生命周期（load -> render -> shutdown）。

---

## 运行参数

程序入口：

- `Examples/VK_GSRenderDemo/source/main.cpp`

命令行参数：

- `VK_GSRenderDemo.exe [ply_path]`
- 若传入 `ply_path`：加载该 3DGS PLY。
- 若不传：默认尝试加载 `Examples/VK_GSRenderDemo/assets/cloudpoints/demo.ply`（并兼容从 `build/bin/<Config>` 启动时的相对路径）。

示例：

- `VK_GSRenderDemo.exe`
- `VK_GSRenderDemo.exe "E:/datasets/garden/point_cloud.ply"`

---

## 相机操作

当前相机支持 `FreeLook` / `Orbit` 两种模式，可在运行时切换，窗口标题会显示当前模式后缀（`[FreeLook]` 或 `[Orbit]`）。

- `Tab`：切换相机模式（`FreeLook` <-> `Orbit`）。
- `R`：重置相机到初始化时“聚焦模型”的状态（位置、朝向、FOV、近远平面）。

`FreeLook` 模式：

- `W/A/S/D` 或 `↑/↓/←/→`：前后左右移动相机。
- 鼠标左键拖拽：旋转相机朝向（第一人称视角）。
- 鼠标中键拖拽 / 鼠标滚轮：缩放视角（修改 FOV）。

`Orbit` 模式：

- `W/A/S/D` 或 `↑/↓/←/→`：绕模型目标点旋转观察（而非平移）。
- `Q/E`：增大/减小轨道半径（拉远/拉近观察距离）。
- 鼠标左键拖拽：绕目标点旋转观察模型。
- 鼠标中键拖拽 / 鼠标滚轮：缩放视角（修改 FOV）。

**注意**：`FreeLook` 模式 和 `Orbit` 模式 相互切换时会重置视角。

---

## 数据格式（PLY 输入约定）

读取逻辑位于：

- `Examples/VK_GSRenderDemo/source/GSSceneLoader.cpp`

当前按二进制 payload 读取下列结构（顺序固定）：

- `position: vec3`
- `normal: vec3`（当前不参与渲染主流程）
- `shs: float[48]`
- `opacity: float`
- `scale: vec3`
- `rotation: vec4`

加载后会转换到 GPU 顶点结构 `GSVertex`：

- `position -> vec4(position, 1)`
- `scale_opacity.xyz = exp(scale.xyz)`
- `scale_opacity.w = sigmoid(opacity)`
- `rotation = normalize(rotation)`
- `shs` 按原 3DGS 约定重排到 `sh[48]`

> 说明：当前 loader 假设 PLY 头部描述和 payload 排列与训练导出一致，不做字段名重映射。

---

## Shader 与编译产物

Shader 源码：

- `resources/shaders/vk/gs_*.comp`
- 公共头：`resources/shaders/vk/gs_common.glsl`

构建时会编译到：

- `build/resources/compiled_shaders/*.spv`
- 运行目录拷贝：`build/bin/<Config>/resources/compiled_shaders/*.spv`

`GSComputeRenderer` 默认加载如下 SPIR-V：

- `gs_precomp_cov3d_comp.spv`
- `gs_preprocess_comp.spv`
- `gs_prefix_sum_comp.spv`
- `gs_preprocess_sort_comp.spv`
- `gs_hist_comp.spv`
- `gs_sort_comp.spv`
- `gs_tile_boundary_comp.spv`
- `gs_render_comp.spv`

---

## Compute Pipeline 时序图（每帧）

```text
+-----------------------+
| update uniforms       |
+-----------+-----------+
            |
            v
+-----------------------+
| preprocess            |  gs_preprocess.comp
| - project to screen   |
| - estimate aabb       |
| - count tile overlap  |
+-----------+-----------+
            |
            v
+-----------------------+
| prefix sum            |  gs_prefix_sum.comp (ping-pong)
| - overlap -> offsets  |
| - tail value => Ninst |
+-----------+-----------+
            |
            v
+-----------------------+
| preprocess_sort       |  gs_preprocess_sort.comp
| - expand gaussian/tile|
| - emit (key,payload)  |
+-----------+-----------+
            |
            v
+-----------------------+
| radix sort (8 passes) |
| hist + sort           |  gs_hist.comp + gs_sort.comp
+-----------+-----------+
            |
            v
+-----------------------+
| tile boundary         |  gs_tile_boundary.comp
| - build [start,end)   |
+-----------+-----------+
            |
            v
+-----------------------+
| render                |  gs_render.comp
| - per tile blend      |
| - output color/depth  |
|   /normal             |
+-----------+-----------+
            |
            v
      present swapchain
```

---

## 关键维护点

- **前缀和输入源切换**：`preprocess_sort` 需根据 prefix sum 最终落在 ping/pong 选择不同 descriptor set。
- **排序容量扩展**：当 `numInstances` 超出当前容量，需重建 sort/hist buffer 并刷新 descriptor。
- **窗口 resize 重建**：需重建 tile boundary buffer 与 depth/normal 输出 image。
- **纯 compute 输出**：swapchain image 与输出 storage image 依赖正确 layout barrier。
- **Y 轴翻转约定**：不要在 `gs_preprocess.comp` 修改 `uv` 的坐标语义（避免影响 `aabb/tile/sort` 一致性）；统一在 `gs_render.comp` 的最终 `imageStore` 前做输出翻转（`out_uv.y = height - 1 - curr_uv.y`）。

---

## 炸毛问题复盘（根因与最终修复）

### 现象

- `.ply` 模型表面出现强烈噪点/飞刺（俗称“炸毛”）。
- 在尝试通过 `preprocess` 阶段翻转 `uv.y` 修正画面方向时，炸毛会明显加重或复发。

### 根本原因

本问题本质是 **数据约定不一致 + 排序链路同步不完整** 的叠加结果，而不是单一 shader 数学公式错误。

1. **排序阶段同步不完整（关键）**
   - 在 radix sort 流程中，早期实现仅对 `sortK`（key）做了 barrier，遗漏了 `sortV`（payload/index）同步。
   - 结果是 key/value 可能短暂失配：tile key 排序看似正确，但 payload 指向错误高斯，最终在 render 阶段混入错误 splat，表现为“炸毛”。

2. **相机参数构建语义偏离 3DGS**
   - `VK_GSRenderDemo` 最初使用 `Camera::GetProjectionMatrix()` 语义直接喂给 preprocess。
   - 与 `3DGS` 的 `tan_fovx/tan_fovy + 手工构建 view/proj` 语义存在偏差，导致投影与协方差映射敏感度变差。

3. **Y 翻转位置错误会破坏分桶一致性**
   - `uv.y` 参与了 `aabb -> tile overlap -> preprocess_sort -> tile_boundary -> render` 全链路。
   - 若在 `preprocess` 阶段用 `-ndc.y` 翻转，会改变分桶/排序坐标语义，容易与其余阶段约定不一致，触发或放大“炸毛”。

### 最终解决方案

1. **补齐排序同步（对齐 3DGS）**
   - `preprocessSort` 后同时 barrier `sortKBufferEven` 与 `sortVBufferEven`。
   - 每轮 radix sort 后同时 barrier 输出 `sortK` 与 `sortV`（Odd/Even）。

2. **相机参数构建对齐 3DGS 计算语义**
   - `updateUniforms()` 中改为显式构建：
     - `view = lookAt(eye, target, up)`
     - `tan_fovx = tan(fov/2)`
     - `tan_fovy = tan_fovx * height / width`
     - `proj = perspective(atan(tan_fovy)*2, aspect, near, far)`
     - `proj_mat = proj * view`
   - 并保留与 3DGS 一致的矩阵分量翻转处理。

3. **仅在最终写屏阶段做 Y 翻转（关键实践）**
   - 保持 `preprocess` 中 `uv` 与分桶/排序空间一致（不做 `-ndc.y`）。
   - 在 `gs_render.comp` 写 `imageStore` 前做一次输出坐标翻转：
     - `out_uv.y = height - 1 - curr_uv.y`
   - 这样可同时保证：
     - 不炸毛（分桶与排序稳定）
     - 画面方向正确（不上下颠倒）

### 经验结论（建议长期遵循）

- **几何/分桶空间** 与 **最终显示空间** 要解耦：前者保持稳定一致，后者在最后一步做坐标适配。
- 对所有“成对缓冲”（如 sort key/value）必须做对称同步，避免“看似排序正常，实际 payload 乱序”的隐蔽错误。
- 修改 shader 后需确认运行时使用的是最新 `*.spv`，避免“源码已修复但二进制未更新”的假回归。

---

## 代码入口总览

- App 组装：`Examples/VK_GSRenderDemo/source/GSRenderDemoApp.cpp`
- 数据加载：`Examples/VK_GSRenderDemo/source/GSSceneLoader.cpp`
- 渲染核心：`Examples/VK_GSRenderDemo/source/GSComputeRenderer.cpp`
- 进程入口：`Examples/VK_GSRenderDemo/source/main.cpp`

