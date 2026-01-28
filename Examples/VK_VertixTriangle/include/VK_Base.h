#pragma once
//GLM
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#include "glm/glm.hpp"
//#include "glm/gtc/matrix_transform.hpp"
//
#include "vulkan/vulkan.h"
#include <vector>
#include <span>
#include <iostream>
#include "VK_Utils.h"

namespace vk
{
    //define the default window size
    constexpr VkExtent2D defaultWindowSize = { 1280, 720 };
    inline auto& outStream = std::cout; // not constexpr, because std::cout has external linkage

    class GraphicsBase
    {
    private:
        //static variable
        static GraphicsBase singleton;
        //--------------------
        GraphicsBase() = default;
        GraphicsBase(GraphicsBase&&) = delete;
        ~GraphicsBase();

        //the singleton class object is static, and members that are not set to initial values and have no constructors will be zero initialized
        VkInstance instance;
        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;

        //this function is used to add string pointers to the instanceLayers or instanceExtensions container, and ensure no duplication
        static void AddLayerOrExtension(std::vector<const char*>& container, const char* name);

        VkDebugUtilsMessengerEXT debugMessenger;
        //this function is used to create the debug messenger
        VkResult CreateDebugMessenger();

        VkSurfaceKHR surface;

        //physicalDevice
        VkPhysicalDevice physicalDevice;
        VkPhysicalDeviceProperties physicalDeviceProperties;
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
        std::vector<VkPhysicalDevice> availablePhysicalDevices;
        VkDevice device;
        // three kinds physical device queue: graphics/presentation/compute
        // the valid index starts from 0, so the default value for the queue family index is VK_QUEUE_FAMILY_IGNORED (UINT32_MAX)
        uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_compute = VK_QUEUE_FAMILY_IGNORED;
        VkQueue queue_graphics;
        VkQueue queue_presentation;
        VkQueue queue_compute;
        std::vector<const char*> deviceExtensions;
        //this function is used to determine the physical device, and if the physical device supports the graphics/compute queues, the corresponding queue family indices will be stored in the queueFamilyIndices array
        VkResult GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue, bool enableComputeQueue, uint32_t(&queueFamilyIndices)[3]);

        //Surface-Swapchain
        std::vector<VkSurfaceFormatKHR> availableSurfaceFormats;
        VkSwapchainKHR swapchain;
        std::vector <VkImage> swapchainImages;
        std::vector <VkImageView> swapchainImageViews;
        // save the creation information of the swapchain to rebuild the swapchain
        VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
        // this function is called by CreateSwapchain(...) and RecreateSwapchain()
        VkResult CreateSwapchain_Internal();

        uint32_t apiVersion = VK_API_VERSION_1_0;

        //
        std::vector<void(*)()> callbacks_createSwapchain;
        std::vector<void(*)()> callbacks_destroySwapchain;
        static void ExecuteCallbacks(std::vector<void(*)()> callbacks);

        std::vector<void(*)()> callbacks_createDevice;
        std::vector<void(*)()> callbacks_destroyDevice;

        // the current image index of the swapchain
        uint32_t currentImageIndex = 0;

    public:
        //static function
        //this function is used to access the singleton
        static GraphicsBase& Base()
        {
            return singleton;
        }

        //Getter
        VkInstance Instance() const {
            return instance;
        }

        void Terminate();

        const std::vector<const char*>& InstanceLayers() const {
            return instanceLayers;
        }
        const std::vector<const char*>& InstanceExtensions() const {
            return instanceExtensions;
        }

        //the following functions are used to add layers or extensions before creating the Vulkan instance
        void AddInstanceLayer(const char* layerName);
        void AddInstanceExtension(const char* extensionName);
        //this function is used to create the Vulkan instance
        VkResult CreateInstance(VkInstanceCreateFlags flags = 0);
        //this function is used to check if the Vulkan instance layers are supported after creating the Vulkan instance failed
        VkResult CheckInstanceLayers(std::span<const char*> layersToCheck);
        void InstanceLayers(const std::vector<const char*>& layerNames)
        {
            instanceLayers = layerNames;
        }
        VkResult CheckInstanceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const;
        void InstanceExtensions(const std::vector<const char*>& extensionNames)
        {
            instanceExtensions = extensionNames;
        }

        VkSurfaceKHR Surface() const
        {
            return surface;
        }

        // this function is used to set the surface before selecting the physical device
        void Surface(VkSurfaceKHR surface);

        //
        VkPhysicalDevice PhysicalDevice() const
        {
            return physicalDevice;
        }
        const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const
        {
            return physicalDeviceProperties;
        }
        const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const
        {
            return physicalDeviceMemoryProperties;
        }
        VkPhysicalDevice AvailablePhysicalDevice(uint32_t index) const
        {
            return availablePhysicalDevices[index];
        }
        uint32_t AvailablePhysicalDeviceCount() const
        {
            return uint32_t(availablePhysicalDevices.size());
        }

        VkDevice Device() const
        {
            return device;
        }
        uint32_t QueueFamilyIndex_Graphics() const
        {
            return queueFamilyIndex_graphics;
        }
        uint32_t QueueFamilyIndex_Presentation() const
        {
            return queueFamilyIndex_presentation;
        }
        uint32_t QueueFamilyIndex_Compute() const
        {
            return queueFamilyIndex_compute;
        }
        VkQueue Queue_Graphics() const
        {
            return queue_graphics;
        }
        VkQueue Queue_Presentation() const
        {
            return queue_presentation;
        }
        VkQueue Queue_Compute() const
        {
            return queue_compute;
        }

        const std::vector<const char*>& DeviceExtensions() const
        {
            return deviceExtensions;
        }
        //this function is used to add extensions before creating the device
        void AddDeviceExtension(const char* extensionName);
        //this function is used to get the physical devices
        VkResult GetPhysicalDevices();

        //this function is used to determine the physical device, and if the physical device supports the graphics/compute queues, the corresponding queue family indices will be stored in the queueFamilyIndices array
        VkResult DeterminePhysicalDevice(bool enableGraphicsQueue, uint32_t deviceIndex = 0, bool enableComputeQueue = true);

        //this function is used to create the logic device
        VkResult CreateDevice(VkDeviceCreateFlags flags = 0);

        //this function is used to check the device extensions after creating the device failed
        VkResult CheckDeviceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const
        {
            /*to be filled in Ch1-3*/
        }
        void DeviceExtensions(const std::vector<const char*>& extensionNames)
        {
            deviceExtensions = extensionNames;
        }

        //surface-Swapchain
        const VkFormat& AvailableSurfaceFormat(uint32_t index) const
        {
            return availableSurfaceFormats[index].format;
        }
        const VkColorSpaceKHR& AvailableSurfaceColorSpace(uint32_t index) const
        {
            return availableSurfaceFormats[index].colorSpace;
        }
        uint32_t AvailableSurfaceFormatCount() const
        {
            return uint32_t(availableSurfaceFormats.size());
        }

        VkSwapchainKHR Swapchain() const
        {
            return swapchain;
        }
        VkImage SwapchainImage(uint32_t index) const
        {
            return swapchainImages[index];
        }
        VkImageView SwapchainImageView(uint32_t index) const
        {
            return swapchainImageViews[index];
        }
        uint32_t SwapchainImageCount() const
        {
            return uint32_t(swapchainImages.size());
        }
        const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const
        {
            return swapchainCreateInfo;
        }
        VkResult GetSurfaceFormats();

        VkResult SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat);

        //this function is used to create the swapchain
        VkResult CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0);

        // this function is used to recreate the swapchain, in the case of enable/diable HDR or resize windows
        VkResult RecreateSwapchain();

        //
        uint32_t ApiVersion() const {
            return apiVersion;
        }
        VkResult UseLatestApiVersion();

        //
        void AddCallback_CreateSwapchain(void(*function)())
        {
            callbacks_createSwapchain.push_back(function);
        }
        void AddCallback_DestroySwapchain(void(*function)())
        {
            callbacks_destroySwapchain.push_back(function);
        }
        void AddCallback_CreateDevice(void(*function)())
        {
            callbacks_createDevice.push_back(function);
        }
        void AddCallback_DestroyDevice(void(*function)())
        {
            callbacks_destroyDevice.push_back(function);
        }

        VkResult WaitIdle() const;

        VkResult RecreateDevice(VkDeviceCreateFlags flags = 0);

        //
        uint32_t CurrentImageIndex() const { return currentImageIndex; }
        // this function is used to get the image index of the swapchain and to destroy the old swapchain when it is needed
        VkResult SwapImage(VkSemaphore semaphore_imageIsAvailable);

        // this function is used to submit the command buffer to the graphics queue
        result_t SubmitCommandBuffer_Graphics(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const;

        // this function is used to submit the command buffer to the graphics queue in the render loop
        result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer,
            VkSemaphore semaphore_imageIsAvailable = VK_NULL_HANDLE,
            VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE,
            VkFence fence = VK_NULL_HANDLE,
            VkPipelineStageFlags waitDstStage_imageIsAvailable = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const;

        // this function is used to submit the command buffer to the graphics queue, and only use the fence
        result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const;

        // this function is used to submit the command buffer to the compute queue
        result_t SubmitCommandBuffer_Compute(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const;

        // this function is used to submit the command buffer to the compute queue, and only use the fence
        result_t SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const;

        result_t PresentImage(VkPresentInfoKHR& presentInfo);

        // this function is used to present the image in the render loop
        result_t PresentImage(VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE);

        result_t SubmitCommandBuffer_Presentation(VkCommandBuffer commandBuffer,
            VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE,
            VkSemaphore semaphore_ownershipIsTransfered = VK_NULL_HANDLE,
            VkFence fence = VK_NULL_HANDLE) const;

        void CmdTransferImageOwnership(VkCommandBuffer commandBuffer) const;
    };
    inline GraphicsBase GraphicsBase::singleton;

    class fence {
        VkFence handle = VK_NULL_HANDLE;
    public:
        //fence() = default;
        fence(VkFenceCreateInfo& createInfo) {
            Create(createInfo);
        }
        // default constructor creates a fence with the non-signaled state
        fence(VkFenceCreateFlags flags = 0) {
            Create(flags);
        }
        fence(fence&& other) noexcept { MoveHandle; }
        ~fence();
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        result_t Wait() const;
        result_t Reset() const;
        // because the situation of "waiting and then resetting immediately" often occurs, define this function
        result_t WaitAndReset() const {
            VkResult result = Wait();
            result || (result = Reset());
            return result;
        }
        result_t Status() const;
        //Non-const Function
        result_t Create(VkFenceCreateInfo& createInfo);
        result_t Create(VkFenceCreateFlags flags = 0)
        {
            VkFenceCreateInfo createInfo = {
                .flags = flags
            };
            return Create(createInfo);
        }
    };

    class semaphore {
        VkSemaphore handle = VK_NULL_HANDLE;
    public:
        //semaphore() = default;
        semaphore(VkSemaphoreCreateInfo& createInfo) {
            Create(createInfo);
        }
        // default constructor creates a semaphore with the non-signaled state
        semaphore(/*VkSemaphoreCreateFlags flags*/) {
            Create();
        }
        semaphore(semaphore&& other) noexcept { MoveHandle; }
        ~semaphore();
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkSemaphoreCreateInfo& createInfo);
        result_t Create(/*VkSemaphoreCreateFlags flags*/) 
        {
            VkSemaphoreCreateInfo createInfo = {};
            return Create(createInfo);
        }
    };

    class commandBuffer {
        friend class commandPool; // the commandPool class that encapsulates the command pool is responsible for allocating and releasing the command buffer, and needs to allow it to access the private member handle
        VkCommandBuffer handle = VK_NULL_HANDLE;
    public:
        commandBuffer() = default;
        commandBuffer(commandBuffer&& other) noexcept { MoveHandle; }
        // because the function to release the command buffer is defined in the commandPool class that encapsulates the command pool, there is no destructor
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        // here I didn't set the default parameter for inheritanceInfo, because in the C++ standard, it is undefined behavior to dereference a null pointer (although it does not happen at runtime, and at least the MSVC compiler allows this code), and I have to pass a reference rather than a pointer, so two Begin(...) are formed.
        result_t Begin(VkCommandBufferUsageFlags usageFlags, VkCommandBufferInheritanceInfo& inheritanceInfo) const;
        result_t Begin(VkCommandBufferUsageFlags usageFlags = 0) const;
        result_t End() const;
    };

    class commandPool {
        VkCommandPool handle = VK_NULL_HANDLE;
    public:
        commandPool() = default;
        commandPool(VkCommandPoolCreateInfo& createInfo) {
            Create(createInfo);
        }
        commandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
            Create(queueFamilyIndex, flags);
        }
        commandPool(commandPool&& other) noexcept { MoveHandle; }
        ~commandPool();
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        result_t AllocateBuffers(arrayRef<VkCommandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;
        result_t AllocateBuffers(arrayRef<commandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const {
            return AllocateBuffers( { &buffers[0].handle, buffers.Count() }, level);
        }
        void FreeBuffers(arrayRef<VkCommandBuffer> buffers) const {
            vkFreeCommandBuffers(GraphicsBase::Base().Device(), handle, buffers.Count(), buffers.Pointer());
            memset(buffers.Pointer(), 0, buffers.Count() * sizeof(VkCommandBuffer));
        }
        void FreeBuffers(arrayRef<commandBuffer> buffers) const {
            FreeBuffers({ &buffers[0].handle, buffers.Count() });
        }
        //Non-const Function
        result_t Create(VkCommandPoolCreateInfo& createInfo);
        result_t Create(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) 
        {
            VkCommandPoolCreateInfo createInfo = {
                .flags = flags,
                .queueFamilyIndex = queueFamilyIndex
            };
            return Create(createInfo);
        }
    };
}
