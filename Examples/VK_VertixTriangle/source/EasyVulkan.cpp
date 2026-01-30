#include "EasyVulkan.h"

const VkExtent2D& windowSize = GraphicsBase::Base().SwapchainCreateInfo().imageExtent;
namespace easy_vk
{
    const renderPassWithFramebuffers& CreateRpwf_Screen()
    {
        static renderPassWithFramebuffers rpwf;

        VkAttachmentDescription attachmentDescription = {
            .format = GraphicsBase::Base().SwapchainCreateInfo().imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        VkAttachmentReference attachmentReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpassDescription = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference
        };
        VkSubpassDependency subpassDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // not earlier than the wait for the semaphore corresponding to waitDstStageMask when submitting the command buffer
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        };

        //write the creation information of the render pass to create the render pass
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 1,
            .pDependencies = &subpassDependency
        };
        rpwf.renderPass.Create(renderPassCreateInfo);

        /**
        Since the size of the framebuffer is related to the swapchain image, when the swapchain is rebuilt, the framebuffer also needs to be rebuilt,
        so the code to create and destroy the framebuffer is thrown into the respective lambda expressions, used as callback functions.
        */

        auto CreateFramebuffers = [] {
            rpwf.framebuffers.resize(GraphicsBase::Base().SwapchainImageCount());
            VkFramebufferCreateInfo framebufferCreateInfo = {
                .renderPass = rpwf.renderPass,
                .attachmentCount = 1,
                .width = windowSize.width,
                .height = windowSize.height,
                .layers = 1
            };
            for (size_t i = 0; i < GraphicsBase::Base().SwapchainImageCount(); i++) {
                VkImageView attachment = GraphicsBase::Base().SwapchainImageView(uint32_t(i));
                framebufferCreateInfo.pAttachments = &attachment;
                rpwf.framebuffers[i].Create(framebufferCreateInfo);
            }
        };
        auto DestroyFramebuffers = [] {
            rpwf.framebuffers.clear(); // when the elements in the vector are cleared, the destructor is executed one by one
        };

        ExecuteOnce(rpwf); // prevent the callback function from being added repeatedly when the function is called again
        GraphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);
        GraphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);

        // If the swapchain is already created (e.g., InitializeWindow was called before CreateRpwf_Screen),
        // immediately create the framebuffers instead of waiting for the callback
        if (GraphicsBase::Base().Swapchain() != VK_NULL_HANDLE && GraphicsBase::Base().SwapchainImageCount() > 0)
        {
            CreateFramebuffers();
        }

        return rpwf;
    }

    void deviceLocalBuffer::TransferData(const void* pData_src, VkDeviceSize size, VkDeviceSize offset) const 
    {
        if (bufferMemory.MemoryProperties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            bufferMemory.BufferData(pData_src, size, offset);
            return;
        }
        stagingBuffer::BufferData_MainThread(pData_src, size);
        auto& commandBuffer = GraphicsBase::Plus().CommandBuffer_Transfer();
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VkBufferCopy region = { 0, offset, size };
        vkCmdCopyBuffer(commandBuffer, stagingBuffer::Buffer_MainThread(), bufferMemory.Buffer(), 1, &region);
        commandBuffer.End();
        GraphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
    }
    // Suitable for updating multiple discontinuous data blocks, stride is the step length between each group of data, here offset is obviously the offset in the target buffer
    void deviceLocalBuffer::TransferData(const void* pData_src, uint32_t elementCount, VkDeviceSize elementSize, VkDeviceSize stride_src, VkDeviceSize stride_dst, VkDeviceSize offset) const 
    {
        if (bufferMemory.MemoryProperties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            void* pData_dst = nullptr;
            bufferMemory.MapMemory(pData_dst, stride_dst * elementCount, offset);
            for (size_t i = 0; i < elementCount; i++)
                memcpy(stride_dst * i + static_cast<uint8_t*>(pData_dst), stride_src * i + static_cast<const uint8_t*>(pData_src), size_t(elementSize));
            bufferMemory.UnmapMemory(elementCount * stride_dst, offset);
            return;
        }
        stagingBuffer::BufferData_MainThread(pData_src, stride_src * elementCount);
        auto& commandBuffer = GraphicsBase::Plus().CommandBuffer_Transfer();
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        std::unique_ptr<VkBufferCopy[]> regions = std::make_unique<VkBufferCopy[]>(elementCount);
        for (size_t i = 0; i < elementCount; i++)
            regions[i] = { stride_src * i, stride_dst * i + offset, elementSize };
        vkCmdCopyBuffer(commandBuffer, stagingBuffer::Buffer_MainThread(), bufferMemory.Buffer(), elementCount, regions.get());
        commandBuffer.End();
        GraphicsBase::Plus().ExecuteCommandBuffer_Graphics(commandBuffer);
    }
    void deviceLocalBuffer::Create(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst)
    {
        VkBufferCreateInfo bufferCreateInfo = {
            .size = size,
            .usage = desiredUsages_Without_transfer_dst | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };
        // Short-circuit execution, the first line of false|| is to align
        false ||
            bufferMemory.CreateBuffer(bufferCreateInfo) ||
            bufferMemory.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && //&& operator has higher priority than ||
            bufferMemory.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ||
            bufferMemory.BindMemory();
    }
}
