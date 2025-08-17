// Copyright (c) 2024. All rights reserved.

/**
 * @file ProceduralMeshActor.h
 * @brief 程序化网格Actor的基类，提供通用的网格生成和管理功能
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS(Abstract, BlueprintType, meta=(DisplayName = "Procedural Mesh Actor"))
class MODELGEN_API AProceduralMeshActor : public AActor
{
    GENERATED_BODY()

public:
    AProceduralMeshActor();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

    //~ Begin Common Properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bUseAsyncCooking = true;

protected:
    UPROPERTY(VisibleAnywhere, Category = "ProceduralMesh|Components")
    UProceduralMeshComponent* ProceduralMesh;

    //~ Begin Common Methods
    virtual void InitializeComponents();
    virtual void ApplyMaterial();
    virtual void SetupCollision();
    virtual void RegenerateMesh();
    
    // 纯虚函数，子类必须实现
    virtual void GenerateMesh() PURE_VIRTUAL(AProceduralMeshActor::GenerateMesh,);

public:
    // 纯虚函数，子类必须实现
    virtual bool IsValid() const PURE_VIRTUAL(AProceduralMeshActor::IsValid, return false;);

public:
    // 通用的网格信息获取方法
    UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMesh; }
    bool ShouldGenerateCollision() const { return bGenerateCollision; }
    bool ShouldUseAsyncCooking() const { return bUseAsyncCooking; }
    
    // 通用的网格操作
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void ClearMesh();
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetMaterial(UMaterialInterface* NewMaterial);
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetCollisionEnabled(bool bEnable);
};
