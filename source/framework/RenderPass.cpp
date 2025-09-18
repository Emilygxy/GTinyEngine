#include "framework/RenderPass.h"
#include "glad/glad.h"
#include "Camera.h"
#include "Light.h"
#include "RenderView.h"
#include "materials/GeometryMaterial.h"
#include "materials/BlinnPhongMaterial.h"
#include "materials/SkyboxMaterial.h"
#include "framework/FullscreenQuad.h"
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include "framework/RenderContext.h"

namespace te
{
    // RenderPass Implementation
    RenderPass::RenderPass() = default;

    bool RenderPass::Initialize(const std::shared_ptr<RenderView>& pView, const std::shared_ptr<RenderContext>& pContext)
    {
        mpAttachView = pView;
        
        // Call subclass initialization
        OnInitialize();

        // Setup FrameBuffer
        SetupFrameBuffer();
        
        mpRenderContext = pContext;

        return true;
    }

    void RenderPass::Shutdown()
    {
        OnShutdown();
        
        if (mFrameBuffer)
        {
            mFrameBuffer.reset();
        }
        
        mInputTextures.clear();
        mOutputTargets.clear();
    }

    bool RenderPass::CheckDependencies(const std::vector<std::shared_ptr<RenderPass>>& allPasses) const
    {
        for (const auto& dep : mConfig.dependencies)
        {
            if (!dep.required) continue;
            
            // Find dependent Pass
            auto it = std::find_if(allPasses.begin(), allPasses.end(),
                [&dep](const std::shared_ptr<RenderPass>& pass) {
                    return pass->GetConfig().name == dep.passName;
                });
            
            if (it == allPasses.end())
            {
                std::cout << "Missing required dependency: " << dep.passName << std::endl;
                return false;
            }
            
            // Check condition
            if (dep.condition && !dep.condition())
            {
                return false;
            }
        }
        
        return true;
    }

    void RenderPass::SetInput(const std::string& name, GLuint textureHandle)
    {
        mInputTextures[name] = textureHandle;
    }

    GLuint RenderPass::GetInput(const std::string& name) const
    {
        auto it = mInputTextures.find(name);
        return (it != mInputTextures.end()) ? it->second : 0;
    }

    std::shared_ptr<RenderTarget> RenderPass::GetOutput(const std::string& name) const
    {
        auto it = mOutputTargets.find(name);
        return (it != mOutputTargets.end()) ? it->second : nullptr;
    }

    void RenderPass::ApplyRenderSettings()
    {
        // Save current state
        glGetIntegerv(GL_VIEWPORT, mSavedState.viewport);
        mSavedState.depthTest = glIsEnabled(GL_DEPTH_TEST);
        mSavedState.blend = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &mSavedState.blendSrc);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &mSavedState.blendDst);

        // Apply Pass settings
        if (mConfig.useCustomViewport)
        {
            glViewport(mConfig.viewport.x, mConfig.viewport.y, 
                      mConfig.viewport.z, mConfig.viewport.w);
        }

        if (mConfig.enableDepthTest)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(mConfig.depthFunc);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }

        if (mConfig.enableBlend)
        {
            glEnable(GL_BLEND);
            glBlendFunc(mConfig.blendSrc, mConfig.blendDst);
        }
        else
        {
            glDisable(GL_BLEND);
        }
    }

    void RenderPass::RestoreRenderSettings()
    {
        // Restore state
        glViewport(mSavedState.viewport[0], mSavedState.viewport[1], 
                  mSavedState.viewport[2], mSavedState.viewport[3]);
        
        if (mSavedState.depthTest)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
            
        if (mSavedState.blend)
        {
            glEnable(GL_BLEND);
            glBlendFunc(mSavedState.blendSrc, mSavedState.blendDst);
        }
        else
        {
            glDisable(GL_BLEND);
        }
    }
    
    bool RenderPass::FindDependency(const std::string& passname)
    {
        for (auto dependency: mConfig.dependencies)
        {
            if (dependency.passName == passname)
            {
                return true;
            }
        }

        return false;
    }
    void RenderPass::SetupFrameBuffer()
    {
        if (mConfig.outputs.empty())
            return;

        // Get the size of the first output
        uint32_t width = mpAttachView->Width(), height = mpAttachView->Height(); // Default size, should be obtained from configuration or global settings
        
        mFrameBuffer = std::make_shared<MultiRenderTarget>();
        if (!mFrameBuffer->Initialize(width, height))
        {
            std::cout << "Failed to initialize FrameBuffer for pass: " << mConfig.name << std::endl;
            return;
        }

        // Add output targets
        for (const auto& output : mConfig.outputs)
        {
            RenderTargetDesc desc;
            desc.name = output.targetName;
            desc.width = width;
            desc.height = height;
            desc.format = output.format;
            
            // Determine type based on format
            switch (output.format)
            {
            case RenderTargetFormat::Depth24:
            case RenderTargetFormat::Depth32F:
                desc.type = RenderTargetType::Depth;
                break;
            case RenderTargetFormat::Depth24Stencil8:
                desc.type = RenderTargetType::ColorDepthStencil;
                break;
            default:
                desc.type = RenderTargetType::Color;
                break;
            }

            if (mFrameBuffer->AddRenderTarget(desc))
            {
                mOutputTargets[output.targetName] = mFrameBuffer->GetRenderTarget(output.targetName);
            }
        }
    }

    void RenderPass::BindInputs()
    {
        int textureUnit = 1; // 0-for diffusemap
        for (const auto& input : mConfig.inputs)
        {
            auto it = mInputTextures.find(input.sourceTarget);
            if (it != mInputTextures.end())
            {
                glActiveTexture(GL_TEXTURE0 + textureUnit);
                glBindTexture(GL_TEXTURE_2D, it->second);

                GLint boundTexture;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
                if (boundTexture != it->second) {
                    std::cout << "ERROR: Material::OnBind() - Failed to bind texture! Expected: " << it->second << ", Got: " << boundTexture << std::endl;
                }
                else {
                    std::cout << "Material::OnBind() - Successfully bound texture " << it->second << " to GL_TEXTURE" << textureUnit << std::endl;
                }

                textureUnit++;
            }
        }
    }

    void RenderPass::UnbindInputs()
    {
        // Unbind textures
        for (size_t i = 0; i < mConfig.inputs.size(); ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        for (const auto& command : mCandidateCommands)
        {
            if (!command.material)
                continue;
            command.material->UnBind();
        }
    }

    void RenderPass::ApplyRenderCommand(const std::vector<RenderCommand>& commands)
    {
        mCandidateCommands.clear();
        
        std::cout << "ApplyRenderCommand for " << mConfig.name << " (flag: " << static_cast<int>(mRenderPassFlag) << ")" << std::endl;

        for (auto cmd: commands)
        {
            std::cout << "  Command flag: " << static_cast<int>(cmd.renderpassflag) << ", match: " << (cmd.renderpassflag & mRenderPassFlag) << std::endl;
            if (cmd.renderpassflag & mRenderPassFlag)
            {
                mCandidateCommands.emplace_back(cmd);
                std::cout << "  Added command to " << mConfig.name << std::endl;
            }
        }
        
        std::cout << "  Total candidate commands for " << mConfig.name << ": " << mCandidateCommands.size() << std::endl;
    }

    // GeometryPass Implementation
    GeometryPass::GeometryPass()
    {
        mConfig.name = "GeometryPass";
        mConfig.type = RenderPassType::Geometry;
        mpOverMaterial = std::make_shared<te::GeometryMaterial>();
        mRenderPassFlag = RenderPassFlag::Geometry;
    }

    void GeometryPass::OnInitialize()
    {
        mConfig.name = "GeometryPass";
        mConfig.type = te::RenderPassType::Geometry;
        mConfig.state = te::RenderPassState::Enabled;
        mConfig.inputs = {}; // inputs
        mConfig.outputs = {
            {"Albedo", "albedo", te::RenderTargetFormat::RGBA8},
            {"Normal", "normal", te::RenderTargetFormat::RGB16F},
            {"Position", "position", te::RenderTargetFormat::RGB16F},
            {"Depth", "depth", te::RenderTargetFormat::Depth24}
        }; // outputs
        mConfig.dependencies = {}; // dependencies
        mConfig.clearColor = true;
        mConfig.clearDepth = true;
        mConfig.clearStencil = false;
        mConfig.clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        mConfig.useCustomViewport = false;
        mConfig.viewport = glm::ivec4(0, 0, 0, 0);
        mConfig.enableDepthTest = true;
        mConfig.depthFunc = GL_LESS;
        mConfig.enableBlend = false;
        mConfig.blendSrc = GL_SRC_ALPHA;
        mConfig.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    }

    void GeometryPass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled() || !mFrameBuffer)
            return;

        OnPreExecute();
        ApplyRenderCommand(commands);

        // Bind FrameBuffer
        mFrameBuffer->Bind();
        
        // Apply render settings
        ApplyRenderSettings();

        // Clear buffers
        GLbitfield clearFlags = 0;
        if (mConfig.clearColor) clearFlags |= GL_COLOR_BUFFER_BIT;
        if (mConfig.clearDepth) clearFlags |= GL_DEPTH_BUFFER_BIT;
        if (mConfig.clearStencil) clearFlags |= GL_STENCIL_BUFFER_BIT;
        
        if (clearFlags != 0)
        {
            glClearColor(mConfig.clearColorValue.r, mConfig.clearColorValue.g, 
                        mConfig.clearColorValue.b, mConfig.clearColorValue.a);
            glClear(clearFlags);
        }

        auto pGeometryMat = std::dynamic_pointer_cast<te::GeometryMaterial>(mpOverMaterial);
        // Render all geometry to G-Buffer
        for (const auto& command : mCandidateCommands)
        {
            if (!command.material || command.vertices.empty() || command.indices.empty())
                continue;

            // Get texture information from original material and set to geometry material
            if (auto blinnPhongMaterial = std::dynamic_pointer_cast<BlinnPhongMaterial>(command.material))
            {
                if (auto texture = blinnPhongMaterial->GetDiffuseTexture())
                {
                    pGeometryMat->SetDiffuseTexture(texture);
                    //// Bind original texture to geometry material
                    //glActiveTexture(GL_TEXTURE0);
                    //glBindTexture(GL_TEXTURE_2D, texture->GetHandle());
                    //mpOverMaterial->GetShader()->setInt("u_diffuseTexture", 0);
                }
            }
            
            // Use geometry material
            pGeometryMat->OnApply();
            
            // Set transformation matrix
            pGeometryMat->GetShader()->setMat4("model", command.transform);
            
            // Set camera matrices
            if (auto pCamera = mpRenderContext->GetAttachedCamera())
            {
                pGeometryMat->GetShader()->setMat4("view", pCamera->GetViewMatrix());
                pGeometryMat->GetShader()->setMat4("projection", pCamera->GetProjectionMatrix());
            }
            
            // Update material uniforms
            pGeometryMat->UpdateUniform();

            // Bind material resources
            pGeometryMat->OnBind();

            // Create and bind VAO
            GLuint VAO, VBO, EBO;
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, command.vertices.size() * sizeof(Vertex), &command.vertices[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, command.indices.size() * sizeof(unsigned int), &command.indices[0], GL_STATIC_DRAW);

            // Position attribute
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            // Normal attribute
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            // UV coordinate attribute
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

            // Draw
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indices.size()), GL_UNSIGNED_INT, 0);

            // Cleanup
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
        }

        // Unbind FrameBuffer
        mFrameBuffer->Unbind();
        
        // Restore render settings
        RestoreRenderSettings();

        OnPostExecute();
    }

    // LightingPass Implementation
    BasePass::BasePass()
    {
        mConfig.name = "BasePass";
        mConfig.type = RenderPassType::Base;
        
        // Setup inputs
        mConfig.inputs = {
            {"BackgroundColor", "SkyboxPass", "backgroundcolor", 0, true},
            {"Albedo", "GeometryPass", "albedo", 0, true},
            {"Normal", "GeometryPass", "normal", 0, true},
            {"Position", "GeometryPass", "position", 0, true},
            {"Depth", "GeometryPass", "depth", 0, true}
        };
        
        // Setup outputs
        mConfig.outputs = {
            {"BaseColor", "basecolor", RenderTargetFormat::RGBA8}
        };

        mRenderPassFlag = RenderPassFlag::BaseColor;
    }

    void BasePass::OnInitialize()
    {
        mConfig.name = "BasePass";
        mConfig.type = te::RenderPassType::Base;
        mConfig.state = te::RenderPassState::Enabled;
        mConfig.inputs = {
            {"Albedo", "GeometryPass", "albedo", 0, true},
            {"Normal", "GeometryPass", "normal", 0, true},
            {"Position", "GeometryPass", "position", 0, true},
            {"Depth", "GeometryPass", "depth", 0, true}
        }; // inputs
        mConfig.outputs = {
            {"BaseColor", "basecolor", te::RenderTargetFormat::RGBA8}
        }; // outputs
        mConfig.dependencies = {
            {"GeometryPass", true, []() { return true; }},
        }; // dependencies
        mConfig.clearColor = true;
        mConfig.clearDepth = true;
        mConfig.clearStencil = false;
        mConfig.clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);  // enable depth test, clear color is (0,0,0,0) for combine with background
        mConfig.useCustomViewport = false;
        mConfig.viewport = glm::ivec4(0, 0, 0, 0);
        mConfig.enableDepthTest = true;
        mConfig.depthFunc = GL_LESS;
        mConfig.enableBlend = false;
        mConfig.blendSrc = GL_SRC_ALPHA;
        mConfig.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    }

    void BasePass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled() || !mFrameBuffer)
            return;

        OnPreExecute();
        ApplyRenderCommand(commands);

        // Bind FrameBuffer
        mFrameBuffer->Bind();
        
        // Apply render settings
        ApplyRenderSettings();

        // Clear buffers
        if (mConfig.clearColor)
        {
            glClearColor(mConfig.clearColorValue.r, mConfig.clearColorValue.g, 
                        mConfig.clearColorValue.b, mConfig.clearColorValue.a);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // Bind input textures
        BindInputs();

        // Render all geometry
        for (const auto& command : mCandidateCommands)
        {
            if (!command.material || command.vertices.empty() || command.indices.empty())
                continue;

            // Use geometry material
            auto pMaterial = command.material;
            if (FindDependency("GeometryPass"))
            {
                pMaterial->SetUseGeometryTarget(false); // GeometryTarget not use in base pass but for fullscreen pass!!
            }

            //attach light
            if (auto pLight = mpRenderContext->GetDefaultLight())
            {
                pMaterial->AttachedLight(pLight);
            }

            pMaterial->OnApply();

            // Set transformation matrix
            pMaterial->GetShader()->setMat4("model", command.transform);

            // Set camera matrices
            if (auto pCamera = mpRenderContext->GetAttachedCamera())
            {
                pMaterial->GetShader()->setMat4("view", pCamera->GetViewMatrix());
                pMaterial->GetShader()->setMat4("projection", pCamera->GetProjectionMatrix());
            }

            // Bind material resources first
            pMaterial->OnBind();

            // Update material uniforms
            pMaterial->UpdateUniform();

            // Create and bind VAO
            GLuint VAO, VBO, EBO;
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, command.vertices.size() * sizeof(Vertex), &command.vertices[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, command.indices.size() * sizeof(unsigned int), &command.indices[0], GL_STATIC_DRAW);

            // Position attribute
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            // Normal attribute
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            // UV coordinate attribute
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

            // Draw
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indices.size()), GL_UNSIGNED_INT, 0);

            // Cleanup
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);

            pMaterial->UnBind();
        }

        // Unbind input textures
        UnbindInputs();

        // Unbind FrameBuffer
        mFrameBuffer->Unbind();
        
        // Restore render settings
        RestoreRenderSettings();

        OnPostExecute();
    }

    // PostProcessPass Implementation
    PostProcessPass::PostProcessPass()
    {
        mConfig.name = "PostProcessPass";
        mConfig.type = RenderPassType::PostProcess;
        mRenderPassFlag = RenderPassFlag::Blit;

        // Setup inputs
        mConfig.inputs = {
            {"BackgroundColor", "SkyboxPass", "backgroundcolor", 0, true},
            {"BaseColor", "BasePass", "basecolor", 0, true}
        };
        
        // Setup outputs
        // render to screen directly
    }

    void PostProcessPass::OnInitialize()
    {
        mConfig.name = "PostProcessPass";
        mConfig.type = te::RenderPassType::PostProcess;
        mConfig.state = te::RenderPassState::Enabled;
        mConfig.inputs = {
            {"BackgroundColor", "SkyboxPass", "backgroundcolor", 0, true},
            {"BaseColor", "BasePass", "basecolor", 0, true}
        }; // inputs - BasePass now handles background fusion
        mConfig.outputs = {}; // outputs - render to screen directly
        mConfig.dependencies = {
            {"SkyboxPass", true, []() { return true; }},
            {"BasePass", true, []() { return true; }}
        }; // dependencies
        mConfig.clearColor = true;
        mConfig.clearDepth = false;
        mConfig.clearStencil = false;
        mConfig.clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // 启用颜色清除
        mConfig.useCustomViewport = true;
        if (mpAttachView) {
            mConfig.viewport = glm::ivec4(0, 0, mpAttachView->Width(), mpAttachView->Height());
        }
        else {
            mConfig.viewport = glm::ivec4(0, 0, 800, 600);  // 默认尺寸
        }
        mConfig.enableDepthTest = true;
        mConfig.depthFunc = GL_LESS;
        mConfig.enableBlend = false;
        mConfig.blendSrc = GL_SRC_ALPHA;
        mConfig.blendDst = GL_ONE_MINUS_SRC_ALPHA;

        // Create fullscreen quad
        mQuadVertices = {
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}
        };
        
        mQuadIndices = {0, 1, 2, 2, 3, 0};

        RenderCommand fullScreenCommand;
        fullScreenCommand.material = nullptr;
        fullScreenCommand.vertices = mQuadVertices;
        fullScreenCommand.indices = mQuadIndices;
        fullScreenCommand.transform = glm::mat4(1.0f); //
        fullScreenCommand.state = RenderMode::Opaque;
        fullScreenCommand.hasUV = true;  // 
        mCandidateCommands.emplace_back(fullScreenCommand);
    }

    void PostProcessPass::BindInputs()
    {
        int textureUnit = 0; // 0-for diffusemap
        for (const auto& input : mConfig.inputs)
        {
            auto it = mInputTextures.find(input.sourceTarget);
            if (it != mInputTextures.end())
            {
                glActiveTexture(GL_TEXTURE0 + textureUnit);
                glBindTexture(GL_TEXTURE_2D, it->second);

                GLint boundTexture;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
                if (boundTexture != it->second) {
                    std::cout << "ERROR: PPS Effect::OnBind() - Failed to bind texture! Expected: " << it->second << ", Got: " << boundTexture << std::endl;
                }
                else {
                    std::cout << "PPS Effect::OnBind() - Successfully bound texture " << it->second << " to GL_TEXTURE" << textureUnit << std::endl;
                }
                textureUnit++;
            }
        }
    }

    void PostProcessPass::UnbindInputs()
    {
        te::RenderPass::UnbindInputs();
        for (const auto& [name, effect] : mEffects)
        {
            if (!effect.enabled && !effect.material)
                continue;

            effect.material->UnBind();
        }
    }

    void PostProcessPass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled())
            return;

        OnPreExecute();

        // Bind FrameBuffer (only if we have outputs)
        if (mFrameBuffer)
        {
            mFrameBuffer->Bind();
        }
        
        // Apply render settings
        ApplyRenderSettings();

        // Clear buffers
        if (mConfig.clearColor)
        {
            glClearColor(mConfig.clearColorValue.r, mConfig.clearColorValue.g, 
                        mConfig.clearColorValue.b, mConfig.clearColorValue.a);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // Bind input textures
        BindInputs();

        // Apply all enabled post-processing effects
        for (const auto& [name, effect] : mEffects)
        {
            if (!effect.enabled || !effect.material)
                continue;

            // Use the effect material
                auto pMaterial = effect.material;
                pMaterial->OnApply();

            // Update material uniforms
            pMaterial->UpdateUniform();

            // Bind material resources
            pMaterial->OnBind();
            
            // Render fullscreen quad
            for (const auto& command : mCandidateCommands)
            {
                if (command.vertices.empty() || command.indices.empty())
                    continue;

                // postprocess material should not transform vertices, so we use the identity matrix
                // Create and bind VAO
                GLuint VAO, VBO, EBO;
                glGenVertexArrays(1, &VAO);
                glGenBuffers(1, &VBO);
                glGenBuffers(1, &EBO);

                glBindVertexArray(VAO);
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferData(GL_ARRAY_BUFFER, command.vertices.size() * sizeof(Vertex), &command.vertices[0], GL_STATIC_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, command.indices.size() * sizeof(unsigned int), &command.indices[0], GL_STATIC_DRAW);

                // Position attribute
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
                // Normal attribute
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
                // UV coordinate attribute
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

                // Draw
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indices.size()), GL_UNSIGNED_INT, 0);

                // Cleanup
                glDeleteVertexArrays(1, &VAO);
                glDeleteBuffers(1, &VBO);
                glDeleteBuffers(1, &EBO);
            }
        }

        // Unbind input textures
        UnbindInputs();

        // Unbind FrameBuffer (only if we have outputs)
        if (mFrameBuffer)
        {
        mFrameBuffer->Unbind();
        }
        
        // Restore render settings
        RestoreRenderSettings();

        OnPostExecute();
    }

    void PostProcessPass::AddEffect(const std::string& name, const std::shared_ptr<MaterialBase>& material)
    {
        mEffects[name] = {material, true};
    }

    void PostProcessPass::RemoveEffect(const std::string& name)
    {
        mEffects.erase(name);
    }

    void PostProcessPass::SetEffectEnabled(const std::string& name, bool enabled)
    {
        auto it = mEffects.find(name);
        if (it != mEffects.end())
        {
            it->second.enabled = enabled;
        }
    }

    // SkyboxPass Implementation
    SkyboxPass::SkyboxPass()
    {
        mpOverMaterial = std::make_shared<SkyboxMaterial>();
        mRenderPassFlag = RenderPassFlag::Background;
    }

    void SkyboxPass::OnInitialize()
    {
        mConfig.name = "SkyboxPass";
        mConfig.type = te::RenderPassType::Skybox;
        mConfig.state = te::RenderPassState::Enabled;
        mConfig.inputs = {}; // inputs
        mConfig.outputs = { 
            {"BackgroundColor", "backgroundcolor", te::RenderTargetFormat::RGBA8},
        }; // outputs
        mConfig.dependencies = {}; // dependencies
        mConfig.clearColor = true;
        mConfig.clearDepth = true;
        mConfig.clearStencil = false;
        mConfig.clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        mConfig.useCustomViewport = true;  // enable custom viewport
        // use RenderView's size to set viewport
        if (mpAttachView) {
            mConfig.viewport = glm::ivec4(0, 0, mpAttachView->Width(), mpAttachView->Height());
        } else {
            mConfig.viewport = glm::ivec4(0, 0, 800, 600);  // default size
        }
        mConfig.enableDepthTest = true;
        mConfig.depthFunc = GL_LEQUAL; // enable depth test with LEQUAL
        mConfig.enableBlend = false;
        mConfig.blendSrc = GL_SRC_ALPHA;
        mConfig.blendDst = GL_ONE_MINUS_SRC_ALPHA;

        // Create skybox cube vertices with proper Vertex structure( ccw vertices)
        mSkyboxVertices = {
            // Front face
            {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // 0
            {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // 1
            {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // 2
            {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // 3
            
            // Back face
            {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // 4
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // 5
            {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // 6
            {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // 7
            
            // Left face
            {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 8
            {{-1.0f, -1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 9
            {{-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 10
            {{-1.0f,  1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 11
            
            // Right face
            {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},  // 12
            {{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},  // 13
            {{ 1.0f,  1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},  // 14
            {{ 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},  // 15
            
            // Top face
            {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},  // 16
            {{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},  // 17
            {{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},  // 18
            {{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},  // 19
            
            // Bottom face
            {{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // 20
            {{ 1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // 21
            {{ 1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // 22
            {{-1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}  // 23
        };

        // Create indices for the skybox cube (2 triangles per face, 6 faces = 12 triangles = 36 indices)
        mSkyboxIndices = {
            // Front face
            0, 1, 2,  2, 3, 0,
            // Back face
            4, 5, 6,  6, 7, 4,
            // Left face
            8, 9, 10, 10, 11, 8,
            // Right face
            12, 13, 14, 14, 15, 12,
            // Top face
            16, 17, 18, 18, 19, 16,
            // Bottom face
            20, 21, 22, 22, 23, 20
        };
    }

    void SkyboxPass::UnbindInputs()
    {
        if (mpOverMaterial)
        {
            mpOverMaterial->UnBind();
        }
    }

    void SkyboxPass::ApplyRenderCommand(const std::vector<RenderCommand>& commands)
    {
        RenderPass::ApplyRenderCommand(commands);

        RenderCommand skycubeCommand;
        skycubeCommand.material = mpOverMaterial;
        skycubeCommand.vertices = mSkyboxVertices;
        skycubeCommand.indices = mSkyboxIndices;
        skycubeCommand.transform = glm::mat4(1.0f); //
        skycubeCommand.state = RenderMode::Opaque;
        skycubeCommand.hasUV = false;  // 
        mCandidateCommands.emplace_back(skycubeCommand);
    }
    
    void SkyboxPass::Execute(const std::vector<RenderCommand>& commands)
    {
        std::cout << "SkyboxPass::Execute called" << std::endl;
        if (!IsEnabled())
        {
            std::cout << "SkyboxPass is disabled" << std::endl;
            return;
        }

        OnPreExecute();
        ApplyRenderCommand(commands);

        // Bind FrameBuffer
        if (mFrameBuffer)
        {
            mFrameBuffer->Bind();
        }
        
        // Apply render settings
        ApplyRenderSettings();

        // Clear buffers
        if (mConfig.clearColor)
        {
            glClearColor(mConfig.clearColorValue.r, mConfig.clearColorValue.g, 
                        mConfig.clearColorValue.b, mConfig.clearColorValue.a);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // make sure skybox is always rendered at the farthest distance
        // disable depth writing, so other geometries can correctly occlude the skybox
        glDepthMask(GL_FALSE);
        
        // skybox needs double-sided rendering, disable face culling
        // save current face culling state
        GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        
        // disable face culling to implement double-sided rendering
        glDisable(GL_CULL_FACE);

        auto pSkyboxMat = std::dynamic_pointer_cast<SkyboxMaterial>(mpOverMaterial);
        std::cout << "SkyboxPass: mCandidateCommands size: " << mCandidateCommands.size() << std::endl;
        for (const auto& command : mCandidateCommands)
        {
            if (command.vertices.empty() || command.indices.empty())
            {
                std::cout << "SkyboxPass: Skipping command with empty vertices/indices" << std::endl;
                continue;
            }
            std::cout << "SkyboxPass: Rendering skybox with " << command.vertices.size() << " vertices, " << command.indices.size() << " indices" << std::endl;

            // Use skybox material
            pSkyboxMat->OnApply(); // bind cubemap to GL_TEXTURE7

            // Set camera matrices
            if (auto pCamera = mpRenderContext->GetAttachedCamera())
            {
                glm::mat4 viewNoTrans = glm::mat4(glm::mat3(pCamera->GetViewMatrix())); // remove translation from view matrix
                pSkyboxMat->GetShader()->setMat4("view", viewNoTrans);
                pSkyboxMat->GetShader()->setMat4("projection", pCamera->GetProjectionMatrix());
            }

            // Bind material resources first
            pSkyboxMat->OnBind();

            // Update material uniforms after binding
            pSkyboxMat->UpdateUniform(); // set u_skyboxMap = 0

            //
            // Create and bind VAO
            GLuint VAO, VBO, EBO;
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, command.vertices.size() * sizeof(Vertex), &command.vertices[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, command.indices.size() * sizeof(unsigned int), &command.indices[0], GL_STATIC_DRAW);

            // Position attribute
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

            // Skybox doesn't need normal or UV attributes, position is used as texture coordinates
            // Normal attribute
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            // UV coordinate attribute
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

            // Draw
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indices.size()), GL_UNSIGNED_INT, 0);

            // Cleanup
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
        }

        UnbindInputs();

        // restore face culling state
        if (cullFaceEnabled) {
            glEnable(GL_CULL_FACE);
        }

        // restore depth writing
        glDepthMask(GL_TRUE);

        // Unbind FrameBuffer
        if (mFrameBuffer)
        {
            mFrameBuffer->Unbind();
        }

        // Restore render settings
        RestoreRenderSettings();

        OnPostExecute();
    }

    // RenderPassManager Implementation
    RenderPassManager& RenderPassManager::GetInstance()
    {
        static RenderPassManager instance;
        return instance;
    }

    bool RenderPassManager::AddPass(const std::shared_ptr<RenderPass>& pass)
    {
        if (!pass)
            return false;

        const std::string& name = pass->GetConfig().name;
        
        // check if pass with same name already exists
        if (mPassIndexMap.find(name) != mPassIndexMap.end())
        {
            std::cout << "RenderPass with name '" << name << "' already exists" << std::endl;
            return false;
        }

        // add to list
        size_t index = mPasses.size();
        mPasses.push_back(pass);
        mPassIndexMap[name] = index;

        return true;
    }

    void RenderPassManager::RemovePass(const std::string& name)
    {
        auto it = mPassIndexMap.find(name);
        if (it == mPassIndexMap.end())
            return;

        size_t index = it->second;
        mPasses.erase(mPasses.begin() + index);
        mPassIndexMap.erase(it);

        mPassIndexMap.clear();
        for (size_t i = 0; i < mPasses.size(); ++i)
        {
            mPassIndexMap[mPasses[i]->GetConfig().name] = i;
        }
    }

    std::shared_ptr<RenderPass> RenderPassManager::GetPass(const std::string& name) const
    {
        auto it = mPassIndexMap.find(name);
        if (it == mPassIndexMap.end())
            return nullptr;
        
        return mPasses[it->second];
    }

    void RenderPassManager::ExecuteAll(const std::vector<RenderCommand>& commands)
    {
        std::cout << "RenderPassManager::ExecuteAll called with " << commands.size() << " commands" << std::endl;
        
        // sort passes by dependencies
        SortPassesByDependencies();

        for (const auto& pass : mPasses)
        {
            if (!pass->IsEnabled())
            {
                std::cout << "Pass " << pass->GetConfig().name << " is disabled, skipping" << std::endl;
                continue;
            }

            if (!pass->CheckDependencies(mPasses))
            {
                std::cout << "Pass " << pass->GetConfig().name << " dependencies not met, skipping" << std::endl;
                continue;
            }

            std::cout << "Executing pass: " << pass->GetConfig().name << std::endl;

            // Setup inputs texture
            for (const auto& input : pass->GetConfig().inputs)
            {
                auto sourcePass = GetPass(input.sourcePass);
                if (sourcePass)
                {
                    auto outputTarget = sourcePass->GetOutput(input.sourceTarget);
                    if (outputTarget)
                    {
                        pass->SetInput(input.sourceTarget, outputTarget->GetTextureHandle());
                        std::cout << "  Set input " << input.sourceTarget << " from " << input.sourcePass << ":" << input.sourceTarget << std::endl;
                    }
                    else
                    {
                        std::cout << "  Failed to get output " << input.sourceTarget << " from " << input.sourcePass << std::endl;
                    }
                }
                else
                {
                    std::cout << "  Source pass " << input.sourcePass << " not found" << std::endl;
                }
            }

            //  Execute Pass
            pass->Execute(commands);
        }
    }

    void RenderPassManager::SortPassesByDependencies()
    {
        // simple topological sort implementation
        std::vector<std::shared_ptr<RenderPass>> sortedPasses;
        std::unordered_set<std::string> processedPasses;
        
        while (sortedPasses.size() < mPasses.size())
        {
            bool found = false;
            
            for (const auto& pass : mPasses)
            {
                const std::string& passName = pass->GetConfig().name;
                
                // if already processed, skip
                if (processedPasses.find(passName) != processedPasses.end())
                    continue;
                
                // check if all dependencies have been processed
                bool canProcess = true;
                for (const auto& dep : pass->GetConfig().dependencies)
                {
                    if (dep.required && processedPasses.find(dep.passName) == processedPasses.end())
                    {
                        canProcess = false;
                        break;
                    }
                }
                
                if (canProcess)
                {
                    sortedPasses.push_back(pass);
                    processedPasses.insert(passName);
                    found = true;
                    break;
                }
            }
            
            if (!found)
            {
                std::cout << "Circular dependency detected in RenderPasses" << std::endl;
                break;
            }
        }
        
        mPasses = std::move(sortedPasses);
        
        // rebuild index map
        mPassIndexMap.clear();
        for (size_t i = 0; i < mPasses.size(); ++i)
        {
            mPassIndexMap[mPasses[i]->GetConfig().name] = i;
        }
    }

    void RenderPassManager::Clear()
    {
        mPasses.clear();
        mPassIndexMap.clear();
    }
}
