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

    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaTime) override;

protected:
    //~ Begin Parameters
    /** 生成参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters")
    FBevelCubeParameters Parameters;

    /** 材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Materials")
    UMaterialInterface* Material;

    /** 是否生成碰撞体 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Collision")
    bool bGenerateCollision = true;

    /** 是否使用异步碰撞体烘焙 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Collision")
    bool bUseAsyncCooking = true;

private:
    //~ Begin Components
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, Category = "BevelCube|Components")
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
    
    /** 使用指定参数生成圆角立方体 */
    UFUNCTION(BlueprintCallable, Category = "BevelCube|Generation")
    void GenerateBeveledCube(float Size, float BevelSize, int32 Sections);
};