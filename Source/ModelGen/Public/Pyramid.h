// Copyright (c) 2024. All rights reserved.

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "Base Radius"))
    float BaseRadius = 70.0f;
    
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetBaseRadius(float NewBaseRadius);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "Height"))
    float Height = 100.0f;
    
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetHeight(float NewHeight);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "3", ClampMax = "25", UIMin = "3", UIMax = "25", DisplayName = "Sides"))
    int32 Sides = 4;
    
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetSides(int32 NewSides);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.0", ClampMax = "500", UIMin = "0.0", UIMax = "500", DisplayName = "Bevel Radius"))
    float BevelRadius = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetBevelRadius(float NewBevelRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Smoothing", 
        meta = (DisplayName = "Smooth Sides", ToolTip = "Smooth sides (soft edges)"))
    bool bSmoothSides = false;

    UFUNCTION(BlueprintCallable, Category = "Pyramid|Parameters")
    void SetSmoothSides(bool bNewSmoothSides);

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