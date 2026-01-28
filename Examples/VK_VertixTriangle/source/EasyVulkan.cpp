#include "EasyVulkan.h"

const VkExtent2D& windowSize = GraphicsBase::Base().SwapchainCreateInfo().imageExtent;

const easy_vk::renderPassWithFramebuffers& easy_vk::CreateRpwf_Screen()
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
