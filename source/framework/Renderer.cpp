#include "framework/Renderer.h"
#include "glad/glad.h"
#include "shader.h"
#include "Camera.h"
#include "Light.h"
#include <iostream>
#include <unordered_map>
#include <functional>

#include "filesystem.h"
#include "skybox/Skybox.h"

// 渲染器工厂实现
std::unique_ptr<IRenderer> RendererFactory::CreateRenderer(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::OpenGL:
        return std::make_unique<OpenGLRenderer>();
    case RendererBackend::OpenGLES:
        // TODO: 实现 OpenGLES 渲染器
        return nullptr;
    case RendererBackend::Vulkan:
        // TODO: 实现 Vulkan 渲染器
        return nullptr;
    default:
        return nullptr;
    }
}

// OpenGL 渲染器实现
OpenGLRenderer::OpenGLRenderer()
{
}

OpenGLRenderer::~OpenGLRenderer()
{
    Shutdown();
}

bool OpenGLRenderer::Initialize()
{
    // OpenGL 已经在其他地方初始化，这里只需要设置一些默认状态
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    mStats.Reset();
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
    if (!mpSkybox)
    {
        std::vector<std::string> faces
        {
            FileSystem::getPath("resources/textures/skybox/right.jpg"),
            FileSystem::getPath("resources/textures/skybox/left.jpg"),
            FileSystem::getPath("resources/textures/skybox/top.jpg"),
            FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
            FileSystem::getPath("resources/textures/skybox/front.jpg"),
            FileSystem::getPath("resources/textures/skybox/back.jpg")
        };
        mpSkybox = std::make_shared<Skybox>(faces);
    }
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
    if (!material || vertices.empty() || indices.empty())
        return;

    // 应用渲染状态
    ApplyRenderState(RenderState::Opaque);

    // 设置材质
    material->OnApply();
    
    // 设置变换矩阵
    material->GetShader()->setMat4("model", transform);
    
    // 设置相机和光照参数
    if (mpCamera)
    {
        material->GetShader()->setMat4("view", mpCamera->GetViewMatrix());
        material->GetShader()->setMat4("projection", mpCamera->GetProjectionMatrix());
    }
    
    if (mpLight)
    {
        material->GetShader()->setVec3("u_lightPos", mpLight->GetPosition());
        material->GetShader()->setVec3("u_lightColor", mpLight->GetColor());
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

void OpenGLRenderer::DrawBackgroud()
{
    //render skybox first
    if (mpSkybox)
    {
        mpSkybox->Draw(mpCamera->GetViewMatrix(), mpCamera->GetProjectionMatrix());
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

void OpenGLRenderer::ApplyRenderState(RenderState state)
{
    if (mCurrentState == state)
        return;

    switch (state)
    {
    case RenderState::Opaque:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        break;
    case RenderState::Wireframe:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        break;
    case RenderState::Points:
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glEnable(GL_DEPTH_TEST);
        break;
    case RenderState::Transparent:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }

    mCurrentState = state;
} 