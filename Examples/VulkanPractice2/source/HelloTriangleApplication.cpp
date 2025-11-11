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
	createInstance();
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

void HelloTriangleApp::createInstance()
{
	// optional, but for optimizer
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "GTinyEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	//neccassary, 
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	//
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;
	createInfo.enabledLayerCount = 0;
	VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("HelloTriangleApp::createInstance failed to create instance!");
	}
}

void HelloTriangleApp::run()
{
	initWindow();
	initVulkan();
	mainLoop();

	cleanUp();
}