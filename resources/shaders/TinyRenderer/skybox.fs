#version 330 core
in vec3 TexCoords;
out vec4 FragColor;
uniform samplerCube u_skyboxMap;
void main()
{
    FragColor = texture(u_skyboxMap, TexCoords);
}