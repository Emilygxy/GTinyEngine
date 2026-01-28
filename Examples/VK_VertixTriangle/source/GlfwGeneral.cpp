#include "GlfwGeneral.h"
#include <iostream>
#include <format> // only support in C++20
#include <sstream>
#ifdef _WIN32
#include "vulkan/vulkan_win32.h"
#endif
namespace easy_vk
{
    using namespace vk;

    GLFWwindow* pWindow = nullptr;
	GLFWmonitor* pMonitor = nullptr;
	const char* windowTitle = "EasyVK";
    bool InitializeWindow(VkExtent2D size, bool fullScreen, bool isResizable, bool limitFrameRate)
    {
        if (!glfwInit()) {
            std::cout << "[ InitializeWindow ] ERROR\nFailed to initialize GLFW!\n";
            return false;
        }

        // diferrent from OpenGL APP - glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        // In Vulkan, GLFW No OpenGL API.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, isResizable);

        // vk info
#ifdef _WIN32
        GraphicsBase::Base().AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
        GraphicsBase::Base().AddInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
        uint32_t extensionCount = 0;
        const char** extensionNames;
        extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
        if (!extensionNames) {
            std::cout << std::format("[ InitializeWindow ]\nVulkan is not available on this machine!\n");
            glfwTerminate();
            return false;
        }
        for (size_t i = 0; i < extensionCount; i++)
        {
            GraphicsBase::Base().AddInstanceExtension(extensionNames[i]);
        }
#endif
        GraphicsBase::Base().AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        // create the Vulkan instance before creating the window surface
        GraphicsBase::Base().UseLatestApiVersion();
        if (GraphicsBase::Base().CreateInstance())
        {
            return false;
        }

        // for using full- screen later
        pMonitor = glfwGetPrimaryMonitor();
        if (!pWindow)
        {
            if (fullScreen)
            {
                const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
                pWindow = glfwCreateWindow(pMode->width, pMode->height, windowTitle, pMonitor, nullptr);
            }
            else
            {
                pWindow = glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);
            }
        }

        if (!pWindow) {
            std::cout << "[ InitializeWindow ]\nFailed to create a glfw window!\n";
            glfwTerminate();
            return false;
        }

        // create the window surface
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (VkResult result = glfwCreateWindowSurface(GraphicsBase::Base().Instance(), pWindow, nullptr, &surface))
        {
            std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a window surface!\nError code: {}\n", int32_t(result));
            glfwTerminate();
            return false;
        }
        GraphicsBase::Base().Surface(surface);

        // through the || operation to execute the code
        if (// get the physical devices, and use the first physical device in the list, here we do not consider the case of replacing the physical device after any function fails
            GraphicsBase::Base().GetPhysicalDevices() ||
            //  one true and one false, temporarily do not need to calculate the queue used for compute operations
            GraphicsBase::Base().DeterminePhysicalDevice(true, 0, false) ||
            // create the logical device
            GraphicsBase::Base().CreateDevice())
        {
            return false;
        }

        return true;
    }

    void TerminateWindow()
    {
        /*chapter 1-4*/
        glfwTerminate();
    }

    void TitleFps() {
        static double time0 = glfwGetTime();
        static double time1;
        static double dt;
        static int dframe = -1;
        static std::stringstream info;
        time1 = glfwGetTime();
        dframe++;
        if ((dt = time1 - time0) >= 1) {
            info.precision(1);
            info << windowTitle << "    " << std::fixed << dframe / dt << " FPS";
            glfwSetWindowTitle(pWindow, info.str().c_str());
            info.str(""); //clear the stringstream
            time0 = time1;
            dframe = 0;
        }
    }
}