// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrismParameters.h
 * @brief 空心棱柱生成参数
 * 
 * 该文件定义了空心棱柱生成器的所有参数。
 * 使用USTRUCT以便在蓝图中使用和编辑。
 */

#pragma once

#include "CoreMinimal.h"
#include "HollowPrismParameters.generated.h"

/**
 * 空心棱柱生成参数
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FHollowPrismParameters
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Inner Radius", ToolTip = "空心棱柱的内半径"))
    float InnerRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Outer Radius", ToolTip = "空心棱柱的外半径"))
    float OuterRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Height", ToolTip = "空心棱柱的高度"))
    float Height = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", 
        DisplayName = "Sides", ToolTip = "空心棱柱的边数"))
    int32 Sides = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", 
        DisplayName = "Outer Sides", ToolTip = "空心棱柱的外边数"))
    int32 OuterSides = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", 
        DisplayName = "Inner Sides", ToolTip = "空心棱柱的内边数"))
    int32 InnerSides = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "360.0", UIMax = "360.0", 
        DisplayName = "Arc Angle", ToolTip = "空心棱柱的弧角（度）"))
    float ArcAngle = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Bevel", 
        meta = (ClampMin = "0.0", UIMin = "0.0", 
        DisplayName = "Bevel Radius", ToolTip = "倒角的半径"))
    float BevelRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Bevel", 
        meta = (ClampMin = "1", UIMin = "1", ClampMax = "20", UIMax = "10", 
        DisplayName = "Bevel Sections", ToolTip = "倒角的分段数"))
    int32 BevelSections = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Options", 
        meta = (DisplayName = "Use Triangle Method", ToolTip = "使用三角形方法生成顶底面"))
    bool bUseTriangleMethod = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Debug", 
        meta = (DisplayName = "Flip Normals", ToolTip = "反转所有法线方向"))
    bool bFlipNormals = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Debug", 
        meta = (DisplayName = "Disable Debounce", ToolTip = "禁用防抖功能，实现实时更新"))
    bool bDisableDebounce = false;

public:
    bool IsValid() const;
    
    float GetWallThickness() const;
    
    bool IsFullCircle() const;
    
    float GetHalfHeight() const { return Height * 0.5f; }
    
    int32 CalculateVertexCountEstimate() const;
    
    int32 CalculateTriangleCountEstimate() const;
    
    void PostEditChangeProperty(const FName& PropertyName);
    
    bool operator==(const FHollowPrismParameters& Other) const;
    bool operator!=(const FHollowPrismParameters& Other) const;
};
