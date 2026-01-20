# PBR (Physically Based Rendering) 渲染原理

## 目录
1. [概述](#概述)
2. [PBR的核心概念](#pbr的核心概念)
3. [Cook-Torrance BRDF模型](#cook-torrance-brdf模型)
4. [材质属性](#材质属性)
5. [实现细节](#实现细节)
6. [使用指南](#使用指南)

---

## 概述

**Physically Based Rendering (PBR)** 是一种基于物理的渲染技术，它试图模拟光线与物体表面相互作用的真实物理过程。与传统的Phong或Blinn-Phong光照模型不同，PBR使用更复杂的数学模型来更准确地描述材质的外观。

### PBR的优势

1. **物理准确性**：基于真实世界的物理规律，结果更符合直觉
2. **材质一致性**：在不同光照条件下，材质表现一致
3. **艺术友好**：艺术家可以使用直观的参数（如金属度、粗糙度）来创建材质
4. **跨平台一致性**：在不同渲染引擎中，相同的参数会产生相似的结果

---

## PBR的核心概念

### 1. 能量守恒 (Energy Conservation)

PBR遵循能量守恒定律：表面反射的光线能量不能超过入射光线的能量。这意味着：
- 漫反射和镜面反射的总和不能超过1.0
- 高镜面反射意味着低漫反射（金属材质）
- 低镜面反射意味着高漫反射（非金属材质）

### 2. 菲涅尔效应 (Fresnel Effect)

菲涅尔效应描述了光线在不同介质界面处的反射行为：
- **掠射角**（grazing angle）：当视线几乎平行于表面时，反射率增加
- **垂直角**（normal angle）：当视线垂直于表面时，反射率较低
- 金属在垂直角时也有较高的反射率，而非金属（电介质）的反射率约为4%

### 3. 微表面理论 (Microsurface Theory)

PBR假设表面由许多微小的镜面组成：
- **粗糙度**（Roughness）：控制微表面的随机性
  - 低粗糙度：表面光滑，微表面朝向一致，产生清晰的镜面反射
  - 高粗糙度：表面粗糙，微表面朝向随机，产生模糊的镜面反射
- **法线分布函数**（NDF）：描述微表面法线的分布

---

## Cook-Torrance BRDF模型

Cook-Torrance BRDF（Bidirectional Reflectance Distribution Function，双向反射分布函数）是PBR的核心数学模型。它将反射分为两个部分：

### BRDF公式

```
BRDF = kD * (albedo / π) + kS * (DFG / (4 * (N·L) * (N·V)))
```

其中：
- **kD**: 漫反射系数（diffuse coefficient）
- **kS**: 镜面反射系数（specular coefficient），kD + kS = 1.0
- **albedo**: 反照率（基础颜色）
- **DFG**: 镜面反射的三个组成部分
  - **D**: 法线分布函数（Normal Distribution Function）
  - **F**: 菲涅尔项（Fresnel term）
  - **G**: 几何函数（Geometry function）

### 1. 法线分布函数 (D) - Trowbridge-Reitz (GGX)

描述微表面法线的分布，控制镜面高光的形状和大小：

```
D(h) = α² / (π * ((n·h)² * (α² - 1) + 1)²)
```

其中：
- `α = roughness²`：粗糙度的平方
- `h`：半角向量（halfway vector），`h = normalize(L + V)`
- `n·h`：法线与半角向量的点积

**特点**：
- 低粗糙度：产生小而锐利的高光
- 高粗糙度：产生大而模糊的高光

### 2. 几何函数 (G) - Smith's Method

描述微表面之间的遮挡和阴影效果：

```
G(l,v) = G1(l) * G1(v)
G1(v) = n·v / ((n·v) * (1 - k) + k)
k = (roughness + 1)² / 8
```

**作用**：
- 在掠射角时减少反射光
- 模拟微表面之间的相互遮挡
- 防止能量损失

### 3. 菲涅尔项 (F) - Schlick近似

描述不同角度下的反射率变化：

```
F(h,v) = F0 + (1 - F0) * (1 - (h·v))⁵
```

其中：
- **F0**：垂直入射时的反射率
  - 电介质（非金属）：F0 ≈ 0.04
  - 金属：F0 = albedo（基础颜色）

**特点**：
- 掠射角时反射率接近1.0
- 垂直角时，金属反射率高，非金属反射率低

---

## 材质属性

PBR材质使用以下属性来描述表面：

### 1. Albedo (反照率/基础颜色)

- **定义**：表面在完全漫反射光照下的颜色
- **范围**：RGB值，通常在线性空间
- **用途**：
  - 非金属：直接作为漫反射颜色
  - 金属：作为F0值（垂直入射反射率）
- **纹理**：`u_albedoMap`

### 2. Metallic (金属度)

- **定义**：表面是金属还是非金属
- **范围**：0.0（非金属/电介质）到 1.0（金属）
- **作用**：
  - 控制F0值的选择
  - 影响漫反射和镜面反射的混合
- **纹理**：`u_metallicMap`（单通道，通常存储在R通道）

### 3. Roughness (粗糙度)

- **定义**：表面微表面的随机程度
- **范围**：0.0（完全光滑/镜面）到 1.0（完全粗糙/漫反射）
- **作用**：
  - 控制镜面高光的大小和模糊程度
  - 影响NDF和几何函数
- **纹理**：`u_roughnessMap`（单通道）

### 4. Normal Map (法线贴图)

- **定义**：用于模拟表面细节的凹凸
- **格式**：切线空间法线，RGB值映射到[-1, 1]
- **作用**：在不增加几何复杂度的情况下增加表面细节
- **纹理**：`u_normalMap`

### 5. Ambient Occlusion (AO) - 环境光遮蔽

- **定义**：描述表面凹陷处接收环境光的减少
- **范围**：0.0（完全遮蔽）到 1.0（无遮蔽）
- **作用**：增加阴影细节，增强深度感
- **纹理**：`u_aoMap`（单通道，可选）

---

## 实现细节

### 纹理单元分配

在我们的实现中，PBR纹理绑定到以下纹理单元：

- **GL_TEXTURE0**: Albedo Map
- **GL_TEXTURE1**: Normal Map
- **GL_TEXTURE2**: Metallic Map
- **GL_TEXTURE3**: Roughness Map
- **GL_TEXTURE4**: AO Map

### 着色器流程

1. **采样纹理**：从各个纹理贴图中采样材质属性
2. **计算F0**：根据金属度混合电介质F0（0.04）和albedo
3. **计算光照**：
   - 计算半角向量 `H = normalize(L + V)`
   - 计算NDF、几何函数和菲涅尔项
   - 组合成Cook-Torrance BRDF
4. **能量守恒**：
   - `kS = F`（菲涅尔项）
   - `kD = (1 - kS) * (1 - metallic)`
5. **最终颜色**：
   - `Lo = (kD * albedo / π + specular) * radiance * (N·L)`
   - `ambient = 0.03 * albedo * ao`
   - `color = ambient + Lo`

### 色调映射 (Tone Mapping)

PBR通常在高动态范围（HDR）中工作，需要色调映射到低动态范围（LDR）用于显示：

```glsl
// Reinhard色调映射
color = color / (color + vec3(1.0));
```

### 伽马校正 (Gamma Correction)

将线性空间颜色转换到sRGB空间：

```glsl
color = pow(color, vec3(1.0 / 2.2));
```

---

## 使用指南

### 创建PBR材质

```cpp
// 创建PBR材质
auto pbrMaterial = std::make_shared<PBRMaterial>();

// 设置纹理路径
pbrMaterial->SetAlbedoTexturePath("path/to/albedo.png");
pbrMaterial->SetNormalTexturePath("path/to/normal.png");
pbrMaterial->SetMetallicTexturePath("path/to/metallic.png");
pbrMaterial->SetRoughnessTexturePath("path/to/roughness.png");
pbrMaterial->SetAOTexturePath("path/to/ao.png"); // 可选

// 或者使用无纹理模式，直接设置属性
pbrMaterial->SetAlbedo(glm::vec3(0.8f, 0.2f, 0.2f)); // 红色
pbrMaterial->SetMetallic(0.0f);  // 非金属
pbrMaterial->SetRoughness(0.5f); // 中等粗糙度
pbrMaterial->SetAO(1.0f);        // 无遮蔽
```

### 材质参数建议

#### 常见材质类型

1. **金属材质**（如铁、铜、金）
   - Metallic: 0.8 - 1.0
   - Roughness: 0.1 - 0.3（抛光金属）或 0.4 - 0.7（生锈金属）
   - Albedo: 金属的基础颜色

2. **非金属材质**（如塑料、木材、石头）
   - Metallic: 0.0
   - Roughness: 0.3 - 0.8
   - Albedo: 材质的漫反射颜色

3. **混合材质**（如涂漆金属）
   - Metallic: 0.0 - 0.3
   - Roughness: 0.2 - 0.6
   - Albedo: 涂层的颜色

### 纹理制作建议

1. **Albedo贴图**：
   - 使用线性空间颜色
   - 不要包含光照信息（阴影、高光等）
   - 金属材质：使用金属的实际颜色

2. **Metallic贴图**：
   - 单通道灰度图
   - 白色（1.0）= 金属，黑色（0.0）= 非金属
   - 可以包含混合区域（0.0 - 1.0之间的值）

3. **Roughness贴图**：
   - 单通道灰度图
   - 白色（1.0）= 粗糙，黑色（0.0）= 光滑
   - 通常与Metallic一起存储在MetallicRoughness贴图中（Metallic在R通道，Roughness在G通道）

4. **Normal贴图**：
   - 切线空间法线
   - 通常从高度图（Height Map）生成
   - RGB值映射到[-1, 1]范围

### 性能优化

1. **纹理压缩**：使用压缩纹理格式（如BC7、ASTC）
2. **纹理分辨率**：根据物体在屏幕上的大小选择合适的纹理分辨率
3. **可选纹理**：AO贴图是可选的，如果不需要可以省略
4. **合并贴图**：可以将Metallic和Roughness存储在同一个纹理的不同通道中

---

## 扩展功能

### Image Based Lighting (IBL)

当前实现使用简单的环境光近似。完整的PBR实现可以包括：

1. **环境贴图**（Environment Map）：用于基于图像的光照
2. **预计算辐照度**（Precomputed Irradiance）：用于漫反射环境光
3. **预滤波环境贴图**（Pre-filtered Environment Map）：用于镜面反射环境光
4. **BRDF积分查找表**（BRDF Integration Lookup Table）：用于镜面反射的精确计算

这些功能可以显著提高渲染质量，特别是在室内或复杂光照环境中。

---

## 参考资料

1. **LearnOpenGL - PBR Theory**: https://learnopengl.com/PBR/Theory
2. **Real-Time Rendering**: 第9章 - Physically Based Shading
3. **SIGGRAPH Course Notes**: "Physically Based Shading at Disney"
4. **Unreal Engine Documentation**: PBR Material Guide

---

## 总结

PBR渲染通过基于物理的数学模型，提供了更真实、更一致的材质表现。Cook-Torrance BRDF模型通过法线分布函数、几何函数和菲涅尔项的组合，准确地描述了光线与表面的相互作用。通过合理设置Albedo、Metallic、Roughness等参数，可以创建各种真实世界的材质效果。
