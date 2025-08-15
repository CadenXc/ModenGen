// Copyright (c) 2024. All rights reserved.

/**
 * @file Pyramid.h
 * @brief 可配置的程序化金字塔生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的金字塔生成器。
 * 支持以下特性：
 * - 可配置的底面半径和高度
 * - 可调节的边数（3-100边）
 * - 可选的底部倒角效果
 * - 实时预览和更新
 * - 材质和碰撞体支持
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

// Generated header must be the last include
#include "Pyramid.generated.h"

// Forward declarations
class UProceduralMeshComponent;
class FPyramidBuilder;
struct FModelGenMeshData;

/**
 * 金字塔Actor
 */
UCLASS(BlueprintType, meta=(DisplayName = "Pyramid"))
class MODELGEN_API APyramid : public AActor
{
    GENERATED_BODY()

public:
    APyramid();

    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    //~ Begin Geometry Parameters
    /** 底面半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Base Radius",
        ToolTip = "金字塔底面的半径"))
    float BaseRadius = 100.0f;
    
    /** 金字塔高度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Height",
        ToolTip = "金字塔的高度"))
    float Height = 200.0f;
    
    /** 底面边数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "3", ClampMax = "100", UIMin = "3", UIMax = "100",
        DisplayName = "Sides", ToolTip = "金字塔底面的边数"))
    int32 Sides = 4;
    
    /** 底部倒角半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Geometry",
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Radius",
        ToolTip = "底部倒角的半径，0表示无倒角"))
    float BevelRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Collision")
    bool bGenerateCollision = true;

    /** 是否使用异步碰撞体烘焙 */
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
    // 参数验证和计算函数
    /** 验证参数的有效性 */
    bool IsValid() const;
    
    /** 获取倒角顶部半径 */
    float GetBevelTopRadius() const;
    
    /** 计算预估的顶点数量 */
    int32 CalculateVertexCountEstimate() const;
    
    /** 计算预估的三角形数量 */
    int32 CalculateTriangleCountEstimate() const;
};