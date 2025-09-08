// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bShowProceduralComponent = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bUseAsyncCooking = true;

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Component")
    void SetProceduralMeshVisibility(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetPMCCollisionEnabled(bool bEnable);



    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bGenerateCollision = true;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bShowStaticMeshComponent = false;

    // 材质设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Materials")
    UMaterialInterface* StaticMeshMaterial = nullptr;

    // 构造时预加载并缓存的默认材质（用于PMC）
    UPROPERTY(VisibleAnywhere, Category = "ProceduralMesh|Materials")
    UMaterialInterface* ProceduralDefaultMaterial = nullptr;

   protected:

    virtual bool IsValid() const {return true;}

    // 子类计算自己的Mesh
    virtual void GenerateMesh() { }

    // 通用的网格信息获取方法
    UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMeshComponent; }
};
