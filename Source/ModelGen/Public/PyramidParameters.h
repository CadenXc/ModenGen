// Copyright (c) 2024. All rights reserved.

/**
 * @file PyramidParameters.h
 * @brief 金字塔生成参数
 * 
 * 该文件定义了金字塔生成器的所有参数。
 * 使用USTRUCT以便在蓝图中使用和编辑。
 */

#pragma once

#include "CoreMinimal.h"
#include "PyramidParameters.generated.h"

/**
 * 金字塔生成参数
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FPyramidParameters
{
    GENERATED_BODY()

public:
    /** 底面半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters",
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Base Radius",
        ToolTip = "金字塔底面的半径"))
    float BaseRadius = 100.0f;
    
    /** 金字塔高度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters",
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Height",
        ToolTip = "金字塔的高度"))
    float Height = 200.0f;
    
    /** 底面边数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters",
        meta = (ClampMin = "3", ClampMax = "100", UIMin = "3", UIMax = "100",
        DisplayName = "Sides", ToolTip = "金字塔底面的边数"))
    int32 Sides = 4;
    
    /** 底部倒角半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters",
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Radius",
        ToolTip = "底部倒角的半径，0表示无倒角"))
    float BevelRadius = 0.0f;

public:
    /** 验证参数的有效性 */
    bool IsValid() const;
    
    /** 获取倒角顶部半径 */
    float GetBevelTopRadius() const;
    
    /** 计算预估的顶点数量 */
    int32 CalculateVertexCountEstimate() const;
    
    /** 计算预估的三角形数量 */
    int32 CalculateTriangleCountEstimate() const;
};
