// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrism.h
 * @brief 可配置的空心棱柱生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的空心棱柱生成器。
 * 支持以下特性：
 * - 可配置的内外半径和高度
 * - 可调节的边数和弧角
 * - 可选的倒角效果
 * - 支持部分弧段生成
 * - 实时预览和更新
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "HollowPrismParameters.h"

// Generated header must be the last include
#include "HollowPrism.generated.h"

// Forward declarations
class UProceduralMeshComponent;
class FHollowPrismBuilder;
struct FModelGenMeshData;



/**
 * 程序化生成的空心棱柱Actor
 * 支持实时参数调整和预览
 */
UCLASS(BlueprintType, meta=(DisplayName = "Hollow Prism"))
class MODELGEN_API AHollowPrism : public AActor
{
    GENERATED_BODY()

public:
    //~ Begin Constructor Section
    AHollowPrism();

    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

protected:
    //~ Begin Parameters
    /** 生成参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Parameters")
    FHollowPrismParameters Parameters;

    /** 材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Materials")
    UMaterialInterface* Material;

    /** 是否生成碰撞体 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Collision")
    bool bGenerateCollision = true;

    /** 是否使用异步碰撞体烘焙 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Collision")
    bool bUseAsyncCooking = true;

private:
    //~ Begin Components
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, Category = "HollowPrism|Components")
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
    
    /** 重新生成网格（蓝图可调用） */
    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Generation")
    void RegenerateMeshBlueprint();
};