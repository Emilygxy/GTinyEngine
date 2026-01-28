#include "VK_Base.h"
#include <iostream>
#include <format>

namespace
{

}

namespace vk
{
GraphicsBase::~GraphicsBase()
{
    /*to be filled in Ch1-4*/
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
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n", int32_t(result));
        return result;
    }
    std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
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
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n", int32_t(result));
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
#ifndef NDEBUG
    AddInstanceLayer("VK_LAYER_KHRONOS_validation");
    AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    
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
        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
        return result;
    }
    // after the vulkan instance is created, output the vulkan version
    std::cout << std::format(
        "Vulkan API Version: {}.{}.{}\n",
        VK_VERSION_MAJOR(apiVersion),
        VK_VERSION_MINOR(apiVersion),
        VK_VERSION_PATCH(apiVersion));
#ifndef NDEBUG
    // after the vulkan instance is created, create the debug messenger
    CreateDebugMessenger();
#endif

    return VK_SUCCESS;
}
VkResult GraphicsBase::CheckInstanceLayers(std::span<const char*> layersToCheck)
{
    uint32_t layerCount;
    std::vector<VkLayerProperties> availableLayers;
    if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr)) {
        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
        return result;
    }

    if (layerCount) {
        availableLayers.resize(layerCount);
        if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data())) 
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: {}\n", int32_t(result));
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
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName) :
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
        return result;
    }

    if (extensionCount)
    {
        availableExtensions.resize(extensionCount);
        if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data())) 
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n", int32_t(result));
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
}
VkResult GraphicsBase::GetPhysicalDevices()
{
    uint32_t deviceCount;
    if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr))
    {
        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of physical devices!\nError code: {}\n", int32_t(result));
        return result;
    }
    if (!deviceCount) 
    {
        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n");
        abort();
    }
    availablePhysicalDevices.resize(deviceCount);
    VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
    if (result)
    {
        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
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
        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n", int32_t(result));
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
    std::cout << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);
    /*to be filled in Ch1-4*/
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
}