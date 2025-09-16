#version 330 core
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec3 gPosition;
layout (location = 3) out float gDepth;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D u_diffuseTexture;
uniform vec3 u_objectColor;

void main()
{
    // store pos of the fragment in the first gbuffer
    gPosition = FragPos;

    // store normal of the fragment in the second gbuffer
    // Convert normal from [-1,1] to [0,1] range for texture storage
    gNormal = normalize(Normal) * 0.5 + 0.5;
    
    // store diffuse color of the fragment in the third gbuffer
    vec3 diffuse = texture(u_diffuseTexture, TexCoords).rgb * u_objectColor;
    gAlbedo = vec4(diffuse, 1.0);
    
    // store depth in the fourth gbuffer (linear depth)
    gDepth = gl_FragCoord.z;
}
