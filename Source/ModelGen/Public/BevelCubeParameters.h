// Copyright (c) 2024. All rights reserved.

/**
 * @file BevelCubeParameters.h
 * @brief 圆角立方体生成参数
 * 
 * 该文件定义了圆角立方体生成器的所有参数。
 * 使用USTRUCT以便在蓝图中使用和编辑。
 */

#pragma once

#include "CoreMinimal.h"
#include "BevelCubeParameters.generated.h"

/**
 * 圆角立方体生成参数
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FBevelCubeParameters
{
    GENERATED_BODY()

public:
    /** 立方体的基础大小 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Size", 
        ToolTip = "立方体的基础大小"))
    float Size = 100.0f;
    
    /** 倒角的大小 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Size", 
        ToolTip = "倒角的大小，不能超过立方体半边长"))
    float BevelSize = 10.0f;
    
    /** 倒角的分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10", 
        DisplayName = "Bevel Sections", ToolTip = "倒角的分段数，影响倒角的平滑度"))
    int32 BevelSections = 3;

public:
    /** 验证参数的有效性 */
    bool IsValid() const;
    
    /** 获取立方体的半边长 */
    float GetHalfSize() const { return Size * 0.5f; }
    
    /** 获取内部偏移量（从中心到倒角起始点的距离） */
    float GetInnerOffset() const { return GetHalfSize() - BevelSize; }
    
    /** 计算预估的顶点数量 */
    int32 CalculateVertexCountEstimate() const;
    
    /** 计算预估的三角形数量 */
    int32 CalculateTriangleCountEstimate() const;
};
