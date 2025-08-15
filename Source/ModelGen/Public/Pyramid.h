// Copyright (c) 2024. All rights reserved.

/**
 * @file Pyramid.h
 * @brief 可配置的程序化金字塔生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Pyramid.generated.h"

class UProceduralMeshComponent;
class FPyramidBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Pyramid"))
class MODELGEN_API APyramid : public AActor
{
    GENERATED_BODY()

public:
    APyramid();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    //~ Begin Geometry Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Base Radius"))
    float BaseRadius = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Height"))
    float Height = 200.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "3", ClampMax = "100", UIMin = "3", UIMax = "100",
        DisplayName = "Sides"))
    int32 Sides = 4;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Radius"))
    float BevelRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Collision")
    bool bUseAsyncCooking = true;

private:
    UPROPERTY(VisibleAnywhere, Category = "Pyramid|Components")
    UProceduralMeshComponent* ProceduralMesh;

    void InitializeComponents();
    void ApplyMaterial();
    void SetupCollision();
    void RegenerateMesh();
    
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Generation")
    void GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides);

public:
    bool IsValid() const;
    float GetBevelTopRadius() const;
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
};