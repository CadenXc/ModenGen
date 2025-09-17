// Copyright (c) 2024. All rights reserved.

/**
 * @file Frustum.h
 * @brief 可配置的程序化截锥体生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "Frustum.generated.h"

class FFrustumBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Frustum"))
class MODELGEN_API AFrustum : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    AFrustum();

    //~ Begin Geometry Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", DisplayName = "Top Radius"))
    float TopRadius = 50.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetTopRadius(float NewTopRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", DisplayName = "Bottom Radius"))
    float BottomRadius = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBottomRadius(float NewBottomRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", DisplayName = "Height"))
    float Height = 200.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetHeight(float NewHeight);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", ClampMax = "16", DisplayName = "Top Sides"))
    int32 TopSides = 8;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetTopSides(int32 NewTopSides);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", ClampMax = "16", DisplayName = "Bottom Sides"))
    int32 BottomSides = 12;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBottomSides(int32 NewBottomSides);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "1", DisplayName = "Height Segments"))
    int32 HeightSegments = 4;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetHeightSegments(int32 NewHeightSegments);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "0.0", DisplayName = "Bevel Radius"))
    float BevelRadius = 5.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBevelRadius(float NewBevelRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (ClampMin = "-1.0", ClampMax = "1.0", DisplayName = "Bend Amount"))
    float BendAmount = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBendAmount(float NewBendAmount);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (ClampMin = "0.0", DisplayName = "Min Bend Radius"))
    float MinBendRadius = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetMinBendRadius(float NewMinBendRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Angular", 
        meta = (ClampMin = "0.0", ClampMax = "360.0", DisplayName = "Arc Angle"))
    float ArcAngle = 360.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetArcAngle(float NewArcAngle);

    virtual void GenerateMesh() override;

public:
    virtual bool IsValid() const override;
    
    float GetHalfHeight() const { return Height * 0.5f; }
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
};