#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include "VK_Base.h"
#include "GLFW/glfw3.h"

namespace easy_vk 
{
	//window pointer, global variable automatically initialized to NULL
	extern GLFWwindow* pWindow;
	//display information pointer
	extern GLFWmonitor* pMonitor;
	//window title
	extern const char* windowTitle;

	bool InitializeWindow(VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true);
	void TerminateWindow();
	void TitleFps();
}
