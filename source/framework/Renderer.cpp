#include "framework/Renderer.h"
#include "framework/RenderPass.h"
#include "framework/RenderPassManager.h"
#include "framework/VulkanDeferredPipeline.h"
#include "framework/VulkanGeometryPass.h"
#include "framework/VulkanPostProcessPass.h"
#include "framework/VulkanPresentPass.h"
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
#include <new>
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
struct OffscreenColorTarget {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

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

vk::RenderPass CreateSingleColorRenderPass(VkFormat format)
{
    VkAttachmentDescription color{};
    color.format = format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments = &color;
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;
    return vk::RenderPass(ci);
}

bool FindDeviceLocalMemoryType(uint32_t memoryTypeBits, uint32_t& outTypeIndex)
{
    const auto& memProps = vk::GraphicsBase::Base().PhysicalDeviceMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memoryTypeBits & (1u << i)) != 0 &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
            outTypeIndex = i;
            return true;
        }
    }
    return false;
}

bool CreateOffscreenColorTarget(VkExtent2D extent, VkFormat format, VkRenderPass renderPass, OffscreenColorTarget& outTarget)
{
    VkImageCreateInfo imageCi{};
    imageCi.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCi.imageType = VK_IMAGE_TYPE_2D;
    imageCi.format = format;
    imageCi.extent = { extent.width, extent.height, 1 };
    imageCi.mipLevels = 1;
    imageCi.arrayLayers = 1;
    imageCi.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCi.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCi.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(vk::GraphicsBase::Base().Device(), &imageCi, nullptr, &outTarget.image) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(vk::GraphicsBase::Base().Device(), outTarget.image, &memReq);
    uint32_t memoryTypeIndex = UINT32_MAX;
    if (!FindDeviceLocalMemoryType(memReq.memoryTypeBits, memoryTypeIndex)) {
        return false;
    }
    VkMemoryAllocateInfo allocCi{};
    allocCi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocCi.allocationSize = memReq.size;
    allocCi.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(vk::GraphicsBase::Base().Device(), &allocCi, nullptr, &outTarget.memory) != VK_SUCCESS) {
        return false;
    }
    if (vkBindImageMemory(vk::GraphicsBase::Base().Device(), outTarget.image, outTarget.memory, 0) != VK_SUCCESS) {
        return false;
    }

    VkImageViewCreateInfo viewCi{};
    viewCi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCi.image = outTarget.image;
    viewCi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCi.format = format;
    viewCi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCi.subresourceRange.baseMipLevel = 0;
    viewCi.subresourceRange.levelCount = 1;
    viewCi.subresourceRange.baseArrayLayer = 0;
    viewCi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(vk::GraphicsBase::Base().Device(), &viewCi, nullptr, &outTarget.view) != VK_SUCCESS) {
        return false;
    }

    VkFramebufferCreateInfo fbCi{};
    fbCi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCi.renderPass = renderPass;
    fbCi.attachmentCount = 1;
    fbCi.pAttachments = &outTarget.view;
    fbCi.width = extent.width;
    fbCi.height = extent.height;
    fbCi.layers = 1;
    if (vkCreateFramebuffer(vk::GraphicsBase::Base().Device(), &fbCi, nullptr, &outTarget.framebuffer) != VK_SUCCESS) {
        return false;
    }
    return true;
}

void DestroyOffscreenColorTarget(OffscreenColorTarget& target)
{
    if (target.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(vk::GraphicsBase::Base().Device(), target.framebuffer, nullptr);
        target.framebuffer = VK_NULL_HANDLE;
    }
    if (target.view != VK_NULL_HANDLE) {
        vkDestroyImageView(vk::GraphicsBase::Base().Device(), target.view, nullptr);
        target.view = VK_NULL_HANDLE;
    }
    if (target.image != VK_NULL_HANDLE) {
        vkDestroyImage(vk::GraphicsBase::Base().Device(), target.image, nullptr);
        target.image = VK_NULL_HANDLE;
    }
    if (target.memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk::GraphicsBase::Base().Device(), target.memory, nullptr);
        target.memory = VK_NULL_HANDLE;
    }
}

void CmdTransitionColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                             VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

vk::pipeline CreatePostProcessPipeline(VkExtent2D extent, VkRenderPass renderPass, VkPipelineLayout layout)
{
    vk::shaderModule vert("resources/compiled_shaders/deferred_lighting_vert.spv");
    vk::shaderModule frag("resources/compiled_shaders/postprocess_frag.spv");
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

vk::pipeline CreatePresentPipeline(VkExtent2D extent, VkRenderPass renderPass, VkPipelineLayout layout)
{
    vk::shaderModule vert("resources/compiled_shaders/deferred_lighting_vert.spv");
    vk::shaderModule frag("resources/compiled_shaders/present_frag.spv");
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
    bool firstFrame = true;
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
    te::VulkanPostProcessPass postProcessPass{};
    te::VulkanPresentPass presentPass{};
    std::unique_ptr<vk::RenderPass> postProcessRenderPass{};
    std::unique_ptr<vk::pipelineLayout> postProcessLayout{};
    std::unique_ptr<vk::pipeline> postProcessPipeline{};
    std::unique_ptr<vk::pipelineLayout> presentLayout{};
    std::unique_ptr<vk::pipeline> presentPipeline{};
    OffscreenColorTarget lightingTarget{};
    OffscreenColorTarget postTarget{};
    OffscreenColorTarget hybridCompositeTarget{};
    std::function<void()> hybridPreprocess_{};
    std::function<void(VkCommandBuffer, uint32_t)> hybridAfterLighting_{};
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
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = static_cast<uint32_t>(sizeof(glm::vec4));
        VkPipelineLayoutCreateInfo layoutCi{};
        layoutCi.setLayoutCount = 2;
        layoutCi.pSetLayouts = setLayouts;
        layoutCi.pushConstantRangeCount = 1;
        layoutCi.pPushConstantRanges = &pushRange;
        impl.geometryLayout.Create(layoutCi);
    }
    impl.geometryPipeline = std::make_unique<vk::pipeline>(CreateDeferredGeometryPipeline(impl.extent, *impl.geometryRenderPass, impl.geometryLayout));
    impl.deferredPipeline.GeometryPass().SetPipeline(*impl.geometryPipeline, impl.geometryLayout);

    impl.postProcessRenderPass = std::make_unique<vk::RenderPass>(CreateSingleColorRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT));
    if (!CreateOffscreenColorTarget(impl.extent, VK_FORMAT_R16G16B16A16_SFLOAT, *impl.postProcessRenderPass, impl.lightingTarget) ||
        !CreateOffscreenColorTarget(impl.extent, VK_FORMAT_R16G16B16A16_SFLOAT, *impl.postProcessRenderPass, impl.postTarget)) {
        std::cout << "VulkanRenderer::Initialize failed to create offscreen targets." << std::endl;
        return false;
    }
    if (impl.hybridAfterLighting_ &&
        !CreateOffscreenColorTarget(impl.extent, VK_FORMAT_R16G16B16A16_SFLOAT, *impl.postProcessRenderPass, impl.hybridCompositeTarget)) {
        std::cout << "VulkanRenderer::Initialize failed to create hybrid composite target." << std::endl;
        return false;
    }
    impl.deferredPipeline.LightingPass().SetRenderTargets(*impl.postProcessRenderPass, impl.lightingTarget.framebuffer);

    {
        VkDescriptorSetLayout setLayout = impl.deferredPipeline.LightingPass().GetDescriptorSetLayout();
        VkPipelineLayoutCreateInfo layoutCi{};
        layoutCi.setLayoutCount = setLayout == VK_NULL_HANDLE ? 0u : 1u;
        layoutCi.pSetLayouts = setLayout == VK_NULL_HANDLE ? nullptr : &setLayout;
        impl.lightingLayout.Create(layoutCi);
    }
    impl.lightingPipeline = std::make_unique<vk::pipeline>(CreateDeferredLightingPipeline(impl.extent, *impl.postProcessRenderPass, impl.lightingLayout));
    impl.deferredPipeline.LightingPass().SetPipeline(*impl.lightingPipeline, impl.lightingLayout);

    te::VulkanPostProcessPassCreateInfo postCi{};
    postCi.extent = impl.extent;
    postCi.renderPass = *impl.postProcessRenderPass;
    postCi.framebuffer = impl.postTarget.framebuffer;
    if (!impl.postProcessPass.Initialize(postCi)) {
        std::cout << "VulkanRenderer::Initialize failed to initialize post process pass." << std::endl;
        return false;
    }
    impl.postProcessLayout = std::make_unique<vk::pipelineLayout>();
    {
        VkDescriptorSetLayout setLayout = impl.postProcessPass.GetDescriptorSetLayout();
        VkPipelineLayoutCreateInfo layoutCi{};
        layoutCi.setLayoutCount = setLayout == VK_NULL_HANDLE ? 0u : 1u;
        layoutCi.pSetLayouts = setLayout == VK_NULL_HANDLE ? nullptr : &setLayout;
        impl.postProcessLayout->Create(layoutCi);
    }
    impl.postProcessPipeline = std::make_unique<vk::pipeline>(CreatePostProcessPipeline(impl.extent, *impl.postProcessRenderPass, *impl.postProcessLayout));
    impl.postProcessPass.SetPipeline(*impl.postProcessPipeline, *impl.postProcessLayout);
    impl.postProcessPass.SetToneMappingParams(1.0f, 2.2f, true);

    te::VulkanPresentPassCreateInfo presentCi{};
    presentCi.extent = impl.extent;
    presentCi.renderPass = impl.screen->renderPass;
    if (!impl.screen->framebuffers.empty()) {
        presentCi.framebuffer = impl.screen->framebuffers[0];
    }
    if (!impl.presentPass.Initialize(presentCi)) {
        std::cout << "VulkanRenderer::Initialize failed to initialize present pass." << std::endl;
        return false;
    }
    impl.presentLayout = std::make_unique<vk::pipelineLayout>();
    {
        VkDescriptorSetLayout setLayout = impl.presentPass.GetDescriptorSetLayout();
        VkPipelineLayoutCreateInfo layoutCi{};
        layoutCi.setLayoutCount = setLayout == VK_NULL_HANDLE ? 0u : 1u;
        layoutCi.pSetLayouts = setLayout == VK_NULL_HANDLE ? nullptr : &setLayout;
        impl.presentLayout->Create(layoutCi);
    }
    impl.presentPipeline = std::make_unique<vk::pipeline>(CreatePresentPipeline(impl.extent, impl.screen->renderPass, *impl.presentLayout));
    impl.presentPass.SetPipeline(*impl.presentPipeline, *impl.presentLayout);

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

    passMgr.SetVulkanPostProcessCallback([this](VkCommandBuffer commandBuffer) {
        if (!impl_ || !impl_->initialized) return;
        auto& impl2 = *impl_;
        if (impl2.hybridAfterLighting_) {
            CmdTransitionColorImage(commandBuffer, impl2.hybridCompositeTarget.image,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        } else {
            CmdTransitionColorImage(commandBuffer, impl2.lightingTarget.image,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        CmdTransitionColorImage(commandBuffer, impl2.postTarget.image,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        const VkImageView postInput = impl2.hybridAfterLighting_ ? impl2.hybridCompositeTarget.view : impl2.lightingTarget.view;
        impl2.postProcessPass.Record(commandBuffer, postInput);
    });
    passMgr.SetVulkanPresentCallback([this](VkCommandBuffer commandBuffer) {
        if (!impl_ || !impl_->initialized) return;
        auto& impl2 = *impl_;
        CmdTransitionColorImage(commandBuffer, impl2.postTarget.image,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        impl2.presentPass.Record(commandBuffer, impl2.postTarget.view);
    });
    passMgr.SetVulkanHybridAfterLightingCallback(impl.hybridAfterLighting_);
    if (!passMgr.BuildVulkanDeferredGraph(&impl.deferredPipeline)) {
        std::cout << "VulkanRenderer::Initialize failed to rebuild Vulkan graph with post/present." << std::endl;
        return false;
    }

    impl.commandPool.Create(vk::GraphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    impl.commandPool.AllocateBuffers(impl.commandBuffer);
    impl.frameFence = std::make_unique<vk::fence>();
    impl.imageAvailable = std::make_unique<vk::semaphore>();
    impl.renderingOverSemaphores.resize(impl.screen->framebuffers.size());
    for (auto& semaphore : impl.renderingOverSemaphores) {
        semaphore = std::make_unique<vk::semaphore>();
    }
    impl.firstFrame = true;
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

    // Pipelines and pipeline layouts reference descriptor set layouts owned by deferred / post / present passes.
    // Destroy pipelines and layouts before pass Shutdown() runs vkDestroyDescriptorSetLayout on those handles.
    impl.geometryPipeline.reset();
    impl.lightingPipeline.reset();
    impl.presentPipeline.reset();
    impl.postProcessPipeline.reset();

    impl.geometryLayout.~pipelineLayout();
    new (&impl.geometryLayout) vk::pipelineLayout();
    impl.lightingLayout.~pipelineLayout();
    new (&impl.lightingLayout) vk::pipelineLayout();

    impl.presentLayout.reset();
    impl.postProcessLayout.reset();

    impl.deferredPipeline.Shutdown();
    impl.presentPass.Shutdown();
    impl.postProcessPass.Shutdown();
    te::RenderPassManager::GetInstance().EnableVulkanGraph(false);
    te::RenderPassManager::GetInstance().Clear();
    DestroyOffscreenColorTarget(impl.postTarget);
    DestroyOffscreenColorTarget(impl.hybridCompositeTarget);
    DestroyOffscreenColorTarget(impl.lightingTarget);
    impl.postProcessRenderPass.reset();
    impl.geometryRenderPass.reset();
    impl.imageAvailable.reset();
    impl.frameFence.reset();
    impl.renderingOverSemaphores.clear();
    impl.pendingCommands.clear();
    impl.initialized = false;
    impl.firstFrame = true;
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
    te::RenderPassManager::GetInstance().SetVulkanCurrentSwapchainImageIndex(imageIndex);
    if (imageIndex < impl.screen->framebuffers.size()) {
        impl.presentPass.SetRenderTargets(impl.screen->renderPass, impl.screen->framebuffers[imageIndex]);
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

    {
        const glm::mat4 vp = proj * view;
        const glm::mat4 invVp = glm::inverse(vp);
        float zNear = 0.1f;
        float zFar = 100.0f;
        if (mpRenderContext && mpRenderContext->GetAttachedCamera()) {
            zNear = mpRenderContext->GetAttachedCamera()->GetNearPlane();
            zFar = mpRenderContext->GetAttachedCamera()->GetFarPlane();
        }
        impl.deferredPipeline.LightingPass().SetDeferredFrameMatrices(invVp, zNear, zFar, impl.extent);
    }

    impl.commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    if (impl.lightingTarget.image != VK_NULL_HANDLE) {
        if (impl.firstFrame) {
            CmdTransitionColorImage(impl.commandBuffer, impl.lightingTarget.image,
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            CmdTransitionColorImage(impl.commandBuffer, impl.postTarget.image,
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    0, VK_ACCESS_SHADER_READ_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            if (impl.hybridAfterLighting_ && impl.hybridCompositeTarget.image != VK_NULL_HANDLE) {
                CmdTransitionColorImage(impl.commandBuffer, impl.hybridCompositeTarget.image,
                                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            }
        } else {
            CmdTransitionColorImage(impl.commandBuffer, impl.lightingTarget.image,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        }
    }
    te::RenderPassManager::GetInstance().SetVulkanCommandBuffer(impl.commandBuffer);
    impl.frameBegun = true;
}

void VulkanRenderer::EndFrame()
{
    auto& impl = *impl_;
    if (!impl.initialized || !impl.frameBegun) {
        return;
    }

    if (impl.hybridPreprocess_) {
        impl.hybridPreprocess_();
    }
    if (mMultiPassEnabled) {
        te::RenderPassManager::GetInstance().ExecuteAll(impl.pendingCommands);
        mStats.vulkanGraphNodesExecuted = te::RenderPassManager::GetInstance().GetLastVulkanGraphPassCount();
    } else {
        impl.deferredPipeline.RecordFrame(impl.commandBuffer, impl.pendingCommands);
        mStats.vulkanGraphNodesExecuted = 0;
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
    impl.firstFrame = false;
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

void VulkanRenderer::SetHybridPreprocessCallback(std::function<void()> callback)
{
    if (impl_) {
        impl_->hybridPreprocess_ = std::move(callback);
    }
}

void VulkanRenderer::SetHybridAfterLightingCallback(std::function<void(VkCommandBuffer, uint32_t)> callback)
{
    if (impl_) {
        impl_->hybridAfterLighting_ = std::move(callback);
    }
}

void VulkanRenderer::SetRenderContext(const std::shared_ptr<RenderContext>& pRenderContext)
{
    mpRenderContext = pRenderContext;
}

VkImageView VulkanRenderer::GetLightingTargetView() const
{
    return impl_ ? impl_->lightingTarget.view : VK_NULL_HANDLE;
}

VkImage VulkanRenderer::GetLightingTargetImage() const
{
    return impl_ ? impl_->lightingTarget.image : VK_NULL_HANDLE;
}

VkImage VulkanRenderer::GetHybridCompositeTargetImage() const
{
    return impl_ ? impl_->hybridCompositeTarget.image : VK_NULL_HANDLE;
}

VkImageView VulkanRenderer::GetHybridCompositeTargetView() const
{
    return impl_ ? impl_->hybridCompositeTarget.view : VK_NULL_HANDLE;
}

VkFramebuffer VulkanRenderer::GetHybridCompositeFramebuffer() const
{
    return impl_ ? impl_->hybridCompositeTarget.framebuffer : VK_NULL_HANDLE;
}

VkRenderPass VulkanRenderer::GetHdrPostRenderPass() const
{
    return impl_ && impl_->postProcessRenderPass ? *impl_->postProcessRenderPass : VK_NULL_HANDLE;
}

VkExtent2D VulkanRenderer::GetFramebufferExtent() const
{
    return impl_ ? impl_->extent : VkExtent2D{0, 0};
}

void* VulkanRenderer::GetDeferredPipelineOpaque()
{
    return impl_ ? static_cast<void*>(&impl_->deferredPipeline) : nullptr;
}
