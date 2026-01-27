#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "textures/Texture.h"

namespace te
{
    // Render target type
    enum class RenderTargetType
    {
        Color,      // Color buffer
        Depth,      // Depth buffer
        Stencil,    // Stencil buffer
        ColorDepth, // Color + Depth
        ColorDepthStencil // Color + Depth + Stencil
    };

    // Render target format
    enum class RenderTargetFormat
    {
        RGB8,       // 8-bit RGB
        RGBA8,      // 8-bit RGBA
        RGB16F,     // 16-bit floating point RGB
        RGBA16F,    // 16-bit floating point RGBA
        RGB32F,     // 32-bit floating point RGB
        RGBA32F,    // 32-bit floating point RGBA
        Depth24,    // 24-bit depth
        Depth32F,   // 32-bit floating point depth
        Depth24Stencil8 // 24-bit depth + 8-bit stencil
    };

    // Render target description
    struct RenderTargetDesc
    {
        std::string name;                    // Target name
        RenderTargetType type;               // Target type
        RenderTargetFormat format;           // Data format
        uint32_t width = 0;                  // Width
        uint32_t height = 0;                 // Height
        bool generateMipmaps = false;        // Whether to generate mipmaps
        GLenum wrapMode = GL_CLAMP_TO_EDGE;  // Wrap mode
        GLenum filterMode = GL_LINEAR;       // Filter mode
        
        RenderTargetDesc() = default;
        RenderTargetDesc(const std::string& n, RenderTargetType t, RenderTargetFormat f, 
                        uint32_t w, uint32_t h)
            : name(n), type(t), format(f), width(w), height(h) {}
    };

    // Render target
    class RenderTarget
    {
    public:
        RenderTarget();
        ~RenderTarget();

        bool Initialize(const RenderTargetDesc& desc);
        void Shutdown();

        // Get resources
        GLuint GetTextureHandle() const { return mTextureHandle; }
        GLuint GetFramebufferHandle() const { return mFramebufferHandle; }
        const RenderTargetDesc& GetDesc() const { return mDesc; }

        // Bind/Unbind
        void Bind();
        void Unbind();

        // Check completeness
        bool IsComplete() const;

        // Set viewport
        void SetViewport();
        void Update(const RenderTargetDesc& desc);

    private:
        void CreateTexture();
        void CreateFramebuffer();
        GLenum GetInternalFormat() const;
        GLenum GetFormat() const;
        GLenum GetDataType() const;

        RenderTargetDesc mDesc;
        GLuint mTextureHandle = 0;
        GLuint mFramebufferHandle = 0;
        bool mInitialized = false;
    };

    // Multiple render targets (MRT)
    class MultiRenderTarget
    {
    public:
        MultiRenderTarget();
        ~MultiRenderTarget();

        bool Initialize(uint32_t width, uint32_t height);
        void Shutdown();

        // Add render target
        bool AddRenderTarget(const RenderTargetDesc& desc);
        void RemoveRenderTarget(const std::string& name);

        // Get render target
        std::shared_ptr<RenderTarget> GetRenderTarget(const std::string& name) const;
        std::shared_ptr<RenderTarget> GetRenderTarget(size_t index) const;

        // Bind/Unbind
        void Bind();
        void Unbind();

        // Set draw buffers
        void SetDrawBuffers(const std::vector<std::string>& targetNames);

        // Check completeness
        bool IsComplete() const;

        // Set viewport
        void SetViewport();

        // Get dimensions
        uint32_t GetWidth() const { return mWidth; }
        uint32_t GetHeight() const { return mHeight; }
        void Resize(uint32_t width, uint32_t height);

    private:
        void UpdateDrawBuffers();

        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
        GLuint mFramebufferHandle = 0;
        std::vector<std::shared_ptr<RenderTarget>> mRenderTargets;
        std::unordered_map<std::string, size_t> mTargetIndexMap;
        std::vector<GLenum> mDrawBuffers;
        bool mInitialized = false;
    };

    // FrameBuffer manager
    class FrameBufferManager
    {
    public:
        static FrameBufferManager& GetInstance();

        // Create/Destroy FrameBuffer
        std::shared_ptr<MultiRenderTarget> CreateFrameBuffer(const std::string& name, 
                                                           uint32_t width, uint32_t height);
        void DestroyFrameBuffer(const std::string& name);

        // Get FrameBuffer
        std::shared_ptr<MultiRenderTarget> GetFrameBuffer(const std::string& name) const;

        // Clear all FrameBuffers
        void Clear();

    private:
        FrameBufferManager() = default;
        ~FrameBufferManager() = default;

        std::unordered_map<std::string, std::shared_ptr<MultiRenderTarget>> mFrameBuffers;
    };
}
