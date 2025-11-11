// math common func
// include comon funcs and params

#ifndef MATH_COMMON_GLSL
#define MATH_COMMON_GLSL

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float HALF_PI = 1.57079632679;
const float INV_PI = 0.31830988618;

float LinearToSRGB(float linear)
{
    if (linear <= 0.0031308)
        return linear * 12.92;
    else
        return 1.055 * pow(linear, 1.0/2.4) - 0.055;
}

vec3 LinearToSRGB(vec3 linear)
{
    return vec3(
        LinearToSRGB(linear.r),
        LinearToSRGB(linear.g),
        LinearToSRGB(linear.b)
    );
}

float SRGBToLinear(float sRGB)
{
    if (sRGB <= 0.04045)
        return sRGB / 12.92;
    else
        return pow((sRGB + 0.055) / 1.055, 2.4);
}

vec3 SRGBToLinear(vec3 sRGB)
{
    return vec3(
        SRGBToLinear(sRGB.r),
        SRGBToLinear(sRGB.g),
        SRGBToLinear(sRGB.b)
    );
}

vec3 ReinhardToneMapping(vec3 color)
{
    return color / (color + vec3(1.0));
}

vec3 ACESToneMapping(vec3 color)
{
    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;
    return clamp((color * (A * color + B)) / (color * (C * color + D) + E), 0.0, 1.0);
}

#endif // MATH_COMMON_GLSL
