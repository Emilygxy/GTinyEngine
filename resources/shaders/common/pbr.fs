#version 330 core

// include common math functions
#include <includes/math_common.glsl>

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// PBR Textures
uniform sampler2D u_albedoMap;      // Albedo (Base Color) - texture unit 0
uniform sampler2D u_normalMap;      // Normal Map - texture unit 1
uniform sampler2D u_metallicMap;    // Metallic Map - texture unit 2
uniform sampler2D u_roughnessMap;   // Roughness Map - texture unit 3
uniform sampler2D u_aoMap;          // Ambient Occlusion Map - texture unit 4

// Material Properties (fallback when textures are not available)
uniform vec3 u_albedo;              // Base color
uniform float u_metallic;           // Metallic factor (0.0 = dielectric, 1.0 = metal)
uniform float u_roughness;          // Roughness factor (0.0 = smooth, 1.0 = rough)
uniform float u_ao;                 // Ambient occlusion factor

// Texture flags
uniform vec4 u_hasTextures;         // x: albedo, y: normal, z: metallic, w: roughness

// Lighting
uniform vec3 u_lightPos;
uniform vec3 u_lightColor;
uniform vec3 u_viewPos;

// Brightness and lighting controls
uniform float u_exposure;             // Exposure for tone mapping (default: 1.0)
uniform vec2 u_intensities;          // x: ambient(default: 0.3), y: light(default: 1.0)

// IBL (Image Based Lighting) - optional
uniform vec2 u_useIBL_ao;             // x: IBL, y: AO

// ============================================
// PBR Functions
// ============================================

// Normal Distribution Function (NDF) - Trowbridge-Reitz (GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / max(denom, 0.0000001); // prevent divide by zero
}

// Geometry Function - Schlick-GGX
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / max(denom, 0.0000001);
}

// Geometry Function - Smith's method
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness (for IBL)
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Calculate normal from normal map
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(u_normalMap, TexCoords).xyz * 2.0 - 1.0;
    
    vec3 Q1 = dFdx(FragPos);
    vec3 Q2 = dFdy(FragPos);
    vec2 st1 = dFdx(TexCoords);
    vec2 st2 = dFdy(TexCoords);
    
    vec3 N = normalize(Normal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * tangentNormal);
}

void main()
{
    // Sample textures or use uniform values
    vec3 albedo = u_hasTextures.x > 0.5 ? mix(pow(texture(u_albedoMap, TexCoords).rgb, vec3(2.2)), u_albedo, 0.05): u_albedo;
    float metallic = u_hasTextures.z > 0.5 ? texture(u_metallicMap, TexCoords).r : u_metallic;
    float roughness = u_hasTextures.w > 0.5 ? texture(u_roughnessMap, TexCoords).r : u_roughness;
    // AO texture is optional, always sample but use uniform as fallback
    float ao = u_useIBL_ao.y > 0.5 ? texture(u_aoMap, TexCoords).r : u_ao;
    if (ao < 0.0) ao = u_ao; // Use uniform value if texture returns 0 (likely unbound)
    
    // Get normal
    vec3 N = u_hasTextures.y > 0.5 ? getNormalFromMap() : normalize(Normal);
    vec3 V = normalize(u_viewPos - FragPos);
    
    // Calculate reflectance at normal incidence
    // For dielectrics, F0 is 0.04, for metals it's the albedo color
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Reflectance equation
    vec3 Lo = vec3(0.0);
    
    // Calculate per-light radiance
    vec3 L = normalize(u_lightPos - FragPos);
    vec3 H = normalize(V + L);
    float distance = length(u_lightPos - FragPos);
    
    // Improved attenuation: use linear + quadratic for better control
    // Formula: 1.0 / (1.0 + linear * distance + quadratic * distance^2)
    float linear = 0.09;
    float quadratic = 0.032;
    float attenuation = 1.0 / (1.0 + linear * distance + quadratic * distance * distance);
    
    // Apply light intensity multiplier
    float lightIntensity = u_intensities.y * attenuation;
    vec3 radiance = u_lightColor * lightIntensity;
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    // Add to outgoing radiance Lo
    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    
    // Ambient lighting with adjustable intensity
    vec3 ambient = u_intensities.x * albedo * ao;
    
    // Combine ambient and direct lighting
    vec3 color = ambient + Lo;
    
    // Apply exposure before tone mapping
    color *= u_exposure;
    
    // HDR tonemapping (Reinhard with exposure)
    color = color / (color + vec3(1.0));
    
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    
    FragColor = vec4(color, 1.0);
}
