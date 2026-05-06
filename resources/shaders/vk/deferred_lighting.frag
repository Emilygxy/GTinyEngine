#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;
layout(set = 0, binding = 3) uniform sampler2D gDepth;

void main() {
    vec3 albedo = texture(gAlbedo, vUV).rgb;
    vec3 normalPacked = texture(gNormal, vUV).rgb;
    vec3 normal = normalize(normalPacked * 2.0 - 1.0);
    vec3 material = texture(gMaterial, vUV).rgb;
    float depth = texture(gDepth, vUV).r;

    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.2));
    float ndotl = max(dot(normal, lightDir), 0.0);

    float roughness = material.r;
    float ambient = mix(0.15, 0.05, roughness);
    vec3 lit = albedo * (ambient + ndotl);

    // Keep depth read alive in MVP path to verify descriptor binding is valid.
    lit *= (0.9 + 0.1 * clamp(depth, 0.0, 1.0));
    outColor = vec4(lit, 1.0);
}

