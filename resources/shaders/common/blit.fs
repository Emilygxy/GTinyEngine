#version 330 core
out vec4 FragColor;

uniform sampler2D u_screenTexture;

in vec2 texCoord;

void main()
{
    // get texture color
    vec4 texColor = texture(u_screenTexture, texCoord);

    FragColor = texColor;
}