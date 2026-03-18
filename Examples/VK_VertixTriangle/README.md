# 记录实践过程中的坑
**Learning link:** https://easyvulkan.github.io/index.html
**学习路径**：
前两章是Step By Step入门教程，很长，初学者请耐心看完。 
然后直接阅读Ch6和Ch7（建议先七后六），按要求阅读Ch3~5。

## Ch1-4 创建交换链
1. 根据教程coding，CreateSwapchain 内先调 vkCreateSwapchainKHR，再调 CreateSwapchain_Internal（里面也调 vkCreateSwapchainKHR）实现设计不合理。
- **原因**：重复创建交换链，逻辑混乱，也容易资源泄露或与 oldSwapchain 的处理冲突。在编译的时候，CreateSwapchain_Internal（里面也调 vkCreateSwapchainKHR）就失败了。
- **优化**：彻底把 “创建 swapchain 句柄” 移出 _Internal，只在 CreateSwapchain / RecreateSwapchain 里调用 vkCreateSwapchainKHR，_Internal 只做“基于现有 swapchain 的后处理”。 优化之后，程序就运行通过了。

## Ch2-2 创建渲染通道和帧缓冲
1. 根据教程coding，运行时在main()的crash 在了 commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        /* start the render pass */ 
        renderPass.CmdBegin(commandBuffer, framebuffers[i], { {}, windowSize }, clearColor); 的std::vector 地方。
- **原因**：问题在于初始化顺序：InitializeWindow() 先创建交换链并触发回调，但 CreateRpwf_Screen() 在之后才注册回调，导致 framebuffers 为空。
- **优化**：在 CreateRpwf_Screen() 中，如果交换链已创建，注册回调后立即创建 framebuffers。

## Ch2-3 创建管线并绘制三角形
1. 根据教程coding，程序运行时，crash在shaderModule(const char* filepath /*VkShaderModuleCreateFlags flags*/) {
            Create(filepath);
        } 的~result_t()中。
- **原因**：
- result_t 析构函数检查 if (uint32_t(result) < VK_RESULT_MAX_ENUM)，当 result == VK_RESULT_MAX_ENUM 时条件为 false，会抛出异常;
- 在 shaderModule(const char* filepath) 构造函数中，如果 Create(filepath) 返回 VK_RESULT_MAX_ENUM（文件打开失败），临时 result_t 对象在语句结束时析构，此时抛出异常,
构造函数未捕获异常，导致程序崩溃。
- **优化**：
- 修改 result_t 的析构函数逻辑，将 VK_RESULT_MAX_ENUM 视为特殊情况，不抛出异常：
VK_SUCCESS：成功，不抛出
VK_RESULT_MAX_ENUM：无效/未指定的错误码，不抛出（关键修复）
其他值 >= VK_RESULT_MAX_ENUM：无效，不抛出
有效的错误码：抛出异常
这样，当 shader 文件打开失败时，不会因 VK_RESULT_MAX_ENUM 而抛出异常，避免崩溃。

2. 根据教程coding，程序运行时，运行程序时，crash在pipeline::Create()中的vkCreateGraphicsPipelines()调用中。
- **原因**：
- shader 文件没找到
- **优化**：
- 调整shader文件路径->放在项目输出bin目录下（***\GTinyEngine\build\bin\Debug\resources）就好了。
- PS: GTinyEngine\build\resources\compiled_shaders目录是构建项目时编译shader的中间位置。
