#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "materials/BaseMaterial.h"
#include "framework/RenderPassFlag.h"
#include "Fragment.h"
#include "mesh/Mesh.h"

class Camera;
class Light;
class Shader;
class RenderContext;
class RenderView;

// Forward declarations for multi-pass rendering
namespace te
{
    class RenderPass;
    class MultiRenderTarget;
}

enum class RenderMode
{
    Opaque,
    Transparent,
    Wireframe,
    Points,
    Lines
};

// with renderobject
struct RenderCommand
{
    std::shared_ptr<FragmentsSource> fragmentsSource{ nullptr };

    RenderMode state;
    bool hasUV;
    RenderPassFlag renderpassflag;
    
    RenderCommand() : state(RenderMode::Opaque), hasUV(false), renderpassflag(RenderPassFlag::None) {}
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
    virtual void DrawMesh(const std::shared_ptr<Mesh> pMesh) = 0;
    
    // batch rendering
    virtual void DrawMeshes(const std::vector<RenderCommand>& commands) = 0;
    
    virtual void SetViewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height) = 0;
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
    virtual void ExecuteRenderPasses(const std::vector<RenderCommand>& commands = {}) = 0;
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
    static std::shared_ptr<IRenderer> CreateRenderer(RendererBackend backend);
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
    /*void DrawMesh(const std::vector<Vertex>& vertices, 
                 const std::vector<unsigned int>& indices,
                 const std::shared_ptr<MaterialBase>& material,
                 const glm::mat4& transform = glm::mat4(1.0f)) override;*/
    void DrawMesh(const std::shared_ptr<Mesh> pMesh) override;
    
    void DrawMeshes(const std::vector<RenderCommand>& commands) override;

    void SetViewport(uint16_t x, uint16_t y, uint16_t width, uint16_t height) override;
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
    void ExecuteRenderPasses(const std::vector<RenderCommand>& commands = {}) override;

    void SetRenderContext(const std::shared_ptr<RenderContext>& pRenderContext) override;
private:
    void ApplyRenderState(RenderMode state);
    
    RenderStats mStats;
    
    // multi-pass rendering
    bool mMultiPassEnabled{ false };
    std::vector<std::shared_ptr<te::RenderPass>> mRenderPasses;
    std::unordered_map<std::string, size_t> mRenderPassIndexMap;
    
    // OpenGL statu
    uint32_t mCurrentVAO = 0;
    uint32_t mCurrentVBO = 0;
    uint32_t mCurrentEBO = 0;
    RenderMode mCurrentState = RenderMode::Opaque;
    
    std::unordered_map<size_t, MeshCache> mMeshCache;

    std::shared_ptr<RenderContext> mpRenderContext{ nullptr };

    std::shared_ptr<RenderView> mpRenderView{ nullptr };
}; 