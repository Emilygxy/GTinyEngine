#include "ultis.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <iostream>
#include "filesystem.h"

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const* path, bool generateMipMap)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    // 设置像素对齐，解决非4字节对齐的图像加载问题
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int width, height, nrComponents;
    std::string fullPath = FileSystem::getPath(path);
    unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        if (generateMipMap)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

unsigned int loadCubemap(const std::vector<std::string>& faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    std::cout << "Generated texture ID: " << textureID << std::endl;
    if (textureID == 0) {
        std::cout << "ERROR: Failed to generate texture!" << std::endl;
        return 0;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    // 设置像素对齐，解决非4字节对齐的图像加载问题
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        std::string fullPath = FileSystem::getPath(faces[i]);
        std::cout << "Loading cubemap face: " << fullPath << std::endl;
        unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format;
            if (nrChannels == 1)
                format = GL_RED;
            else if (nrChannels == 3)
                format = GL_RGB;
            else if (nrChannels == 4)
                format = GL_RGBA;
            
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
            std::cout << "Successfully loaded cubemap face " << i << ": " << width << "x" << height << " channels: " << nrChannels << std::endl;
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << fullPath << std::endl;
            std::cout << "STBI error: " << stbi_failure_reason() << std::endl;
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
