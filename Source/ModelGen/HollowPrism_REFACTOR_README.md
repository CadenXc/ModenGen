# HollowPrism 重新设计说明

## 概述

重新设计的HollowPrism类现在确保内外多边形的边数保持一致，并提供了两种连接方法：三角形方法和四边形方法。

## 主要改进

### 1. 统一的边数参数
- **之前**: 分别设置 `InnerSides` 和 `OuterSides`
- **现在**: 使用统一的 `Sides` 参数，确保内外多边形边数一致

### 2. 三角形方法支持
- 新增 `bUseTriangleMethod` 参数来控制连接方式
- 当启用时，使用中心点连接内外多边形，形成三角形网格
- 当禁用时，使用传统的四边形连接方式

### 3. 参数结构优化
```cpp
USTRUCT(BlueprintType)
struct FPrismParameters
{
    // 几何参数
    float InnerRadius = 50.0f;
    float OuterRadius = 100.0f;
    float Height = 200.0f;
    
    // 统一的边数参数
    int32 Sides = 8;
    
    // 角度参数
    float ArcAngle = 360.0f;
    
    // 三角形方法开关
    bool bUseTriangleMethod = true;
    
    // 倒角参数
    float ChamferRadius = 5.0f;
    bool bUseChamfer = false;
};
```

### 4. 倒角功能
- 新增 `bUseChamfer` 参数控制是否启用倒角
- 新增 `ChamferRadius` 参数控制倒角半径
- 新增 `ChamferSections` 参数控制倒角分段数（用于创建圆滑边缘）
- 倒角算法：在边缘创建圆滑的过渡，而不是简单的半径调整
- 自动验证倒角参数，确保不会导致内外环重叠

## 三角形方法详解

### 顶面三角形方法
1. 生成内外环顶点
2. 从外环到内环创建四边形连接，形成梯形网格
3. 保持内孔开放，不被遮挡

### 底面三角形方法
1. 生成内外环顶点
2. 从外环到内环创建四边形连接，形成梯形网格
3. 确保正确的法线方向（向下）

### 修复的问题
- **内孔遮挡问题**：移除了中心点连接，避免内环到中心的三角形遮挡内孔
- **法线方向问题**：修正了底面四边形的顶点顺序，确保法线指向正确方向
- **倒角UV映射问题**：修复了倒角模式下UV映射错误导致顶面不可见的问题
- **倒角法线问题**：修正了倒角顶面的四边形顶点顺序，确保法线指向正确方向

## 使用方法

### 蓝图使用
1. 将HollowPrism拖入场景
2. 在Details面板中调整参数：
   - `Sides`: 设置内外多边形的边数（默认8）
   - `bUseTriangleMethod`: 启用三角形方法（默认true）
   - `InnerRadius`/`OuterRadius`: 设置内外半径
   - `Height`: 设置高度
   - `ArcAngle`: 设置弧角（360度为完整圆环）
       - `bUseChamfer`: 启用倒角功能（默认false）
    - `ChamferRadius`: 设置倒角半径（默认5.0）
    - `ChamferSections`: 设置倒角分段数（默认3）

### C++使用
```cpp
AHollowPrism* Prism = GetWorld()->SpawnActor<AHollowPrism>();
Prism->Parameters.Sides = 8;
Prism->Parameters.bUseTriangleMethod = true;
Prism->Parameters.InnerRadius = 50.0f;
Prism->Parameters.OuterRadius = 100.0f;
Prism->Parameters.Height = 200.0f;
Prism->Parameters.bUseChamfer = true;
Prism->Parameters.ChamferRadius = 5.0f;
Prism->Parameters.ChamferSections = 3;
Prism->Regenerate();
```

## 视觉效果

### 三角形方法
- 内外多边形边数完全一致
- 从中心点辐射出三角形网格
- 更均匀的网格分布
- 适合需要中心连接的应用场景

### 四边形方法
- 传统的四边形连接方式
- 内外多边形边数一致
- 更简洁的网格结构
- 适合需要简单连接的应用场景

## 技术细节

### 网格生成流程
1. 参数验证和边界检查
2. 根据 `bUseChamfer` 选择生成方法：
   - 启用倒角：使用倒角算法生成几何
   - 禁用倒角：使用标准算法生成几何
3. 生成侧面几何（内外环）
4. 根据 `bUseTriangleMethod` 选择顶面/底面生成方法
5. 生成端盖（如果ArcAngle < 360度）
6. 创建最终网格

### 倒角算法
- **圆滑边缘**：在内外环边缘创建圆滑的过渡曲面
- **分段控制**：通过 `ChamferSections` 控制倒角的圆滑程度
- **法线混合**：在边缘处混合法线，创建自然的过渡
- **参数验证**：确保 `ChamferRadius <= (OuterRadius - InnerRadius) * 0.5`
- **应用范围**：顶部和底部的内外环边缘都应用倒角效果

### 性能优化
- 预分配顶点和三角形数组
- 使用统一的边数减少计算复杂度
- 支持异步网格烹饪

## 兼容性

- 保持了原有的API接口
- 向后兼容现有的蓝图设置
- 新增参数都有合理的默认值

## 注意事项

1. 确保 `OuterRadius > InnerRadius`
2. `Sides` 参数最小值为3
3. `ArcAngle` 范围是0-360度
4. 所有几何参数都有最小值限制
5. 倒角功能注意事项：
   - `ChamferRadius` 不能超过 `(OuterRadius - InnerRadius) * 0.5`
   - `ChamferSections` 控制倒角的圆滑程度，数值越大越圆滑
   - 倒角效果会应用到顶部和底部的内外环边缘
   - 倒角会创建额外的几何面，增加网格复杂度 