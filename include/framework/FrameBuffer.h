#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "textures/Texture.h"

namespace te
{
    // 渲染目标类型
    enum class RenderTargetType
    {
        Color,      // 颜色缓冲区
        Depth,      // 深度缓冲区
        Stencil,    // 模板缓冲区
        ColorDepth, // 颜色+深度
        ColorDepthStencil // 颜色+深度+模板
    };

    // 渲染目标格式
    enum class RenderTargetFormat
    {
        RGB8,       // 8位RGB
        RGBA8,      // 8位RGBA
        RGB16F,     // 16位浮点RGB
        RGBA16F,    // 16位浮点RGBA
        RGB32F,     // 32位浮点RGB
        RGBA32F,    // 32位浮点RGBA
        Depth24,    // 24位深度
        Depth32F,   // 32位浮点深度
        Depth24Stencil8 // 24位深度+8位模板
    };

    // 渲染目标描述
    struct RenderTargetDesc
    {
        std::string name;                    // 目标名称
        RenderTargetType type;               // 目标类型
        RenderTargetFormat format;           // 数据格式
        uint32_t width = 0;                  // 宽度
        uint32_t height = 0;                 // 高度
        bool generateMipmaps = false;        // 是否生成Mipmap
        GLenum wrapMode = GL_CLAMP_TO_EDGE;  // 包装模式
        GLenum filterMode = GL_LINEAR;       // 过滤模式
        
        RenderTargetDesc() = default;
        RenderTargetDesc(const std::string& n, RenderTargetType t, RenderTargetFormat f, 
                        uint32_t w, uint32_t h)
            : name(n), type(t), format(f), width(w), height(h) {}
    };

    // 渲染目标
    class RenderTarget
    {
    public:
        RenderTarget();
        ~RenderTarget();

        bool Initialize(const RenderTargetDesc& desc);
        void Shutdown();

        // 获取资源
        GLuint GetTextureHandle() const { return mTextureHandle; }
        GLuint GetFramebufferHandle() const { return mFramebufferHandle; }
        const RenderTargetDesc& GetDesc() const { return mDesc; }

        // 绑定/解绑
        void Bind();
        void Unbind();

        // 检查完整性
        bool IsComplete() const;

        // 设置视口
        void SetViewport();

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

    // 多重渲染目标（MRT）
    class MultiRenderTarget
    {
    public:
        MultiRenderTarget();
        ~MultiRenderTarget();

        bool Initialize(uint32_t width, uint32_t height);
        void Shutdown();

        // 添加渲染目标
        bool AddRenderTarget(const RenderTargetDesc& desc);
        void RemoveRenderTarget(const std::string& name);

        // 获取渲染目标
        std::shared_ptr<RenderTarget> GetRenderTarget(const std::string& name) const;
        std::shared_ptr<RenderTarget> GetRenderTarget(size_t index) const;

        // 绑定/解绑
        void Bind();
        void Unbind();

        // 设置绘制缓冲区
        void SetDrawBuffers(const std::vector<std::string>& targetNames);

        // 检查完整性
        bool IsComplete() const;

        // 设置视口
        void SetViewport();

        // 获取尺寸
        uint32_t GetWidth() const { return mWidth; }
        uint32_t GetHeight() const { return mHeight; }

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

    // FrameBuffer管理器
    class FrameBufferManager
    {
    public:
        static FrameBufferManager& GetInstance();

        // 创建/销毁FrameBuffer
        std::shared_ptr<MultiRenderTarget> CreateFrameBuffer(const std::string& name, 
                                                           uint32_t width, uint32_t height);
        void DestroyFrameBuffer(const std::string& name);

        // 获取FrameBuffer
        std::shared_ptr<MultiRenderTarget> GetFrameBuffer(const std::string& name) const;

        // 清理所有FrameBuffer
        void Clear();

    private:
        FrameBufferManager() = default;
        ~FrameBufferManager() = default;

        std::unordered_map<std::string, std::shared_ptr<MultiRenderTarget>> mFrameBuffers;
    };
}
