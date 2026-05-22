#ifndef SHADOW_COMMON_GLSL
#define SHADOW_COMMON_GLSL

float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap, vec3 normal, vec3 lightDir, float bias)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
    {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float closestDepth = texture(shadowMap, projCoords.xy).r;

    float slopeBias = max(0.05 * (1.0 - dot(normal, lightDir)), bias);
    return (currentDepth - slopeBias > closestDepth) ? 1.0 : 0.0;
}

#endif // SHADOW_COMMON_GLSL
