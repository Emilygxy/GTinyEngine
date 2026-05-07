#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldNormal;
layout(location = 2) in vec2 vUV;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;

layout(set = 1, binding = 0) uniform sampler2D uDiffuse;

layout(push_constant) uniform MatPush {
    layout(offset = 0) vec4 uMatParams;
} matPush;

void main() {
    vec3 albedo = texture(uDiffuse, vUV).rgb;
    vec3 n = normalize(vWorldNormal);
    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(n * 0.5 + 0.5, 1.0);
    float roughness = clamp(matPush.uMatParams.x, 0.04, 1.0);
    float metallic = clamp(matPush.uMatParams.y, 0.0, 1.0);
    float ao = clamp(matPush.uMatParams.z, 0.0, 1.0);
    outMaterial = vec4(roughness, metallic, ao, 0.0);
}
