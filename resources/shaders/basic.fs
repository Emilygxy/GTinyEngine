#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// include common math functions
#include <includes/math_common.glsl>

uniform sampler2D u_diffuseTexture;
uniform vec3 u_objectColor;
uniform vec3 u_lightPos;
uniform vec3 u_lightColor;
uniform vec3 u_viewPos;

void main()
{
    // ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * u_lightColor;
    
    // diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(u_lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_lightColor;
    
    // specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(u_viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * u_lightColor;
    
    // combine results
    vec3 result = (ambient + diffuse + specular) * u_objectColor;
    
    // apply texture if available
    vec3 textureColor = texture(u_diffuseTexture, TexCoords).rgb;
    result *= textureColor;
    
    FragColor = vec4(result, 1.0);
}
