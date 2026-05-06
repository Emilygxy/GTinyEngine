#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;
layout(set = 0, binding = 3) uniform sampler2D gDepth;
layout(set = 0, binding = 4) uniform LightingUbo {
    vec4 dirLightDirection;
    vec4 dirLightColor;
    vec4 pointLightPositions[4];
    vec4 pointLightColors[4];
    vec4 cameraPos;
} lightingUbo;

void main() {
    vec3 albedo = texture(gAlbedo, vUV).rgb;
    vec3 normalPacked = texture(gNormal, vUV).rgb;
    vec3 normal = normalize(normalPacked * 2.0 - 1.0);
    vec3 material = texture(gMaterial, vUV).rgb;
    float depth = texture(gDepth, vUV).r;

    vec3 lightDir = normalize(lightingUbo.dirLightDirection.xyz);
    float ndotl = max(dot(normal, lightDir), 0.0);

    float roughness = material.r;
    float ambient = mix(0.15, 0.05, roughness);
    vec3 lit = albedo * lightingUbo.dirLightColor.rgb * (ambient + ndotl);
    vec3 fragPos = vec3((vUV - vec2(0.5)) * 2.0, depth);
    for (int i = 0; i < 4; ++i) {
        vec3 toLight = lightingUbo.pointLightPositions[i].xyz - fragPos;
        float dist2 = max(dot(toLight, toLight), 0.05);
        vec3 l = normalize(toLight);
        float nDotL = max(dot(normal, l), 0.0);
        lit += albedo * lightingUbo.pointLightColors[i].rgb * nDotL / dist2;
    }

    // Keep depth read alive in MVP path to verify descriptor binding is valid.
    float viewFalloff = clamp(1.0 / (1.0 + length(lightingUbo.cameraPos.xyz) * 0.1), 0.5, 1.0);
    lit *= (0.9 + 0.1 * clamp(depth, 0.0, 1.0)) * viewFalloff;
    outColor = vec4(lit, 1.0);
}

