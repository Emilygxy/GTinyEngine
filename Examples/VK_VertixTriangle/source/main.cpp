#include "GlfwGeneral.h"
// learning link: https://easyvulkan.github.io/index.html
using namespace easy_vk;

int main() {
    if (!InitializeWindow({ 1280, 720 }))
        return -1;

    vk::fence fence(VK_FENCE_CREATE_SIGNALED_BIT);
    vk::semaphore semaphore_imageIsAvailable;
    vk::semaphore semaphore_ownershipIsTransfered;

    vk::commandBuffer commandBuffer_graphics;
    vk::commandBuffer commandBuffer_presentation;
    vk::commandPool commandPool_graphics(vk::GraphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    vk::commandPool commandPool_presentation(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, vk::GraphicsBase::Base().QueueFamilyIndex_Presentation());
    commandPool_graphics.AllocateBuffers(commandBuffer_graphics);
    commandPool_presentation.AllocateBuffers(commandBuffer_presentation);

    while (!glfwWindowShouldClose(pWindow)) {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        vk::GraphicsBase::Base().SwapImage(semaphore_imageIsAvailable);

        // submit the command buffer to the graphics queue
        commandBuffer_graphics.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        /*render commands, to be filled*/
        vk::GraphicsBase::Base().CmdTransferImageOwnership(commandBuffer_graphics);
        commandBuffer_graphics.End();
        vk::GraphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer_graphics, semaphore_imageIsAvailable);

        // submit the command buffer to the presentation queue
        commandBuffer_presentation.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vk::GraphicsBase::Base().CmdTransferImageOwnership(commandBuffer_presentation);
        commandBuffer_presentation.End();
        vk::GraphicsBase::Base().SubmitCommandBuffer_Presentation(commandBuffer_presentation, VK_NULL_HANDLE, semaphore_ownershipIsTransfered, fence);

        vk::GraphicsBase::Base().PresentImage(semaphore_ownershipIsTransfered);

        glfwPollEvents();
        TitleFps();

        fence.WaitAndReset();
    }
    TerminateWindow();
    return 0;
}