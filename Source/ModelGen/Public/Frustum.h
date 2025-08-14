// Copyright (c) 2024. All rights reserved.

/**
 * @file Frustum.h
 * @brief 可配置的程序化截锥体生成器
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "FrustumParameters.h"

// Generated header must be the last include
#include "Frustum.generated.h"

// Forward declarations
class UProceduralMeshComponent;
class FFrustumBuilder;
struct FModelGenMeshData;

/**
 * 截锥体Actor
 */
UCLASS(BlueprintType, meta=(DisplayName = "Frustum"))
class MODELGEN_API AFrustum : public AActor
{
    GENERATED_BODY()

public:
    AFrustum();

    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Parameters")
    FFrustumParameters Parameters;

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
    void GenerateFrustum(float TopRadius, float BottomRadius, float Height, int32 Sides);
    
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation")
    void GenerateFrustumWithDifferentSides(float TopRadius, float BottomRadius, float Height, 
                                          int32 TopSides, int32 BottomSides);
    
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation")
    void RegenerateMeshBlueprint();
    
    UFUNCTION(BlueprintCallable, Category = "Frustum|Materials")
    void SetMaterial(UMaterialInterface* NewMaterial);
};