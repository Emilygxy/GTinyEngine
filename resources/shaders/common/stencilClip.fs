uniform vec4 u_ClippingPlanes;  // xyz: normal vector, w: distance from origin

float dist = 0.0;
dist += clamp(dot(a_position.xyz, u_ClippingPlanes.xyz) - u_ClippingPlanes.w, 0.0, 1000.0);
if (dist > 0.0) {
    discard;
}