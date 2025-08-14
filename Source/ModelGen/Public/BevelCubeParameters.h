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

USTRUCT(BlueprintType)
struct FBevelCubeParameters
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Size", 
        ToolTip = "立方体的基础大小"))
    float Size = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Size", 
        ToolTip = "倒角的大小，不能超过立方体半边长"))
    float BevelRadius = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10", 
        DisplayName = "Bevel Sections", ToolTip = "倒角的分段数，影响倒角的平滑度"))
    int32 BevelSegments = 3;

public:
    bool IsValid() const;
    float GetHalfSize() const { return Size * 0.5f; }
    float GetInnerOffset() const { return GetHalfSize() - BevelRadius; }
    int32 GetVertexCount() const;
    int32 GetTriangleCount() const;
};
