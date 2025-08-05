#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
uniform sampler2D diffuseTexture;

uniform vec3 objectColor;

void main()
{
    // get texture color
    vec4 texColor = texture(diffuseTexture, TexCoords);

    // combine object color with texture color
    vec3 result = objectColor * texColor.rgb;
    FragColor = vec4(result, texColor.a);
}