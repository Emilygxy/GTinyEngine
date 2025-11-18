#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_backgroundMap;
uniform vec4 u_backgroungColor;
uniform vec2 u_texture_factor; // x = enable texture, y = texture mix factor

void main() {
    vec4 outColor = u_backgroungColor;
    float mix_factor = u_texture_factor.y;
    bool enable_texture = u_texture_factor.x > 0.5;
    if (enable_texture) 
    {
        outColor = texture(u_backgroundMap, TexCoord);
        outColor = mix(outColor, u_backgroungColor, mix_factor); // mix with background color, mix_factor == 0 means no mix
    }
    else
    {
        outColor = u_backgroungColor;
    }

    FragColor = outColor;
}