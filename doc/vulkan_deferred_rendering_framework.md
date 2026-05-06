# GTinyEngine Vulkan Deferred Rendering 实现框架设计

## 1. 目标与结论

你当前的问题是：**Vulkan 的 G-Buffer（多 MRT）+ Lighting Pass 应该放在 `GTVulkan` 还是 `framework`？**

结论是采用分层方案：

- `GTVulkan`：只做 Vulkan 细节封装与资源生命周期管理（RenderPass/Framebuffer/Pipeline/Descriptor/Barrier 等）。
- `framework`：做渲染语义层编排（GeometryPass、LightingPass、PostProcess、RenderGraph 依赖与调度）。
- `TinyRenderer/Renderer`：作为后端无关入口，按后端选择 OpenGL 或 Vulkan 的 pass executor。

这能保证：

- `GTVulkan` 维持“通用 Vulkan 抽象层”，不被具体渲染管线绑死。
- `framework` 统一表达“延迟渲染流程”，便于后续支持 Forward+/Clustered/PBR。
- 现有 OpenGL 多 pass 思路可以复用到 Vulkan 版本，降低迁移成本。

---

## 2. 当前现状（与你现有代码的关系）

- `GTVulkan` 已有基础对象封装（`RenderPass`、`Framebuffer`、`pipeline` 等），但默认 `easy_vk::CreateRpwf_Screen()` 是单 color attachment 屏幕路径。
- `framework` 已有 Geometry/Base/PostProcess/Skybox 的多 pass 语义，但目前实现主要走 OpenGL API（`gl*` 调用）。

所以当前最合理的方向不是“把整套 deferred 都塞进 GTVulkan”，而是：

1. 在 `GTVulkan` 补齐 Vulkan deferred 所需基础能力；
2. 在 `framework` 新增 Vulkan 后端的 pass 实现与 RenderGraph 连接。

---

## 3. 分层职责设计

## 3.1 GTVulkan（底层）

建议新增/完善以下能力（只负责 Vulkan 资源与命令，不写业务渲染流程）：

- `VkImage`/`VkImageView`/`VkDeviceMemory` 的 RAII 纹理附件对象（Color/Depth）。
- 多附件 framebuffer 构建工具（支持 4+ attachments + depth）。
- descriptor set builder（GBuffer 纹理采样绑定最常用）。
- 通用 barrier/layout 转换助手（ColorAttachment -> ShaderReadOnly）。
- fullscreen triangle/quad 的最小绘制支持（可放工具层）。

建议目录（示意）：

- `GTVulkan/include/GTVulkan/VK_Image.h`
- `GTVulkan/source/VK_Image.cpp`
- `GTVulkan/include/GTVulkan/VK_Descriptor.h`
- `GTVulkan/source/VK_Descriptor.cpp`
- `GTVulkan/include/GTVulkan/VK_RenderTargets.h`
- `GTVulkan/source/VK_RenderTargets.cpp`

## 3.2 framework（流程层）

在 `framework` 定义 Vulkan 版本的 Pass 语义对象与依赖关系：

- `VkGeometryPass`：写 GBuffer MRT。
- `VkLightingPass`：读 GBuffer（sampled image），输出 `LightingColor`。
- `VkPostProcessPass`：读 `LightingColor` 输出到 swapchain 或中间 RT。
- `VkPresentPass`：最终 blit/compose 到 swapchain（可与 Post 合并）。

建议目录（示意）：

- `include/framework/vulkan/VkRenderPassBase.h`
- `include/framework/vulkan/VkGeometryPass.h`
- `include/framework/vulkan/VkLightingPass.h`
- `source/framework/vulkan/*.cpp`

`RenderPassManager` / `RenderGraph` 保持统一接口，仅在执行层分派到 Vulkan backend。

---

## 4. Vulkan Deferred 最小可行管线（MVP）

## 4.1 GBuffer 附件建议

建议先做 4 附件：

1. `gAlbedo`：`VK_FORMAT_R8G8B8A8_UNORM`
2. `gNormal`：`VK_FORMAT_A2B10G10R10_UNORM_PACK32` 或 `VK_FORMAT_R16G16B16A16_SFLOAT`
3. `gMaterial`：`VK_FORMAT_R8G8B8A8_UNORM`（roughness/metallic/ao/flags）
4. `gDepth`：depth attachment（`VK_FORMAT_D32_SFLOAT` 或 `D24_UNORM_S8_UINT`）

说明：

- 如需要 world position，推荐先用 depth + inverse VP 在 lighting pass 重建，减少带宽。
- MVP 阶段可先不做 MSAA；后续再加入 resolve 方案。

## 4.2 Pass 顺序

单帧命令流建议：

1. `GeometryPass`  
   - render pass A：多 MRT + depth 写入。
2. barrier  
   - GBuffer attachments: `COLOR_ATTACHMENT_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL`
   - depth: `DEPTH_STENCIL_ATTACHMENT_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL`（若采样深度）
3. `LightingPass`  
   - render pass B：输出 `LightingColor`（或直接 swapchain）。
4. `PostProcessPass`（可选）  
   - tone mapping / FXAA / bloom combine。
5. `Present`

---

## 5. 两种实现路线：推荐先走路线 A

## 路线 A：双 RenderPass（推荐）

- `GeometryPass` 与 `LightingPass` 分离，之间用显式 barrier。
- 优点：实现简单、调试容易、与 RenderGraph 语义一致。
- 缺点：中间写回显存更多。

## 路线 B：单 RenderPass 多 Subpass（进阶）

- 在一个 Vulkan render pass 中用 subpass input attachments。
- 优点：tile-based GPU 可能更快。
- 缺点：复杂度高，后处理与扩展性受限。

建议先 A 后 B：先跑通功能，再针对移动端/性能瓶颈评估 subpass 化。

---

## 6. 关键接口草案（建议）

以下仅为设计草案，便于你后续落代码。

```cpp
// framework 层：后端无关语义
struct DeferredFrameData {
    Mat4 view;
    Mat4 proj;
    Mat4 invViewProj;
    Vec3 cameraPos;
    LightData mainLight;
};

class IRenderBackend {
public:
    virtual void ExecuteDeferred(const SceneView& scene, const DeferredFrameData& frame) = 0;
};
```

```cpp
// Vulkan backend：资源集合
struct VulkanGBuffer {
    vk::Image albedo;
    vk::Image normal;
    vk::Image material;
    vk::Image depth;
    vk::Framebuffer framebuffer;
    VkExtent2D extent{};
};

class VkDeferredRenderer {
public:
    bool Initialize(VkExtent2D extent);
    void Resize(VkExtent2D extent);
    void Render(VkCommandBuffer cmd, const SceneView& scene, const DeferredFrameData& frame);
    void Shutdown();
private:
    void BuildGBufferResources();
    void BuildGeometryPipeline();
    void BuildLightingPipeline();
};
```

---

## 7. 与现有工程的落地映射

## 7.1 GTVulkan 需要补齐

- image attachment 创建/销毁封装（当前主要是 buffer/renderpass/pipeline 封装）。
- descriptor set 分配与更新工具（lighting pass 会频繁用）。
- 统一的 image barrier helper（简化每帧命令录制代码）。

## 7.2 framework 需要新增

- Vulkan 版 `Geometry/Lighting/PostProcess` pass 类（不要复用 OpenGL `gl*` 实现）。
- RenderGraph 节点可绑定 backend-specific executor。
- 材质系统补 Vulkan shader 变体（`geometry.vs/fs` 的 SPIR-V 版本与 descriptor layout 对齐）。

## 7.3 不建议做的事

- 不建议把“GeometryPass/LightingPass 业务逻辑”放到 `GTVulkan`。
- 不建议在 `framework` 直接写大量原始 Vulkan 资源管理代码（会破坏分层）。

---

## 8. 里程碑计划（可执行）

## M1：跑通最小 deferred（1-2 周）

- 新增 `VulkanGBuffer` 资源与 resize。
- Geometry pass 写 `albedo + normal + depth`（先不加 material）。
- Lighting pass 全屏三角形读取 GBuffer 输出到 swapchain。
- 单方向光 + 无阴影。

验收标准：

- 可看到与 forward 接近的基础光照结果。
- 窗口 resize 后资源重建正确，无验证层报错。

## M2：接入 framework RenderGraph（1 周）

- 在 `RenderPassManager` 注册 Vulkan deferred pass 节点。
- 实现跨 pass 资源句柄传递（texture/view/sampler）。
- OpenGL 与 Vulkan 共用同一套 pass 依赖拓扑定义。

验收标准：

- 可通过配置切换后端和渲染路径（forward/deferred）。

## M3：完善材质与后处理（1-2 周）

- 加入 `gMaterial`（metallic/roughness/ao）。
- 增加 tone mapping、gamma、可选 FXAA。
- 加入 GPU profiler 标记与统计。

---

## 9. 风险与规避

- **同步风险**：layout/barrier 错误最常见。  
  规避：统一 barrier helper + validation layer + RenderDoc 检查。

- **描述符复杂度**：材质与 pass 共用资源时易乱。  
  规避：先固定 lighting descriptor layout，再做材质扩展。

- **重建时机**：swapchain resize 导致 GBuffer 尺寸变化。  
  规避：统一 `OnResize()`，集中销毁重建顺序。

---

## 10. 最终建议（一句话）

**Deferred 的“流程编排”放 `framework`，Deferred 的“Vulkan 基础设施”放 `GTVulkan`；先做双 render pass MVP 跑通，再考虑 subpass 优化。**

