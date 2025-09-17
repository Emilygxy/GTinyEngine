#include "framework/RenderPass.h"
#include "glad/glad.h"
#include "Camera.h"
#include "Light.h"
#include "RenderView.h"
#include "skybox/Skybox.h"
#include "materials/GeometryMaterial.h"
#include "materials/BlinnPhongMaterial.h"
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
        mConfig = te::RenderPassConfig{
        "GeometryPass",
        te::RenderPassType::Geometry,
        te::RenderPassState::Enabled,
        {}, // inputs
        {
            {"Albedo", "albedo", te::RenderTargetFormat::RGBA8},
            {"Normal", "normal", te::RenderTargetFormat::RGB16F},
            {"Position", "position", te::RenderTargetFormat::RGB16F},
            {"Depth", "depth", te::RenderTargetFormat::Depth24}
        }, // outputs
        {}, // dependencies
        true, true, false, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
        };
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
        mConfig = te::RenderPassConfig{
        "BasePass",
        te::RenderPassType::Base,
        te::RenderPassState::Enabled,
        {
            {"Albedo", "GeometryPass", "albedo", 0, true},
            {"Normal", "GeometryPass", "normal", 0, true},
            {"Position", "GeometryPass", "position", 0, true},
            {"Depth", "GeometryPass", "depth", 0, true}
        }, // inputs
        {
            {"BaseColor", "basecolor", te::RenderTargetFormat::RGBA8}
        }, // outputs
        {
            {"GeometryPass", true, []() { return true; }},
        }, // dependencies
        true, true, false, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)  // enable depth test, clear color is (0,0,0,0) for combine with background
        };
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
        mConfig = te::RenderPassConfig{
        "PostProcessPass",
        te::RenderPassType::PostProcess,
        te::RenderPassState::Enabled,
        {
            {"BackgroundColor", "SkyboxPass", "backgroundcolor", 0, true},
            {"BaseColor", "BasePass", "basecolor", 0, true}
        }, // inputs - BasePass now handles background fusion
        {}, // outputs - render to screen directly
        {
            {"SkyboxPass", true, []() { return true; }},
            {"BasePass", true, []() { return true; }}
        }, // dependencies
        true, false, false, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)  // 启用颜色清除
        };

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
                textureUnit++;
            }
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
        if (!mpSkybox)
        {
            std::vector<std::string> faces
            {
                "resources/textures/skybox/right.jpg",
                "resources/textures/skybox/left.jpg",
                "resources/textures/skybox/top.jpg",
                "resources/textures/skybox/bottom.jpg",
                "resources/textures/skybox/front.jpg",
                "resources/textures/skybox/back.jpg"
            };
            mpSkybox = std::make_shared<Skybox>(faces);
        }
    }

    void SkyboxPass::OnInitialize()
    {
        mConfig = te::RenderPassConfig{
        "SkyboxPass",
        te::RenderPassType::Skybox,
        te::RenderPassState::Enabled,
        {}, // inputs
        { 
            {"BackgroundColor", "backgroundcolor", te::RenderTargetFormat::RGBA8},
        }, // outputs
        {}, // dependencies
        true, true, false, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // enable deptyh test,LEQUAL
        };
    }

    void SkyboxPass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled() || !mpSkybox)
            return;

        OnPreExecute();

        // Bind FrameBuffer（如果存在）
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

        // render skybox
        if (auto pAttachCamera = mpRenderContext->GetAttachedCamera())
        {
            mpSkybox->Draw(pAttachCamera->GetViewMatrix(), pAttachCamera->GetProjectionMatrix());
        }

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
