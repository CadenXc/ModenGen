// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrism.h
 * @brief 可配置的空心棱柱生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "HollowPrism.generated.h"

class UProceduralMeshComponent;
class FHollowPrismBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Hollow Prism"))
class MODELGEN_API AHollowPrism : public AActor
{
    GENERATED_BODY()

public:
    AHollowPrism();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

    //~ Begin Geometry Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Inner Radius"))
    float InnerRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Outer Radius"))
    float OuterRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Height"))
    float Height = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", DisplayName = "Sides"))
    int32 Sides = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", DisplayName = "Outer Sides"))
    int32 OuterSides = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", DisplayName = "Inner Sides"))
    int32 InnerSides = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "360.0", UIMax = "360.0", DisplayName = "Arc Angle"))
    float ArcAngle = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Bevel", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Radius"))
    float BevelRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Bevel", 
        meta = (ClampMin = "1", UIMin = "1", ClampMax = "20", UIMax = "10", DisplayName = "Bevel Sections"))
    int32 BevelSegments = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Options", 
        meta = (DisplayName = "Use Triangle Method"))
    bool bUseTriangleMethod = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Debug", 
        meta = (DisplayName = "Flip Normals"))
    bool bFlipNormals = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Debug", 
        meta = (DisplayName = "Disable Debounce"))
    bool bDisableDebounce = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Collision")
    bool bUseAsyncCooking = true;

private:
    UPROPERTY(VisibleAnywhere, Category = "HollowPrism|Components")
    UProceduralMeshComponent* ProceduralMesh;

    void InitializeComponents();
    void ApplyMaterial();
    void SetupCollision();
    void RegenerateMesh();
    
    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Generation")
    void RegenerateMeshBlueprint();
    
    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Materials")
    void SetMaterial(UMaterialInterface* NewMaterial);

public:
    bool IsValid() const;
    float GetWallThickness() const;
    bool IsFullCircle() const;
    float GetHalfHeight() const { return Height * 0.5f; }
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
    bool operator==(const AHollowPrism& Other) const;
    bool operator!=(const AHollowPrism& Other) const;
};