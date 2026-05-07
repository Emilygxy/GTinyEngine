#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;
layout(set = 0, binding = 3) uniform sampler2D gDepth;

layout(std140, set = 0, binding = 4) uniform LightingUbo {
    vec4 dirLightDirection;
    vec4 dirLightColor;
    vec4 pointLightPositions[4];
    vec4 pointLightColors[4];
    vec4 cameraPos;
    mat4 invViewProj;
    vec4 clipInfo;
} lightingUbo;

vec3 reconstructWorldPos(vec2 uv, float depthSample) {
    vec2 ndc = uv * 2.0 - 1.0;
    vec4 clip = vec4(ndc, depthSample, 1.0);
    vec4 world = lightingUbo.invViewProj * clip;
    return world.xyz / max(world.w, 1e-5);
}

void main() {
    vec3 albedo = texture(gAlbedo, vUV).rgb;
    vec3 normal = normalize(texture(gNormal, vUV).rgb * 2.0 - 1.0);
    vec4 matSample = texture(gMaterial, vUV);
    float roughness = clamp(matSample.r, 0.04, 1.0);
    float metallic = clamp(matSample.g, 0.0, 1.0);
    float ao = clamp(matSample.b, 0.0, 1.0);
    float depth = texture(gDepth, vUV).r;

    vec3 worldPos = reconstructWorldPos(vUV, depth);
    vec3 viewDir = normalize(lightingUbo.cameraPos.xyz - worldPos);

    vec3 lightDir = normalize(lightingUbo.dirLightDirection.xyz);
    float ndotl = max(dot(normal, lightDir), 0.0);
    vec3 halfVec = normalize(lightDir + viewDir);
    float shininess = mix(96.0, 4.0, roughness);
    float specMask = mix(0.35, 1.0, metallic) * (1.0 - roughness * 0.85);
    float spec = pow(max(dot(normal, halfVec), 0.0), shininess) * specMask;

    float ambient = mix(0.1, 0.025, roughness) * ao;
    vec3 lit = albedo * lightingUbo.dirLightColor.rgb * (ambient + ndotl);
    lit += lightingUbo.dirLightColor.rgb * spec * 0.35;

    for (int i = 0; i < 4; ++i) {
        vec3 pc = lightingUbo.pointLightColors[i].rgb;
        if (dot(pc, pc) < 1e-6) {
            continue;
        }
        vec3 toLight = lightingUbo.pointLightPositions[i].xyz - worldPos;
        float dist2 = max(dot(toLight, toLight), 1e-4);
        vec3 L = toLight * inversesqrt(dist2);
        float nDotL = max(dot(normal, L), 0.0);
        float atten = 1.0 / dist2;
        lit += albedo * pc * nDotL * atten * ao;
    }

    outColor = vec4(lit, 1.0);
}
