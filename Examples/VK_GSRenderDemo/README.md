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
- **Y 轴翻转约定**：`gs_preprocess.comp` 中屏幕投影使用 `uv.y = ndc2Pix(-ndc.y, height)`。原因是当前 `EasyVulkan + storage image -> swapchain present` 链路与原 `3DGS` 运行环境在屏幕原点方向约定上存在差异；若不翻转会出现“画面上下颠倒”。若后续替换呈现链路（例如改为 graphics pass / blit / 不同平台 WSI），请优先核对这一行，避免重复踩坑。

---

## 代码入口总览

- App 组装：`Examples/VK_GSRenderDemo/source/GSRenderDemoApp.cpp`
- 数据加载：`Examples/VK_GSRenderDemo/source/GSSceneLoader.cpp`
- 渲染核心：`Examples/VK_GSRenderDemo/source/GSComputeRenderer.cpp`
- 进程入口：`Examples/VK_GSRenderDemo/source/main.cpp`

