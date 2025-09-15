#include "framework/RenderPass.h"
#include "glad/glad.h"
#include "Camera.h"
#include "Light.h"
#include "RenderView.h"
#include "skybox/Skybox.h"
#include "materials/GeometryMaterial.h"
#include "materials/LightingMaterial.h"
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

    bool RenderPass::Initialize(const RenderPassConfig& config, const std::shared_ptr<RenderView>& pView, const std::shared_ptr<RenderContext>& pContext)
    {
        mConfig = config;
        
        // 设置FrameBuffer
        SetupFrameBuffer();
        
        // 调用子类初始化
        OnInitialize();
        
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
            
            // 查找依赖的Pass
            auto it = std::find_if(allPasses.begin(), allPasses.end(),
                [&dep](const std::shared_ptr<RenderPass>& pass) {
                    return pass->GetConfig().name == dep.passName;
                });
            
            if (it == allPasses.end())
            {
                std::cout << "Missing required dependency: " << dep.passName << std::endl;
                return false;
            }
            
            // 检查条件
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
        // 保存当前状态
        glGetIntegerv(GL_VIEWPORT, mSavedState.viewport);
        mSavedState.depthTest = glIsEnabled(GL_DEPTH_TEST);
        mSavedState.blend = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &mSavedState.blendSrc);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &mSavedState.blendDst);

        // 应用Pass设置
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
        // 恢复状态
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

    void RenderPass::SetupFrameBuffer()
    {
        if (mConfig.outputs.empty())
            return;

        // 获取第一个输出的尺寸
        uint32_t width = 800, height = 600; // 默认尺寸，应该从配置或全局设置获取
        
        mFrameBuffer = std::make_shared<MultiRenderTarget>();
        if (!mFrameBuffer->Initialize(width, height))
        {
            std::cout << "Failed to initialize FrameBuffer for pass: " << mConfig.name << std::endl;
            return;
        }

        // 添加输出目标
        for (const auto& output : mConfig.outputs)
        {
            RenderTargetDesc desc;
            desc.name = output.name;
            desc.width = width;
            desc.height = height;
            desc.format = output.format;
            
            // 根据格式确定类型
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
                mOutputTargets[output.name] = mFrameBuffer->GetRenderTarget(output.name);
            }
        }
    }

    void RenderPass::BindInputs()
    {
        int textureUnit = 0;
        for (const auto& input : mConfig.inputs)
        {
            auto it = mInputTextures.find(input.name);
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
        // 解绑纹理
        for (size_t i = 0; i < mConfig.inputs.size(); ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void RenderPass::ApplyRenderCommand(const std::vector<RenderCommand>& commands)
    {
        mCandidateCommands.clear();

        for (auto cmd: commands)
        {
            if (cmd.renderpassflag & mRenderPassFlag)
            {
                mCandidateCommands.emplace_back(cmd);
            }
        }
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
        // 设置输出
        mConfig.outputs = {
            {"Albedo", "albedo", RenderTargetFormat::RGBA8},
            {"Normal", "normal", RenderTargetFormat::RGB16F},
            {"Position", "position", RenderTargetFormat::RGB16F},
            {"Depth", "depth", RenderTargetFormat::Depth24}
        };
    }

    void GeometryPass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled() || !mFrameBuffer)
            return;

        OnPreExecute();
        ApplyRenderCommand(commands);

        // 绑定FrameBuffer
        mFrameBuffer->Bind();
        
        // 应用渲染设置
        ApplyRenderSettings();

        // 清除缓冲区
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
        // 渲染所有几何体到G-Buffer
        for (const auto& command : mCandidateCommands)
        {
            if (!command.material || command.vertices.empty() || command.indices.empty())
                continue;

            // 从原始材质中获取纹理信息并设置到几何体材质
            if (auto blinnPhongMaterial = std::dynamic_pointer_cast<BlinnPhongMaterial>(command.material))
            {
                if (auto texture = blinnPhongMaterial->GetDiffuseTexture())
                {
                    pGeometryMat->SetDiffuseTexture(texture);
                    //// 绑定原始纹理到几何体材质
                    //glActiveTexture(GL_TEXTURE0);
                    //glBindTexture(GL_TEXTURE_2D, texture->GetHandle());
                    //mpOverMaterial->GetShader()->setInt("u_diffuseTexture", 0);
                }
            }
            
            // 使用几何体材质
            pGeometryMat->OnApply();
            
            // 设置变换矩阵
            pGeometryMat->GetShader()->setMat4("model", command.transform);
            
            // 设置相机矩阵
            if (auto pCamera = mpRenderContext->GetAttachedCamera())
            {
                pGeometryMat->GetShader()->setMat4("view", pCamera->GetViewMatrix());
                pGeometryMat->GetShader()->setMat4("projection", pCamera->GetProjectionMatrix());
            }
            
            // 更新材质uniform
            pGeometryMat->UpdateUniform();

            // 绑定材质资源
            pGeometryMat->OnBind();

            // 创建并绑定VAO
            GLuint VAO, VBO, EBO;
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, command.vertices.size() * sizeof(Vertex), &command.vertices[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, command.indices.size() * sizeof(unsigned int), &command.indices[0], GL_STATIC_DRAW);

            // 位置属性
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            // 法线属性
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            // UV坐标属性
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

            // 绘制
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indices.size()), GL_UNSIGNED_INT, 0);

            // 清理
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
        }

        // 解绑FrameBuffer
        mFrameBuffer->Unbind();
        
        // 恢复渲染设置
        RestoreRenderSettings();

        OnPostExecute();
    }

    // LightingPass Implementation
    BasePass::BasePass()
    {
        mConfig.name = "LightingPass";
        mConfig.type = RenderPassType::Lighting;
        
        // 设置输入
        mConfig.inputs = {
            {"Albedo", "GeometryPass", "albedo", 0, true},
            {"Normal", "GeometryPass", "normal", 0, true},
            {"Position", "GeometryPass", "position", 0, true},
            {"Depth", "GeometryPass", "depth", 0, true}
        };
        
        // 设置输出
        mConfig.outputs = {
            {"Lighting", "lighting", RenderTargetFormat::RGBA8}
        };

        mRenderPassFlag = RenderPassFlag::BaseColor;
    }

    void BasePass::OnInitialize()
    {
        // 光照材质已在构造函数中创建
    }

    void BasePass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled() || !mFrameBuffer)
            return;

        OnPreExecute();
        ApplyRenderCommand(commands);

        // 绑定FrameBuffer
        mFrameBuffer->Bind();
        
        // 应用渲染设置
        ApplyRenderSettings();

        // 清除缓冲区
        if (mConfig.clearColor)
        {
            glClearColor(mConfig.clearColorValue.r, mConfig.clearColorValue.g, 
                        mConfig.clearColorValue.b, mConfig.clearColorValue.a);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // 绑定输入纹理
        BindInputs();

        // 渲染所有几何体
        for (const auto& command : mCandidateCommands)
        {
            if (!command.material || command.vertices.empty() || command.indices.empty())
                continue;

            // 使用几何体材质
            auto pMaterial = command.material;

            //attach light
            if (auto pLight = mpRenderContext->GetDefaultLight())
            {
                pMaterial->AttachedLight(pLight);
            }

            pMaterial->OnApply();

            // 设置变换矩阵
            pMaterial->GetShader()->setMat4("model", command.transform);

            // 设置相机矩阵
            if (auto pCamera = mpRenderContext->GetAttachedCamera())
            {
                pMaterial->GetShader()->setMat4("view", pCamera->GetViewMatrix());
                pMaterial->GetShader()->setMat4("projection", pCamera->GetProjectionMatrix());
            }

            // 更新材质uniform
            pMaterial->UpdateUniform();

            // 创建并绑定VAO
            GLuint VAO, VBO, EBO;
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, command.vertices.size() * sizeof(Vertex), &command.vertices[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, command.indices.size() * sizeof(unsigned int), &command.indices[0], GL_STATIC_DRAW);

            // 位置属性
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            // 法线属性
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            // UV坐标属性
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

            // 绘制
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indices.size()), GL_UNSIGNED_INT, 0);

            // 清理
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
        }

        // 解绑输入纹理
        UnbindInputs();

        // 解绑FrameBuffer
        mFrameBuffer->Unbind();
        
        // 恢复渲染设置
        RestoreRenderSettings();

        OnPostExecute();
    }

    // PostProcessPass Implementation
    PostProcessPass::PostProcessPass()
    {
        mConfig.name = "PostProcessPass";
        mConfig.type = RenderPassType::PostProcess;
        mRenderPassFlag = RenderPassFlag::Blit;

        // 设置输入
        mConfig.inputs = {
            {"Color", "LightingPass", "lighting", 0, true}
        };
        
        // 设置输出
        mConfig.outputs = {
            {"Final", "final", RenderTargetFormat::RGBA8}
        };
    }

    void PostProcessPass::OnInitialize()
    {
        // 创建全屏四边形
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
        fullScreenCommand.hasUV = false;
        mCandidateCommands.emplace_back(fullScreenCommand);
    }

    void PostProcessPass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled() || !mFrameBuffer)
            return;

        OnPreExecute();

        // 绑定FrameBuffer
        mFrameBuffer->Bind();
        
        // 应用渲染设置
        ApplyRenderSettings();

        // 清除缓冲区
        if (mConfig.clearColor)
        {
            glClearColor(mConfig.clearColorValue.r, mConfig.clearColorValue.g, 
                        mConfig.clearColorValue.b, mConfig.clearColorValue.a);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // 绑定输入纹理
        BindInputs();

        // 应用所有启用的后处理效果
        for (const auto& [name, effect] : mEffects)
        {
            if (!effect.enabled || !effect.material)
                continue;

            //effect.material->UpdateUniform();
            //effect.material->OnBind();
            
            // 渲染全屏四边形
            for (const auto& command : mCandidateCommands)
            {
                if (!command.material || command.vertices.empty() || command.indices.empty())
                    continue;

                // 使用几何体材质
                auto pMaterial = effect.material;

                pMaterial->OnApply();

                // 设置变换矩阵
                pMaterial->GetShader()->setMat4("model", command.transform);

                // 设置相机矩阵
                if (auto pCamera = mpRenderContext->GetAttachedCamera())
                {
                    pMaterial->GetShader()->setMat4("view", pCamera->GetViewMatrix());
                    pMaterial->GetShader()->setMat4("projection", pCamera->GetProjectionMatrix());
                }

                // 更新材质uniform
                pMaterial->UpdateUniform();

                // 创建并绑定VAO
                GLuint VAO, VBO, EBO;
                glGenVertexArrays(1, &VAO);
                glGenBuffers(1, &VBO);
                glGenBuffers(1, &EBO);

                glBindVertexArray(VAO);
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferData(GL_ARRAY_BUFFER, command.vertices.size() * sizeof(Vertex), &command.vertices[0], GL_STATIC_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, command.indices.size() * sizeof(unsigned int), &command.indices[0], GL_STATIC_DRAW);

                // 位置属性
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
                // 法线属性
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
                // UV坐标属性
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

                // 绘制
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indices.size()), GL_UNSIGNED_INT, 0);

                // 清理
                glDeleteVertexArrays(1, &VAO);
                glDeleteBuffers(1, &VBO);
                glDeleteBuffers(1, &EBO);
            }
        }

        // 解绑输入纹理
        UnbindInputs();

        // 解绑FrameBuffer
        mFrameBuffer->Unbind();
        
        // 恢复渲染设置
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
        mConfig.name = "SkyboxPass";
        mConfig.type = RenderPassType::Skybox;
        mConfig.enableDepthTest = true;
        mConfig.depthFunc = GL_LEQUAL; // 天空盒使用LEQUAL深度测试
        mRenderPassFlag = RenderPassFlag::Background;

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
        // 天空盒Pass通常不需要FrameBuffer，直接渲染到当前缓冲区
    }

    void SkyboxPass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled() || !mpSkybox)
            return;

        OnPreExecute();

        // 应用渲染设置
        ApplyRenderSettings();

        // 渲染天空盒
        if (auto pAttachCamera = mpRenderContext->GetAttachedCamera())
        {
            mpSkybox->Draw(pAttachCamera->GetViewMatrix(), pAttachCamera->GetProjectionMatrix());
        }

        // 恢复渲染设置
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
        
        // 检查是否已存在
        if (mPassIndexMap.find(name) != mPassIndexMap.end())
        {
            std::cout << "RenderPass with name '" << name << "' already exists" << std::endl;
            return false;
        }

        // 添加到列表
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

        // 重新构建索引映射
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
        // 按依赖关系排序
        SortPassesByDependencies();

        // 执行所有启用的Pass
        for (const auto& pass : mPasses)
        {
            if (!pass->IsEnabled())
                continue;

            // 检查依赖
            if (!pass->CheckDependencies(mPasses))
                continue;

            // 设置输入纹理
            for (const auto& input : pass->GetConfig().inputs)
            {
                auto sourcePass = GetPass(input.sourcePass);
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

    void RenderPassManager::SortPassesByDependencies()
    {
        // 简单的拓扑排序实现
        std::vector<std::shared_ptr<RenderPass>> sortedPasses;
        std::unordered_set<std::string> processedPasses;
        
        while (sortedPasses.size() < mPasses.size())
        {
            bool found = false;
            
            for (const auto& pass : mPasses)
            {
                const std::string& passName = pass->GetConfig().name;
                
                // 如果已经处理过，跳过
                if (processedPasses.find(passName) != processedPasses.end())
                    continue;
                
                // 检查所有依赖是否已处理
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
        
        // 重新构建索引映射
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
