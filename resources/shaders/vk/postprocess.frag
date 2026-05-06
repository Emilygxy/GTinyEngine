#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uScene;

layout(set = 0, binding = 1) uniform ToneMappingParams {
    vec4 params; // x: exposure, y: gamma
} uParams;

vec3 ToneMap(vec3 hdrColor, float exposure)
{
    return vec3(1.0) - exp(-hdrColor * exposure);
}

void main()
{
    vec3 hdr = texture(uScene, vUV).rgb;
    float exposure = max(uParams.params.x, 0.0001);
    float gamma = max(uParams.params.y, 0.0001);
    vec3 mapped = ToneMap(hdr, exposure);
    mapped = pow(mapped, vec3(1.0 / gamma));
    outColor = vec4(mapped, 1.0);
}

