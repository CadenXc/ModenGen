# PolygonTorus 光滑功能测试指南

## 测试步骤

### 1. 基本功能测试
1. 在UE编辑器中创建一个新的关卡
2. 将PolygonTorus拖入场景
3. 在Details面板中找到"Torus Parameters"分类
4. 调整以下参数进行测试：
   - Major Radius: 100
   - Minor Radius: 25
   - Major Segments: 8
   - Minor Segments: 4
   - Torus Angle: 360

### 2. 光滑参数测试

#### 测试1: 完全硬边
- Smooth Cross Section: **false**
- Smooth Vertical Section: **false**
- **预期效果**: 模型显示清晰的硬边，每个面都有明显的边界

#### 测试2: 完全光滑
- Smooth Cross Section: **true**
- Smooth Vertical Section: **true**
- **预期效果**: 模型呈现光滑的圆环表面，没有明显的硬边

#### 测试3: 横切面硬边
- Smooth Cross Section: **false**
- Smooth Vertical Section: **true**
- **预期效果**: 横切面（截面）保持多边形边界的硬边，竖面（沿圆环方向）光滑

#### 测试4: 竖面硬边
- Smooth Cross Section: **true**
- Smooth Vertical Section: **false**
- **预期效果**: 竖面保持分段边界的硬边，横切面光滑

### 3. 视觉验证

#### 硬边效果检查
- 当两个光滑参数都为false时，应该看到：
  - 每个截面都有清晰的边界线
  - 沿圆环方向也有清晰的分段边界
  - 没有平滑的光影过渡

#### 光滑效果检查
- 当两个光滑参数都为true时，应该看到：
  - 整个模型呈现光滑的圆环表面
  - 没有明显的硬边或边界线
  - 平滑的光影过渡

#### 混合效果检查
- 当只有一个参数为true时，应该看到：
  - 对应方向的光滑效果
  - 另一个方向保持硬边效果

### 4. 性能测试
- 增加Major Segments和Minor Segments的数量
- 验证光滑功能在高精度模型上的表现
- 检查帧率是否受到影响

### 5. 边界情况测试
- 设置Torus Angle为180度（半圆）
- 验证端面的法线是否正确
- 测试极端参数值（如Major Radius = Minor Radius）

## 常见问题排查

### 问题1: 模型显示异常深色
**可能原因**: 法线计算错误
**解决方案**: 检查GeneratePolygonVertices函数生成的法线是否正确

### 问题2: 光滑效果不明显
**可能原因**: 分段数太少
**解决方案**: 增加Major Segments和Minor Segments的数量

### 问题3: 硬边效果不明显
**可能原因**: 材质设置问题
**解决方案**: 检查材质的光照设置，确保硬边能够正确显示

## 预期结果

正确的实现应该能够：
1. 在完全硬边模式下显示清晰的网格结构
2. 在完全光滑模式下显示平滑的圆环表面
3. 在混合模式下正确显示部分光滑、部分硬边的效果
4. 实时响应参数变化，无需重新编译 