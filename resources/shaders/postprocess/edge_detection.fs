#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D u_normalMap;
uniform sampler2D u_depthMap;
uniform vec2 u_screenSize;
uniform float u_edgeThreshold;

void main()
{
    vec2 texelSize = 1.0 / u_screenSize;
    
    // sample normal of the surrounding pixels
    vec3 normal = texture(u_normalMap, TexCoords).rgb;
    vec3 normalRight = texture(u_normalMap, TexCoords + vec2(texelSize.x, 0.0)).rgb;
    vec3 normalDown = texture(u_normalMap, TexCoords + vec2(0.0, texelSize.y)).rgb;
    
    // sample depth of the surrounding pixels
    float depth = texture(u_depthMap, TexCoords).r;
    float depthRight = texture(u_depthMap, TexCoords + vec2(texelSize.x, 0.0)).r;
    float depthDown = texture(u_depthMap, TexCoords + vec2(0.0, texelSize.y)).r;
    
    // calculate the difference of the normal
    float normalEdge = length(normal - normalRight) + length(normal - normalDown);
    
    // calculate the difference of the depth
    float depthEdge = abs(depth - depthRight) + abs(depth - depthDown);
    
    // combine the edge detection
    float edge = max(normalEdge, depthEdge * 10.0);
    edge = step(u_edgeThreshold, edge);
    
    FragColor = vec4(vec3(edge), 1.0);
}
