// Copyright (c) 2024. All rights reserved.

/**
 * @file Sphere.h
 * @brief 可配置的程序化球体生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "Sphere.generated.h"

class FSphereBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Sphere"))
class MODELGEN_API ASphere : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    ASphere();

    //~ Begin Geometry Parameters
    /** 边数（球体的分段数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere|Geometry", 
        meta = (ClampMin = "4", ClampMax = "64", UIMin = "4", UIMax = "64", DisplayName = "边数"))
    int32 Sides = 16;

    UFUNCTION(BlueprintCallable, Category = "Sphere|Parameters")
    void SetSides(int32 NewSides);

    /** 横截断（0-1，表示从顶部到底部的截断比例） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere|Geometry", 
        meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "横截断"))
    float HorizontalCut = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Sphere|Parameters")
    void SetHorizontalCut(float NewHorizontalCut);

    /** 竖截断（0-360，表示球体的角度范围） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere|Geometry", 
        meta = (ClampMin = "0.0", ClampMax = "360.0", UIMin = "0.0", UIMax = "360.0", DisplayName = "竖截断"))
    float VerticalCut = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Sphere|Parameters")
    void SetVerticalCut(float NewVerticalCut);

    /** 半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere|Geometry", 
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "Radius"))
    float Radius = 50.0f;

    UFUNCTION(BlueprintCallable, Category = "Sphere|Parameters")
    void SetRadius(float NewRadius);

    virtual void GenerateMesh() override;

private:
    bool TryGenerateMeshInternal();

public:
    virtual bool IsValid() const override;
    
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
};

