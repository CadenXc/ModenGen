// Copyright (c) 2024. All rights reserved.

/**
 * @file Frustum.h
 * @brief 可配置的程序化截锥体生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的截锥体生成器。
 * 支持以下特性：
 * - 可配置的截锥体几何参数
 * - 可调节的倒角大小和分段数
 * - 实时预览和更新
 * - 材质和碰撞体支持
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
    //~ Begin Parameters
    /** 生成参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Parameters")
    FFrustumParameters Parameters;

    /** 材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Materials")
    UMaterialInterface* Material;

    /** 是否生成碰撞体 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Collision")
    bool bGenerateCollision = true;

    /** 是否使用异步碰撞体烘焙 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Collision")
    bool bUseAsyncCooking = true;

private:
    //~ Begin Components
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, Category = "Frustum|Components")
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
    
    /** 使用指定参数生成截锥体 */
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation")
    void GenerateFrustum(float TopRadius, float BottomRadius, float Height, int32 Sides);
    
    /** 使用不同的顶部和底部边数生成截锥体 */
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation")
    void GenerateFrustumWithDifferentSides(float TopRadius, float BottomRadius, float Height, 
                                          int32 TopSides, int32 BottomSides);
    
    /** 重新生成网格（蓝图可调用） */
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation")
    void RegenerateMeshBlueprint();
    
    /** 设置材质（蓝图可调用） */
    UFUNCTION(BlueprintCallable, Category = "Frustum|Materials")
    void SetMaterial(UMaterialInterface* NewMaterial);
};