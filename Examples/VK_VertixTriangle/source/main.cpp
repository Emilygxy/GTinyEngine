#include "GlfwGeneral.h"
#include "EasyVulkan.h"

// learning link: https://easyvulkan.github.io/index.html
using namespace easy_vk;

int main() {
    if (!InitializeWindow({ 1280, 720 }))
        return -1;

    const auto& [renderPass, framebuffers] = CreateRpwf_Screen();

    fence fence;
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    commandBuffer commandBuffer;
    commandPool commandPool(vk::GraphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    VkClearValue clearColor = { .color = { 1.f, 0.f, 0.f, 1.f } }; // red

    while (!glfwWindowShouldClose(pWindow)) {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        vk::GraphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        // because the framebuffer corresponds to the swapchain image obtained, get the swapchain image index
        auto i = vk::GraphicsBase::Base().CurrentImageIndex();

        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        /* start the render pass */ 
        renderPass.CmdBegin(commandBuffer, framebuffers[i], { {}, windowSize }, clearColor);
        /* render commands, to be filled */
        /* end the render pass */ 
        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        vk::GraphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
        vk::GraphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        fence.WaitAndReset();
    }
    TerminateWindow();
    return 0;
}