// Copyright (c) 2024. All rights reserved.

/**
 * @file FrustumParameters.h
 * @brief 截锥体生成参数
 */

#pragma once

#include "CoreMinimal.h"
#include "FrustumParameters.generated.h"

USTRUCT(BlueprintType)
struct MODELGEN_API FFrustumParameters
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Top Radius"))
    float TopRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Bottom Radius"))
    float BottomRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Height"))
    float Height = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", UIMin = "3", DisplayName = "Top Sides"))
    int32 TopSides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", UIMin = "3", DisplayName = "Bottom Sides"))
    int32 BottomSides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "1", UIMin = "1", DisplayName = "Height Segments"))
    int32 HeightSegments = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Radius"))
    float BevelRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "1", UIMin = "1", DisplayName = "Bevel Sections"))
    int32 BevelSegments = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (UIMin = "-1.0", UIMax = "1.0", DisplayName = "Bend Amount"))
    float BendAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Min Bend Radius"))
    float MinBendRadius = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Angular", 
        meta = (UIMin = "0.0", UIMax = "360.0", DisplayName = "Arc Angle"))
    float ArcAngle = 360.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Collision", 
        meta = (DisplayName = "Generate Collision", ToolTip = "是否生成碰撞体"))
    bool bGenerateCollision = true;
    


public:
    bool IsValid() const;
    float GetHalfHeight() const { return Height * 0.5f; }
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
    void PostEditChangeProperty(const FName& PropertyName);
    
    bool operator==(const FFrustumParameters& Other) const;
    bool operator!=(const FFrustumParameters& Other) const;
};
