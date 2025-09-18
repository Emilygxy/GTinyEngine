#include "framework/FurRenderPass.h"
#include "glad/glad.h"
#include "Camera.h"
#include "Light.h"
#include "geometry/FurGeometryGenerator.h"
#include "RenderView.h"
#include "framework/RenderContext.h"
#include <iostream>

FurRenderPass::FurRenderPass()
{
    mConfig.name = "FurRenderPass";
    mConfig.type = te::RenderPassType::PostProcess; // excute after geometry pass
    mRenderPassFlag = RenderPassFlag::Transparent; // use transparent render flag
}

void FurRenderPass::OnInitialize()
{
    mConfig.name = "FurRenderPass";
    mConfig.type = te::RenderPassType::PostProcess;
    mConfig.state = te::RenderPassState::Enabled;
    mConfig.inputs = {}; // no input texture
    mConfig.outputs = {}; // render to screen
    mConfig.dependencies = {
        {"GeometryPass", true, []() { return true; }}
    };
    mConfig.clearColor = false; // do not clear color, mix with geometry
    mConfig.clearDepth = false; // do not clear depth, keep depth test
    mConfig.clearStencil = false;
    mConfig.clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    mConfig.useCustomViewport = false;
    mConfig.viewport = glm::ivec4(0, 0, 0, 0);
    mConfig.enableDepthTest = true;
    mConfig.depthFunc = GL_LESS;
    mConfig.enableBlend = true; // enable blend to support transparency
    mConfig.blendSrc = GL_SRC_ALPHA;
    mConfig.blendDst = GL_ONE_MINUS_SRC_ALPHA;

    std::cout << "FurRenderPass::OnInitialize() - Initialized successfully" << std::endl;
}

void FurRenderPass::Execute(const std::vector<RenderCommand>& commands)
{
    if (!IsEnabled() || !mpFurMaterial) {
        std::cout << "FurRenderPass::Execute() - Pass disabled or no fur material" << std::endl;
        return;
    }

    OnPreExecute();
    ApplyRenderCommand(commands);

    // apply render settings
    ApplyRenderSettings();

    // enable blend
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // disable face culling (fur needs double-sided rendering)
    glDisable(GL_CULL_FACE);

    // render fur layers
    RenderHairLayers(commands);

    glEnable(GL_CULL_FACE);
    // restore render settings
    RestoreRenderSettings();

    OnPostExecute();
}

void FurRenderPass::RenderHairLayers(const std::vector<RenderCommand>& commands)
{
    if (!mpFurMaterial) {
        std::cout << "FurRenderPass::RenderHairLayers() - No fur material available" << std::endl;
        return;
    }

    // get fur geometry generator
    static FurGeometryGenerator geometryGenerator;
    
    // render fur for each command with fur flag
    for (const auto& command : mCandidateCommands) {
        if (command.vertices.empty() || command.indices.empty()) {
            continue;
        }

        // check if command contains fur flag
        if (command.renderpassflag & RenderPassFlag::Transparent) {
            // generate fur geometry (can be optimized to cache)
            geometryGenerator.GenerateHairFromBaseMesh(
                command.vertices, 
                command.indices,
                mpFurMaterial->GetNumLayers(),
                mpFurMaterial->GetHairLength(),
                mpFurMaterial->GetHairDensity()
            );

            const auto& hairVertices = geometryGenerator.GetHairVertices();
            const auto& hairIndices = geometryGenerator.GetHairIndices();

            if (hairVertices.empty() || hairIndices.empty()) {
                continue;
            }

            // apply fur material
            mpFurMaterial->OnApply();

            // set transform matrix
            mpFurMaterial->GetShader()->setMat4("model", command.transform);

            // update material uniform
            mpFurMaterial->UpdateUniform();

            // bind material resources
            mpFurMaterial->OnBind();

            // create and bind VAO
            GLuint VAO, VBO, EBO;
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, hairVertices.size() * sizeof(Vertex), &hairVertices[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, hairIndices.size() * sizeof(unsigned int), &hairIndices[0], GL_STATIC_DRAW);

            // vertex attributes
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

            // draw fur
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(hairIndices.size()), GL_UNSIGNED_INT, 0);

            // clean up
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);

            // unbind material
            mpFurMaterial->UnBind();
        }
    }
}