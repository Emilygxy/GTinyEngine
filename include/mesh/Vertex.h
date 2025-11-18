#pragma once
#include <glm/glm.hpp>

struct Vertex {
    Vertex() = default;
    Vertex(glm::vec3 pos, glm::vec3 nor, glm::vec2 uv)
    {
        position = pos;
        normal = nor;
        texCoords = uv;
    }

    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};
