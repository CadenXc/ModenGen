// Copyright (c) 2024. All rights reserved.

/**
 * @file Pyramid.h
 * @brief 可配置的程序化金字塔生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "Pyramid.generated.h"

class FPyramidBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Pyramid"))
class MODELGEN_API APyramid : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    APyramid();

    //~ Begin Geometry Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.01", DisplayName = "Base Radius"))
    float BaseRadius = 100.0f;
    
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetBaseRadius(float NewBaseRadius);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.01", DisplayName = "Height"))
    float Height = 200.0f;
    
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetHeight(float NewHeight);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "3", ClampMax = "100", DisplayName = "Sides"))
    int32 Sides = 4;
    
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetSides(int32 NewSides);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.0", DisplayName = "Bevel Radius"))
    float BevelRadius = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetBevelRadius(float NewBevelRadius);

    virtual void GenerateMesh() override;

private:
    bool TryGenerateMeshInternal();

public:
    virtual bool IsValid() const override;
    
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Generation")
    void GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides);

    float GetBevelTopRadius() const;
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
};