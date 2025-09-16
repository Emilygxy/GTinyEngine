#version 330 core
out vec4 FragColor;

uniform sampler2D u_backgroundMap;  // SkyboxPass output
uniform sampler2D u_screenTexture;  // BasePass output

in vec2 TexCoords;

void main()
{
    // Sample the textures
    vec4 background = texture(u_backgroundMap, TexCoords);
    vec4 screen = texture(u_screenTexture, TexCoords);
    
    // In deferred rendering, BasePass should contain the final lit scene
    // If BasePass has content (alpha > 0), use it; otherwise use background
    if (screen.a > 0.0)
    {
        // Use BasePass result (lit geometry)
        FragColor = screen;
    }
    else
    {
        // Use SkyboxPass result (background)
        FragColor = background;
    }
}