#include "framework/Renderer.h"
#include "framework/RenderPass.h"
#include "glad/glad.h"
#include "shader.h"
#include "Camera.h"
#include "Light.h"
#include <iostream>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include "framework/RenderContext.h"

// Render Factory Impl
std::shared_ptr<IRenderer> RendererFactory::CreateRenderer(RendererBackend backend)
{
    std::cout << "RendererFactory::CreateRenderer called with backend: " << static_cast<int>(backend) << std::endl;
    
    switch (backend)
    {
    case RendererBackend::OpenGL:
        std::cout << "Creating OpenGL renderer..." << std::endl;
        return std::make_shared<OpenGLRenderer>();
    case RendererBackend::OpenGLES:
        // TODO: OpenGLES renderer Impl
        std::cout << "OpenGLES renderer not implemented" << std::endl;
        return nullptr;
    case RendererBackend::Vulkan:
        // TODO: Vulkan renderer Impl
        std::cout << "Vulkan renderer not implemented" << std::endl;
        return nullptr;
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
    
    // OpenGL 已经在其他地方初始化，这里只需要设置一些默认状态
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);  // 设置深度函数
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // 设置默认清除颜色
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    
    // 设置点大小（用于 Points 渲染状态）
    glPointSize(1.0f);
    
    // 设置线宽（用于 Lines 和 Wireframe 渲染状态）
    glLineWidth(1.0f);
    
    mStats.Reset();

    std::cout << "OpenGLRenderer::Initialize completed successfully" << std::endl;
    return true;
}

void OpenGLRenderer::Shutdown()
{
    // 清理缓存的网格数据
    for (auto& [hash, cache] : mMeshCache)
    {
        CleanupMeshBuffers(cache.vao, cache.vbo, cache.ebo);
    }
    mMeshCache.clear();
}

void OpenGLRenderer::BeginFrame()
{
    mStats.Reset();

    // init bachground
    
}

void OpenGLRenderer::EndFrame()
{
    // 可以在这里添加帧结束时的清理工作
}

void OpenGLRenderer::DrawMesh(const RenderCommand& command)
{
   DrawMesh(command.vertices, command.indices, command.material, command.transform);
}

void OpenGLRenderer::DrawMesh(const std::vector<Vertex>& vertices, 
                            const std::vector<unsigned int>& indices,
                            const std::shared_ptr<MaterialBase>& material,
                            const glm::mat4& transform)
{
   if (!material || vertices.empty() || indices.empty() || (!mpRenderContext))
       return;

   // 应用渲染状态
   ApplyRenderState(RenderMode::Opaque);

   // 设置材质
   material->OnApply();
   
   // 设置变换矩阵
   material->GetShader()->setMat4("model", transform);
   
   // 设置相机和光照参数
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
   
   // 更新材质 uniform
   material->UpdateUniform();
   
   // 绑定材质资源
   material->OnBind();

   // 计算网格哈希用于缓存
   size_t meshHash = std::hash<std::string>{}(std::string((char*)vertices.data(), vertices.size() * sizeof(Vertex))) ^
                    std::hash<std::string>{}(std::string((char*)indices.data(), indices.size() * sizeof(unsigned int)));

   // 查找或创建缓存的网格数据
   auto it = mMeshCache.find(meshHash);
   if (it == mMeshCache.end())
   {
       MeshCache cache;
       SetupMeshBuffers(vertices, indices, cache.vao, cache.vbo, cache.ebo);
       cache.vertexCount = vertices.size();
       cache.indexCount = indices.size();
       mMeshCache[meshHash] = cache;
       it = mMeshCache.find(meshHash);
   }

   // 绑定 VAO 并绘制
   glBindVertexArray(it->second.vao);
   glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(it->second.indexCount), GL_UNSIGNED_INT, 0);
   glBindVertexArray(0);

   // 更新统计信息
   mStats.drawCalls++;
   mStats.triangles += indices.size() / 3;
   mStats.vertices += vertices.size();
}

void OpenGLRenderer::DrawMeshes(const std::vector<RenderCommand>& commands)
{
   for (const auto& command : commands)
   {
       DrawMesh(command);
   }
}

void OpenGLRenderer::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y), 
               static_cast<GLsizei>(width), static_cast<GLsizei>(height));
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

void OpenGLRenderer::SetupMeshBuffers(const std::vector<Vertex>& vertices, 
                                     const std::vector<unsigned int>& indices,
                                     uint32_t& vao, uint32_t& vbo, uint32_t& ebo)
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // 位置属性
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // 法线属性
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // UV 坐标属性
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    glBindVertexArray(0);
}

void OpenGLRenderer::CleanupMeshBuffers(uint32_t vao, uint32_t vbo, uint32_t ebo)
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
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
        glDisable(GL_CULL_FACE);  // 点渲染通常不需要面剔除
        glPointSize(2.0f);        // 设置点大小，使其更可见
        break;
    case RenderMode::Lines:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);  // 线渲染通常不需要面剔除
        glLineWidth(1.0f);        // 设置线宽
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
    
    // 检查是否已存在
    if (mRenderPassIndexMap.find(name) != mRenderPassIndexMap.end())
    {
        std::cout << "RenderPass with name '" << name << "' already exists" << std::endl;
        return;
    }

    // 添加到列表
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

    // 重新构建索引映射
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

    // 按依赖关系排序Pass
    // 这里使用简单的排序，实际应该实现拓扑排序
    std::sort(mRenderPasses.begin(), mRenderPasses.end(),
        [](const std::shared_ptr<te::RenderPass>& a, const std::shared_ptr<te::RenderPass>& b) {
            return static_cast<int>(a->GetConfig().type) < static_cast<int>(b->GetConfig().type);
        });

    // 执行所有Pass
    for (const auto& pass : mRenderPasses)
    {
        if (!pass->IsEnabled())
            continue;

        // 设置输入纹理
        for (const auto& input : pass->GetConfig().inputs)
        {
            auto sourcePass = GetRenderPass(input.sourcePass);
            if (sourcePass)
            {
                auto outputTarget = sourcePass->GetOutput(input.sourceTarget);
                if (outputTarget)
                {
                    pass->SetInput(input.name, outputTarget->GetTextureHandle());
                }
            }
        }

        // 执行Pass
        pass->Execute(commands);
    }
} 

void OpenGLRenderer::SetRenderContext(const std::shared_ptr<RenderContext>& pRenderContext)
{
    mpRenderContext = pRenderContext;
}
