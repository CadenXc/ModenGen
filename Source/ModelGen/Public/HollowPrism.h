// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrism.h
 * @brief 可配置的空心棱柱生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "HollowPrism.generated.h"

class UProceduralMeshComponent;
class FHollowPrismBuilder;

UCLASS(BlueprintType, meta=(DisplayName = "Hollow Prism"))
class MODELGEN_API AHollowPrism : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    AHollowPrism();

    //~ Begin Geometry Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", DisplayName = "Inner Radius"))
    float InnerRadius = 50.0f;

    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Parameters")
    void SetInnerRadius(float NewInnerRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", DisplayName = "Outer Radius"))
    float OuterRadius = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Parameters")
    void SetOuterRadius(float NewOuterRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", DisplayName = "Height"))
    float Height = 200.0f;

    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Parameters")
    void SetHeight(float NewHeight);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", ClampMax = "100", DisplayName = "Outer Sides"))
    int32 OuterSides = 4;

    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Parameters")
    void SetOuterSides(int32 NewOuterSides);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", ClampMax = "100", DisplayName = "Inner Sides"))
    int32 InnerSides = 3;

    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Parameters")
    void SetInnerSides(int32 NewInnerSides);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.0", ClampMax = "360.0", DisplayName = "Arc Angle"))
    float ArcAngle = 360.0f;

    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Parameters")
    void SetArcAngle(float NewArcAngle);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Bevel", 
        meta = (ClampMin = "0.0", DisplayName = "Bevel Radius"))
    float BevelRadius = 5.0f;

    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Parameters")
    void SetBevelRadius(float NewBevelRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Bevel", 
        meta = (ClampMin = "1", ClampMax = "20", DisplayName = "Bevel Sections"))
    int32 BevelSegments = 4;

    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Parameters")
    void SetBevelSegments(int32 NewBevelSegments);

protected:
    virtual void GenerateMesh() override;

private:
    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Generation")
    void RegenerateMeshBlueprint();
    
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