#pragma once

#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct GSVertex {
    glm::vec4 position;
    glm::vec4 scale_opacity;
    glm::vec4 rotation;
    float sh[48];
};

struct alignas(16) GSVertexAttributeCPU {
    glm::vec4 conic_opacity;
    glm::vec4 color_radii;
    glm::uvec4 aabb;
    glm::vec4 normal;
    glm::vec2 uv;
    float depth;
    uint32_t magic;
};

struct GSUniformBufferCPU {
    glm::vec4 camera_position;
    glm::mat4 proj_mat;
    glm::mat4 view_mat;
    uint32_t width;
    uint32_t height;
    float tan_fovx;
    float tan_fovy;
};

struct GSRenderPushConstants {
    uint32_t width;
    uint32_t height;
    float depth_near;
    float depth_far;
};

struct GSRadixSortPushConstants {
    uint32_t g_num_elements;
    uint32_t g_shift;
    uint32_t g_num_workgroups;
    uint32_t g_num_blocks_per_workgroup;
};

struct GSCameraState {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

