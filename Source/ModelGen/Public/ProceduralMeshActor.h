// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

#include "ProceduralMeshActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS(BlueprintType, meta=(DisplayName = "Procedural Mesh Actor"))
class MODELGEN_API AProceduralMeshActor : public AActor
{
    GENERATED_BODY()

public:
    AProceduralMeshActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    // ProceduralMesh相关
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Component")
    UProceduralMeshComponent* ProceduralMeshComponent;

    // StaticMesh相关
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    UStaticMeshComponent* StaticMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bShowProceduralComponent = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bUseAsyncCooking = true;

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Component")
    void SetProceduralMeshVisibility(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetPMCCollisionEnabled(bool bEnable);

    // 为ProceduralMeshComponent自动生成碰撞数据（如果还没有的话）
    // 基于实际顶点计算包围盒并添加到PMC的BodySetup中
    // 这样在转换时就可以使用方式1（从PMC复制碰撞数据），获得更好的性能
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Collision")
    void GeneratePMCollisionData();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bGenerateCollision = true;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bShowStaticMeshComponent = true;

    // 材质设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Materials")
    UMaterialInterface* StaticMeshMaterial = nullptr;

    // 构造时预加载并缓存的默认材质（用于PMC）
    UPROPERTY(VisibleAnywhere, Category = "ProceduralMesh|Materials")
    UMaterialInterface* ProceduralDefaultMaterial = nullptr;

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    UStaticMesh* ConvertProceduralMeshToStaticMesh();

    // 更新StaticMeshComponent的方法（在细节面板中显示为按钮）
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|StaticMesh",
        meta = (CallInEditor = "true", DisplayName = "转换到 StaticMesh"))
    void UpdateStaticMeshComponent();

   protected:

    virtual bool IsValid() const {return true;}

    // 通用的网格信息获取方法
    UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMeshComponent; }

public:
    // 子类计算自己的Mesh
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    virtual void GenerateMesh() { }
};
