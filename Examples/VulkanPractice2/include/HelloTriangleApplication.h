#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>  // will include 'vulkan.h'

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>

class HelloTriangleApp
{
public:
	HelloTriangleApp(uint16_t width = 1280u, uint16_t height = 800u);
	~HelloTriangleApp();

	void run();

private:
	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanUp();

private:
	GLFWwindow* m_window{nullptr};

	uint16_t m_width;
	uint16_t m_height;
};

