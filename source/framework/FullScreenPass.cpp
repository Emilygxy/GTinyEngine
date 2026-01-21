#include "framework/FullScreenPass.h"
#include "RenderView.h"
#include "framework/FullscreenQuad.h"

namespace te
{
    FullScreenPass::FullScreenPass()
    {
        mConfig.name = "FullScreenPass";
        mConfig.type = RenderPassType::PostProcess;
        mRenderPassFlag = RenderPassFlag::Blit;

        // Setup inputs
        mConfig.inputs = {
        };
    }

    void FullScreenPass::Execute(const std::vector<RenderCommand>& commands)
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
        // Use the effect material
        if (auto& pEffect = mQuad->GetMaterial())
        {
            pEffect->OnApply();

            // Update material uniforms
            pEffect->UpdateUniform();

            // Bind material resources
            pEffect->OnBind();
        }

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
        // Unbind input textures

        // Unbind FrameBuffer (only if we have outputs)
        if (mFrameBuffer)
        {
            mFrameBuffer->Unbind();
        }

        // Restore render settings
        RestoreRenderSettings();

        OnPostExecute();
    }

    void FullScreenPass::OnInitialize()
    {
        mConfig.type = te::RenderPassType::PostProcess;
        mConfig.state = te::RenderPassState::Enabled;
        mConfig.outputs = {
             {"FullScreen", "fullscreencolor", te::RenderTargetFormat::RGBA8}
        }; // outputs - render to screen directly
        mConfig.dependencies = {
        }; // dependencies
        mConfig.clearColor = true;
        mConfig.clearDepth = false;
        mConfig.clearStencil = false;
        mConfig.clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // enable color clear
        mConfig.useCustomViewport = true;
        if (mpAttachView) {
            mConfig.viewport = glm::ivec4(0, 0, mpAttachView->Width(), mpAttachView->Height());
        }
        else {
            mConfig.viewport = glm::ivec4(0, 0, 800, 600);  // default size
        }
        mConfig.enableDepthTest = true;
        mConfig.depthFunc = GL_LESS;
        mConfig.enableBlend = false;
        mConfig.blendSrc = GL_SRC_ALPHA;
        mConfig.blendDst = GL_ONE_MINUS_SRC_ALPHA;

        if (!mQuad)
        {
            mQuad = std::make_shared<FullscreenQuad>();
        }

        RenderCommand fullScreenCommand;
        fullScreenCommand.material = nullptr;
        fullScreenCommand.vertices = mQuad->GetVertices();
        fullScreenCommand.indices = mQuad->GetIndices();
        fullScreenCommand.transform = glm::mat4(1.0f); //
        fullScreenCommand.state = RenderMode::Opaque;
        fullScreenCommand.hasUV = true;  // 
        mCandidateCommands.emplace_back(fullScreenCommand);
    }
}
