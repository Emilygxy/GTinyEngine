#version 330 core
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec3 gPosition;
layout (location = 3) out float gDepth;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in float HairLayer;

uniform vec3 u_hairColor;
uniform vec3 u_lightPos;
uniform vec3 u_lightColor;
uniform vec3 u_viewPos;
uniform float u_hairDensity;
uniform float u_hairLength;

// fur lighting calculation (simplified Marschner model)
vec3 CalculateHairLighting(vec3 hairColor, vec3 lightDir, vec3 viewDir, vec3 hairTangent) {
    // R component (specular reflection)
    vec3 R = reflect(-lightDir, hairTangent);
    float specularR = pow(max(dot(viewDir, R), 0.0), 32.0);
    
    // TT component (transmission-transmission)
    vec3 TT = hairColor * 0.3;
    
    // TRT component (transmission-reflection-transmission)
    vec3 TRT = hairColor * 0.1;
    
    return hairColor * (specularR + TT + TRT);
}

void main() {
    // calculate hair transparency (the outer layer is more transparent)
    float alpha = (1.0 - HairLayer) * u_hairDensity;
    
    // fur lighting calculation
    vec3 lightDir = normalize(u_lightPos - FragPos);
    vec3 viewDir = normalize(u_viewPos - FragPos);
    vec3 hairTangent = normalize(Normal);
    
    vec3 hairLighting = CalculateHairLighting(u_hairColor, lightDir, viewDir, hairTangent);
    
    // output to G-Buffer
    gPosition = FragPos;
    gNormal = normalize(Normal) * 0.5 + 0.5; // convert to [0,1] range
    gAlbedo = vec4(hairLighting, alpha);
    gDepth = gl_FragCoord.z;
}