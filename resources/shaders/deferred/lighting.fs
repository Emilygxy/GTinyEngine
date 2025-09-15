#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float lightIntensity;

void main()
{
    // 从G缓冲中获取数据
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Albedo = texture(gAlbedo, TexCoords).rgb;
    
    // 如果位置为0，说明这个像素没有几何体，跳过光照计算
    if (length(FragPos) < 0.001)
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    // 环境光
    vec3 ambient = 0.15 * Albedo;
    
    // 漫反射
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(Normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * Albedo * lightIntensity;
    
    // 镜面反射
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, Normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * lightColor * lightIntensity;
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
