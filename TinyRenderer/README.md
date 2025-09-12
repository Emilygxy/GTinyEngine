# TinyRenderer
## Introduction
This is a simple renderer that can render a 3D scene with a single triangle. It is written in C++ and uses OpenGL for rendering. The code is well commented and easy to understand.

## Features
- Single triangle rendering
- Simple lighting model
- Textured triangle
- Camera movement
- Mouse control
- Keyboard control
- Render Framework

## Usage
To use the renderer, you need to follow these steps:
1. Clone the repository
2. Open the project in your favorite IDE (e.g. Visual Studio)
3. Build the project
4. Run the project
5. Use the mouse and keyboard to control the camera and the triangle

## Render Framework
### EventHelper class
The EventHelper class is a singleton class that handles the input events. It provides a simple interface for registering and handling events. The class is implemented using the Meyer's Singleton pattern.  
Meyer's Singleton的优点：  
线程安全：C++11保证局部静态变量的初始化是线程安全的  
延迟初始化：只有在第一次使用时才创建实例  
自动清理：程序结束时自动销毁  
简洁：代码量少，易于维护  
性能好：没有锁开销  
使用方案1，你的EventHelper就变成了一个真正的单例，具有以下特性：  
✅ 全局唯一实例  
✅ 线程安全  
✅ 延迟初始化  
✅ 自动资源管理  
✅ 防止拷贝和赋值  
✅ 简洁的API  
这样实现后，我们可以在任何地方通过EventHelper::GetInstance()来访问EventHelper的实例。  