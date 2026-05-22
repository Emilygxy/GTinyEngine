#ifndef SHADOW_COMMON_GLSL
#define SHADOW_COMMON_GLSL

float ShadowCompare(float currentDepth, float closestDepth, float slopeBias)
{
    return (currentDepth - slopeBias > closestDepth) ? 1.0 : 0.0;
}

float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap, vec3 normal, vec3 lightDir, float bias, float enablePCF)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.z < 0.0)
    {
        return 0.0;
    }

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
    {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float slopeBias = max(0.05 * (1.0 - dot(normal, lightDir)), bias);

    if (enablePCF < 0.5)
    {
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        return ShadowCompare(currentDepth, closestDepth, slopeBias);
    }

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += ShadowCompare(currentDepth, pcfDepth, slopeBias);
        }
    }
    return shadow / 9.0;
}

#endif // SHADOW_COMMON_GLSL
