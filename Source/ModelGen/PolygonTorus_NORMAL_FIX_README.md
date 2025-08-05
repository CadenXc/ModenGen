# PolygonTorus 纹理变暗问题解决方案

## 问题描述

在使用亮的纹理时，PolygonTorus模型显示为暗色，这通常是由于法线计算不正确导致的。

## 问题原因分析

### 1. 法线方向错误
- 法线指向内部而不是外部
- 导致光照计算时，表面被认为是背向光源

### 2. 法线长度异常
- 法线没有正确标准化
- 导致光照强度计算错误

### 3. 平滑法线计算错误
- 面法线累加时方向不一致
- 导致最终法线方向错误

## 解决方案

### 1. 修复基础法线计算

在 `GeneratePolygonVertices` 函数中：
```cpp
// 计算法线（从中心指向顶点，确保指向外部）
FVector Normal = (VertexPos - Center).GetSafeNormal();

// 确保法线指向外部（对于圆环，法线应该指向远离中心的方向）
FVector ExpectedDirection = (VertexPos - Center);
if (ExpectedDirection.Dot(Normal) < 0)
{
    Normal = -Normal;
}
```

### 2. 修复平滑法线计算

在 `CalculateSmoothNormals` 函数中：
```cpp
// 确保面法线指向外部
FVector FaceCenter = (Vertices[V0] + Vertices[V1] + Vertices[V2]) / 3.0f;
FVector ToCenter = FVector(FaceCenter.X, FaceCenter.Y, 0.0f).GetSafeNormal();

// 如果面法线与指向中心的方向夹角小于90度，则翻转法线
if (FaceNormal.Dot(ToCenter) > 0)
{
    FaceNormal = -FaceNormal;
}
```

### 3. 添加法线验证

新增 `ValidateNormalDirections` 函数来检测法线方向：
```cpp
void ValidateNormalDirections(
    const TArray<FVector>& Vertices,
    const TArray<FVector>& Normals,
    const TArray<int32>& SectionStartIndices,
    float MajorRad,
    int32 MajorSegs,
    int32 MinorSegs
);
```

## 调试步骤

### 1. 检查日志输出
在UE编辑器的Output Log中查看：
- 法线统计信息
- 错误法线警告
- 平均法线方向

### 2. 使用调试材质
创建一个简单的调试材质：
- 使用 `World Normal` 节点
- 连接到 `Base Color`
- 观察法线方向是否正确

### 3. 验证法线方向
正确的法线应该：
- 指向远离圆环中心的方向
- 与从中心到顶点的方向夹角大于90度
- 所有法线都指向外部

## 测试方法

### 1. 基本测试
1. 创建一个新的关卡
2. 拖入PolygonTorus
3. 设置参数：
   - Major Radius: 100
   - Minor Radius: 25
   - Major Segments: 8
   - Minor Segments: 4
   - Torus Angle: 360

### 2. 材质测试
1. 创建一个简单的亮色材质
2. 应用到PolygonTorus
3. 观察是否仍然显示为暗色

### 3. 光照测试
1. 添加多个光源
2. 调整光源位置
3. 观察光照效果是否正常

## 常见问题排查

### 问题1: 仍然显示暗色
**可能原因**: 法线方向仍然错误
**解决方案**: 
1. 检查日志中的法线验证信息
2. 确认所有法线都指向外部
3. 尝试翻转所有法线方向

### 问题2: 部分区域显示正常，部分暗色
**可能原因**: 平滑法线计算不一致
**解决方案**:
1. 检查 `bSmoothCrossSection` 和 `bSmoothVerticalSection` 设置
2. 尝试禁用平滑功能
3. 检查特定区域的法线方向

### 问题3: 在特定角度下显示暗色
**可能原因**: 视角相关的法线问题
**解决方案**:
1. 检查法线标准化是否正确
2. 验证法线长度是否为1
3. 检查法线与视角的关系

## 性能优化

### 1. 减少验证开销
```cpp
#if WITH_EDITOR
ValidateNormalDirections(Vertices, Normals, SectionStartIndices, MajorRad, MajorSegs, MinorSegs);
#endif
```

### 2. 优化法线计算
- 只在需要时重新计算法线
- 缓存中间计算结果
- 使用向量化操作

## 预期结果

修复后应该看到：
1. 亮的纹理显示为正确的亮色
2. 光照效果正常
3. 没有异常的暗色区域
4. 法线验证通过

## 总结

通过修复法线计算逻辑，确保所有法线都正确指向外部，可以解决纹理变暗的问题。关键是要确保：
- 基础法线方向正确
- 平滑法线计算一致
- 法线长度标准化
- 法线方向验证 