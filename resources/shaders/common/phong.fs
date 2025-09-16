#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D u_diffuseTexture; // texture sampler(optional)
uniform sampler2D u_backgroundMap; // texture sampler(optional)
uniform sampler2D u_geomAlbedoMap; // texture sampler(optional)
uniform sampler2D u_geomNormalMap; // texture sampler(optional)
uniform sampler2D u_geomDepthMap; // texture sampler(optional)

uniform vec3 u_lightPos;
uniform vec3 u_viewPos;
uniform vec3 u_lightColor;
uniform vec3 u_objectColor;
uniform vec4 u_Strengths; // x: ambientStrength, y: diffuseStrength, z: specularStrength, w: shininess
uniform vec4 u_useBlinn_Geometry; // if true, use Blinn-Phong shading model, otherwise use Phong shading model

bool isBlinnPhong()
{
    return u_useBlinnPhong.x > 0.5;
}

bool isUseGeometryTarget()
{
    return u_useBlinnPhong.y > 0.5;
}

vec3 CalculatePhongColor()
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
    result *= texture(u_diffuseTexture, TexCoords).rgb;

    if(isUseGeometryTarget())
    {
        vec3 geomAlbedo = texture(u_geomAlbedoMap, TexCoords).rgb;
        vec3 geomNormal = texture(u_geomNormalMap, TexCoords).rgb;
        float geomDepth = texture(u_geomDepthMap, TexCoords).r;
        vec3 geomPos = vec3(TexCoords, geomDepth) * 2.0 - 1.0;
        vec3 geomViewDir = normalize(u_viewPos - geomPos);
        vec3 geomNormalDir = normalize(geomNormal * 2.0 - 1.0);
        vec3 geomLightDir = normalize(u_lightPos - geomPos);
        float geomDiffuse = max(dot(geomNormalDir, geomLightDir), 0.0);
        float geomSpecular = 0.0;
        if(isBlinnPhong())
        {
            vec3 geomHalfwayDir = normalize(geomLightDir + geomViewDir);
            geomSpecular = pow(max(dot(geomNormalDir, geomHalfwayDir), 0.0), u_Strengths.w);
        }
        else
        {
            vec3 geomReflectDir = reflect(-geomLightDir, geomNormalDir);
            geomSpecular = pow(max(dot(geomViewDir, geomReflectDir), 0.0), u_Strengths.w);
        }
        vec3 geomColor = geomAlbedo * (u_Strengths.y * geomDiffuse + u_Strengths.z * geomSpecular);
        result *= geomColor;
        FragColor = vec4(result, 1.0);
        return;
    }

    result *= CalculatePhongColor();

    FragColor = vec4(result, 1.0);
}