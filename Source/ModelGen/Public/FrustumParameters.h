// Copyright (c) 2024. All rights reserved.

/**
 * @file FrustumParameters.h
 * @brief 截锥体生成参数
 * 
 * 该文件定义了截锥体生成器的所有参数。
 * 使用USTRUCT以便在蓝图中使用和编辑。
 */

#pragma once

#include "CoreMinimal.h"
#include "FrustumParameters.generated.h"

/**
 * 截锥体生成参数
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FFrustumParameters
{
    GENERATED_BODY()

public:
    //~ Begin Geometry Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Top Radius", 
        ToolTip = "截锥体顶部的半径"))
    float TopRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Bottom Radius", 
        ToolTip = "截锥体底部的半径"))
    float BottomRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Height", 
        ToolTip = "截锥体的高度"))
    float Height = 200.0f;

    //~ Begin Tessellation Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", UIMin = "3", DisplayName = "Top Sides", 
        ToolTip = "截锥体顶部的边数"))
    int32 TopSides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", UIMin = "3", DisplayName = "Bottom Sides", 
        ToolTip = "截锥体底部的边数"))
    int32 BottomSides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "1", UIMin = "1", DisplayName = "Height Segments", 
        ToolTip = "高度方向的分段数"))
    int32 HeightSegments = 4;

    //~ Begin Bevel Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Radius", 
        ToolTip = "倒角的半径大小"))
    float BevelRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "1", UIMin = "1", DisplayName = "Bevel Sections", 
        ToolTip = "倒角的分段数，影响倒角的平滑度"))
    int32 BevelSections = 4;

    //~ Begin Bending Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (UIMin = "-1.0", UIMax = "1.0", DisplayName = "Bend Amount", 
        ToolTip = "截锥体的弯曲量"))
    float BendAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Min Bend Radius", 
        ToolTip = "最小弯曲半径"))
    float MinBendRadius = 1.0f;

    //~ Begin Angular Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Angular", 
        meta = (UIMin = "0.0", UIMax = "360.0", DisplayName = "Arc Angle", 
        ToolTip = "截锥体的弧角，360度为完整圆形"))
    float ArcAngle = 360.0f;
    
    /** 反转法线 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Debug", 
        meta = (DisplayName = "Flip Normals", ToolTip = "反转所有法线方向"))
    bool bFlipNormals = false;
    
    /** 禁用防抖（实时更新） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Debug", 
        meta = (DisplayName = "Disable Debounce", ToolTip = "禁用防抖功能，实现实时更新"))
    bool bDisableDebounce = false;

public:
    /** 验证参数的有效性 */
    bool IsValid() const;
    
    /** 获取截锥体的半高 */
    float GetHalfHeight() const { return Height * 0.5f; }
    
    /** 计算预估的顶点数量 */
    int32 CalculateVertexCountEstimate() const;
    
    /** 计算预估的三角形数量 */
    int32 CalculateTriangleCountEstimate() const;
    
    /** 属性变更通知 */
    void PostEditChangeProperty(const FName& PropertyName);
    
    bool operator==(const FFrustumParameters& Other) const;
    bool operator!=(const FFrustumParameters& Other) const;
};
