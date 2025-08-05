# Phong 光照模型理论与实现

## 一、Phong 光照模型理论

Phong 光照模型是一种经典的局部光照模型，用于模拟物体表面的高光和漫反射效果。它由三部分组成：

### 1. 环境光（Ambient）

表示场景中无处不在的光线，通常用一个常量近似。
\[
I_{ambient} = k_a \cdot I_a
\]
- \(k_a\)：环境光反射系数
- \(I_a\)：环境光强度

### 2. 漫反射（Diffuse）

表示光线照射到粗糙表面后向各个方向反射的部分，遵循 Lambert 定律。
\[
I_{diffuse} = k_d \cdot I_l \cdot \max(0, \mathbf{N} \cdot \mathbf{L})
\]
- \(k_d\)：漫反射系数
- \(I_l\)：光源强度
- \(\mathbf{N}\)：法线向量
- \(\mathbf{L}\)：指向光源的单位向量

### 3. 镜面反射（Specular）

表示光线在光滑表面上产生的高光，依赖于视角。
\[
I_{specular} = k_s \cdot I_l \cdot \max(0, \mathbf{R} \cdot \mathbf{V})^n
\]
- \(k_s\)：镜面反射系数
- \(\mathbf{R}\)：反射方向
- \(\mathbf{V}\)：视线方向
- \(n\)：高光指数（shininess）

### 4. 总光照

\[
I = I_{ambient} + I_{diffuse} + I_{specular}
\]

---

## 二、Phong 光照模型 GLSL 实现示例

假设输入有：
- `in vec3 FragPos;`
- `in vec3 Normal;`
- `in vec2 TexCoords;`
- uniform 变量：`lightPos`, `viewPos`, `lightColor`, `objectColor`, `ambientStrength`, `diffuseStrength`, `specularStrength`, `shininess`
- 可选：纹理采样

```glsl
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

uniform float ambientStrength;   // 环境光强度
uniform float diffuseStrength;   // 漫反射强度
uniform float specularStrength;  // 镜面反射强度
uniform float shininess;         // 高光指数

uniform sampler2D diffuseTexture; // 可选：有纹理时

void main()
{
    // 1. 环境光
    vec3 ambient = ambientStrength * lightColor;

    // 2. 漫反射
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diffuseStrength * diff * lightColor;

    // 3. 镜面反射
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    // 4. 纹理采样（如有）
    vec3 baseColor = objectColor;
    // 如果有纹理，使用纹理颜色
    baseColor *= texture(diffuseTexture, TexCoords).rgb;

    // 5. 合成
    vec3 result = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(result, 1.0);
}
```
---

## 三、使用建议

- 在 C++ 代码中设置各项参数（如 `ambientStrength`、`diffuseStrength`、`specularStrength`、`shininess`）。
- 可以通过 uniform 传递不同的光源、材质参数，实现不同的视觉效果。
- 如果没有纹理，可以去掉 `texture` 相关代码，直接用 `objectColor`。


## 参考

- [Phong Reflection Model - Wikipedia](https://en.wikipedia.org/wiki/Phong_reflection_model)
- [LearnOpenGL - Lighting](https://learnopengl.com/Lighting/Basic-Lighting)