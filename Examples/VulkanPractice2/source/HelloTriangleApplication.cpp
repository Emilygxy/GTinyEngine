#include "HelloTriangleApplication.h"


HelloTriangleApp::HelloTriangleApp(uint16_t width , uint16_t height)
	:m_width(width)
	,m_height(height)
{
}

HelloTriangleApp::~HelloTriangleApp()
{
}

void HelloTriangleApp::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // not to create OpenGL context
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); 

	m_window = glfwCreateWindow(m_width, m_height, "Vulkan-HelloTriangle", nullptr, nullptr);
}

void HelloTriangleApp::initVulkan()
{
}

void HelloTriangleApp::mainLoop()
{
	if (!m_window)
	{
		return;
	}

	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
	}
}

void HelloTriangleApp::cleanUp()
{
	if (m_window)
	{
		glfwDestroyWindow(m_window);
	}

	glfwTerminate();
}

void HelloTriangleApp::run()
{
	initWindow();
	initVulkan();
	mainLoop();

	cleanUp();
}