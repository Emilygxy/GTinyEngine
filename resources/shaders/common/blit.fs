#version 330 core
out vec4 FragColor;

uniform sampler2D u_backgroundMap;
uniform sampler2D u_screenTexture;

in vec2 TexCoords;

void main()
{
    // Sample the background texture
    vec4 background = texture(u_backgroundMap, TexCoords);
    
    // Simply blit the input texture
    vec4 screen = texture(u_screenTexture, TexCoords);
    vec4 finalColor = mix(background,screen,0.5);
    //gamma correction
    // finalColor = pow(finalColor, vec4(1.0/2.2));
    FragColor = finalColor;
}