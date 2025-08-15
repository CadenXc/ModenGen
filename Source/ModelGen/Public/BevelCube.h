// Copyright (c) 2024. All rights reserved.

/**
 * @file BevelCube.h
 * @brief 可配置的程序化圆角立方体生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "BevelCube.generated.h"

class UProceduralMeshComponent;
class FBevelCubeBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Bevel Cube"))
class MODELGEN_API ABevelCube : public AActor
{
    GENERATED_BODY()

public:
    ABevelCube();
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Size"))
    float Size = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Size"))
    float BevelRadius = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10", 
        DisplayName = "Bevel Sections"))
    int32 BevelSegments = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Collision")
    bool bUseAsyncCooking = true;

public:
    bool IsValid() const;
    float GetHalfSize() const { return Size * 0.5f; }
    float GetInnerOffset() const { return GetHalfSize() - BevelRadius; }
    int32 GetVertexCount() const;
    int32 GetTriangleCount() const;

private:
    UPROPERTY(VisibleAnywhere, Category = "BevelCube|Components")
    UProceduralMeshComponent* ProceduralMesh;

    void InitializeComponents();
    void ApplyMaterial();
    void SetupCollision();
    void RegenerateMesh();
    
    UFUNCTION(BlueprintCallable, Category = "BevelCube|Generation")
    void GenerateBeveledCube(float InSize, float InBevelSize, int32 InSections);
};