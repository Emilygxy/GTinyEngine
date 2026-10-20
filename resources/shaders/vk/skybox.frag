#version 450

layout(location = 0) in vec3 vDir;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform samplerCube uSkybox;

void main() {
    outColor = vec4(texture(uSkybox, vDir).rgb, 1.0);
}
