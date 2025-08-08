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
    /** 顶部半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Top Radius", 
        ToolTip = "截锥体顶部的半径"))
    float TopRadius = 50.0f;

    /** 底部半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Bottom Radius", 
        ToolTip = "截锥体底部的半径"))
    float BottomRadius = 100.0f;

    /** 高度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Height", 
        ToolTip = "截锥体的高度"))
    float Height = 200.0f;

    //~ Begin Tessellation Parameters
    /** 侧面数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", UIMin = "3", DisplayName = "Sides", 
        ToolTip = "截锥体侧面的数量"))
    int32 Sides = 16;

    /** 高度分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "1", UIMin = "1", DisplayName = "Height Segments", 
        ToolTip = "高度方向的分段数"))
    int32 HeightSegments = 4;

    //~ Begin Chamfer Parameters
    /** 倒角半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Chamfer", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Chamfer Radius", 
        ToolTip = "倒角的半径大小"))
    float ChamferRadius = 5.0f;

    /** 倒角分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Chamfer", 
        meta = (ClampMin = "1", UIMin = "1", DisplayName = "Chamfer Sections", 
        ToolTip = "倒角的分段数，影响倒角的平滑度"))
    int32 ChamferSections = 4;



    //~ Begin Bending Parameters
    /** 弯曲量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (UIMin = "-1.0", UIMax = "1.0", DisplayName = "Bend Amount", 
        ToolTip = "截锥体的弯曲量"))
    float BendAmount = 0.0f;

    /** 最小弯曲半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Min Bend Radius", 
        ToolTip = "最小弯曲半径"))
    float MinBendRadius = 1.0f;

    //~ Begin Angular Parameters
    /** 弧角 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Angular", 
        meta = (UIMin = "0.0", UIMax = "360.0", DisplayName = "Arc Angle", 
        ToolTip = "截锥体的弧角，360度为完整圆形"))
    float ArcAngle = 360.0f;

    //~ Begin Caps Parameters
    /** 端面厚度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Caps", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Cap Thickness", 
        ToolTip = "端面的厚度"))
    float CapThickness = 10.0f;
    
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
    
    /** 相等比较操作符 */
    bool operator==(const FFrustumParameters& Other) const;
    
    /** 不等比较操作符 */
    bool operator!=(const FFrustumParameters& Other) const;
};
