# GTinyEngine
A Simple Game Engine Project

## 注意事项
### 1. 关于Vulkan环境
若VK 实例化的时候，提示验证层不可用，则应先安装Vulkan SDK，从官网下载：https://vulkan.lunarg.com/
或者根据Cmake提示 运行环境配置的bat文件

## 开发进程记录
### 2025.01.01
- 实现基本库的开发搭建
- 完成了基本的窗口创建、事件处理、渲染循环、资源加载、渲染对象、渲染管线的基本实现。
### 2025.08.03
- 完成了 skybox 天空盒的实现和相机交互系统的实现。
### 2025.08.05
- 完成了 Light的实现和材质重构的实现。

### 2025.08.10
- 完成了 鼠标点选并集成 imgui 展示点选结果。

### 2025.09.12
- 完成了 MRT 多渲染目标的实现。融合skybox、物体的渲染和后处理效果的预备支持。

### 2026.01.14
- 在Camera和RenderView之间增加了观察者设计模式，使得RenderView可以观察Camera中的对象，随之变化渲染视口。

### 2026.01.16
- 新增了简单的多线程渲染的支持：通过简单实现典型的<生产-消费者>模型，实现了多线程渲染的支持。特别处理了在共享OpenGL上下文的情况下，多线程渲染结合UI的同步问题，可详见 [多线程渲染架构分析与改造方案](doc/multithreaded_rendering_architecture.md "多线程渲染架构分析与改造方案") 和 [ImGui 多线程渲染问题修复](doc/imgui_multithreaded_fix.md "ImGui 多线程渲染问题修复") 。