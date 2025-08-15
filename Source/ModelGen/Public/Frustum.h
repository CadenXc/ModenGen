// Copyright (c) 2024. All rights reserved.

/**
 * @file Frustum.h
 * @brief 可配置的程序化截锥体生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Frustum.generated.h"

class UProceduralMeshComponent;
class FFrustumBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Frustum"))
class MODELGEN_API AFrustum : public AActor
{
    GENERATED_BODY()

public:
    AFrustum();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

    //~ Begin Geometry Parameters
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Collision")
    bool bUseAsyncCooking = true;

private:
    UPROPERTY(VisibleAnywhere, Category = "Frustum|Components")
    UProceduralMeshComponent* ProceduralMesh;

    void InitializeComponents();
    void ApplyMaterial();
    void SetupCollision();
    void RegenerateMesh();
    
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation")
    void GenerateFrustum(float InTopRadius, float InBottomRadius, float InHeight, int32 InSides);
    
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation")
    void GenerateFrustumWithDifferentSides(float InTopRadius, float InBottomRadius, float InHeight, 
                                          int32 InTopSides, int32 InBottomSides);
    
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation")
    void RegenerateMeshBlueprint();
    
    UFUNCTION(BlueprintCallable, Category = "Frustum|Materials")
    void SetMaterial(UMaterialInterface* NewMaterial);

public:
    // 参数验证和计算函数
    bool IsValid() const;
    float GetHalfHeight() const { return Height * 0.5f; }
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
    void PostEditChangeProperty(const FName& PropertyName);
    
    bool operator==(const AFrustum& Other) const;
    bool operator!=(const AFrustum& Other) const;
};