#pragma once
//GLM
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#include "glm/glm.hpp"
//#include "glm/gtc/matrix_transform.hpp"
//
#include "vulkan/vulkan.h"
#include "vulkan/utility/vk_format_utils.h"
#include <vector>
#include <span>
#include <iostream>
#include "VK_Utils.h"
#include "VK_Format.h"

namespace vk
{
    //define the default window size
    constexpr VkExtent2D defaultWindowSize = { 1280, 720 };
    inline auto& outStream = std::cout; // not constexpr, because std::cout has external linkage

    // Forward declaration
    class GraphicsBasePlus;

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

        GraphicsBasePlus* pPlus = nullptr; //=nullptr can be omitted, because it is a member of the singleton class, automatically zero initialized

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

        static GraphicsBasePlus& Plus() { return *singleton.pPlus; }
        //*pPlus's Setter, only allow setting pPlus once
        static void Plus(GraphicsBasePlus& plus) { if (!singleton.pPlus) singleton.pPlus = &plus; }
    };
    inline GraphicsBase GraphicsBase::singleton;

    constexpr formatInfo FormatInfo(VkFormat format);
    constexpr VkFormat Corresponding16BitFloatFormat(VkFormat format_32BitFloat);
    const VkFormatProperties& FormatProperties(VkFormat format);

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
            vkFreeCommandBuffers(GraphicsBase::Base().Device(), handle, uint32_t(buffers.Count()), buffers.Pointer());
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

    class RenderPass {
        VkRenderPass handle = VK_NULL_HANDLE;
    public:
        RenderPass() = default;
        RenderPass(VkRenderPassCreateInfo& createInfo) {
            Create(createInfo);
        }
        RenderPass(RenderPass&& other) noexcept { MoveHandle; }
        ~RenderPass() { DestroyHandleBy(vkDestroyRenderPass); }
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        void CmdBegin(VkCommandBuffer commandBuffer, VkRenderPassBeginInfo& beginInfo, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = handle;
            vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
        }
        void CmdBegin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkRect2D renderArea, arrayRef<const VkClearValue> clearValues = {}, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
            VkRenderPassBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = handle,
                .framebuffer = framebuffer,
                .renderArea = renderArea,
                .clearValueCount = uint32_t(clearValues.Count()),
                .pClearValues = clearValues.Pointer()
            };
            vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
        }
        void CmdNext(VkCommandBuffer commandBuffer, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
            vkCmdNextSubpass(commandBuffer, subpassContents);
        }
        void CmdEnd(VkCommandBuffer commandBuffer) const {
            vkCmdEndRenderPass(commandBuffer);
        }
        //Non-const Function
        result_t Create(VkRenderPassCreateInfo& createInfo);
    };

    class Framebuffer {
        VkFramebuffer handle = VK_NULL_HANDLE;
    public:
        Framebuffer() = default;
        Framebuffer(VkFramebufferCreateInfo& createInfo) {
            Create(createInfo);
        }
        Framebuffer(Framebuffer&& other) noexcept { MoveHandle; }
        ~Framebuffer() { DestroyHandleBy(vkDestroyFramebuffer); }
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkFramebufferCreateInfo& createInfo);
    };

    class pipelineLayout {
        VkPipelineLayout handle = VK_NULL_HANDLE;
    public:
        pipelineLayout() = default;
        pipelineLayout(VkPipelineLayoutCreateInfo& createInfo) {
            Create(createInfo);
        }
        pipelineLayout(pipelineLayout&& other) noexcept { MoveHandle; }
        ~pipelineLayout() { DestroyHandleBy(vkDestroyPipelineLayout); }
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkPipelineLayoutCreateInfo& createInfo);
    };

    struct graphicsPipelineCreateInfoPack {
        VkGraphicsPipelineCreateInfo createInfo =
        { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        //Vertex Input
        VkPipelineVertexInputStateCreateInfo vertexInputStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        std::vector<VkVertexInputBindingDescription> vertexInputBindings;
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
        //Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        //Tessellation
        VkPipelineTessellationStateCreateInfo tessellationStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
        //Viewport
        VkPipelineViewportStateCreateInfo viewportStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        std::vector<VkViewport> viewports;
        std::vector<VkRect2D> scissors;
        uint32_t dynamicViewportCount = 1; // dynamic viewport/scissor will not use the above vectors, so the number of dynamic viewport and scissor is manually specified to these two variables
        uint32_t dynamicScissorCount = 1;
        //Rasterization
        VkPipelineRasterizationStateCreateInfo rasterizationStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        //Multisample
        VkPipelineMultisampleStateCreateInfo multisampleStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        //Depth & Stencil
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        //Color Blend
        VkPipelineColorBlendStateCreateInfo colorBlendStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
        //Dynamic
        VkPipelineDynamicStateCreateInfo dynamicStateCi =
        { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        std::vector<VkDynamicState> dynamicStates;
        //--------------------
        graphicsPipelineCreateInfoPack() {
            SetCreateInfos();
            // if not derived pipeline, createInfo.basePipelineIndex must not be 0, set to -1
            createInfo.basePipelineIndex = -1;
        }
        // move constructor, all pointers need to be re-assigned
        graphicsPipelineCreateInfoPack(const graphicsPipelineCreateInfoPack& other) noexcept {
            createInfo = other.createInfo;
            SetCreateInfos();
    
            vertexInputStateCi = other.vertexInputStateCi;
            inputAssemblyStateCi = other.inputAssemblyStateCi;
            tessellationStateCi = other.tessellationStateCi;
            viewportStateCi = other.viewportStateCi;
            rasterizationStateCi = other.rasterizationStateCi;
            multisampleStateCi = other.multisampleStateCi;
            depthStencilStateCi = other.depthStencilStateCi;
            colorBlendStateCi = other.colorBlendStateCi;
            dynamicStateCi = other.dynamicStateCi;
    
            shaderStages = other.shaderStages;
            vertexInputBindings = other.vertexInputBindings;
            vertexInputAttributes = other.vertexInputAttributes;
            viewports = other.viewports;
            scissors = other.scissors;
            colorBlendAttachmentStates = other.colorBlendAttachmentStates;
            dynamicStates = other.dynamicStates;
            UpdateAllArrayAddresses();
        }
        //Getter, I didn't use the const modifier here
        operator VkGraphicsPipelineCreateInfo& () { return createInfo; }
        //Non-const Function
        // This function is used to assign the address of the data in each vector to the corresponding member of each creation information, and change the corresponding count
        void UpdateAllArrays() {
            // Only update stageCount if shaderStages is not empty, otherwise leave it as set by the user
            if (!shaderStages.empty())
                createInfo.stageCount = uint32_t(shaderStages.size());
            vertexInputStateCi.vertexBindingDescriptionCount = uint32_t(vertexInputBindings.size());
            vertexInputStateCi.vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size());
            viewportStateCi.viewportCount = viewports.size() ? uint32_t(viewports.size()) : dynamicViewportCount;
            viewportStateCi.scissorCount = scissors.size() ? uint32_t(scissors.size()) : dynamicScissorCount;
            colorBlendStateCi.attachmentCount = uint32_t(colorBlendAttachmentStates.size());
            dynamicStateCi.dynamicStateCount = uint32_t(dynamicStates.size());
            UpdateAllArrayAddresses();
        }
    private:
        // This function is used to assign the address of the creation information to the corresponding member of basePipelineIndex
        void SetCreateInfos() {
            createInfo.pVertexInputState = &vertexInputStateCi;
            createInfo.pInputAssemblyState = &inputAssemblyStateCi;
            createInfo.pTessellationState = &tessellationStateCi;
            createInfo.pViewportState = &viewportStateCi;
            createInfo.pRasterizationState = &rasterizationStateCi;
            createInfo.pMultisampleState = &multisampleStateCi;
            createInfo.pDepthStencilState = &depthStencilStateCi;
            createInfo.pColorBlendState = &colorBlendStateCi;
            createInfo.pDynamicState = &dynamicStateCi;
        }
        // This function is used to assign the address of the data in each vector to the corresponding member of each creation information, but does not change the count
        void UpdateAllArrayAddresses() {
            // Only set pStages if shaderStages is not empty, otherwise leave it as set by the user
            if (!shaderStages.empty())
                createInfo.pStages = shaderStages.data();
            vertexInputStateCi.pVertexBindingDescriptions = vertexInputBindings.empty() ? nullptr : vertexInputBindings.data();
            vertexInputStateCi.pVertexAttributeDescriptions = vertexInputAttributes.empty() ? nullptr : vertexInputAttributes.data();
            viewportStateCi.pViewports = viewports.empty() ? nullptr : viewports.data();
            viewportStateCi.pScissors = scissors.empty() ? nullptr : scissors.data();
            colorBlendStateCi.pAttachments = colorBlendAttachmentStates.empty() ? nullptr : colorBlendAttachmentStates.data();
            dynamicStateCi.pDynamicStates = dynamicStates.empty() ? nullptr : dynamicStates.data();
        }
    };

    class pipeline {
        VkPipeline handle = VK_NULL_HANDLE;
    public:
        pipeline() = default;
        pipeline(VkGraphicsPipelineCreateInfo& createInfo) {
            Create(createInfo);
        }
        pipeline(VkComputePipelineCreateInfo& createInfo) {
            Create(createInfo);
        }
        pipeline(pipeline&& other) noexcept { MoveHandle; }
        ~pipeline() { DestroyHandleBy(vkDestroyPipeline); }
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkGraphicsPipelineCreateInfo& createInfo);
        result_t Create(VkComputePipelineCreateInfo& createInfo);
    };

    class shaderModule {
        VkShaderModule handle = VK_NULL_HANDLE;
    public:
        shaderModule() = default;
        shaderModule(VkShaderModuleCreateInfo& createInfo) {
            Create(createInfo);
        }
        shaderModule(const char* filepath /*VkShaderModuleCreateFlags flags*/) {
            Create(filepath);
        }
        shaderModule(size_t codeSize, const uint32_t* pCode /*VkShaderModuleCreateFlags flags*/) {
            Create(codeSize, pCode);
        }
        shaderModule(shaderModule&& other) noexcept { MoveHandle; }
        ~shaderModule() { DestroyHandleBy(vkDestroyShaderModule); }
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        VkPipelineShaderStageCreateInfo StageCreateInfo(VkShaderStageFlagBits stage, const char* entry = "main") const {
            return {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //sType
                nullptr,                                             //pNext
                0,                                                   //flags
                stage,                                               //stage
                handle,                                              //module
                entry,                                               //pName
                nullptr                                              //pSpecializationInfo
            };
        }
        //Non-const Function
        result_t Create(VkShaderModuleCreateInfo& createInfo);
        result_t Create(const char* filepath /*VkShaderModuleCreateFlags flags*/);
        result_t Create(size_t codeSize, const uint32_t* pCode /*VkShaderModuleCreateFlags flags*/) {
            VkShaderModuleCreateInfo createInfo = {
                .codeSize = codeSize,
                .pCode = pCode
            };
            return Create(createInfo);
        }
    };

    class deviceMemory {
        VkDeviceMemory handle = VK_NULL_HANDLE;
        VkDeviceSize allocationSize = 0;            // The actual size of the allocated memory
        VkMemoryPropertyFlags memoryProperties = 0; // The memory properties
        //--------------------
        // This function is used to adjust the range of non-host coherent memory region when mapping the memory region
        VkDeviceSize AdjustNonCoherentMemoryRange(VkDeviceSize& size, VkDeviceSize& offset) const;
    protected:
        // Used for bufferMemory or imageMemory, defined here to save 8 bytes
        class {
            friend class bufferMemory;
            friend class imageMemory;
            bool value = false;
            operator bool() const { return value; }
            auto& operator=(bool value) { this->value = value; return *this; }
        } areBound;
    public:
        deviceMemory() = default;
        deviceMemory(VkMemoryAllocateInfo& allocateInfo) {
            Allocate(allocateInfo);
        }
        deviceMemory(deviceMemory&& other) noexcept {
            MoveHandle;
            allocationSize = other.allocationSize;
            memoryProperties = other.memoryProperties;
            other.allocationSize = 0;
            other.memoryProperties = 0;
        }
        ~deviceMemory() { DestroyHandleBy(vkFreeMemory); allocationSize = 0; memoryProperties = 0; }
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        VkDeviceSize AllocationSize() const { return allocationSize; }
        VkMemoryPropertyFlags MemoryProperties() const { return memoryProperties; }
        //Const Function
        // Map the host visible memory region
        result_t MapMemory(void*& pData, VkDeviceSize size, VkDeviceSize offset = 0) const;
        // Unmap the host visible memory region
        result_t UnmapMemory(VkDeviceSize size, VkDeviceSize offset = 0) const;
        // BufferData(...) is used to conveniently update the device memory region, suitable for the case of writing data to the memory region using memcpy(...) and immediately unmapping the memory region
        result_t BufferData(const void* pData_src, VkDeviceSize size, VkDeviceSize offset = 0) const;
        result_t BufferData(const auto& data_src) const 
        {
            return BufferData(&data_src, sizeof data_src);
        }
        // RetrieveData(...) is used to conveniently retrieve data from the device memory region, suitable for the case of retrieving data from the memory region using memcpy(...) and immediately unmapping the memory region
        result_t RetrieveData(void* pData_dst, VkDeviceSize size, VkDeviceSize offset = 0) const;
        //Non-const Function
        result_t Allocate(VkMemoryAllocateInfo& allocateInfo);
    };

    class buffer {
        VkBuffer handle = VK_NULL_HANDLE;
    public:
        buffer() = default;
        buffer(VkBufferCreateInfo& createInfo) {
            Create(createInfo);
        }
        buffer(buffer&& other) noexcept { MoveHandle; }
        ~buffer() { DestroyHandleBy(vkDestroyBuffer); }
        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        VkMemoryAllocateInfo MemoryAllocateInfo(VkMemoryPropertyFlags desiredMemoryProperties) const;
        result_t BindMemory(VkDeviceMemory deviceMemory, VkDeviceSize memoryOffset = 0) const;
        //Non-const Function
        result_t Create(VkBufferCreateInfo& createInfo);
    };

    class bufferMemory : buffer, deviceMemory {
        public:
            bufferMemory() = default;
            bufferMemory(VkBufferCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) {
                Create(createInfo, desiredMemoryProperties);
            }
            bufferMemory(bufferMemory&& other) noexcept : buffer(std::move(other)), deviceMemory(std::move(other)) 
            {
                areBound = other.areBound;
                other.areBound = false;
            }
            ~bufferMemory() { areBound = false; }
            //Getter
            // Don't define the conversion functions to VkBuffer and VkDeviceMemory, because in 32-bit mode, both of these types are aliases of uint64_t, which will cause conflicts (although, who the hell still uses 32-bit PCs!)
            VkBuffer Buffer() const { return static_cast<const buffer&>(*this); }
            const VkBuffer* AddressOfBuffer() const { return buffer::Address(); }
            VkDeviceMemory Memory() const { return static_cast<const deviceMemory&>(*this); }
            const VkDeviceMemory* AddressOfMemory() const { return deviceMemory::Address(); }
            // If areBond is true, then the device memory has been successfully allocated, the buffer has been successfully created, and they have been successfully bound together
            bool AreBound() const { return areBound; }
            using deviceMemory::AllocationSize;
            using deviceMemory::MemoryProperties;
            //Const Function
            using deviceMemory::MapMemory;
            using deviceMemory::UnmapMemory;
            using deviceMemory::BufferData;
            using deviceMemory::RetrieveData;
            //Non-const Function
            // The following three functions are only used for the case where Create(...) may fail
            result_t CreateBuffer(VkBufferCreateInfo& createInfo) 
            {
                return buffer::Create(createInfo);
            }
            result_t AllocateMemory(VkMemoryPropertyFlags desiredMemoryProperties) 
            {
                VkMemoryAllocateInfo allocateInfo = MemoryAllocateInfo(desiredMemoryProperties);
                if (allocateInfo.memoryTypeIndex >= GraphicsBase::Base().PhysicalDeviceMemoryProperties().memoryTypeCount)
                    return VK_RESULT_MAX_ENUM; // No suitable error code, don't use VK_ERROR_UNKNOWN
                return Allocate(allocateInfo);
            }
            result_t BindMemory()
            {
                if (VkResult result = buffer::BindMemory(Memory()))
                    return result;
                areBound = true;
                return VK_SUCCESS;
            }
            // Allocate device memory, create buffer, and bind
            result_t Create(VkBufferCreateInfo& createInfo, VkMemoryPropertyFlags desiredMemoryProperties) 
            {
                VkResult result;
                false || // This line is used to deal with the alignment of code in Visual Studio
                    (result = CreateBuffer(createInfo)) || // Use || short-circuit execution
                    (result = AllocateMemory(desiredMemoryProperties)) ||
                    (result = BindMemory());
                return result;
            }
        };

        class bufferView 
        {
            VkBufferView handle = VK_NULL_HANDLE;
        public:
            bufferView() = default;
            bufferView(VkBufferViewCreateInfo& createInfo) {
                Create(createInfo);
            }
            bufferView(VkBuffer buffer, VkFormat format, VkDeviceSize offset = 0, VkDeviceSize range = 0 /*VkBufferViewCreateFlags flags*/) {
                Create(buffer, format, offset, range);
            }
            bufferView(bufferView&& other) noexcept { MoveHandle; }
            ~bufferView() { DestroyHandleBy(vkDestroyBufferView); }
            //Getter
            DefineHandleTypeOperator;
            DefineAddressFunction;
            //Non-const Function
            result_t Create(VkBufferViewCreateInfo& createInfo);
            result_t Create(VkBuffer buffer, VkFormat format, VkDeviceSize offset = 0, VkDeviceSize range = 0 /*VkBufferViewCreateFlags flags*/);
        };

        class image {
            VkImage handle = VK_NULL_HANDLE;
        public:
            image() = default;
            image(VkImageCreateInfo& createInfo) {
                Create(createInfo);
            }
            image(image&& other) noexcept { MoveHandle; }
            ~image() { DestroyHandleBy(vkDestroyImage); }
            //Getter
            DefineHandleTypeOperator;
            DefineAddressFunction;
            //Const Function
            VkMemoryAllocateInfo MemoryAllocateInfo(VkMemoryPropertyFlags desiredMemoryProperties) const;
            result_t BindMemory(VkDeviceMemory deviceMemory, VkDeviceSize memoryOffset = 0) const;
            //Non-const Function
            result_t Create(VkImageCreateInfo& createInfo);
        };

        class GraphicsBasePlus
        {
            VkFormatProperties formatProperties[std::size(formatInfos_v1_0)] = {};
            commandPool commandPool_graphics;
            commandPool commandPool_presentation;
            commandPool commandPool_compute;
            commandBuffer commandBuffer_transfer; // Allocate from commandPool_graphics
            commandBuffer commandBuffer_presentation;
            // Static variable
            static GraphicsBasePlus singleton;
            //--------------------
            GraphicsBasePlus();
            GraphicsBasePlus(GraphicsBasePlus&&) = delete;
            ~GraphicsBasePlus() = default;
        public:
            //Getter
            const VkFormatProperties& FormatProperties(VkFormat format) const;
            const commandPool& CommandPool_Graphics() const { return commandPool_graphics; }
            const commandPool& CommandPool_Compute() const { return commandPool_compute; }
            const commandBuffer& CommandBuffer_Transfer() const { return commandBuffer_transfer; }
            //Const Function
            result_t ExecuteCommandBuffer_Graphics(VkCommandBuffer commandBuffer) const 
            {
                fence fence;
                VkSubmitInfo submitInfo = {
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer
                };
                VkResult result = GraphicsBase::Base().SubmitCommandBuffer_Graphics(submitInfo, fence);
                if (!result)
                    fence.Wait();
                return result;
            }
            // This function is specifically used to submit the command buffer to the presentation queue for acquiring the ownership of the queue family of the swapchain image
            result_t AcquireImageOwnership_Presentation(VkSemaphore semaphore_renderingIsOver, VkSemaphore semaphore_ownershipIsTransfered, VkFence fence = VK_NULL_HANDLE) const {
                if (VkResult result = commandBuffer_presentation.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
                    return result;
                GraphicsBase::Base().CmdTransferImageOwnership(commandBuffer_presentation);
                if (VkResult result = commandBuffer_presentation.End())
                    return result;
                return GraphicsBase::Base().SubmitCommandBuffer_Presentation(commandBuffer_presentation, semaphore_renderingIsOver, semaphore_ownershipIsTransfered, fence);
            }
        };
        inline GraphicsBasePlus GraphicsBasePlus::singleton;
}
