# PolygonTorus 光滑功能实现

## 概述
为PolygonTorus模型添加了横切面光滑和竖面光滑两个参数，可以根据需要控制模型的光滑程度。

## 新增参数

### bSmoothCrossSection (横切面光滑)
- **类型**: bool
- **默认值**: true
- **功能**: 控制横切面（截面）的光滑程度
- **效果**: 
  - `true`: 横切面呈现光滑效果，顶点法线基于相邻面的平均
  - `false`: 横切面保持硬边效果，顶点法线基于截面中心

### bSmoothVerticalSection (竖面光滑)
- **类型**: bool
- **默认值**: true
- **功能**: 控制竖面（沿圆环方向）的光滑程度
- **效果**:
  - `true`: 竖面呈现光滑效果，顶点法线基于相邻面的平均
  - `false`: 竖面保持硬边效果，顶点法线基于截面方向

## 使用方法

### 在蓝图中使用
1. 将PolygonTorus拖入场景
2. 在Details面板中找到"Torus Parameters"分类
3. 调整以下参数：
   - Major Radius: 圆环主半径
   - Minor Radius: 截面半径
   - Major Segments: 圆环分段数
   - Minor Segments: 截面分段数
   - Torus Angle: 圆环角度
   - **Smooth Cross Section**: 横切面光滑开关
   - **Smooth Vertical Section**: 竖面光滑开关

### 在代码中使用
```cpp
// 创建圆环并设置光滑参数
APolygonTorus* Torus = GetWorld()->SpawnActor<APolygonTorus>();
Torus->bSmoothCrossSection = true;   // 启用横切面光滑
Torus->bSmoothVerticalSection = false; // 禁用竖面光滑
Torus->GeneratePolygonTorus(100.0f, 25.0f, 8, 4, 360.0f, true, false);
```

## 技术实现

### 法线计算算法
1. **生成基础网格**: 按照原有的多边形截面方法生成顶点和三角形
2. **计算面法线**: 对每个三角形计算面法线
3. **累加顶点法线**: 将相邻面的法线累加到共享顶点
4. **标准化**: 对所有顶点法线进行标准化
5. **条件调整**: 根据光滑参数调整特定顶点的法线

### 光滑控制逻辑
- **横切面不平滑**: 使用从截面中心指向顶点的法线
- **竖面不平滑**: 使用沿圆环方向的截面法线
- **完全光滑**: 使用基于相邻面平均的法线

## 效果对比

### 完全光滑 (bSmoothCrossSection=true, bSmoothVerticalSection=true)
- 整个模型呈现光滑的圆环表面
- 适合需要平滑渲染的场合

### 横切面硬边 (bSmoothCrossSection=false, bSmoothVerticalSection=true)
- 横切面保持多边形边界的硬边效果
- 竖面仍然光滑
- 适合需要显示截面边界的场合

### 竖面硬边 (bSmoothCrossSection=true, bSmoothVerticalSection=false)
- 竖面保持分段边界的硬边效果
- 横切面仍然光滑
- 适合需要显示圆环分段的场合

### 完全硬边 (bSmoothCrossSection=false, bSmoothVerticalSection=false)
- 所有面都保持硬边效果
- 适合需要显示完整网格结构的场合

## 注意事项
- 光滑参数的变化会实时更新网格的法线
- 建议在编辑器中调整参数来观察效果
- 对于高精度模型，可能需要增加分段数来获得更好的光滑效果 