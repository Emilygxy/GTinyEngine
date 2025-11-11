#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>  // will include 'vulkan.h'

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>

const std::vector<const char*> validationLayers = {
	//"VK_LAYER_KHRONOS_validation",
	"VK_LAYER_NV_optimus",  // locally
	//"VK_LAYER_RENDERDOC_Capture",
	//"VK_LAYER_EOS_Overlay"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


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

	void createInstance();

	bool checkValidationLayerSupport();
	void setupDebugMessenger();
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

private:
	GLFWwindow* m_window{nullptr};

	uint16_t m_width;
	uint16_t m_height;

	//--------------
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;

};

