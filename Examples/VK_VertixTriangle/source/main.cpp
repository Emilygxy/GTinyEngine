#include "GlfwGeneral.h"
#include "EasyVulkan.h"
#include <format>
#include "glm/glm.hpp"

// learning link: https://easyvulkan.github.io/index.html
using namespace easy_vk;
using namespace vk;

pipelineLayout pipelineLayout_triangle; //pipeline layout
pipeline pipeline_triangle;             //pipeline

struct vertex {
    glm::vec2 position;
    glm::vec4 color;
};

//This function calls easyVulkan::CreateRpwf_Screen() and stores the returned reference to a static variable
const auto& RenderPassAndFramebuffers() {
    static const auto& rpwf = CreateRpwf_Screen();
    return rpwf;
}
//This function is used to create the pipeline layout
void CreateLayout() {
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);
}
//This function is used to create the pipeline
void CreatePipeline() {
    static shaderModule vert("resources/compiled_shaders/VertexBuffer_vert.spv");// FirstTriangle.vert.spv
    static shaderModule frag("resources/compiled_shaders/VertexBuffer_frag.spv");// FirstTriangle.frag.spv

    // Verify shader modules were created successfully
    if (!vert || !frag) {
        outStream << std::format("[ pipeline ] ERROR\nFailed to create shader modules!\n");
        /*outStream << std::format("  Vertex shader handle: {}\n", static_cast<VkShaderModule>(vert));
        outStream << std::format("  Fragment shader handle: {}\n", static_cast<VkShaderModule>(frag));*/
        abort();
    }
    
    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangle[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
    };
    auto Create = [] {
        graphicsPipelineCreateInfoPack pipelineCiPack;
        pipelineCiPack.createInfo.layout = pipelineLayout_triangle;
        pipelineCiPack.createInfo.renderPass = RenderPassAndFramebuffers().renderPass;
        pipelineCiPack.createInfo.subpass = 0; // Required field
        //Data comes from the 0th vertex buffer, input frequency is vertex by vertex
        pipelineCiPack.vertexInputBindings.emplace_back(0, static_cast<uint32_t>(sizeof(vertex)), VK_VERTEX_INPUT_RATE_VERTEX);
        //Location 0, data comes from the 0th vertex buffer, vec2 corresponds to VK_FORMAT_R32G32_SFLOAT, use offsetof to calculate the starting position of position in vertex
        pipelineCiPack.vertexInputAttributes.emplace_back(0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(vertex, position)));
        //Location 1, data comes from the 0th vertex buffer, vec4 corresponds to VK_FORMAT_R32G32B32A32_SFLOAT, use offsetof to calculate the starting position of color in vertex
        pipelineCiPack.vertexInputAttributes.emplace_back(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(vertex, color)));

        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineCiPack.viewports.emplace_back(0.f, 0.f, float(windowSize.width), float(windowSize.height), 0.f, 1.f);
        pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);
        // Set rasterization state defaults (required fields)
        pipelineCiPack.rasterizationStateCi.polygonMode = VK_POLYGON_MODE_FILL;
        pipelineCiPack.rasterizationStateCi.cullMode = VK_CULL_MODE_NONE;
        pipelineCiPack.rasterizationStateCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipelineCiPack.rasterizationStateCi.lineWidth = 1.0f;
        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipelineCiPack.colorBlendAttachmentStates.push_back({ .colorWriteMask = 0b1111 }); // mask RGBA
        pipelineCiPack.UpdateAllArrays();
        // Set shader stages after UpdateAllArrays to override the empty shaderStages vector
        pipelineCiPack.createInfo.stageCount = 2u;
        pipelineCiPack.createInfo.pStages = shaderStageCreateInfos_triangle;
        pipeline_triangle.Create(pipelineCiPack);
    };
    auto Destroy = [] {
        pipeline_triangle.~pipeline();
    };
    GraphicsBase::Base().AddCallback_CreateSwapchain(Create);
    GraphicsBase::Base().AddCallback_DestroySwapchain(Destroy);
    Create();
}

int main() {
    if (!InitializeWindow({ 1280, 720 }))
        return -1;

    const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();
    CreateLayout();
    CreatePipeline();

    fence fence;
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    commandBuffer commandBuffer;
    commandPool commandPool(GraphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    VkClearValue clearColor = { .color = { 1.f, 0.f, 0.f, 1.f } }; // red

    vertex vertices[] = {
    { { -.5f, -.5f }, { 1, 1, 0, 1 } },
    { {  .5f, -.5f }, { 1, 0, 0, 1 } },
    { { -.5f,  .5f }, { 0, 1, 0, 1 } },
    { {  .5f,  .5f }, { 0, 0, 1, 1 } }
    };
    vertexBuffer vertexBuffer(sizeof vertices);
    vertexBuffer.TransferData(vertices);

    uint16_t indices[] = {
    0, 1, 2,
    1, 2, 3
    };
    indexBuffer indexBuffer(sizeof indices);
    indexBuffer.TransferData(indices);

    while (!glfwWindowShouldClose(pWindow)) {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        GraphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        auto i = GraphicsBase::Base().CurrentImageIndex();

        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        renderPass.CmdBegin(commandBuffer, framebuffers[i], { {}, windowSize }, clearColor);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffer.Address(), &offset);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);
        vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        GraphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
        GraphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        fence.WaitAndReset();
    }
    TerminateWindow();
    return 0;
}