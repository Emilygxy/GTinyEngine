#include "glad/glad.h"
#include "GTVulkan/GlfwGeneral.h"
#include "GTVulkan/EasyVulkan.h"
#include "framework/VulkanDeferredPipeline.h"
#include "mesh/Mesh.h"

#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <memory>
#include <vector>

using namespace vk;
using namespace easy_vk;

namespace {

const auto& ScreenRpwf()
{
    static const auto& rpwf = CreateRpwf_Screen();
    return rpwf;
}

RenderPass CreateGeometryRenderPass(VkFormat albedoFormat, VkFormat normalFormat, VkFormat materialFormat, VkFormat depthFormat)
{
    VkAttachmentDescription attachments[4]{};
    for (uint32_t i = 0; i < 3; ++i) {
        attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    attachments[0].format = albedoFormat;
    attachments[1].format = normalFormat;
    attachments[2].format = materialFormat;

    attachments[3].format = depthFormat;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // Depth is sampled in lighting pass, so it must be stored.
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRefs[3] = {
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };
    VkAttachmentReference depthRef{ 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 3;
    subpass.pColorAttachments = colorRefs;
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo renderPassCi{};
    renderPassCi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCi.attachmentCount = 4;
    renderPassCi.pAttachments = attachments;
    renderPassCi.subpassCount = 1;
    renderPassCi.pSubpasses = &subpass;

    return RenderPass(renderPassCi);
}

pipeline CreateGeometryPipeline(VkRenderPass renderPass, VkPipelineLayout layout)
{
    shaderModule vert("resources/compiled_shaders/deferred_geometry_vert.spv");
    shaderModule frag("resources/compiled_shaders/deferred_geometry_frag.spv");
    VkPipelineShaderStageCreateInfo stages[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    graphicsPipelineCreateInfoPack ciPack;
    ciPack.createInfo.layout = layout;
    ciPack.createInfo.renderPass = renderPass;
    ciPack.createInfo.subpass = 0;
    ciPack.createInfo.stageCount = 2;
    ciPack.createInfo.pStages = stages;

    // Explicitly define fixed vertex input state for Vertex {pos, normal, uv}.
    ciPack.vertexInputBindings.push_back(te::VulkanGeometryPass::VertexBindingDescription());
    for (const auto& attr : te::VulkanGeometryPass::VertexAttributeDescriptions()) {
        ciPack.vertexInputAttributes.push_back(attr);
    }

    ciPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ciPack.viewports.emplace_back(0.f, 0.f, float(windowSize.width), float(windowSize.height), 0.f, 1.f);
    ciPack.scissors.emplace_back(VkOffset2D{}, windowSize);
    ciPack.rasterizationStateCi.polygonMode = VK_POLYGON_MODE_FILL;
    // Disable culling for now to avoid winding/clip-space convention mismatch
    // while validating deferred pipeline bring-up.
    ciPack.rasterizationStateCi.cullMode = VK_CULL_MODE_NONE;
    ciPack.rasterizationStateCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    ciPack.rasterizationStateCi.lineWidth = 1.0f;
    ciPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ciPack.depthStencilStateCi.depthTestEnable = VK_TRUE;
    ciPack.depthStencilStateCi.depthWriteEnable = VK_TRUE;
    ciPack.depthStencilStateCi.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ciPack.colorBlendAttachmentStates.resize(3);
    for (auto& blendAttachment : ciPack.colorBlendAttachmentStates) {
        blendAttachment.colorWriteMask = 0b1111;
    }
    ciPack.UpdateAllArrays();

    return pipeline(ciPack);
}

pipeline CreateLightingPipeline(VkRenderPass renderPass, VkPipelineLayout layout)
{
    shaderModule vert("resources/compiled_shaders/deferred_lighting_vert.spv");
    shaderModule frag("resources/compiled_shaders/deferred_lighting_frag.spv");
    VkPipelineShaderStageCreateInfo stages[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    graphicsPipelineCreateInfoPack ciPack;
    ciPack.createInfo.layout = layout;
    ciPack.createInfo.renderPass = renderPass;
    ciPack.createInfo.subpass = 0;
    ciPack.createInfo.stageCount = 2;
    ciPack.createInfo.pStages = stages;

    ciPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ciPack.viewports.emplace_back(0.f, 0.f, float(windowSize.width), float(windowSize.height), 0.f, 1.f);
    ciPack.scissors.emplace_back(VkOffset2D{}, windowSize);
    ciPack.rasterizationStateCi.polygonMode = VK_POLYGON_MODE_FILL;
    ciPack.rasterizationStateCi.cullMode = VK_CULL_MODE_NONE;
    ciPack.rasterizationStateCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    ciPack.rasterizationStateCi.lineWidth = 1.0f;
    ciPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ciPack.colorBlendAttachmentStates.resize(1);
    ciPack.colorBlendAttachmentStates[0].colorWriteMask = 0b1111;
    ciPack.UpdateAllArrays();

    return pipeline(ciPack);
}

std::vector<RenderCommand> CreateSceneCommands()
{
    auto meshA = std::make_shared<Mesh>();
    auto meshB = std::make_shared<Mesh>();
    const Vertex vertices[] = {
        { {-0.7f, -0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
        { { 0.7f, -0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
        { { 0.7f,  0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },
        { {-0.7f,  0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} }
    };
    const int indices[] = { 0, 1, 2, 0, 2, 3 };
    meshA->DoGenerateMesh(vertices, 4, indices, 6, true);
    meshB->DoGenerateMesh(vertices, 4, indices, 6, true);
    meshA->SetWorldTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-0.6f, 0.0f, 0.0f)));
    meshB->SetWorldTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.6f, 0.0f, 0.0f)));

    RenderCommand cmdA;
    cmdA.fragmentsSource = meshA;
    cmdA.hasUV = true;
    cmdA.state = RenderMode::Opaque;
    RenderCommand cmdB;
    cmdB.fragmentsSource = meshB;
    cmdB.hasUV = true;
    cmdB.state = RenderMode::Opaque;
    return { cmdA, cmdB };
}

} // namespace

int main()
{
    if (!InitializeWindow({ 1280, 720 })) {
        return -1;
    }

    {
        const auto& screen = ScreenRpwf();

        te::VulkanDeferredPipeline deferredPipeline;
        te::VulkanDeferredPipelineCreateInfo deferredCi{};
        deferredCi.geometry.extent = windowSize;
        deferredCi.geometry.depthFormat = VK_FORMAT_D32_SFLOAT;
        deferredCi.lighting.extent = windowSize;
        deferredCi.lighting.renderPass = screen.renderPass;
        if (!screen.framebuffers.empty()) {
            deferredCi.lighting.framebuffer = screen.framebuffers[0];
        }
        if (!deferredPipeline.Initialize(deferredCi)) {
            TerminateWindow();
            return -1;
        }

        auto& gbuffer = deferredPipeline.GeometryPass().GetGBuffer();
        RenderPass geometryRenderPass = CreateGeometryRenderPass(
            gbuffer.ColorFormat(vk::GBufferSlot::Albedo),
            gbuffer.ColorFormat(vk::GBufferSlot::Normal),
            gbuffer.ColorFormat(vk::GBufferSlot::Material),
            gbuffer.DepthFormat());
        deferredPipeline.GeometryPass().SetRenderPass(geometryRenderPass);

        pipelineLayout geometryLayout;
        {
            VkDescriptorSetLayout setLayouts[2] = {
                deferredPipeline.GeometryPass().GetDescriptorSetLayout(),
                deferredPipeline.GeometryPass().GetTextureDescriptorSetLayout()
            };
            VkPipelineLayoutCreateInfo layoutCi{};
            layoutCi.setLayoutCount = 2;
            layoutCi.pSetLayouts = setLayouts;
            geometryLayout.Create(layoutCi);
        }
        pipeline geometryPipeline = CreateGeometryPipeline(geometryRenderPass, geometryLayout);
        deferredPipeline.GeometryPass().SetPipeline(geometryPipeline, geometryLayout);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f),
                                     glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), float(windowSize.width) / float(windowSize.height), 0.1f, 100.0f);
        proj[1][1] *= -1.0f; // Vulkan clip-space Y convention
        deferredPipeline.GeometryPass().SetViewProjection(view, proj);

        // Align lighting pipeline layout with VulkanLightingPass descriptor set layout.
        pipelineLayout lightingLayout;
        {
            VkDescriptorSetLayout setLayout = deferredPipeline.LightingPass().GetDescriptorSetLayout();
            VkPipelineLayoutCreateInfo layoutCi{};
            layoutCi.setLayoutCount = setLayout == VK_NULL_HANDLE ? 0u : 1u;
            layoutCi.pSetLayouts = setLayout == VK_NULL_HANDLE ? nullptr : &setLayout;
            lightingLayout.Create(layoutCi);
        }
        pipeline lightingPipeline = CreateLightingPipeline(screen.renderPass, lightingLayout);
        deferredPipeline.LightingPass().SetPipeline(lightingPipeline, lightingLayout);
        std::array<glm::vec4, 4> pointPositions = {
            glm::vec4(-1.0f,  1.0f, 1.0f, 1.0f),
            glm::vec4( 1.0f,  1.0f, 1.0f, 1.0f),
            glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f),
            glm::vec4( 1.0f, -1.0f, 1.0f, 1.0f)
        };
        std::array<glm::vec4, 4> pointColors = {
            glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),
            glm::vec4(0.3f, 1.0f, 0.3f, 1.0f),
            glm::vec4(0.3f, 0.3f, 1.0f, 1.0f),
            glm::vec4(1.0f, 1.0f, 0.3f, 1.0f)
        };
        deferredPipeline.LightingPass().SetLightingParams(glm::vec3(0.5f, 1.0f, 0.2f),
                                                          glm::vec3(1.0f, 0.95f, 0.9f),
                                                          glm::vec3(0.0f, 0.0f, 2.0f),
                                                          pointPositions,
                                                          pointColors);

        commandBuffer cmd;
        commandPool cmdPool(GraphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        cmdPool.AllocateBuffers(cmd);

        fence frameFence;
        const uint32_t swapImageCount = static_cast<uint32_t>(screen.framebuffers.size());
        semaphore imageAvailable;
        std::vector<semaphore> renderingOverSemaphores(swapImageCount);
        std::vector<RenderCommand> commands = CreateSceneCommands();

        while (!glfwWindowShouldClose(pWindow)) {
            while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED)) {
                glfwWaitEvents();
            }

            GraphicsBase::Base().SwapImage(imageAvailable);
            const uint32_t imageIndex = GraphicsBase::Base().CurrentImageIndex();
            deferredPipeline.LightingPass().SetRenderTargets(screen.renderPass, screen.framebuffers[imageIndex]);

            const float t = static_cast<float>(glfwGetTime());
            for (size_t i = 0; i < commands.size(); ++i) {
                auto mesh = std::dynamic_pointer_cast<Mesh>(commands[i].fragmentsSource);
                if (!mesh) continue;
                const float baseX = (i == 0) ? -0.6f : 0.6f;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(baseX, 0.0f, 0.0f));
                model = glm::rotate(model, t * (i == 0 ? 1.0f : -1.3f), glm::vec3(0.0f, 0.0f, 1.0f));
                mesh->SetWorldTransform(model);
            }

            cmd.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            deferredPipeline.RecordFrame(cmd, commands);
            cmd.End();

            auto& renderingOver = renderingOverSemaphores[imageIndex];
            GraphicsBase::Base().SubmitCommandBuffer_Graphics(cmd, imageAvailable, renderingOver, frameFence);
            GraphicsBase::Base().PresentImage(renderingOver);
            frameFence.WaitAndReset();

            glfwPollEvents();
            TitleFps();
        }

        // Ensure present/submit work is fully retired before destroying semaphores
        // and deferred pipeline owned Vulkan resources.
        GraphicsBase::Base().WaitIdle();
        deferredPipeline.Shutdown();
    }

    TerminateWindow();
    return 0;
}

