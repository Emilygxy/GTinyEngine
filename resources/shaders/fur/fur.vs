#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out float HairLayer; // fur layer info

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float u_hairLength;
uniform float u_currentLayer;
uniform int u_numLayers;

void main() {
    // caculate dur expanding direction
    vec3 hairDirection = normalize(aNormal);
    
    // caculate fur pos base on current layer
    float layerRatio = u_currentLayer / float(u_numLayers);
    vec3 hairPos = aPos + hairDirection * u_hairLength * layerRatio;
    
    FragPos = vec3(model * vec4(hairPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    HairLayer = layerRatio;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}