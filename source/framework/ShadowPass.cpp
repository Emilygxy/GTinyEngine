#include "framework/RenderPass.h"
#include "framework/LightSpaceMatrix.h"
#include "framework/RenderContext.h"
#include "materials/ShadowDepthMaterial.h"
#include "mesh/Mesh.h"
#include "mesh/Vertex.h"
#include "Light.h"
#include "glad/glad.h"
#include <iostream>

namespace te
{
    ShadowPass::ShadowPass()
    {
        mConfig.name = "ShadowPass";
        mConfig.type = RenderPassType::Shadow;
        mpOverMaterial = std::make_shared<ShadowDepthMaterial>();
        mRenderPassFlag = RenderPassFlag::Shadowing;
    }

    void ShadowPass::OnInitialize()
    {
        mConfig.name = "ShadowPass";
        mConfig.type = RenderPassType::Shadow;
        mConfig.state = RenderPassState::Enabled;
        mConfig.inputs = {};
        mConfig.outputs = {
            {"Depth", "depth", RenderTargetFormat::Depth32F}
        };
        mConfig.dependencies = {};
        mConfig.clearColor = false;
        mConfig.clearDepth = true;
        mConfig.clearStencil = false;
        mConfig.useCustomViewport = true;
        mConfig.viewport = glm::ivec4(0, 0, kShadowMapSize, kShadowMapSize);
        mConfig.enableDepthTest = true;
        mConfig.depthFunc = GL_LESS;
        mConfig.enableBlend = false;
    }

    void ShadowPass::SetupFrameBuffer()
    {
        if (mConfig.outputs.empty())
        {
            return;
        }

        const uint32_t width = kShadowMapSize;
        const uint32_t height = kShadowMapSize;

        mFrameBuffer = std::make_shared<MultiRenderTarget>();
        if (!mFrameBuffer->Initialize(width, height))
        {
            std::cout << "ShadowPass: failed to initialize framebuffer" << std::endl;
            return;
        }

        for (const auto& output : mConfig.outputs)
        {
            RenderTargetDesc desc;
            desc.name = output.targetName;
            desc.width = width;
            desc.height = height;
            desc.format = output.format;
            desc.type = RenderTargetType::Depth;
            desc.filterMode = GL_NEAREST;
            desc.wrapMode = GL_CLAMP_TO_EDGE;

            if (mFrameBuffer->AddRenderTarget(desc))
            {
                mOutputTargets[output.targetName] = mFrameBuffer->GetRenderTarget(output.targetName);
            }
        }
    }

    GLuint ShadowPass::GetShadowMapTexture() const
    {
        auto depthTarget = GetOutput("depth");
        if (!depthTarget)
        {
            return 0;
        }
        return depthTarget->GetTextureHandle();
    }

    AaBB ShadowPass::ComputeSceneBounds(const std::vector<RenderCommand>& commands) const
    {
        AaBB bounds = AaBB::EmptyAaBB();
        for (const auto& command : commands)
        {
            if (!(command.renderpassflag & RenderPassFlag::Shadowing))
            {
                continue;
            }
            if (!command.fragmentsSource)
            {
                continue;
            }
            const AaBB objectBounds = ComputeSceneBoundsFromFragmentsSource(command.fragmentsSource);
            if (!objectBounds.IsEmpty())
            {
                bounds.Union(objectBounds);
            }
        }
        return bounds;
    }

    glm::vec3 ShadowPass::ResolveLightDirection(const glm::vec3& sceneCenter) const
    {
        if (!mpRenderContext)
        {
            return glm::normalize(glm::vec3(0.3f, -1.0f, 0.3f));
        }

        if (auto light = mpRenderContext->GetDefaultLight())
        {
            const glm::vec3 direction = light->GetDirection();
            if (glm::length(direction) > 1e-4f)
            {
                return glm::normalize(direction);
            }
            const glm::vec3 toScene = sceneCenter - light->GetPosition();
            if (glm::length(toScene) > 1e-4f)
            {
                return glm::normalize(toScene);
            }
        }
        return glm::normalize(glm::vec3(0.3f, -1.0f, 0.3f));
    }

    void ShadowPass::Execute(const std::vector<RenderCommand>& commands)
    {
        if (!IsEnabled() || !mFrameBuffer)
        {
            return;
        }

        OnPreExecute();
        ApplyRenderCommand(commands);

        const AaBB sceneBounds = ComputeSceneBounds(commands);
        const glm::vec3 sceneCenter = sceneBounds.IsEmpty()
            ? glm::vec3(0.0f)
            : (sceneBounds.min + sceneBounds.max) * 0.5f;
        const glm::vec3 lightDirection = ResolveLightDirection(sceneCenter);

        LightSpaceMatrixParams params;
        params.orthoPadding = 1.5f;
        params.shadowDistance = 15.0f;
        params.nearPlane = 0.1f;
        params.farPlane = 40.0f;

        const LightSpaceMatrices matrices = ComputeDirectionalLightSpaceMatrix(
            lightDirection, sceneBounds, params);
        mLightSpaceMatrix = matrices.lightSpace;

        auto shadowMaterial = std::dynamic_pointer_cast<ShadowDepthMaterial>(mpOverMaterial);
        if (!shadowMaterial)
        {
            return;
        }
        shadowMaterial->SetLightSpaceMatrix(mLightSpaceMatrix);

        mFrameBuffer->Bind();
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        ApplyRenderSettings();

        if (mConfig.clearDepth)
        {
            glClear(GL_DEPTH_BUFFER_BIT);
        }

        shadowMaterial->OnApply();

        for (const auto& command : mCandidateCommands)
        {
            if (!command.fragmentsSource)
            {
                continue;
            }

            const auto& fragments = command.fragmentsSource->GetFragments();
            for (const auto& frag : fragments)
            {
                if (!frag.IsReady())
                {
                    continue;
                }

                auto vertices = frag.mpGeometry->GetVertices();
                auto indices = frag.mpGeometry->GetIndices();
                const auto transform = frag.mpGeometry->GetWorldTransform();

                if (vertices.empty() || indices.empty())
                {
                    continue;
                }

                shadowMaterial->GetShader()->setMat4("model", transform);
                shadowMaterial->UpdateUniform();

                GLuint VAO = 0, VBO = 0, EBO = 0;
                glGenVertexArrays(1, &VAO);
                glGenBuffers(1, &VBO);
                glGenBuffers(1, &EBO);

                glBindVertexArray(VAO);
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));

                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);

                glDeleteVertexArrays(1, &VAO);
                glDeleteBuffers(1, &VBO);
                glDeleteBuffers(1, &EBO);
            }
        }

        mFrameBuffer->Unbind();
        RestoreRenderSettings();
        OnPostExecute();
    }
}
