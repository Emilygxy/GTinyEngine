#version 450

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 vDir;

layout(std140, set = 0, binding = 0) uniform SkyUbo {
    mat4 view;
    mat4 proj;
} skyUbo;

void main() {
    vDir = inPos;
    vec4 clip = skyUbo.proj * skyUbo.view * vec4(inPos, 1.0);
    gl_Position = clip.xyww;
}
