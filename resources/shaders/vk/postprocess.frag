#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uScene;

layout(std140, set = 0, binding = 1) uniform ToneMappingParams {
    vec4 params;
} uParams;

vec3 toneMapExposure(vec3 hdrColor, float exposure)
{
    return vec3(1.0) - exp(-hdrColor * exposure);
}

float rgbToLuma(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

vec3 fxaaSample(sampler2D tex, vec2 uv, vec2 rcpFrame)
{
    vec3 rgbN = texture(tex, uv + vec2(0.0, -rcpFrame.y)).rgb;
    vec3 rgbW = texture(tex, uv + vec2(-rcpFrame.x, 0.0)).rgb;
    vec3 rgbE = texture(tex, uv + vec2(rcpFrame.x, 0.0)).rgb;
    vec3 rgbS = texture(tex, uv + vec2(0.0, rcpFrame.y)).rgb;
    vec3 rgbM = texture(tex, uv).rgb;

    float lumaM = rgbToLuma(rgbM);
    float lumaN = rgbToLuma(rgbN);
    float lumaW = rgbToLuma(rgbW);
    float lumaE = rgbToLuma(rgbE);
    float lumaS = rgbToLuma(rgbS);

    float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaE, lumaS)));
    float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaE, lumaS)));
    float range = rangeMax - rangeMin;
    if (range < max(0.05 * rangeMax, 1e-4)) {
        return rgbM;
    }

    vec2 dir = vec2(
        -((lumaN + lumaS) - (lumaW + lumaE)),
        ((lumaN + lumaW) - (lumaS + lumaE))
    );
    float dirReduce = max((lumaN + lumaW + lumaE + lumaS) * 0.03125, 1e-4);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * rcpFrame;

    vec3 rgbA = 0.5 * (texture(tex, uv + dir * -0.5).rgb + texture(tex, uv + dir * 0.5).rgb);
    float lumaA = rgbToLuma(rgbA);
    if (lumaA < rangeMin || lumaA > rangeMax) {
        return rgbM;
    }
    return rgbA;
}

void main()
{
    float exposure = max(uParams.params.x, 0.0001);
    float gamma = max(uParams.params.y, 0.0001);
    float useFxaa = uParams.params.z;

    vec2 rcpFrame = 1.0 / vec2(textureSize(uScene, 0));
    vec3 hdr = texture(uScene, vUV).rgb;
    if (useFxaa > 0.5) {
        hdr = fxaaSample(uScene, vUV, rcpFrame);
    }

    vec3 mapped = toneMapExposure(hdr, exposure);
    mapped = pow(mapped, vec3(1.0 / gamma));
    outColor = vec4(mapped, 1.0);
}
