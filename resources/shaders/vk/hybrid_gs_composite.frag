#version 450

layout (location = 0) in vec2 v_uv;

layout (set = 0, binding = 0) uniform sampler2D u_litHdr;
layout (set = 0, binding = 1) uniform sampler2D u_gsColor;
layout (set = 0, binding = 2) uniform sampler2D u_gsDepth;
layout (set = 0, binding = 3) uniform sampler2D u_sceneDepth;

layout (std140, set = 0, binding = 4) uniform CompositeUbo {
    mat4 invViewProj;
    mat4 invProj;
    vec4 clipInfo;   // zNear, zFar, width, height
    vec4 params;     // depth_bias, depth_test_min_alpha, alpha_cutoff, unused
    vec4 nearFar;    // gs depth linearization near, far (same as GS push constants)
};

layout (location = 0) out vec4 out_color;

void main() {
    vec3 lit = texture(u_litHdr, v_uv).rgb;
    vec4 gsc = texture(u_gsColor, v_uv);
    float gsA = gsc.a;
    float depthBias = params.x;
    float minAlpha = params.y;
    float alphaCut = params.z;

    if (gsA < alphaCut) {
        out_color = vec4(lit, 1.0);
        return;
    }

    vec2 ndc = v_uv * 2.0 - 1.0;
    float sceneRaw = texture(u_sceneDepth, v_uv).r;
    vec4 clipScene = vec4(ndc.x, ndc.y, sceneRaw, 1.0);
    vec4 viewScene = invProj * clipScene;
    float meshVz = viewScene.z / max(viewScene.w, 1e-6);

    float gsPacked = texture(u_gsDepth, v_uv).r;
    float gn = nearFar.x;
    float gf = nearFar.y;
    float gsViewZ = -(gn + gsPacked * (gf - gn));

    bool depthTest = gsA >= minAlpha;
    if (depthTest && gsViewZ > meshVz + depthBias) {
        out_color = vec4(lit, 1.0);
        return;
    }

    vec3 rgb = gsc.rgb;
    out_color = vec4(rgb * gsA + lit * (1.0 - gsA), 1.0);
}
