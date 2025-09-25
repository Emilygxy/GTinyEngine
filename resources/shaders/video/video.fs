#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D u_videoTexture;
uniform float u_videoWidth;
uniform float u_videoHeight;
uniform float u_currentTime;
uniform float u_duration;
uniform float u_frameRate;
uniform float u_time;
uniform float u_isPlaying;

void main()
{
    // 获取视频纹理颜色
    vec4 videoColor = texture(u_videoTexture, TexCoords);
    
    // 添加一些视觉效果
    float timeEffect = sin(u_time * 2.0) * 0.1 + 0.9; // 轻微的时间变化
    float playingEffect = u_isPlaying > 0.5 ? 1.0 : 0.5; // 播放时更亮
    
    // 应用效果
    videoColor.rgb *= timeEffect * playingEffect;
    
    // 添加边框效果
    vec2 border = smoothstep(vec2(0.0), vec2(0.05), TexCoords) * 
                  smoothstep(vec2(1.0), vec2(0.95), TexCoords);
    float borderFactor = border.x * border.y;
    
    // 边框颜色
    vec3 borderColor = vec3(0.2, 0.2, 0.2);
    videoColor.rgb = mix(borderColor, videoColor.rgb, borderFactor);
    
    // 添加进度条效果（在底部）
    if (TexCoords.y < 0.05 && u_duration > 0.0) {
        float progress = u_currentTime / u_duration;
        if (TexCoords.x < progress) {
            videoColor.rgb = mix(videoColor.rgb, vec3(1.0, 0.0, 0.0), 0.8);
        }
    }
    
    FragColor = videoColor;
}

