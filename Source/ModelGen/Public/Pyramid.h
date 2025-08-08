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
#include "PyramidParameters.h"

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
    virtual void Tick(float DeltaTime) override;

protected:
    //~ Begin Parameters
    /** 生成参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters")
    FPyramidParameters Parameters;

    /** 材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Materials")
    UMaterialInterface* Material;

    /** 是否生成碰撞体 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Collision")
    bool bGenerateCollision = true;

    /** 是否使用异步碰撞体烘焙 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Collision")
    bool bUseAsyncCooking = true;

private:
    //~ Begin Components
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, Category = "Pyramid|Components")
    UProceduralMeshComponent* ProceduralMesh;

    //~ Begin Private Methods
    /** 初始化组件 */
    void InitializeComponents();
    
    /** 应用材质 */
    void ApplyMaterial();
    
    /** 设置碰撞 */
    void SetupCollision();
    
    /** 重新生成网格 */
    void RegenerateMesh();
    
    /** 使用指定参数生成金字塔 */
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Generation")
    void GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides);
};