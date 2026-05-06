#include "framework/Renderer.h"
#include "framework/RenderPass.h"
#include "framework/RenderPassManager.h"
#include "framework/VulkanDeferredPipeline.h"
#include "glad/glad.h"
#include "shader.h"
#include "Camera.h"
#include "Light.h"
#include "mesh/Mesh.h"
#include "GTVulkan/GlfwGeneral.h"
#include "GTVulkan/EasyVulkan.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <array>
#include <utility>
#include "framework/RenderContext.h"
#include "RenderView.h"

// Render Factory Impl
std::shared_ptr<IRenderer> RendererFactory::CreateRenderer(RendererBackend backend)
{
    std::cout << "RendererFactory::CreateRenderer called with backend: " << static_cast<int>(backend) << std::endl;
    auto& passManager = te::RenderPassManager::GetInstance();

    switch (backend)
    {
    case RendererBackend::OpenGL:
        std::cout << "Creating OpenGL renderer..." << std::endl;
        passManager.SyncActiveBackend(backend);
        return std::make_shared<OpenGLRenderer>();
    case RendererBackend::OpenGLES:
        // TODO: OpenGLES renderer Impl
        std::cout << "OpenGLES renderer not implemented" << std::endl;
        return nullptr;
    case RendererBackend::Vulkan:
        std::cout << "Creating Vulkan renderer..." << std::endl;
        passManager.SyncActiveBackend(backend);
        return std::make_shared<VulkanRenderer>();
    default:
        std::cout << "Unknown renderer backend" << std::endl;
        return nullptr;
    }
}

// OpenGL renderer Impl
OpenGLRenderer::OpenGLRenderer()
{
    std::cout << "OpenGLRenderer constructor called" << std::endl;
    // mpRenderContext is initialized in header with nullptr
}

OpenGLRenderer::~OpenGLRenderer()
{
    Shutdown();
}

bool OpenGLRenderer::Initialize()
{
    std::cout << "OpenGLRenderer::Initialize called" << std::endl;
    te::RenderPassManager::GetInstance().SyncActiveBackend(RendererBackend::OpenGL);
    
    // OpenGL already initialized, here only need to set some default states
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);  // set depth function
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // set default clear color
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    
    // set point size (for Points render state)
    glPointSize(1.0f);
    
    // set line width (for Lines and Wireframe render state)
    glLineWidth(1.0f);
    
    mStats.Reset();

    if (!mpRenderView)
    {
        mpRenderView = std::make_shared<RenderView>(1200,1280);
    }

    std::cout << "OpenGLRenderer::Initialize completed successfully" << std::endl;
    return true;
}

void OpenGLRenderer::Shutdown()
{
    // clean up cached mesh data
    for (auto& [hash, cache] : mMeshCache)
    {
        Mesh::CleanupMeshBuffers(cache.vao, cache.vbo, cache.ebo);
    }
    mMeshCache.clear();
}

void OpenGLRenderer::BeginFrame()
{
    mStats.Reset();

    // init bachground
    if (mpRenderView)
    {
        mpRenderView->BindCamera(mpRenderContext->GetAttachedCamera());
        mpRenderView->Update();
    }
}

void OpenGLRenderer::EndFrame()
{
    // can add cleanup work here
}

void OpenGLRenderer::DrawMesh(const RenderCommand& command)
{
    if(!command.fragmentsSource || !command.fragmentsSource->GetDefaultFragment().IsReady())
        return;

    // apply render state
    ApplyRenderState(command.state);

    auto& frag = command.fragmentsSource->GetDefaultFragment();
    if (!frag.IsReady())
        return;

    // get data from fragment
    auto material = command.fragmentsSource->GetMaterial();
    auto vertices = frag.mpGeometry->GetVertices();
    auto indices = frag.mpGeometry->GetIndices();
    auto transform = frag.mpGeometry->GetWorldTransform();

    // set material
    material->OnApply();

    // set transform matrix
    material->GetShader()->setMat4("model", transform);

    // set camera and light parameters
    if (auto pCamera = mpRenderContext->GetAttachedCamera())
    {
        material->GetShader()->setMat4("view", pCamera->GetViewMatrix());
        material->GetShader()->setMat4("projection", pCamera->GetProjectionMatrix());
    }

    if (auto pLight = mpRenderContext->GetDefaultLight())
    {
        material->GetShader()->setVec3("u_lightPos", pLight->GetPosition());
        material->GetShader()->setVec3("u_lightColor", pLight->GetColor());
    }

    // update material uniform
    material->UpdateUniform();

    // bind material resources
    material->OnBind();

    // calculate mesh hash for caching
    size_t meshHash = std::hash<std::string>{}(std::string((char*)vertices.data(), vertices.size() * sizeof(Vertex))) ^
        std::hash<std::string>{}(std::string((char*)indices.data(), indices.size() * sizeof(unsigned int)));

    // find or create cached mesh data
    auto it = mMeshCache.find(meshHash);
    if (it == mMeshCache.end())
    {
        MeshCache cache;
        Mesh::SetupMeshBuffers(vertices, indices, cache.vao, cache.vbo, cache.ebo);
        cache.vertexCount = vertices.size();
        cache.indexCount = indices.size();
        mMeshCache[meshHash] = cache;
        it = mMeshCache.find(meshHash);
    }

    // bind VAO and draw
    glBindVertexArray(it->second.vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(it->second.indexCount), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // update stats
    mStats.drawCalls++;
    mStats.triangles += uint32_t(indices.size()) / 3;
    mStats.vertices += uint32_t(vertices.size());
}

void OpenGLRenderer::DrawMesh(const std::shared_ptr<Mesh> pMesh)
{
    if (!pMesh || (!pMesh->IsReady()))
    {
        return;
    }

    // apply render state
    ApplyRenderState(RenderMode::Opaque);

    auto material = pMesh->GetMaterial();
   // set material
   material->OnApply();
   
   // set transform matrix
   material->GetShader()->setMat4("model", pMesh->GetWorldTransform());
   
   // set camera and light parameters
   if (auto pCamera = mpRenderContext->GetAttachedCamera())
   {
       material->GetShader()->setMat4("view", pCamera->GetViewMatrix());
       material->GetShader()->setMat4("projection", pCamera->GetProjectionMatrix());
   }
   
   if (auto pLight = mpRenderContext->GetDefaultLight())
   {
       material->GetShader()->setVec3("u_lightPos", pLight->GetPosition());
       material->GetShader()->setVec3("u_lightColor", pLight->GetColor());
   }
   
   // update material uniform
   material->UpdateUniform();
   
   // bind material resources
   material->OnBind();

   // will update stats
   pMesh->Draw(mMeshCache, mStats);
}

void OpenGLRenderer::DrawMeshes(const std::vector<RenderCommand>& commands)
{
   for (const auto& command : commands)
   {
       DrawMesh(command);
   }
}

void OpenGLRenderer::SetViewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    mpRenderView->SetViewPort({ x, y, width, height });
}

void OpenGLRenderer::SetClearColor(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
}

void OpenGLRenderer::Clear(uint32_t flags)
{
    GLbitfield glFlags = 0;
    if (flags & 0x1) glFlags |= GL_COLOR_BUFFER_BIT;
    if (flags & 0x2) glFlags |= GL_DEPTH_BUFFER_BIT;
    if (flags & 0x4) glFlags |= GL_STENCIL_BUFFER_BIT;
    
    glClear(glFlags);
}

void OpenGLRenderer::ApplyRenderState(RenderMode state)
{
    if (mCurrentState == state)
        return;

    switch (state)
    {
    case RenderMode::Opaque:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        break;
    case RenderMode::Wireframe:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        break;
    case RenderMode::Points:
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);  // point rendering usually doesn't need face culling
        glPointSize(2.0f);        // set point size to make it more visible
        break;
    case RenderMode::Lines:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);  // line rendering usually doesn't need face culling
        glLineWidth(1.0f);        // set line width
        break;
    case RenderMode::Transparent:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }

    mCurrentState = state;
}

// Multi-pass rendering implementation
void OpenGLRenderer::AddRenderPass(const std::shared_ptr<te::RenderPass>& pass)
{
    if (!pass)
        return;

    const std::string& name = pass->GetConfig().name;
    
    // check if already exists
    if (mRenderPassIndexMap.find(name) != mRenderPassIndexMap.end())
    {
        std::cout << "RenderPass with name '" << name << "' already exists" << std::endl;
        return;
    }

    // add to list
    size_t index = mRenderPasses.size();
    mRenderPasses.push_back(pass);
    mRenderPassIndexMap[name] = index;
}

void OpenGLRenderer::RemoveRenderPass(const std::string& name)
{
    auto it = mRenderPassIndexMap.find(name);
    if (it == mRenderPassIndexMap.end())
        return;

    size_t index = it->second;
    mRenderPasses.erase(mRenderPasses.begin() + index);
    mRenderPassIndexMap.erase(it);

    // rebuild index mapping
    mRenderPassIndexMap.clear();
    for (size_t i = 0; i < mRenderPasses.size(); ++i)
    {
        mRenderPassIndexMap[mRenderPasses[i]->GetConfig().name] = i;
    }
}

std::shared_ptr<te::RenderPass> OpenGLRenderer::GetRenderPass(const std::string& name) const
{
    auto it = mRenderPassIndexMap.find(name);
    if (it == mRenderPassIndexMap.end())
        return nullptr;
    
    return mRenderPasses[it->second];
}

void OpenGLRenderer::ExecuteRenderPasses(const std::vector<RenderCommand>& commands)
{
    if (!mMultiPassEnabled || mRenderPasses.empty())
        return;
    // set inputs
    for (const auto& pass : mRenderPasses)
    {
        // set input textures
        for (const auto& input : pass->GetConfig().inputs)
        {
            auto sourcePass = GetRenderPass(input.sourcePass);
            if (sourcePass)
            {
                auto outputTarget = sourcePass->GetOutput(input.sourceTarget);
                if (outputTarget)
                {
                    pass->SetInput(input.sourceTarget, outputTarget->GetTextureHandle());
                }
            }
        }
    }

    // sort Pass by dependency
    // here use simple sort, should implement topological sort in the future
    std::sort(mRenderPasses.begin(), mRenderPasses.end(),
        [](const std::shared_ptr<te::RenderPass>& a, const std::shared_ptr<te::RenderPass>& b) {
            return static_cast<int>(a->GetConfig().type) < static_cast<int>(b->GetConfig().type);
        });

    // execute all Pass
    for (const auto& pass : mRenderPasses)
    {
        if (!pass->IsEnabled())
            continue;

        // execute Pass
        pass->Execute(commands);
    }
} 

void OpenGLRenderer::SetRenderContext(const std::shared_ptr<RenderContext>& pRenderContext)
{
    mpRenderContext = pRenderContext;
}

namespace
{
vk::RenderPass CreateDeferredGeometryRenderPass(VkFormat albedoFormat, VkFormat normalFormat, VkFormat materialFormat, VkFormat depthFormat)
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
    return vk::RenderPass(renderPassCi);
}

vk::pipeline CreateDeferredGeometryPipeline(VkExtent2D extent, VkRenderPass renderPass, VkPipelineLayout layout)
{
    vk::shaderModule vert("resources/compiled_shaders/deferred_geometry_vert.spv");
    vk::shaderModule frag("resources/compiled_shaders/deferred_geometry_frag.spv");
    VkPipelineShaderStageCreateInfo stages[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    vk::graphicsPipelineCreateInfoPack ciPack;
    ciPack.createInfo.layout = layout;
    ciPack.createInfo.renderPass = renderPass;
    ciPack.createInfo.subpass = 0;
    ciPack.createInfo.stageCount = 2;
    ciPack.createInfo.pStages = stages;
    ciPack.vertexInputBindings.push_back(te::VulkanGeometryPass::VertexBindingDescription());
    for (const auto& attr : te::VulkanGeometryPass::VertexAttributeDescriptions()) {
        ciPack.vertexInputAttributes.push_back(attr);
    }
    ciPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ciPack.viewports.emplace_back(0.f, 0.f, float(extent.width), float(extent.height), 0.f, 1.f);
    ciPack.scissors.emplace_back(VkOffset2D{}, extent);
    ciPack.rasterizationStateCi.polygonMode = VK_POLYGON_MODE_FILL;
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
    return vk::pipeline(ciPack);
}

vk::pipeline CreateDeferredLightingPipeline(VkExtent2D extent, VkRenderPass renderPass, VkPipelineLayout layout)
{
    vk::shaderModule vert("resources/compiled_shaders/deferred_lighting_vert.spv");
    vk::shaderModule frag("resources/compiled_shaders/deferred_lighting_frag.spv");
    VkPipelineShaderStageCreateInfo stages[2] = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    vk::graphicsPipelineCreateInfoPack ciPack;
    ciPack.createInfo.layout = layout;
    ciPack.createInfo.renderPass = renderPass;
    ciPack.createInfo.subpass = 0;
    ciPack.createInfo.stageCount = 2;
    ciPack.createInfo.pStages = stages;
    ciPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ciPack.viewports.emplace_back(0.f, 0.f, float(extent.width), float(extent.height), 0.f, 1.f);
    ciPack.scissors.emplace_back(VkOffset2D{}, extent);
    ciPack.rasterizationStateCi.polygonMode = VK_POLYGON_MODE_FILL;
    ciPack.rasterizationStateCi.cullMode = VK_CULL_MODE_NONE;
    ciPack.rasterizationStateCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    ciPack.rasterizationStateCi.lineWidth = 1.0f;
    ciPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ciPack.colorBlendAttachmentStates.resize(1);
    ciPack.colorBlendAttachmentStates[0].colorWriteMask = 0b1111;
    ciPack.UpdateAllArrays();
    return vk::pipeline(ciPack);
}
}

struct VulkanRenderer::Impl
{
    bool initialized = false;
    bool ownsWindow = false;
    bool frameBegun = false;
    VkExtent2D extent{ 1280, 720 };
    te::VulkanDeferredPipeline deferredPipeline{};
    const easy_vk::renderPassWithFramebuffers* screen = nullptr;
    std::unique_ptr<vk::RenderPass> geometryRenderPass{};
    vk::pipelineLayout geometryLayout{};
    std::unique_ptr<vk::pipeline> geometryPipeline{};
    vk::pipelineLayout lightingLayout{};
    std::unique_ptr<vk::pipeline> lightingPipeline{};
    vk::commandPool commandPool{};
    vk::commandBuffer commandBuffer{};
    std::unique_ptr<vk::fence> frameFence{};
    std::unique_ptr<vk::semaphore> imageAvailable{};
    std::vector<std::unique_ptr<vk::semaphore>> renderingOverSemaphores{};
    std::vector<RenderCommand> pendingCommands{};
};

VulkanRenderer::VulkanRenderer() : impl_(std::make_unique<Impl>()) {}

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

bool VulkanRenderer::Initialize()
{
    auto& impl = *impl_;
    if (impl.initialized) {
        return true;
    }

    if (!easy_vk::pWindow) {
        if (!easy_vk::InitializeWindow({ viewportWidth_, viewportHeight_ })) {
            std::cout << "VulkanRenderer::Initialize failed to create Vulkan window." << std::endl;
            return false;
        }
        impl.ownsWindow = true;
    }

    impl.screen = &easy_vk::CreateRpwf_Screen();
    impl.extent = windowSize;

    te::VulkanDeferredPipelineCreateInfo deferredCi{};
    deferredCi.geometry.extent = impl.extent;
    deferredCi.geometry.depthFormat = VK_FORMAT_D32_SFLOAT;
    deferredCi.lighting.extent = impl.extent;
    deferredCi.lighting.renderPass = impl.screen->renderPass;
    if (!impl.screen->framebuffers.empty()) {
        deferredCi.lighting.framebuffer = impl.screen->framebuffers[0];
    }
    if (!impl.deferredPipeline.Initialize(deferredCi)) {
        std::cout << "VulkanRenderer::Initialize failed to initialize deferred pipeline." << std::endl;
        return false;
    }

    auto& passMgr = te::RenderPassManager::GetInstance();
    passMgr.SyncActiveBackend(RendererBackend::Vulkan);
    passMgr.EnableVulkanGraph(true);
    if (!passMgr.BuildVulkanDeferredGraph(&impl.deferredPipeline)) {
        std::cout << "VulkanRenderer::Initialize failed to build Vulkan pass graph." << std::endl;
        impl.deferredPipeline.Shutdown();
        return false;
    }

    auto& gbuffer = impl.deferredPipeline.GeometryPass().GetGBuffer();
    impl.geometryRenderPass = std::make_unique<vk::RenderPass>(CreateDeferredGeometryRenderPass(
        gbuffer.ColorFormat(vk::GBufferSlot::Albedo),
        gbuffer.ColorFormat(vk::GBufferSlot::Normal),
        gbuffer.ColorFormat(vk::GBufferSlot::Material),
        gbuffer.DepthFormat()));
    impl.deferredPipeline.GeometryPass().SetRenderPass(*impl.geometryRenderPass);

    {
        VkDescriptorSetLayout setLayouts[2] = {
            impl.deferredPipeline.GeometryPass().GetDescriptorSetLayout(),
            impl.deferredPipeline.GeometryPass().GetTextureDescriptorSetLayout()
        };
        VkPipelineLayoutCreateInfo layoutCi{};
        layoutCi.setLayoutCount = 2;
        layoutCi.pSetLayouts = setLayouts;
        impl.geometryLayout.Create(layoutCi);
    }
    impl.geometryPipeline = std::make_unique<vk::pipeline>(CreateDeferredGeometryPipeline(impl.extent, *impl.geometryRenderPass, impl.geometryLayout));
    impl.deferredPipeline.GeometryPass().SetPipeline(*impl.geometryPipeline, impl.geometryLayout);

    {
        VkDescriptorSetLayout setLayout = impl.deferredPipeline.LightingPass().GetDescriptorSetLayout();
        VkPipelineLayoutCreateInfo layoutCi{};
        layoutCi.setLayoutCount = setLayout == VK_NULL_HANDLE ? 0u : 1u;
        layoutCi.pSetLayouts = setLayout == VK_NULL_HANDLE ? nullptr : &setLayout;
        impl.lightingLayout.Create(layoutCi);
    }
    impl.lightingPipeline = std::make_unique<vk::pipeline>(CreateDeferredLightingPipeline(impl.extent, impl.screen->renderPass, impl.lightingLayout));
    impl.deferredPipeline.LightingPass().SetPipeline(*impl.lightingPipeline, impl.lightingLayout);

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
    impl.deferredPipeline.LightingPass().SetLightingParams(
        glm::vec3(0.5f, 1.0f, 0.2f),
        glm::vec3(1.0f, 0.95f, 0.9f),
        glm::vec3(0.0f, 0.0f, 2.0f),
        pointPositions,
        pointColors);

    impl.commandPool.Create(vk::GraphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    impl.commandPool.AllocateBuffers(impl.commandBuffer);
    impl.frameFence = std::make_unique<vk::fence>();
    impl.imageAvailable = std::make_unique<vk::semaphore>();
    impl.renderingOverSemaphores.resize(impl.screen->framebuffers.size());
    for (auto& semaphore : impl.renderingOverSemaphores) {
        semaphore = std::make_unique<vk::semaphore>();
    }
    impl.initialized = true;
    return true;
}

void VulkanRenderer::Shutdown()
{
    auto& impl = *impl_;
    if (!impl.initialized) {
        return;
    }

    vk::GraphicsBase::Base().WaitIdle();
    impl.deferredPipeline.Shutdown();
    te::RenderPassManager::GetInstance().EnableVulkanGraph(false);
    te::RenderPassManager::GetInstance().Clear();
    impl.lightingPipeline.reset();
    impl.geometryPipeline.reset();
    impl.geometryRenderPass.reset();
    impl.imageAvailable.reset();
    impl.frameFence.reset();
    impl.renderingOverSemaphores.clear();
    impl.pendingCommands.clear();
    impl.initialized = false;
    impl.frameBegun = false;

    if (impl.ownsWindow) {
        easy_vk::TerminateWindow();
        impl.ownsWindow = false;
    }
}

void VulkanRenderer::BeginFrame()
{
    auto& impl = *impl_;
    if (!impl.initialized || impl.frameBegun) {
        return;
    }

    mStats.Reset();
    impl.pendingCommands.clear();
    if (!impl.imageAvailable || !impl.frameFence || impl.renderingOverSemaphores.empty()) {
        return;
    }
    vk::GraphicsBase::Base().SwapImage(*impl.imageAvailable);
    const uint32_t imageIndex = vk::GraphicsBase::Base().CurrentImageIndex();
    if (imageIndex < impl.screen->framebuffers.size()) {
        impl.deferredPipeline.LightingPass().SetRenderTargets(impl.screen->renderPass, impl.screen->framebuffers[imageIndex]);
    }

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f),
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                      float(impl.extent.width) / (std::max)(1.0f, float(impl.extent.height)),
                                      0.1f,
                                      100.0f);
    if (mpRenderContext && mpRenderContext->GetAttachedCamera()) {
        view = mpRenderContext->GetAttachedCamera()->GetViewMatrix();
        proj = mpRenderContext->GetAttachedCamera()->GetProjectionMatrix();
    }
    proj[1][1] *= -1.0f;
    impl.deferredPipeline.GeometryPass().SetViewProjection(view, proj);

    if (mpRenderContext && mpRenderContext->GetDefaultLight()) {
        const glm::vec3 lightPos = mpRenderContext->GetDefaultLight()->GetPosition();
        const glm::vec3 lightColor = mpRenderContext->GetDefaultLight()->GetColor();
        std::array<glm::vec4, 4> pointPositions = {
            glm::vec4(lightPos, 1.0f),
            glm::vec4(0.0f),
            glm::vec4(0.0f),
            glm::vec4(0.0f)
        };
        std::array<glm::vec4, 4> pointColors = {
            glm::vec4(lightColor, 1.0f),
            glm::vec4(0.0f),
            glm::vec4(0.0f),
            glm::vec4(0.0f)
        };
        impl.deferredPipeline.LightingPass().SetLightingParams(glm::normalize(glm::vec3(0.5f, 1.0f, 0.2f)),
                                                               lightColor,
                                                               glm::vec3(glm::inverse(view)[3]),
                                                               pointPositions,
                                                               pointColors);
    }

    impl.commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    te::RenderPassManager::GetInstance().SetVulkanCommandBuffer(impl.commandBuffer);
    impl.frameBegun = true;
}

void VulkanRenderer::EndFrame()
{
    auto& impl = *impl_;
    if (!impl.initialized || !impl.frameBegun) {
        return;
    }

    if (mMultiPassEnabled) {
        te::RenderPassManager::GetInstance().ExecuteAll(impl.pendingCommands);
    } else {
        impl.deferredPipeline.RecordFrame(impl.commandBuffer, impl.pendingCommands);
    }
    impl.commandBuffer.End();

    const uint32_t imageIndex = vk::GraphicsBase::Base().CurrentImageIndex();
    if (imageIndex >= impl.renderingOverSemaphores.size()) {
        return;
    }
    auto& renderingOver = impl.renderingOverSemaphores[imageIndex];
    if (!renderingOver) {
        return;
    }
    vk::GraphicsBase::Base().SubmitCommandBuffer_Graphics(impl.commandBuffer, *impl.imageAvailable, *renderingOver, *impl.frameFence);
    vk::GraphicsBase::Base().PresentImage(*renderingOver);
    impl.frameFence->WaitAndReset();

    for (const auto& cmd : impl.pendingCommands) {
        if (!cmd.fragmentsSource) {
            continue;
        }
        auto& frag = cmd.fragmentsSource->GetDefaultFragment();
        if (!frag.mpGeometry) {
            continue;
        }
        // Avoid Fragment::IsReady() in Vulkan path: it triggers OpenGL VAO/VBO setup.
        ++mStats.drawCalls;
        mStats.vertices += static_cast<uint32_t>(frag.mpGeometry->GetVertices().size());
        mStats.triangles += static_cast<uint32_t>(frag.mpGeometry->GetIndices().size() / 3);
    }

    impl.pendingCommands.clear();
    impl.frameBegun = false;
}

void VulkanRenderer::DrawMesh(const RenderCommand& command)
{
    if (!impl_ || !impl_->initialized) {
        return;
    }
    impl_->pendingCommands.push_back(command);
}

void VulkanRenderer::DrawMesh(const std::shared_ptr<Mesh> pMesh)
{
    if (!pMesh) {
        return;
    }
    RenderCommand command;
    command.fragmentsSource = pMesh;
    command.state = RenderMode::Opaque;
    command.hasUV = true;
    DrawMesh(command);
}

void VulkanRenderer::DrawMeshes(const std::vector<RenderCommand>& commands)
{
    if (!impl_ || !impl_->initialized) {
        return;
    }
    impl_->pendingCommands.insert(impl_->pendingCommands.end(), commands.begin(), commands.end());
}

void VulkanRenderer::SetViewport(uint16_t, uint16_t, uint16_t width, uint16_t height)
{
    viewportWidth_ = width;
    viewportHeight_ = height;
}

void VulkanRenderer::SetClearColor(float r, float g, float b, float a)
{
    mClearColor = glm::vec4(r, g, b, a);
}

void VulkanRenderer::Clear(uint32_t)
{
}

void VulkanRenderer::AddRenderPass(const std::shared_ptr<te::RenderPass>& pass)
{
    te::RenderPassManager::GetInstance().AddPass(pass);
}

void VulkanRenderer::RemoveRenderPass(const std::string& name)
{
    te::RenderPassManager::GetInstance().RemovePass(name);
}

std::shared_ptr<te::RenderPass> VulkanRenderer::GetRenderPass(const std::string& name) const
{
    return te::RenderPassManager::GetInstance().GetPass(name);
}

void VulkanRenderer::ExecuteRenderPasses(const std::vector<RenderCommand>& commands)
{
    if (!impl_ || !impl_->initialized) {
        return;
    }
    DrawMeshes(commands);
}

void VulkanRenderer::SetRenderContext(const std::shared_ptr<RenderContext>& pRenderContext)
{
    mpRenderContext = pRenderContext;
}
