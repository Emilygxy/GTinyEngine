#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D u_diffuseTexture; // texture sampler(optional)

uniform vec3 u_lightPos;
uniform vec3 u_viewPos;
uniform vec3 u_lightColor;
uniform vec3 u_objectColor;
uniform vec4 u_Strengths; // x: ambientStrength, y: diffuseStrength, z: specularStrength, w: shininess

void main()
{
    // 1. Calculate the ambient lighting
    float ambientStrength = u_Strengths.x;
    vec3 ambient = ambientStrength * u_lightColor;

    // 2. Calculate the diffuse lighting
    float diffuseStrength = u_Strengths.y;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(u_lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diffuseStrength * diff * u_lightColor;

    // 3. Calculate the specular lighting
    float specularStrength = u_Strengths.z;
    float shininess = u_Strengths.w;
    vec3 viewDir = normalize(u_viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * u_lightColor;
    
    // sample texture if there is a texture sampler
    vec3 baseColor = u_objectColor;
    baseColor *= texture(u_diffuseTexture, TexCoords).rgb;
    // 4. Combine the lighting results and write the final color to the FragColor output
    vec3 result = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(result, 1.0);
}