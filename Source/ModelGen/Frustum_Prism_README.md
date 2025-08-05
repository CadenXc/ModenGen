# Frustum 棱柱类实现

## 概述

`AFrustum` 类实现了一个可以改变边数的棱柱生成器，参考了 `ChamferCube` 的设计模式。这个类可以生成具有可配置边数的棱柱几何体，支持从三角形到32边形的各种形状。

## 主要特性

### 1. 可配置参数
- **SideCount**: 边数 (3-32)
- **Radius**: 底部半径
- **Height**: 棱柱高度
- **TopRadiusRatio**: 顶部半径比例 (用于创建截锥)
- **HeightSegments**: 高度分段数 (1-10)
- **ChamferSize**: 倒角大小 (0.0+)
- **ChamferSections**: 倒角分段数 (1-8)
- **bEnableChamfer**: 是否启用倒角

### 2. 几何生成
- **侧面**: 自动生成所有侧面，支持高度分段
- **端面**: 自动生成顶部和底部端面
- **倒角**: 支持边缘和角落倒角，创建平滑的过渡
- **法线**: 正确的法线计算，确保光照效果
- **UV坐标**: 合理的UV映射

### 3. 材质和碰撞
- 支持自定义材质
- 可配置碰撞生成
- 异步碰撞烹饪支持

## 类结构

### AFrustum (主类)
- 继承自 `AActor`
- 包含 `UProceduralMeshComponent`
- 提供蓝图可调用的接口

### FPrismGeometry (几何数据结构)
- 存储顶点、三角形、法线、UV等数据
- 提供数据验证和清理功能

### FPrismBuilder (几何生成器)
- 负责实际的几何体生成
- 包含参数验证
- 实现顶点去重和优化

## 使用方法

### 1. 在蓝图中使用
```cpp
// 创建棱柱
AFrustum* Prism = GetWorld()->SpawnActor<AFrustum>();

// 设置参数
Prism->SideCount = 6;        // 六边形棱柱
Prism->Radius = 50.0f;       // 半径50
Prism->Height = 100.0f;      // 高度100
Prism->TopRadiusRatio = 0.5f; // 顶部半径为底部的一半
Prism->HeightSegments = 3;   // 3个高度分段

// 重新生成网格
Prism->RegenerateMesh();
```

### 2. 在C++中使用
```cpp
// 使用GeneratePrism函数
Prism->GeneratePrism(8, 50.0f, 25.0f, 100.0f, 2);
// 参数: 边数, 底部半径, 顶部半径, 高度, 分段数

// 使用GenerateChamferedPrism函数（带倒角）
Prism->GenerateChamferedPrism(8, 50.0f, 25.0f, 100.0f, 2, 5.0f, 3);
// 参数: 边数, 底部半径, 顶部半径, 高度, 分段数, 倒角大小, 倒角分段数
```

## 参数说明

### SideCount (边数)
- **范围**: 3-32
- **默认值**: 6
- **说明**: 控制棱柱的边数，3为三角形，4为正方形，6为六边形等

### Radius (半径)
- **范围**: 1.0-1000.0
- **默认值**: 50.0f
- **说明**: 棱柱底部的半径

### Height (高度)
- **范围**: 1.0-1000.0
- **默认值**: 100.0f
- **说明**: 棱柱的总高度

### TopRadiusRatio (顶部半径比例)
- **范围**: 0.0-1.0
- **默认值**: 1.0f
- **说明**: 顶部半径相对于底部半径的比例，1.0为圆柱，小于1.0为截锥

### HeightSegments (高度分段)
- **范围**: 1-10
- **默认值**: 1
- **说明**: 高度方向的分段数，影响侧面细节

### ChamferSize (倒角大小)
- **范围**: 0.0+
- **默认值**: 5.0f
- **说明**: 倒角的大小，必须小于半径

### ChamferSections (倒角分段)
- **范围**: 1-8
- **默认值**: 3
- **说明**: 倒角的分段数，影响倒角的平滑度

### bEnableChamfer (启用倒角)
- **类型**: bool
- **默认值**: true
- **说明**: 是否启用倒角功能

## 几何生成算法

### 1. 顶点计算
```cpp
FVector CalculateVertexPosition(int32 SideIndex, int32 HeightIndex, float Radius)
{
    const float AngleStep = 2.0f * PI / SideCount;
    const float CurrentAngle = SideIndex * AngleStep;
    const float HeightRatio = HeightIndex / HeightSegments;
    const float CurrentHeight = HeightRatio * Height;
    const float CurrentRadius = FMath::Lerp(BottomRadius, TopRadius, HeightRatio);
    
    return FVector(
        CurrentRadius * FMath::Cos(CurrentAngle),
        CurrentRadius * FMath::Sin(CurrentAngle),
        CurrentHeight - Height * 0.5f
    );
}
```

### 2. 侧面生成
- 为每个边生成四边形网格
- 支持高度分段，创建更平滑的侧面
- 正确的法线计算，指向外部

### 3. 端面生成
- 底部端面：中心顶点 + 边缘三角形扇形
- 顶部端面：类似底部，但法线方向相反
- 支持不同半径的顶部和底部

### 4. 倒角生成
- **边缘倒角**: 为每个边缘生成平滑的倒角过渡
- **角落倒角**: 为角落生成四分之一球体倒角
- **参数控制**: 通过ChamferSize和ChamferSections控制倒角效果

## 性能优化

### 1. 顶点去重
- 使用 `TMap<FVector, int32>` 避免重复顶点
- 减少内存使用和渲染开销

### 2. 参数验证
- 所有参数都有合理的范围限制
- 防止无效参数导致的错误

### 3. 异步碰撞
- 支持异步碰撞烹饪
- 提高大型网格的生成性能

## 示例用法

### 创建六边形棱柱
```cpp
AFrustum* HexPrism = GetWorld()->SpawnActor<AFrustum>();
HexPrism->SideCount = 6;
HexPrism->Radius = 50.0f;
HexPrism->Height = 100.0f;
HexPrism->RegenerateMesh();
```

### 创建截锥
```cpp
AFrustum* Frustum = GetWorld()->SpawnActor<AFrustum>();
Frustum->SideCount = 8;
Frustum->Radius = 60.0f;
Frustum->TopRadiusRatio = 0.3f;  // 顶部半径为底部的30%
Frustum->Height = 120.0f;
Frustum->HeightSegments = 5;      // 更多分段以获得平滑效果
Frustum->RegenerateMesh();
```

### 创建三角形棱柱
```cpp
AFrustum* TrianglePrism = GetWorld()->SpawnActor<AFrustum>();
TrianglePrism->GeneratePrism(3, 40.0f, 40.0f, 80.0f, 1);
```

### 创建倒角棱柱
```cpp
AFrustum* ChamferedPrism = GetWorld()->SpawnActor<AFrustum>();
ChamferedPrism->GenerateChamferedPrism(6, 50.0f, 50.0f, 100.0f, 2, 8.0f, 4);
// 六边形棱柱，倒角大小8，倒角分段4
```

### 创建倒角截锥
```cpp
AFrustum* ChamferedFrustum = GetWorld()->SpawnActor<AFrustum>();
ChamferedFrustum->GenerateChamferedPrism(8, 60.0f, 30.0f, 120.0f, 3, 6.0f, 3);
// 八边形截锥，倒角大小6，倒角分段3
```

## 注意事项

1. **边数限制**: 边数必须在3-32之间，过少的边数会产生尖锐的形状
2. **半径比例**: TopRadiusRatio为0时会创建没有顶部端面的棱柱
3. **倒角限制**: 倒角大小必须小于半径，否则会产生无效几何
4. **性能考虑**: 边数越多，顶点和三角形数量越多，影响性能
5. **倒角性能**: 启用倒角会增加顶点和三角形数量
6. **材质应用**: 确保材质支持程序化网格，否则可能显示不正确

## 扩展可能性

1. **纹理支持**: 可以扩展UV映射以支持更复杂的纹理
2. **变形支持**: 可以添加顶点变形功能
3. **LOD系统**: 可以添加细节层次系统
4. **动画支持**: 可以添加参数动画功能 