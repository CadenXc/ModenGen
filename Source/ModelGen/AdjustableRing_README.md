# AdjustableRing - 可调节环形模型

## 概述

`AdjustableRing` 是一个基于UE4的ProceduralMeshComponent的可调节参数环形模型类。它继承自Actor类，可以生成各种形状的环形几何体，支持多种参数调节。

## 主要特性

### 可调节参数

1. **内径 (InnerRadius)**
   - 环形内圆的半径
   - 最小值：0.01
   - 默认值：50.0

2. **外径 (OuterRadius)**
   - 环形外圆的半径
   - 最小值：0.01
   - 必须大于内径值
   - 默认值：100.0

3. **高度 (Height)**
   - 环形模型的高度
   - 最小值：0.01
   - 默认值：200.0

4. **内多边形数量 (InnerSides)**
   - 内圆的多边形边数
   - 最小值：3
   - 默认值：8

5. **外多边形数量 (OuterSides)**
   - 外圆的多边形边数
   - 最小值：3
   - 必须大于等于内多边形数量
   - 默认值：16

6. **倒角半径 (ChamferRadius)**
   - 边缘倒角的半径
   - 最小值：0.0
   - 默认值：5.0

7. **倒角阶数 (ChamferSteps)**
   - 倒角的细分阶数
   - 范围：1-10
   - 默认值：2

8. **扇形角度 (SectorAngle)**
   - 环形扇形的角度（度数）
   - 范围：0.0-360.0
   - 默认值：360.0（完整圆环）

## 约束条件

根据您的要求，模型实现了以下约束：

1. **内径值最大必须低于外径值**
   - 通过参数验证确保 `OuterRadius > InnerRadius`

2. **内多边形数量不能多于外多边形数量**
   - 通过参数验证确保 `InnerSides <= OuterSides`

3. **倒角阶数默认保持2**
   - 默认值设置为2，但可以在1-10范围内调节

## 使用方法

### 在蓝图中使用

1. 在内容浏览器中右键，选择"蓝图类"
2. 搜索并选择 `AdjustableRing`
3. 在蓝图编辑器中，您可以在"AdjustableRing Parameters"类别下找到所有可调节参数
4. 修改参数后，模型会自动重新生成

### 在C++中使用

```cpp
// 创建AdjustableRing实例
AAdjustableRing* Ring = GetWorld()->SpawnActor<AAdjustableRing>();

// 设置参数
Ring->Parameters.InnerRadius = 30.0f;
Ring->Parameters.OuterRadius = 80.0f;
Ring->Parameters.Height = 150.0f;
Ring->Parameters.InnerSides = 6;
Ring->Parameters.OuterSides = 12;
Ring->Parameters.ChamferRadius = 3.0f;
Ring->Parameters.ChamferSteps = 2;
Ring->Parameters.SectorAngle = 270.0f;

// 重新生成网格
Ring->RegenerateMesh();
```

### 通过函数调用生成

```cpp
// 使用GenerateAdjustableRing函数
Ring->GenerateAdjustableRing(
    30.0f,  // InnerRadius
    80.0f,  // OuterRadius
    150.0f, // Height
    6,      // InnerSides
    12,     // OuterSides
    3.0f,   // ChamferRadius
    2,      // ChamferSteps
    270.0f  // SectorAngle
);
```

## 材质设置

- 可以通过 `Material` 属性设置自定义材质
- 如果不设置，将使用默认的 `M_Basic_Wall` 材质

## 碰撞设置

- `bGenerateCollision`: 是否生成碰撞体
- `bUseAsyncCooking`: 是否使用异步碰撞体生成

## 示例应用场景

1. **建筑构件**: 生成各种形状的管道、环形装饰
2. **机械零件**: 制作齿轮、轴承等机械组件
3. **游戏道具**: 创建戒指、手环等游戏物品
4. **建筑装饰**: 制作门框、窗框等装饰元素

## 技术实现

- 使用 `ProceduralMeshComponent` 进行几何生成
- 支持实时参数调节和网格重新生成
- 实现了完整的参数验证和错误处理
- 支持编辑器中实时预览和调节

## 注意事项

1. 参数修改后会自动重新生成网格
2. 确保参数值在有效范围内，否则会生成错误日志
3. 对于复杂的几何体，建议适当调整多边形数量以平衡性能和视觉效果
4. 倒角半径不应超过内外半径差值的一半，以确保几何体的有效性 