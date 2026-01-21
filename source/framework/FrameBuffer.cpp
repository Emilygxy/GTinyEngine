#include "framework/FrameBuffer.h"
#include "glad/glad.h"
#include <iostream>
#include <algorithm>

namespace te
{
    // RenderTarget Implementation
    RenderTarget::RenderTarget() = default;

    RenderTarget::~RenderTarget()
    {
        Shutdown();
    }

    bool RenderTarget::Initialize(const RenderTargetDesc& desc)
    {
        if (mInitialized)
        {
            std::cout << "RenderTarget already initialized" << std::endl;
            return false;
        }

        mDesc = desc;
        
        CreateTexture();
        CreateFramebuffer();

        mInitialized = true;
        return IsComplete();
    }

    void RenderTarget::Shutdown()
    {
        if (mTextureHandle != 0)
        {
            glDeleteTextures(1, &mTextureHandle);
            mTextureHandle = 0;
        }

        if (mFramebufferHandle != 0)
        {
            glDeleteFramebuffers(1, &mFramebufferHandle);
            mFramebufferHandle = 0;
        }

        mInitialized = false;
    }

    void RenderTarget::CreateTexture()
    {
        glGenTextures(1, &mTextureHandle);
        glBindTexture(GL_TEXTURE_2D, mTextureHandle);

        GLenum internalFormat = GetInternalFormat();
        GLenum format = GetFormat();
        GLenum dataType = GetDataType();

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, mDesc.width, mDesc.height, 0, format, dataType, nullptr);

        // set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mDesc.wrapMode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mDesc.wrapMode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mDesc.filterMode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mDesc.filterMode);

        if (mDesc.generateMipmaps)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void RenderTarget::CreateFramebuffer()
    {
        glGenFramebuffers(1, &mFramebufferHandle);
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferHandle);

        // bind texture to framebuffer
        if (mDesc.type == RenderTargetType::Color || 
            mDesc.type == RenderTargetType::ColorDepth ||
            mDesc.type == RenderTargetType::ColorDepthStencil)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextureHandle, 0);
        }

        if (mDesc.type == RenderTargetType::Depth ||
            mDesc.type == RenderTargetType::ColorDepth ||
            mDesc.type == RenderTargetType::ColorDepthStencil)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mTextureHandle, 0);
        }

        if (mDesc.type == RenderTargetType::ColorDepthStencil)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, mTextureHandle, 0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void RenderTarget::Bind()
    {
        if (!mInitialized) return;
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferHandle);
        SetViewport();
    }

    void RenderTarget::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool RenderTarget::IsComplete() const
    {
        if (!mInitialized) return false;
        
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferHandle);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        return status == GL_FRAMEBUFFER_COMPLETE;
    }

    void RenderTarget::SetViewport()
    {
        glViewport(0, 0, mDesc.width, mDesc.height);
    }

    void RenderTarget::Update(const RenderTargetDesc& desc)
    {
        // Check if update is actually needed (only check width/height for resize)
        bool needsUpdate = (mDesc.width != desc.width || mDesc.height != desc.height || 
                           mDesc.format != desc.format || mDesc.type != desc.type);
        
        if (!needsUpdate && mInitialized)
        {
            return; // No update needed
        }

        // Delete old resources if they exist
        if (mInitialized)
        {
            if (mTextureHandle != 0)
            {
                glDeleteTextures(1, &mTextureHandle);
                mTextureHandle = 0;
            }
            if (mFramebufferHandle != 0)
            {
                glDeleteFramebuffers(1, &mFramebufferHandle);
                mFramebufferHandle = 0;
            }
        }

        mDesc = desc;
        CreateTexture();
        CreateFramebuffer();
        mInitialized = true;
    }

    GLenum RenderTarget::GetInternalFormat() const
    {
        switch (mDesc.format)
        {
        case RenderTargetFormat::RGB8:   return GL_RGB8;
        case RenderTargetFormat::RGBA8:  return GL_RGBA8;
        case RenderTargetFormat::RGB16F: return GL_RGB16F;
        case RenderTargetFormat::RGBA16F: return GL_RGBA16F;
        case RenderTargetFormat::RGB32F: return GL_RGB32F;
        case RenderTargetFormat::RGBA32F: return GL_RGBA32F;
        case RenderTargetFormat::Depth24: return GL_DEPTH_COMPONENT24;
        case RenderTargetFormat::Depth32F: return GL_DEPTH_COMPONENT32F;
        case RenderTargetFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
        default: return GL_RGBA8;
        }
    }

    GLenum RenderTarget::GetFormat() const
    {
        switch (mDesc.format)
        {
        case RenderTargetFormat::RGB8:
        case RenderTargetFormat::RGB16F:
        case RenderTargetFormat::RGB32F:
            return GL_RGB;
        case RenderTargetFormat::RGBA8:
        case RenderTargetFormat::RGBA16F:
        case RenderTargetFormat::RGBA32F:
            return GL_RGBA;
        case RenderTargetFormat::Depth24:
        case RenderTargetFormat::Depth32F:
            return GL_DEPTH_COMPONENT;
        case RenderTargetFormat::Depth24Stencil8:
            return GL_DEPTH_STENCIL;
        default: return GL_RGBA;
        }
    }

    GLenum RenderTarget::GetDataType() const
    {
        switch (mDesc.format)
        {
        case RenderTargetFormat::RGB8:
        case RenderTargetFormat::RGBA8:
            return GL_UNSIGNED_BYTE;
        case RenderTargetFormat::RGB16F:
        case RenderTargetFormat::RGBA16F:
            return GL_HALF_FLOAT;
        case RenderTargetFormat::RGB32F:
        case RenderTargetFormat::RGBA32F:
        case RenderTargetFormat::Depth32F:
            return GL_FLOAT;
        case RenderTargetFormat::Depth24:
            return GL_UNSIGNED_INT;
        case RenderTargetFormat::Depth24Stencil8:
            return GL_UNSIGNED_INT_24_8;
        default: return GL_UNSIGNED_BYTE;
        }
    }

    // MultiRenderTarget Implementation
    MultiRenderTarget::MultiRenderTarget() = default;

    MultiRenderTarget::~MultiRenderTarget()
    {
        Shutdown();
    }

    bool MultiRenderTarget::Initialize(uint32_t width, uint32_t height)
    {
        if (mInitialized)
        {
            std::cout << "MultiRenderTarget already initialized" << std::endl;
            return false;
        }

        mWidth = width;
        mHeight = height;

        glGenFramebuffers(1, &mFramebufferHandle);
        mInitialized = true;
        return true;
    }

    void MultiRenderTarget::Shutdown()
    {
        if (mFramebufferHandle != 0)
        {
            glDeleteFramebuffers(1, &mFramebufferHandle);
            mFramebufferHandle = 0;
        }

        mRenderTargets.clear();
        mTargetIndexMap.clear();
        mDrawBuffers.clear();
        mInitialized = false;
    }

    bool MultiRenderTarget::AddRenderTarget(const RenderTargetDesc& desc)
    {
        if (!mInitialized)
        {
            std::cout << "MultiRenderTarget not initialized" << std::endl;
            return false;
        }

        // check if name already exists
        if (mTargetIndexMap.find(desc.name) != mTargetIndexMap.end())
        {
            std::cout << "RenderTarget with name '" << desc.name << "' already exists" << std::endl;
            return false;
        }

        // create render target
        auto renderTarget = std::make_shared<RenderTarget>();
        if (!renderTarget->Initialize(desc))
        {
            std::cout << "Failed to initialize RenderTarget: " << desc.name << std::endl;
            return false;
        }

        // add to list
        size_t index = mRenderTargets.size();
        mRenderTargets.push_back(renderTarget);
        mTargetIndexMap[desc.name] = index;

        // update draw buffers
        UpdateDrawBuffers();

        return true;
    }

    void MultiRenderTarget::RemoveRenderTarget(const std::string& name)
    {
        auto it = mTargetIndexMap.find(name);
        if (it == mTargetIndexMap.end())
            return;

        size_t index = it->second;
        mRenderTargets.erase(mRenderTargets.begin() + index);
        mTargetIndexMap.erase(it);

        // rebuild index mapping
        mTargetIndexMap.clear();
        for (size_t i = 0; i < mRenderTargets.size(); ++i)
        {
            mTargetIndexMap[mRenderTargets[i]->GetDesc().name] = i;
        }

        UpdateDrawBuffers();
    }

    std::shared_ptr<RenderTarget> MultiRenderTarget::GetRenderTarget(const std::string& name) const
    {
        auto it = mTargetIndexMap.find(name);
        if (it == mTargetIndexMap.end())
            return nullptr;
        
        return mRenderTargets[it->second];
    }

    std::shared_ptr<RenderTarget> MultiRenderTarget::GetRenderTarget(size_t index) const
    {
        if (index >= mRenderTargets.size())
            return nullptr;
        
        return mRenderTargets[index];
    }

    void MultiRenderTarget::Bind()
    {
        if (!mInitialized) return;
        
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferHandle);
        
        // bind all render targets
        for (size_t i = 0; i < mRenderTargets.size(); ++i)
        {
            const auto& target = mRenderTargets[i];
            const auto& desc = target->GetDesc();
            
            if (desc.type == RenderTargetType::Color || 
                desc.type == RenderTargetType::ColorDepth ||
                desc.type == RenderTargetType::ColorDepthStencil)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, 
                                     GL_TEXTURE_2D, target->GetTextureHandle(), 0);
            }
            
            if (desc.type == RenderTargetType::Depth ||
                desc.type == RenderTargetType::ColorDepth ||
                desc.type == RenderTargetType::ColorDepthStencil)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
                                     GL_TEXTURE_2D, target->GetTextureHandle(), 0);
            }
        }
        
        // update draw buffers
        UpdateDrawBuffers();
        
        SetViewport();
    }

    void MultiRenderTarget::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void MultiRenderTarget::SetDrawBuffers(const std::vector<std::string>& targetNames)
    {
        mDrawBuffers.clear();
        
        for (const auto& name : targetNames)
        {
            auto it = mTargetIndexMap.find(name);
            if (it != mTargetIndexMap.end())
            {
                mDrawBuffers.push_back(GL_COLOR_ATTACHMENT0 + it->second);
            }
        }
        
        if (!mDrawBuffers.empty())
        {
            glDrawBuffers(static_cast<GLsizei>(mDrawBuffers.size()), mDrawBuffers.data());
        }
    }

    bool MultiRenderTarget::IsComplete() const
    {
        if (!mInitialized) return false;
        
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferHandle);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        return status == GL_FRAMEBUFFER_COMPLETE;
    }

    void MultiRenderTarget::SetViewport()
    {
        glViewport(0, 0, mWidth, mHeight);
    }

    void MultiRenderTarget::Resize(uint32_t width, uint32_t height)
    {
        // Check if resize is actually needed
        if (mWidth == width && mHeight == height)
        {
            return;
        }

        mWidth = width;
        mHeight = height;

        // Update all RenderTargets with new dimensions
        for (auto& renderTarget : mRenderTargets)
        {
            if (renderTarget)
            {
                RenderTargetDesc desc = renderTarget->GetDesc();
                desc.width = width;
                desc.height = height;
                renderTarget->Update(desc);
            }
        }

        // Recreate framebuffer and rebind all render targets
        if (mFramebufferHandle != 0)
        {
            glDeleteFramebuffers(1, &mFramebufferHandle);
            mFramebufferHandle = 0;
        }

        glGenFramebuffers(1, &mFramebufferHandle);
        
        // Rebind all render targets to the new framebuffer
        // This will be done when Bind() is called next time
    }

    void MultiRenderTarget::UpdateDrawBuffers()
    {
        mDrawBuffers.clear();
        
        for (size_t i = 0; i < mRenderTargets.size(); ++i)
        {
            const auto& desc = mRenderTargets[i]->GetDesc();
            if (desc.type == RenderTargetType::Color || 
                desc.type == RenderTargetType::ColorDepth ||
                desc.type == RenderTargetType::ColorDepthStencil)
            {
                mDrawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
            }
        }
        
        // set draw buffers
        if (!mDrawBuffers.empty())
        {
            glDrawBuffers(static_cast<GLsizei>(mDrawBuffers.size()), mDrawBuffers.data());
        }
    }

    // FrameBufferManager Implementation
    FrameBufferManager& FrameBufferManager::GetInstance()
    {
        static FrameBufferManager instance;
        return instance;
    }

    std::shared_ptr<MultiRenderTarget> FrameBufferManager::CreateFrameBuffer(
        const std::string& name, uint32_t width, uint32_t height)
    {
        auto frameBuffer = std::make_shared<MultiRenderTarget>();
        if (!frameBuffer->Initialize(width, height))
        {
            std::cout << "Failed to create FrameBuffer: " << name << std::endl;
            return nullptr;
        }

        mFrameBuffers[name] = frameBuffer;
        return frameBuffer;
    }

    void FrameBufferManager::DestroyFrameBuffer(const std::string& name)
    {
        auto it = mFrameBuffers.find(name);
        if (it != mFrameBuffers.end())
        {
            mFrameBuffers.erase(it);
        }
    }

    std::shared_ptr<MultiRenderTarget> FrameBufferManager::GetFrameBuffer(const std::string& name) const
    {
        auto it = mFrameBuffers.find(name);
        if (it != mFrameBuffers.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void FrameBufferManager::Clear()
    {
        mFrameBuffers.clear();
    }
}
