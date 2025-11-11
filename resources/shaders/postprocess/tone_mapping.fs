#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

// include common math functions
#include <includes/math_common.glsl>

uniform sampler2D u_albedoMap;
uniform float u_exposure;
uniform int u_toneMappingType; // 0: Reinhard, 1: ACES

void main()
{
    vec3 color = texture(u_albedoMap, TexCoords).rgb;
    
    // apply exposure
    color *= u_exposure;
    
    // tone mapping
    vec3 mapped;
    if (u_toneMappingType == 0)
    {
        mapped = ReinhardToneMapping(color);
    }
    else
    {
        mapped = ACESToneMapping(color);
    }
    
    // gamma correction
    mapped = LinearToSRGB(mapped);
    
    FragColor = vec4(mapped, 1.0);
}
