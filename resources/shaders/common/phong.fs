#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D u_diffuseTexture; // texture sampler(optional)
uniform sampler2D u_backgroundMap; // texture sampler(optional)
uniform sampler2D u_geomAlbedoMap; // texture sampler(optional)
uniform sampler2D u_geomNormalMap; // texture sampler(optional)
uniform sampler2D u_geomPositionMap; // texture sampler(optional)
uniform sampler2D u_geomDepthMap; // texture sampler(optional)

uniform vec3 u_lightPos;
uniform vec3 u_viewPos;
uniform vec3 u_lightColor;
uniform vec3 u_objectColor;
uniform vec4 u_Strengths; // x: ambientStrength, y: diffuseStrength, z: specularStrength, w: shininess
uniform vec4 u_useBlinn_Geometry; // if true, use Blinn-Phong shading model, otherwise use Phong shading model

bool isBlinnPhong()
{
    return u_useBlinn_Geometry.x > 0.5;
}

bool isUseGeometryTarget()
{
    return u_useBlinn_Geometry.y > 0.5;
}

vec3 CalculatePhongColor()
{
    // 1. Calculate the ambient lighting
    float ambientStrength = u_Strengths.x;
    vec3 ambient = ambientStrength * u_lightColor;

    // 2. Calculate the diffuse lighting
    float diffuseStrength = u_Strengths.y;

    vec3 norm = normalize(Normal);
    vec3 fragPos = FragPos;
    if(isUseGeometryTarget())
    {
        norm =texture(u_geomNormalMap, TexCoords).rgb;
        norm = normalize(norm);
        fragPos = texture(u_geomPositionMap, TexCoords).rgb;
    }
    vec3 lightDir = normalize(u_lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diffuseStrength * diff * u_lightColor;

    // 3. Calculate the specular lighting
    float specularStrength = u_Strengths.z;
    float shininess = u_Strengths.w;
    vec3 viewDir = normalize(u_viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = 0.0;
    if(isBlinnPhong())
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    }
    else
    {
        spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    }
    vec3 specular = specularStrength * spec * u_lightColor;
    
    // 4. Combine the lighting results and write the final color to the FragColor output
    vec3 result = (ambient + diffuse + specular);

    return result;
}

void main()
{
    // sample texture if there is a texture sampler
    vec3 result = u_objectColor;
    result *= texture(u_backgroundMap, TexCoords).rgb;
    vec3 geomAlbedo = texture(u_diffuseTexture, TexCoords).rgb;
    if(isUseGeometryTarget())
    {
        geomAlbedo = texture(u_geomAlbedoMap, TexCoords).rgb;
    }
    result *= geomAlbedo;

    result *= CalculatePhongColor();
    // gamma correction
    // result = pow(result, vec3(1.0/2.2));

    FragColor = vec4(result, 1.0);
}