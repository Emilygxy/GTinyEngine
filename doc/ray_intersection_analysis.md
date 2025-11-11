# Ray-AABB Intersection 算法分析

## 概述

`RayIntersection`函数实现了射线与轴对齐包围盒（Axis-Aligned Bounding Box, AABB）的相交检测算法。这是3D图形学中鼠标拾取（Mouse Picking）功能的核心算法之一。

## 算法原理：Slab Method（平板法）

### 基本思想

Slab方法基于以下观察：一个AABB可以看作是三个相互垂直的"平板"（slab）的交集：
- X轴平板：由两个平行于YZ平面的平面组成
- Y轴平板：由两个平行于XZ平面的平面组成  
- Z轴平板：由两个平行于XY平面的平面组成

### 数学原理

对于射线方程：`P(t) = Origin + t * Direction`

对于每个轴i（X, Y, Z），射线与AABB的两个平行平面相交的参数为：
```
t1 = (aabb.min[i] - rayOrigin[i]) / rayDirection[i]
t2 = (aabb.max[i] - rayOrigin[i]) / rayDirection[i]
```

## 算法步骤详解

### 步骤1：初始化
```cpp
float tMin = 0.0f;                              // 最近相交点参数
float tMax = std::numeric_limits<float>::max(); // 最远相交点参数
```

### 步骤2：逐轴检测
对每个轴（X, Y, Z）进行检测：

#### 2.1 处理平行情况
```cpp
if (std::abs(rayDirection[i]) < 1e-6f) {
    // 射线平行于该轴平面
    if (rayOrigin[i] < aabb.min[i] || rayOrigin[i] > aabb.max[i]) {
        return false; // 射线在AABB外部
    }
}
```

#### 2.2 计算相交参数
```cpp
float invDir = 1.0f / rayDirection[i];
float t1 = (aabb.min[i] - rayOrigin[i]) * invDir;
float t2 = (aabb.max[i] - rayOrigin[i]) * invDir;
```

#### 2.3 确保t1 ≤ t2
```cpp
if (t1 > t2) {
    std::swap(t1, t2);
}
```

#### 2.4 更新相交区间
```cpp
tMin = std::max(tMin, t1);  // 取较大值作为最近点
tMax = std::min(tMax, t2);  // 取较小值作为最远点

if (tMin > tMax) {
    return false; // 相交区间为空，无相交
}
```

### 步骤3：确定最终相交点
```cpp
if (tMin < 0.0f) {
    if (tMax < 0.0f) {
        return false; // AABB在射线起点后方
    }
    t = tMax; // 使用远相交点
} else {
    t = tMin; // 使用近相交点
}
```

## 可视化示意图

### 2D情况下的Slab方法

```
Y轴
 ↑
 │     AABB
 │   ┌─────┐ ← t2_y (远平面)
 │   │     │
 │   │     │
 │   └─────┘ ← t1_y (近平面)
 │
 └─────────────→ X轴

射线: Origin + t * Direction

与Y轴平板相交：
t1_y = (min.y - origin.y) / direction.y  (近平面)
t2_y = (max.y - origin.y) / direction.y  (远平面)

与X轴平板相交：
t1_x = (min.x - origin.x) / direction.x  (近平面)  
t2_x = (max.x - origin.x) / direction.x  (远平面)

相交区间：[max(t1_x, t1_y), min(t2_x, t2_y)]
```

### 算法执行流程图

```
开始
 ↓
初始化 tMin=0, tMax=∞
 ↓
对每个轴 i ∈ {X,Y,Z}:
 ├─ 射线平行于平面？
 │   ├─ 是 → 检查原点是否在AABB内
 │   │        ├─ 在 → 继续下一个轴
 │   │        └─ 不在 → 返回false
 │   └─ 否 → 计算 t1, t2
 │           ↓
 │       交换 t1, t2 (确保 t1 ≤ t2)
 │           ↓
 │       更新 tMin = max(tMin, t1)
 │       更新 tMax = min(tMax, t2)
 │           ↓
 │       tMin > tMax？
 │        ├─ 是 → 返回false (无相交)
 │        └─ 否 → 继续下一个轴
 ↓
所有轴处理完成
 ↓
tMin < 0？
├─ 是 → tMax < 0？
│       ├─ 是 → 返回false (AABB在后方)
│       └─ 否 → t = tMax, 返回true
└─ 否 → t = tMin, 返回true
```

### 3D情况下的相交区间

```
     Z轴
      ↑
      │
      │    AABB
      │   ┌─────┐
      │   │     │
      │   │     │
      │   └─────┘
      │
      └─────────────→ Y轴
     /
    /
   /
  /
 ↙
X轴

对于3D AABB，需要同时满足：
- X轴相交区间：[tMin_x, tMax_x]
- Y轴相交区间：[tMin_y, tMax_y]  
- Z轴相交区间：[tMin_z, tMax_z]

最终相交区间：[max(tMin_x, tMin_y, tMin_z), min(tMax_x, tMax_y, tMax_z)]
```

## 特殊情况处理

### 1. 射线平行于某个轴平面
当`rayDirection[i] ≈ 0`时，射线平行于该轴的平面：
- 检查射线起点是否在AABB范围内
- 如果不在，则无相交

### 2. 射线起点在AABB内部
当`tMin < 0`但`tMax ≥ 0`时：
- 射线起点在AABB内部
- 返回`tMax`作为相交点

### 3. AABB在射线后方
当`tMax < 0`时：
- 整个AABB都在射线起点后方
- 返回false（无相交）

## 算法复杂度

- **时间复杂度**：O(1) - 常数时间，只需检查3个轴
- **空间复杂度**：O(1) - 只使用几个临时变量

## 优势与特点

1. **高效性**：无需复杂的几何计算，只涉及简单的算术运算
2. **鲁棒性**：能正确处理各种边界情况
3. **通用性**：适用于任意轴对齐的包围盒
4. **精确性**：使用浮点数运算，精度较高

## 在项目中的应用

在GTinyEngine中，此函数用于：

1. **鼠标拾取**：将屏幕点击转换为3D射线，检测与几何体的相交
2. **碰撞检测**：检测射线是否与场景中的对象相交
3. **对象选择**：在3D场景中选择被点击的对象

### 调用流程
```
HandleMouseClick() 
    ↓
ScreenToWorldRay() - 生成射线
    ↓  
RayIntersection() - 检测相交
    ↓
更新选择状态
```

## 数值示例

假设有以下参数：
- 射线起点：`origin = (0, 0, 0)`
- 射线方向：`direction = (1, 1, 1)` (已归一化)
- AABB：`min = (2, 2, 2)`, `max = (4, 4, 4)`

### 计算过程：

**X轴检测：**
```
invDir = 1/1 = 1
t1 = (2 - 0) * 1 = 2
t2 = (4 - 0) * 1 = 4
t1 ≤ t2 ✓，无需交换
tMin = max(0, 2) = 2
tMax = min(∞, 4) = 4
```

**Y轴检测：**
```
invDir = 1/1 = 1  
t1 = (2 - 0) * 1 = 2
t2 = (4 - 0) * 1 = 4
t1 ≤ t2 ✓，无需交换
tMin = max(2, 2) = 2
tMax = min(4, 4) = 4
```

**Z轴检测：**
```
invDir = 1/1 = 1
t1 = (2 - 0) * 1 = 2  
t2 = (4 - 0) * 1 = 4
t1 ≤ t2 ✓，无需交换
tMin = max(2, 2) = 2
tMax = min(4, 4) = 4
```

**最终结果：**
```
tMin = 2 ≥ 0，所以 t = tMin = 2
相交点 = origin + t * direction = (0,0,0) + 2*(1,1,1) = (2,2,2)
返回 true
```

## 代码示例

```cpp
// 使用示例
glm::vec3 rayOrigin = camera->GetEye();
glm::vec3 rayDirection = normalize(worldFar - worldNear);
te::AaBB aabb = geometry->GetWorldAABB();

float t;
if (RayIntersection(rayOrigin, rayDirection, aabb, t)) {
    // 射线与AABB相交，t为相交距离
    glm::vec3 intersectionPoint = rayOrigin + t * rayDirection;
    std::cout << "Intersection at distance: " << t << std::endl;
} else {
    // 射线与AABB不相交
    std::cout << "No intersection" << std::endl;
}
```

## 总结

Ray-AABB相交检测是3D图形学中的基础算法，通过Slab方法实现了高效的相交检测。该算法在鼠标拾取、碰撞检测等场景中发挥重要作用，是GTinyEngine交互系统的重要组成部分。
