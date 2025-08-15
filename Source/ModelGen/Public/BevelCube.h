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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Size", 
        ToolTip = "立方体的基础大小"))
    float Size = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Size", 
        ToolTip = "倒角的大小，不能超过立方体半边长"))
    float BevelRadius = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10", 
        DisplayName = "Bevel Sections", ToolTip = "倒角的分段数，影响倒角的平滑度"))
    int32 BevelSegments = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Collision")
    bool bUseAsyncCooking = true;

public:
    // 参数验证和计算函数
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