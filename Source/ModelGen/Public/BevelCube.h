// Copyright (c) 2024. All rights reserved.

/**
 * @file BevelCube.h
 * @brief 可配置的程序化圆角立方体生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的圆角立方体生成器。
 * 支持以下特性：
 * - 可配置的立方体大小
 * - 可调节的倒角大小和分段数
 * - 实时预览和更新
 * - 材质和碰撞体支持
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "BevelCubeParameters.h"

// Generated header must be the last include
#include "BevelCube.generated.h"

// Forward declarations
class UProceduralMeshComponent;
class FBevelCubeBuilder;
struct FModelGenMeshData;

/**
 * 圆角立方体Actor
 */
UCLASS(BlueprintType, meta=(DisplayName = "Bevel Cube"))
class MODELGEN_API ABevelCube : public AActor
{
    GENERATED_BODY()

public:
    ABevelCube();
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters")
    FBevelCubeParameters Parameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Collision")
    bool bUseAsyncCooking = true;

private:
    UPROPERTY(VisibleAnywhere, Category = "BevelCube|Components")
    UProceduralMeshComponent* ProceduralMesh;

    void InitializeComponents();
    
    void ApplyMaterial();
    
    void SetupCollision();
    
    void RegenerateMesh();
    
    UFUNCTION(BlueprintCallable, Category = "BevelCube|Generation")
    void GenerateBeveledCube(float Size, float BevelSize, int32 Sections);
};