#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "mesh/Vertex.h"
#include "materials/BaseMaterial.h"

class Camera;
class Light;
class Shader;
class RenderContext;

// Forward declarations for multi-pass rendering
namespace te
{
    class RenderPass;
    class MultiRenderTarget;
}

enum class RenderState
{
    Opaque,
    Transparent,
    Wireframe,
    Points,
    Lines
};

struct RenderCommand
{
    std::shared_ptr<MaterialBase> material;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::mat4 transform;
    RenderState state;
    bool hasUV;
    
    RenderCommand() : transform(1.0f), state(RenderState::Opaque), hasUV(false) {}
};

struct RenderStats
{
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    uint32_t vertices = 0;
    
    void Reset() { drawCalls = 0; triangles = 0; vertices = 0; }
};

class IRenderer
{
public:
    virtual ~IRenderer() = default;
    
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    
    // frame begine/end
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    
    // core rendering api
    virtual void DrawMesh(const RenderCommand& command) = 0;
    virtual void DrawMesh(const std::vector<Vertex>& vertices, 
                         const std::vector<unsigned int>& indices,
                         const std::shared_ptr<MaterialBase>& material,
                         const glm::mat4& transform = glm::mat4(1.0f)) = 0;
    
    // batch rendering
    virtual void DrawMeshes(const std::vector<RenderCommand>& commands) = 0;
    
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void SetClearColor(float r, float g, float b, float a) = 0;
    virtual void Clear(uint32_t flags) = 0;
    
    virtual const RenderStats& GetRenderStats() const = 0;
    virtual void ResetRenderStats() = 0;
    
    // multi-pass rendering support
    virtual void SetMultiPassEnabled(bool enabled) = 0;
    virtual bool IsMultiPassEnabled() const = 0;
    virtual void AddRenderPass(const std::shared_ptr<te::RenderPass>& pass) = 0;
    virtual void RemoveRenderPass(const std::string& name) = 0;
    virtual std::shared_ptr<te::RenderPass> GetRenderPass(const std::string& name) const = 0;
    virtual void ExecuteRenderPasses() = 0;
    virtual void SetRenderContext(const std::shared_ptr<RenderContext>& pRenderContext) = 0;
};

enum class RendererBackend
{
    OpenGL,
    OpenGLES,
    Vulkan
};

class RendererFactory
{
public:
    static std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend);
};

class OpenGLRenderer : public IRenderer
{
public:
    OpenGLRenderer();
    ~OpenGLRenderer();
    
    bool Initialize() override;
    void Shutdown() override;
    
    void BeginFrame() override;
    void EndFrame() override;
    
    void DrawMesh(const RenderCommand& command) override;
    void DrawMesh(const std::vector<Vertex>& vertices, 
                 const std::vector<unsigned int>& indices,
                 const std::shared_ptr<MaterialBase>& material,
                 const glm::mat4& transform = glm::mat4(1.0f)) override;
    
    void DrawMeshes(const std::vector<RenderCommand>& commands) override;

    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void SetClearColor(float r, float g, float b, float a) override;
    void Clear(uint32_t flags) override;
    
    const RenderStats& GetRenderStats() const override { return mStats; }
    void ResetRenderStats() override { mStats.Reset(); }
    
    // multi-pass rendering support
    void SetMultiPassEnabled(bool enabled) override { mMultiPassEnabled = enabled; }
    bool IsMultiPassEnabled() const override { 
        return mMultiPassEnabled; 
    }
    void AddRenderPass(const std::shared_ptr<te::RenderPass>& pass) override;
    void RemoveRenderPass(const std::string& name) override;
    std::shared_ptr<te::RenderPass> GetRenderPass(const std::string& name) const override;
    void ExecuteRenderPasses() override;

    //
    void SetRenderContext(const std::shared_ptr<RenderContext>& pRenderContext) override;
private:
    void SetupMeshBuffers(const std::vector<Vertex>& vertices, 
                         const std::vector<unsigned int>& indices,
                         uint32_t& vao, uint32_t& vbo, uint32_t& ebo);
    void CleanupMeshBuffers(uint32_t vao, uint32_t vbo, uint32_t ebo);
    void ApplyRenderState(RenderState state);
    
    RenderStats mStats;
    
    // multi-pass rendering
    bool mMultiPassEnabled{ false };
    std::vector<std::shared_ptr<te::RenderPass>> mRenderPasses;
    std::unordered_map<std::string, size_t> mRenderPassIndexMap;
    
    // OpenGL statu
    uint32_t mCurrentVAO = 0;
    uint32_t mCurrentVBO = 0;
    uint32_t mCurrentEBO = 0;
    RenderState mCurrentState = RenderState::Opaque;
    
    // cache
    struct MeshCache
    {
        uint32_t vao, vbo, ebo;
        size_t vertexCount, indexCount;
    };
    std::unordered_map<size_t, MeshCache> mMeshCache;

    std::shared_ptr<RenderContext> mpRenderContext;
}; 