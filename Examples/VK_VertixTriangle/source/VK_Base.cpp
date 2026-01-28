#include "VK_Base.h"
#include <format>
#include "glm/glm.hpp"

namespace vk
{
GraphicsBase::~GraphicsBase()
{
    if (!instance)
        return;

    if (device)
    {
        WaitIdle();
        if (swapchain)
        {
            ExecuteCallbacks(callbacks_destroySwapchain);
            for (auto& i : swapchainImageViews)
            {
                if (i)
                {
                    vkDestroyImageView(device, i, nullptr);
                }
            }
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        }
        ExecuteCallbacks(callbacks_destroyDevice);
        vkDestroyDevice(device, nullptr);
    }

    if (surface)
        vkDestroySurfaceKHR(instance, surface, nullptr);

    if (debugMessenger) 
    {
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (vkDestroyDebugUtilsMessenger)
            vkDestroyDebugUtilsMessenger(instance, debugMessenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
}

void GraphicsBase::AddLayerOrExtension(std::vector<const char*>& container, const char* name)
{
    for (auto& i : container)
        if (!strcmp(name, i)) //strcmp(...) returns 0 when strings match
            return;           //if the layer/extension name is already in the container, return
        container.push_back(name);
}
VkResult GraphicsBase::CreateDebugMessenger()
{
    static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback = [](
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)->VkBool32 {
            std::cout << std::format("{}\n\n", pCallbackData->pMessage);
            return VK_FALSE;
    };
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugUtilsMessengerCallback
    };

    //
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (vkCreateDebugUtilsMessenger) {
        VkResult result = vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger);
        if (result)
            outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n", int32_t(result));
        return result;
    }
    outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
    return VK_RESULT_MAX_ENUM;
}

VkResult GraphicsBase::GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue, bool enableComputeQueue, uint32_t(&queueFamilyIndices)[3])
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    if (!queueFamilyCount)
        return VK_RESULT_MAX_ENUM;
    std::vector<VkQueueFamilyProperties> queueFamilyPropertieses(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyPropertieses.data());
    auto& [ig, ip, ic] = queueFamilyIndices; // respectively correspond to graphics, presentation, and compute
    ig = ip = ic = VK_QUEUE_FAMILY_IGNORED;
    for (uint32_t i = 0; i < queueFamilyCount; i++) 
    {
        // these three VkBool32 variables indicate whether the corresponding queue family index can be obtained (i.e. should be obtained and can be obtained)
        // only get the queue family index that supports graphics operations when enableGraphicsQueue is true
        VkBool32 supportGraphics = enableGraphicsQueue && queueFamilyPropertieses[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        VkBool32 supportPresentation = false;
            // only get the queue family index that supports compute operations when enableComputeQueue is true
        VkBool32 supportCompute = enableComputeQueue && queueFamilyPropertieses[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
            // only get the queue family index that supports presentation when the window surface is created
        if (surface) 
        {
            if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation)) 
            {
                outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n", int32_t(result));
                return result;
            }
        }
        // if a queue family supports both graphics operations and compute operations
        if (supportGraphics && supportCompute) 
        {
            // if presentation is needed, the three queue family indices should be the same
            if (supportPresentation) 
            {
                ig = ip = ic = i;
                break;
            }
            // unless ig and ic are already obtained and the same, overwrite their values to i to ensure that the two queue family indices are the same
            if (ig != ic || ig == VK_QUEUE_FAMILY_IGNORED)
            {
                ig = ic = i;
            }
            // if presentation is not needed, then it can be broken
            if (!surface)
                break;
        }
        // if any queue family index can be obtained but not yet obtained, overwrite its value to i
        if (supportGraphics && (ig == VK_QUEUE_FAMILY_IGNORED))
            ig = i;
        if (supportPresentation && (ip == VK_QUEUE_FAMILY_IGNORED))
            ip = i;
        if (supportCompute && (ic == VK_QUEUE_FAMILY_IGNORED))
            ic = i;
    }
    // if any queue family index that is needed to be obtained is not yet obtained, the function execution fails
    if ((ig == VK_QUEUE_FAMILY_IGNORED) && enableGraphicsQueue ||
        (ip == VK_QUEUE_FAMILY_IGNORED) && surface ||
        (ic == VK_QUEUE_FAMILY_IGNORED) && enableComputeQueue)
    {
        return VK_RESULT_MAX_ENUM;
    }

    // when the function execution succeeds, write the obtained queue family indices to the member variables
    queueFamilyIndex_graphics = ig;
    queueFamilyIndex_presentation = ip;
    queueFamilyIndex_compute = ic;

    return VK_SUCCESS;
}
VkResult GraphicsBase::CreateSwapchain_Internal()
{
    // At this point, 'swapchain' is assumed to have been created successfully
    // by CreateSwapchain(...) or RecreateSwapchain(). This function is
    // responsible only for querying swapchain images and creating image views.

    // get the swapchain images
    uint32_t swapchainImageCount;
    if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr)) 
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: {}\n", int32_t(result));
        return result;
    }
    swapchainImages.resize(swapchainImageCount);
    if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data())) 
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", int32_t(result));
        return result;
    }

    // create the image views
    swapchainImageViews.resize(swapchainImageCount);
    VkImageViewCreateInfo imageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchainCreateInfo.imageFormat,
        //.components = {}, // all four members are VK_COMPONENT_SWIZZLE_IDENTITY
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    for (size_t i = 0; i < swapchainImageCount; i++) 
    {
        imageViewCreateInfo.image = swapchainImages[i];
        if (VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i])) 
        {
            outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: {}\n", int32_t(result));
            return result;
        }
    }

    return VK_SUCCESS;
}
void GraphicsBase::ExecuteCallbacks(std::vector<void(*)()> callbacks)
{
    for (size_t size = callbacks.size(), i = 0; i < size; i++)
    {
        callbacks[i]();
    }
}
void GraphicsBase::Terminate()
{
    this->~GraphicsBase();

    instance = VK_NULL_HANDLE;
    physicalDevice = VK_NULL_HANDLE;
    device = VK_NULL_HANDLE;
    surface = VK_NULL_HANDLE;
    swapchain = VK_NULL_HANDLE;
    swapchainImages.resize(0);
    swapchainImageViews.resize(0);
    swapchainCreateInfo = {};
    debugMessenger = VK_NULL_HANDLE;
}

void GraphicsBase::AddInstanceLayer(const char* layerName)
{
    AddLayerOrExtension(instanceLayers, layerName);
}
void GraphicsBase::AddInstanceExtension(const char* extensionName)
{
    AddLayerOrExtension(instanceExtensions, extensionName);
}
VkResult GraphicsBase::CreateInstance(VkInstanceCreateFlags flags)
{
    if constexpr (ENABLE_DEBUG_MESSENGER)
    {
        AddInstanceLayer("VK_LAYER_KHRONOS_validation");
        AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    VkApplicationInfo applicatianInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = apiVersion
    };

    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // must be the value here
        .flags = flags,
        .pApplicationInfo = &applicatianInfo,
        .enabledLayerCount = uint32_t(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = uint32_t(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data()
    };

    if (VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance)) {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
        return result;
    }
    // after the vulkan instance is created, output the vulkan version
    outStream  << std::format(
        "Vulkan API Version: {}.{}.{}\n",
        VK_VERSION_MAJOR(apiVersion),
        VK_VERSION_MINOR(apiVersion),
        VK_VERSION_PATCH(apiVersion));

    if constexpr (ENABLE_DEBUG_MESSENGER)
    {
        // after the vulkan instance is created, create the debug messenger
        CreateDebugMessenger();
    }

    return VK_SUCCESS;
}
VkResult GraphicsBase::CheckInstanceLayers(std::span<const char*> layersToCheck)
{
    uint32_t layerCount;
    std::vector<VkLayerProperties> availableLayers;
    if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr)) {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
        return result;
    }

    if (layerCount) {
        availableLayers.resize(layerCount);
        if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data())) 
        {
            outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: {}\n", int32_t(result));
            return result;
        }
        for (auto& i : layersToCheck) 
        {
            bool found = false;
            for (auto& j : availableLayers)
            {
                if (!strcmp(i, j.layerName)) {
                    found = true;
                    break;
                }
                if (!found)
                    i = nullptr;
            }
        }
    }
    else
    {
        for (auto& i : layersToCheck)
        {
            i = nullptr;
        }
    }

    return VK_SUCCESS;
}
VkResult GraphicsBase::CheckInstanceExtensions(std::span<const char*> extensionsToCheck, const char* layerName) const
{
    uint32_t extensionCount;
    std::vector<VkExtensionProperties> availableExtensions;

    if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr)) 
    {
        layerName ?
            outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName) :
            outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
        return result;
    }

    if (extensionCount)
    {
        availableExtensions.resize(extensionCount);
        if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data())) 
        {
            outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n", int32_t(result));
            return result;
        }
        for (auto& i : extensionsToCheck) 
        {
            bool found = false;
            for (auto& j : availableExtensions) 
            {
                if (!strcmp(i, j.extensionName))
                {
                    found = true;
                    break;
                }
                if (!found)
                    i = nullptr;
            }
        }
    }
    else
    {
        for (auto& i : extensionsToCheck) 
        {
            i = nullptr;
        }
    }

    return VK_SUCCESS;
}
void GraphicsBase::Surface(VkSurfaceKHR surface)
{
    if (!this->surface)
    {
        this->surface = surface;
    }
}
void GraphicsBase::AddDeviceExtension(const char* extensionName)
{
    AddLayerOrExtension(deviceExtensions, extensionName);
}
VkResult GraphicsBase::GetPhysicalDevices()
{
    uint32_t deviceCount;
    if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr))
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get the count of physical devices!\nError code: {}\n", int32_t(result));
        return result;
    }
    if (!deviceCount) 
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n");
        abort();
    }
    availablePhysicalDevices.resize(deviceCount);
    VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
    if (result)
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
    }
    
    return result;
}
VkResult GraphicsBase::DeterminePhysicalDevice(bool enableGraphicsQueue, uint32_t deviceIndex, bool enableComputeQueue)
{
    //define a special value to mark that a queue family index has been searched but not found
    static constexpr uint32_t notFound = INT32_MAX; //== VK_QUEUE_FAMILY_IGNORED & INT32_MAX
    // define the structure of the queue family index combination
    struct queueFamilyIndexCombination {
        uint32_t graphics = VK_QUEUE_FAMILY_IGNORED;
        uint32_t presentation = VK_QUEUE_FAMILY_IGNORED;
        uint32_t compute = VK_QUEUE_FAMILY_IGNORED;
    };
    //queueFamilyIndexCombinations is used to save a combination of queue family indices for each physical device
    static std::vector<queueFamilyIndexCombination> queueFamilyIndexCombinations(availablePhysicalDevices.size());
    auto& [ig, ip, ic] = queueFamilyIndexCombinations[deviceIndex];

    // if any queue family index has been searched but not found, return VK_RESULT_MAX_ENUM
    if (ig == notFound && enableGraphicsQueue ||
        ip == notFound && surface ||
        ic == notFound && enableComputeQueue)
        return VK_RESULT_MAX_ENUM;

    //if any queue family index should be obtained but not yet searched
    if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue ||
        ip == VK_QUEUE_FAMILY_IGNORED && surface ||
        ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue) {
        uint32_t indices[3];
        VkResult result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex], enableGraphicsQueue, enableComputeQueue, indices);
         //if GetQueueFamilyIndices(...) returns VK_SUCCESS or VK_RESULT_MAX_ENUM (vkGetPhysicalDeviceSurfaceSupportKHR(...) executes successfully but does not find all the required queue families),
        //the conclusion has been made for the queue family indices that should be obtained, and the result is saved to the corresponding variables in queueFamilyIndexCombinations[deviceIndex]
        //the queue family indices that should be obtained should still be VK_QUEUE_FAMILY_IGNORED, indicating that the corresponding queue family is not found, the value obtained by bitwise AND of VK_QUEUE_FAMILY_IGNORED（~0u） and INT32_MAX is equal to notFound
        if (result == VK_SUCCESS ||
            result == VK_RESULT_MAX_ENUM) {
            if (enableGraphicsQueue)
                ig = indices[0] & INT32_MAX;
            if (surface)
                ip = indices[1] & INT32_MAX;
            if (enableComputeQueue)
                ic = indices[2] & INT32_MAX;
        }
        //if GetQueueFamilyIndices(...) executes failed, return
        if (result)
            return result;
    }

    //if neither of the two if branches is executed, then all the queue family indices that should be obtained have been obtained, and the indices are obtained from queueFamilyIndexCombinations[deviceIndex]
    else {
        queueFamilyIndex_graphics = enableGraphicsQueue ? ig : VK_QUEUE_FAMILY_IGNORED;
        queueFamilyIndex_presentation = surface ? ip : VK_QUEUE_FAMILY_IGNORED;
        queueFamilyIndex_compute = enableComputeQueue ? ic : VK_QUEUE_FAMILY_IGNORED;
    }
    physicalDevice = availablePhysicalDevices[deviceIndex];
    
    return VK_SUCCESS;
}
VkResult GraphicsBase::CreateDevice(VkDeviceCreateFlags flags)
{
    float queuePriority = 1.f;
    VkDeviceQueueCreateInfo queueCreateInfos[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority } 
    };
    uint32_t queueCreateInfoCount = 0;
    if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
    {
        queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
    }
    if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED &&
        queueFamilyIndex_presentation != queueFamilyIndex_graphics)
    {
        queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
    }
    if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED &&
        queueFamilyIndex_compute != queueFamilyIndex_graphics &&
        queueFamilyIndex_compute != queueFamilyIndex_presentation)
    {
        queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_compute;
    }
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .flags = flags,
        .queueCreateInfoCount = queueCreateInfoCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledExtensionCount = uint32_t(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &physicalDeviceFeatures
    };
    if (VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device)) {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n", int32_t(result));
        return result;
    }
    if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
        vkGetDeviceQueue(device, queueFamilyIndex_graphics, 0, &queue_graphics);
    if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED)
        vkGetDeviceQueue(device, queueFamilyIndex_presentation, 0, &queue_presentation);
    if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED)
        vkGetDeviceQueue(device, queueFamilyIndex_compute, 0, &queue_compute);
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
    // output the name of the physical device
    outStream  << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);
    
    outStream  << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);
    ExecuteCallbacks(callbacks_createDevice);

    return VK_SUCCESS;
}
VkResult GraphicsBase::GetSurfaceFormats()
{
    uint32_t surfaceFormatCount;
    if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr)) {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: {}\n", int32_t(result));
        return result;
    }
    if (!surfaceFormatCount)
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to find any supported surface format!\n");
        abort();
    }

    availableSurfaceFormats.resize(surfaceFormatCount);
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data());
    if (result)
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get surface formats!\nError code: {}\n", int32_t(result));

    return result;
}
VkResult GraphicsBase::SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat)
{
    bool formatIsAvailable = false;
    if (!surfaceFormat.format) 
    {
        // if the format is not specified, only match the color space, the image format is whatever it is
        for (auto& i : availableSurfaceFormats)
            if (i.colorSpace == surfaceFormat.colorSpace) {
                swapchainCreateInfo.imageFormat = i.format;
                swapchainCreateInfo.imageColorSpace = i.colorSpace;
                formatIsAvailable = true;
                break;
            }
    }
    else
    {
        // otherwise match the format and color space
        for (auto& i : availableSurfaceFormats)
        {
            if (i.format == surfaceFormat.format &&
                i.colorSpace == surfaceFormat.colorSpace) {
                swapchainCreateInfo.imageFormat = i.format;
                swapchainCreateInfo.imageColorSpace = i.colorSpace;
                formatIsAvailable = true;
                break;
            }
        }
    }
    // if there is no matching format, there is a semantic error code
    if (!formatIsAvailable)
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    // if the swapchain already exists, call RecreateSwapchain() to rebuild the swapchain
    if (swapchain)
        return RecreateSwapchain();

    return VK_SUCCESS;
}

VkResult GraphicsBase::CreateSwapchain(bool limitFrameRate, VkSwapchainCreateFlagsKHR flags)
{
    // parameters related to VkSurfaceCapabilitiesKHR
    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
        return result;
    }

    //  for the number of swapchain images, it is better not to be too few to avoid blocking (blocking means that when a new image needs to be rendered, all swapchain images are either being read by the presentation engine or being rendered), but also not too many to avoid unnecessary memory overhead.
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + (surfaceCapabilities.maxImageCount > surfaceCapabilities.minImageCount);
    // if the size of the window surface is not determined, the width and height of surfaceCapabilities.currentExtent should both be the special value -1, so only the width is judged here
    swapchainCreateInfo.imageExtent =
                (surfaceCapabilities.currentExtent.width == -1) ?
                VkExtent2D{
                    glm::clamp(defaultWindowSize.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
                    glm::clamp(defaultWindowSize.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height) 
                } : surfaceCapabilities.currentExtent;
    // swapchainCreateInfo.imageArrayLayers = 1;
    // // specify the transformation mode
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    // specify the composite alpha
    // the composite alpha is determined by the surface capabilities
    if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    {
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    else
    {
        for (size_t i = 0; i < 4; i++)
        {
            if (surfaceCapabilities.supportedCompositeAlpha & 1 << i)
            {
                swapchainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(surfaceCapabilities.supportedCompositeAlpha & 1 << i);
                break;
            }
        }
    }
    // specify the image format
    // the image usage is determined by the surface capabilities
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    else
        outStream  << std::format("[ GraphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFER_DST_BIT isn't supported!\n");

    if (availableSurfaceFormats.empty())
    {
        if (VkResult result = GetSurfaceFormats())
        {
            return result;
        }
    }

    if (!swapchainCreateInfo.imageFormat) {
        // use the && operator to short-circuit execution
        if (SetSurfaceFormat({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) &&
            SetSurfaceFormat({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })) 
        {
            // if the above image format and color space combinations are not found, then whatever can be used is used, and the first group in availableSurfaceFormats is used
            swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
            swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
            outStream  << std::format("[ GraphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!\n");
        }
    }

    // specify the present mode
    // get the count of surface present modes
    // VK_PRESENT_MODE_IMMEDIATE_KHR: immediate mode
    // VK_PRESENT_MODE_MAILBOX_KHR: mailbox mode
    // VK_PRESENT_MODE_FIFO_KHR: FIFO mode
    uint32_t surfacePresentModeCount;
    if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr)) 
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError code: {}\n", int32_t(result));
        return result;
    }
    if (!surfacePresentModeCount)
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to find any surface present mode!\n");
        abort();
    }

    std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
    if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data())) 
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get surface present modes!\nError code: {}\n", int32_t(result));
        return result;
    }
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!limitFrameRate) {
        for (size_t i = 0; i < surfacePresentModeCount; i++)
        {
            if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) 
            {
                swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.flags = flags;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.clipped = VK_TRUE;

    // create the swapchain
    if (VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain)) 
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to create a swapchain!\nError code: {}\n", int32_t(result));
        return result;
    }

    // initialize images and image views for this swapchain
    if (VkResult result = CreateSwapchain_Internal())
    {
        return result;
    }

    // execute the callback functions, ExecuteCallbacks(...) see later
    ExecuteCallbacks(callbacks_createSwapchain);
    return VK_SUCCESS;
}
VkResult GraphicsBase::RecreateSwapchain()
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
    if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
        return result;
    }
    if (surfaceCapabilities.currentExtent.width == 0 ||
        surfaceCapabilities.currentExtent.height == 0)
    {
        return VK_SUBOPTIMAL_KHR;
    }
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.oldSwapchain = swapchain;

    VkResult result = vkQueueWaitIdle(queue_graphics);
    // only wait for the graphics queue if it succeeds, and only wait for the presentation queue if the graphics queue and the presentation queue are not the same
    if (!result && (queue_graphics != queue_presentation))
        result = vkQueueWaitIdle(queue_presentation);
    if (result) 
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: {}\n", int32_t(result));
        return result;
    }

    // destroy the old swapchain related objects
    // execute the callback functions, ExecuteCallbacks(...) see later
    ExecuteCallbacks(callbacks_destroySwapchain);
    for (auto& i : swapchainImageViews)
    {
        if (i)
        {
            vkDestroyImageView(device, i, nullptr);
        }
    }
    swapchainImageViews.resize(0);

    // create the new swapchain with updated create info
    if (VkResult createResult = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain))
    {
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to recreate a swapchain!\nError code: {}\n", int32_t(createResult));
        return createResult;
    }

    // re-initialize images and image views for the new swapchain
    if (VkResult initResult = CreateSwapchain_Internal())
        return initResult;

    // execute the callback functions, ExecuteCallbacks(...) see later
    ExecuteCallbacks(callbacks_createSwapchain);
    return VK_SUCCESS;
}
VkResult GraphicsBase::UseLatestApiVersion()
{
    if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
    {
        return vkEnumerateInstanceVersion(&apiVersion);
    }

    return VK_SUCCESS;
}
VkResult GraphicsBase::WaitIdle() const
{
    VkResult result = vkDeviceWaitIdle(device);
    if (result)
        outStream  << std::format("[ GraphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: {}\n", int32_t(result));
    return result;
}

VkResult GraphicsBase::RecreateDevice(VkDeviceCreateFlags flags)
{
    // destroy the existing logical device
    if (device) {
        if (VkResult result = WaitIdle();
            result != VK_SUCCESS &&
            result != VK_ERROR_DEVICE_LOST)
        {
            if (swapchain) 
            {
                // execute the callback functions, ExecuteCallbacks(...) see later
                ExecuteCallbacks(callbacks_destroySwapchain);
                // destroy the image views of the swapchain
                for (auto& i : swapchainImageViews)
                {
                    if (i)
                    {
                        vkDestroyImageView(device, i, nullptr);
                    }
                }
                swapchainImageViews.resize(0);
                // destroy the swapchain
                vkDestroySwapchainKHR(device, swapchain, nullptr);
                // reset the swapchain handle
                swapchain = VK_NULL_HANDLE;
                // reset the swapchain create information
                swapchainCreateInfo = {};
            }
        }
        
        ExecuteCallbacks(callbacks_destroyDevice);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    // create the new logical device
    return CreateDevice(flags);
}

VkResult GraphicsBase::SwapImage(VkSemaphore semaphore_imageIsAvailable)
{
    // Logic of the function:
    // if the swapchain is rebuilt in the current frame, then destroy the swapchain in the next frame (this is why break is used after rebuilding the swapchain to execute the while again, rather than recursively calling SwapImage(...))

    // destroy the old swapchain (if it exists)
    if (swapchainCreateInfo.oldSwapchain && (swapchainCreateInfo.oldSwapchain != swapchain))
    {
        vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
        swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    }

    // get the image index of the swapchain
    while (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_imageIsAvailable, VK_NULL_HANDLE, &currentImageIndex))
    {
        switch (result) 
        {
            case VK_SUBOPTIMAL_KHR:
            case VK_ERROR_OUT_OF_DATE_KHR:
                if (VkResult result = RecreateSwapchain())
                    return result;
                break; // note that after the swapchain is rebuilt, the image still needs to be obtained, through break recursion, again execute the condition judgment statement of the while
            default:
                outStream << std::format("[ GraphicsBase ] ERROR\nFailed to acquire the next image!\nError code: {}\n", int32_t(result));
                return result;
        }
    }
    return VK_SUCCESS;
}

result_t GraphicsBase::SubmitCommandBuffer_Graphics(VkSubmitInfo& submitInfo, VkFence fence) const
{
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkResult result = vkQueueSubmit(queue_graphics, 1, &submitInfo, fence);
    if (result)
        outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", int32_t(result));
    return result;
}

result_t GraphicsBase::SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkSemaphore semaphore_imageIsAvailable, VkSemaphore semaphore_renderingIsOver, VkFence fence, VkPipelineStageFlags waitDstStage_imageIsAvailable) const
{
    VkSubmitInfo submitInfo = {
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    if (semaphore_imageIsAvailable)
        submitInfo.waitSemaphoreCount = 1,
        submitInfo.pWaitSemaphores = &semaphore_imageIsAvailable,
        submitInfo.pWaitDstStageMask = &waitDstStage_imageIsAvailable;
    if (semaphore_renderingIsOver)
        submitInfo.signalSemaphoreCount = 1,
        submitInfo.pSignalSemaphores = &semaphore_renderingIsOver;
    return SubmitCommandBuffer_Graphics(submitInfo, fence);
}

result_t GraphicsBase::SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence) const
{
    VkSubmitInfo submitInfo = {
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    return SubmitCommandBuffer_Graphics(submitInfo, fence);
}

result_t GraphicsBase::SubmitCommandBuffer_Compute(VkSubmitInfo& submitInfo, VkFence fence) const
{
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkResult result = vkQueueSubmit(queue_compute, 1, &submitInfo, fence);
    if (result)
        outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", int32_t(result));
    return result;
}

result_t GraphicsBase::SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence) const
{
    VkSubmitInfo submitInfo = {
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    return SubmitCommandBuffer_Compute(submitInfo, fence);
}

result_t GraphicsBase::PresentImage(VkPresentInfoKHR& presentInfo)
{
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    switch (VkResult result = vkQueuePresentKHR(queue_presentation, &presentInfo)) {
    case VK_SUCCESS:
        return VK_SUCCESS;
    case VK_SUBOPTIMAL_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
        // this will cause the loss of 1 frame. To keep this 1 frame, it is necessary to get the image index of the swapchain and present the image after rebuilding the swapchain, considering that the temporary synchronization object is also created when getting the image index of the swapchain, the code will be written more complicated,todo.
        return RecreateSwapchain(); // return directly, note that this will cause the loss of 1 frame.
    default:
        outStream << std::format("[ graphicsBase ] ERROR\nFailed to queue the image for presentation!\nError code: {}\n", int32_t(result));
        return result;
    }
}

result_t GraphicsBase::PresentImage(VkSemaphore semaphore_renderingIsOver)
{
    VkPresentInfoKHR presentInfo = {
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &currentImageIndex
    };

    if (semaphore_renderingIsOver)
    {
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &semaphore_renderingIsOver;
    }
    return PresentImage(presentInfo);
}

result_t GraphicsBase::SubmitCommandBuffer_Presentation(VkCommandBuffer commandBuffer, VkSemaphore semaphore_renderingIsOver, VkSemaphore semaphore_ownershipIsTransfered, VkFence fence) const
{
    static constexpr VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    if (semaphore_renderingIsOver)
        submitInfo.waitSemaphoreCount = 1,
        submitInfo.pWaitSemaphores = &semaphore_renderingIsOver,
        submitInfo.pWaitDstStageMask = &waitDstStage;
    if (semaphore_ownershipIsTransfered)
        submitInfo.signalSemaphoreCount = 1,
        submitInfo.pSignalSemaphores = &semaphore_ownershipIsTransfered;
    VkResult result = vkQueueSubmit(queue_presentation, 1, &submitInfo, fence);
    if (result)
        outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the presentation command buffer!\nError code: {}\n", int32_t(result));
    return result;
}

void GraphicsBase::CmdTransferImageOwnership(VkCommandBuffer commandBuffer) const
{
    VkImageMemoryBarrier imageMemoryBarrier_g2p = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = queueFamilyIndex_graphics,
        .dstQueueFamilyIndex = queueFamilyIndex_presentation,
        .image = swapchainImages[currentImageIndex],
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
        0, nullptr, 0, nullptr, 1, &imageMemoryBarrier_g2p);
}

fence::~fence()
{
    DestroyHandleBy(vkDestroyFence);
}

result_t fence::Wait() const
{
    VkResult result = vkWaitForFences(GraphicsBase::Base().Device(), 1, &handle, false, UINT64_MAX);
    if (result)
        outStream << std::format("[ fence ] ERROR\nFailed to wait for the fence!\nError code: {}\n", int32_t(result));
    return result;
}

result_t fence::Reset() const
{
    VkResult result = vkResetFences(GraphicsBase::Base().Device(), 1, &handle);
    if (result)
        outStream << std::format("[ fence ] ERROR\nFailed to reset the fence!\nError code: {}\n", int32_t(result));
    return result;
}

result_t fence::Status() const
{
    VkResult result = vkGetFenceStatus(GraphicsBase::Base().Device(), handle);
    if (result < 0) //vkGetFenceStatus(...) succeeds with two results, so it cannot simply judge whether result is non-0
        outStream << std::format("[ fence ] ERROR\nFailed to get the status of the fence!\nError code: {}\n", int32_t(result));
    return result;
}

result_t fence::Create(VkFenceCreateInfo& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkResult result = vkCreateFence(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
    if (result)
        outStream << std::format("[ fence ] ERROR\nFailed to create a fence!\nError code: {}\n", int32_t(result));
    return result;
}

commandPool::~commandPool()
{
    DestroyHandleBy(vkDestroyCommandPool);
}

result_t commandPool::Create(VkCommandPoolCreateInfo& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    VkResult result = vkCreateCommandPool(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
    if (result)
        outStream << std::format("[ commandPool ] ERROR\nFailed to create a command pool!\nError code: {}\n", int32_t(result));
    return result;
}

semaphore::~semaphore()
{
    DestroyHandleBy(vkDestroySemaphore);
}

result_t semaphore::Create(VkSemaphoreCreateInfo& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkResult result = vkCreateSemaphore(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
    if (result)
        outStream << std::format("[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: {}\n", int32_t(result));
    return result;
}

result_t commandBuffer::Begin(VkCommandBufferUsageFlags usageFlags, VkCommandBufferInheritanceInfo& inheritanceInfo) const
{
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = usageFlags,
        .pInheritanceInfo = &inheritanceInfo
    };
    VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
    if (result)
        outStream << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", int32_t(result));
    return result;
}

result_t commandBuffer::Begin(VkCommandBufferUsageFlags usageFlags) const
{
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = usageFlags,
    };
    VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
    if (result)
        outStream << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", int32_t(result));
    return result;
}

result_t commandBuffer::End() const
{
    VkResult result = vkEndCommandBuffer(handle);
    if (result)
        outStream << std::format("[ commandBuffer ] ERROR\nFailed to end a command buffer!\nError code: {}\n", int32_t(result));
    return result;
}

result_t commandPool::AllocateBuffers(arrayRef<VkCommandBuffer> buffers, VkCommandBufferLevel level) const 
{
    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = handle,
        .level = level,
        .commandBufferCount = uint32_t(buffers.Count())
    };
    VkResult result = vkAllocateCommandBuffers(GraphicsBase::Base().Device(), &allocateInfo, buffers.Pointer());
    if (result)
        outStream << std::format("[ commandPool ] ERROR\nFailed to allocate command buffers!\nError code: {}\n", int32_t(result));
    return result;
}

result_t RenderPass::Create(VkRenderPassCreateInfo& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    VkResult result = vkCreateRenderPass(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
    if (result)
        outStream << std::format("[ renderPass ] ERROR\nFailed to create a render pass!\nError code: {}\n", int32_t(result));
    return result;
}

result_t Framebuffer::Create(VkFramebufferCreateInfo& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    VkResult result = vkCreateFramebuffer(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
    if (result)
        outStream << std::format("[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: {}\n", int32_t(result));
    return result;
}

}