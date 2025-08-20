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
class UStaticMesh;
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Component")
    UProceduralMeshComponent* ProceduralMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Component")
    UStaticMeshComponent* StaticMeshComponent;

protected:
    //~ Begin Common Methods
    virtual void InitializeComponents();
    virtual void ApplyMaterial();
    virtual void SetupCollision();
    virtual void RegenerateMesh();
    
    // 纯虚函数，子类必须实现
    virtual void GenerateMesh() PURE_VIRTUAL(AProceduralMeshActor::GenerateMesh,);

private:
    // 跟踪当前是否使用静态网格
    bool bUsingStaticMesh = false;

public:
    // 纯虚函数，子类必须实现
    virtual bool IsValid() const PURE_VIRTUAL(AProceduralMeshActor::IsValid, return false;);

public:
    // 通用的网格信息获取方法
    UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMeshComponent; }
    bool ShouldGenerateCollision() const { return bGenerateCollision; }
    bool ShouldUseAsyncCooking() const { return bUseAsyncCooking; }
    
    // 通用的网格操作
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void ClearMesh();
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetMaterial(UMaterialInterface* NewMaterial);
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetCollisionEnabled(bool bEnable);


	UStaticMesh* ConvertProceduralMeshToStaticMesh(
        UProceduralMeshComponent* ProcMesh);
    
    // 转换程序化网格为静态网格组件
    void ConvertToStaticMeshComponent();
    
    // 切换显示模式（程序化网格 <-> 静态网格）
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void ToggleDisplayMode();
    
    // 获取当前显示模式
    UFUNCTION(BlueprintPure, Category = "ProceduralMesh|Operations")
    bool IsUsingStaticMesh() const;
    
    // 转换程序化网格为静态网格并返回（蓝图可调用）
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    UStaticMesh* ConvertAndGetStaticMesh();
    
    // 同时显示两个组件（程序化网格和静态网格）
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void ShowBothComponents();
    
    // 只显示程序化网格组件
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void ShowProceduralMeshOnly();
    
    // 只显示静态网格组件
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void ShowStaticMeshOnly();
};
