#version 330 core
in vec3 TexCoords;
out vec4 FragColor;
uniform samplerCube u_skyboxMap;
void main()
{
    
    
    // Original texture sampling (commented out for debugging)
    FragColor = texture(u_skyboxMap, TexCoords);

    // Debug: Output different colors based on TexCoords to verify shader is working
    if (TexCoords.x > 0.0) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red for positive X
    } else if (TexCoords.y > 0.0) {
        FragColor = vec4(0.0, 1.0, 0.0, 1.0); // Green for positive Y
    } else if (TexCoords.z > 0.0) {
        FragColor = vec4(0.0, 0.0, 1.0, 1.0); // Blue for positive Z
    } else {
        FragColor = vec4(1.0, 1.0, 0.0, 1.0); // Yellow for negative values
    }
}