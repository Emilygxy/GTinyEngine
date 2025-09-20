// light calculateing common func 
// include common func and params

#ifndef LIGHTING_COMMON_GLSL
#define LIGHTING_COMMON_GLSL

// light type
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

vec3 CalculateDirectionalLight(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor, float intensity)
{
    // diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * intensity;
    
    // specular 
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * lightColor * intensity;
    
    return diffuse + specular;
}

vec3 CalculatePointLight(vec3 normal, vec3 viewDir, vec3 fragPos, vec3 lightPos, vec3 lightColor, float intensity, float range)
{
    vec3 lightDir = normalize(lightPos - fragPos);
    float distance = length(lightPos - fragPos);
    
    // distance attenuation
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    attenuation = clamp(attenuation, 0.0, 1.0);
    
    // range attenuation
    float rangeAttenuation = 1.0 - clamp(distance / range, 0.0, 1.0);
    rangeAttenuation = rangeAttenuation * rangeAttenuation;
    
    return CalculateDirectionalLight(normal, viewDir, lightDir, lightColor, intensity) * attenuation * rangeAttenuation;
}

#endif // LIGHTING_COMMON_GLSL
