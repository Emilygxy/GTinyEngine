#include "skybox/Skybox.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include "shader.h"
#include "ultis.h"

namespace
{
	// skybox cube vertices
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
}

Skybox::Skybox(const std::vector<std::string>& faces)
{
    // 1. load cubemap
    mCubemapTexture = loadCubemap(faces);

    // 2. Set VAO/VBO
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // 3. load shader
    //mShaderProgram = loadShader("skybox.vs", "skybox.fs");
    mShader = std::make_shared<Shader>("resources/shaders/TinyRenderer/skybox.vs", "resources/shaders/TinyRenderer/skybox.fs");
}

Skybox::~Skybox()
{
    glDeleteVertexArrays(1, &mVAO);
    glDeleteBuffers(1, &mVBO);
    glDeleteTextures(1, &mCubemapTexture);
    mShader = nullptr;
}

void Skybox::Draw(const glm::mat4& view, const glm::mat4& projection)
{
    glDepthFunc(GL_LEQUAL);
    mShader->use();

    // Remove the translation part of the view
    glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
    glUniformMatrix4fv(glGetUniformLocation(mShader->GetID(), "view"), 1, GL_FALSE, glm::value_ptr(viewNoTrans));
    glUniformMatrix4fv(glGetUniformLocation(mShader->GetID(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(mVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mCubemapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}
