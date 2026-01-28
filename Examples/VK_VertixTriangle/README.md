# 记录实践过程中的坑
Learning link: https://easyvulkan.github.io/index.html

## Ch1-4 创建交换链
1. 根据教程coding，CreateSwapchain 内先调 vkCreateSwapchainKHR，再调 CreateSwapchain_Internal（里面也调 vkCreateSwapchainKHR）实现设计不合理。
- 原因：重复创建交换链，逻辑混乱，也容易资源泄露或与 oldSwapchain 的处理冲突。在编译的时候，CreateSwapchain_Internal（里面也调 vkCreateSwapchainKHR）就失败了。
- 优化：彻底把 “创建 swapchain 句柄” 移出 _Internal，只在 CreateSwapchain / RecreateSwapchain 里调用 vkCreateSwapchainKHR，_Internal 只做“基于现有 swapchain 的后处理”。 优化之后，程序就运行通过了。