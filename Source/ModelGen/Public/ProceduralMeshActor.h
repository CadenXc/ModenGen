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

UCLASS(BlueprintType, meta=(DisplayName = "Procedural Mesh Actor"))
class MODELGEN_API AProceduralMeshActor : public AActor
{
    GENERATED_BODY()

public:
    AProceduralMeshActor();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    //~ Begin Common Properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Materials", meta = (ToolTip = "ProceduralMeshComponent使用的材质"))
    UMaterialInterface* ProceduralMeshMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Materials", meta = (ToolTip = "StaticMeshComponent使用的材质"))
    UMaterialInterface* StaticMeshMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Materials", meta = (ToolTip = "为ProceduralMesh的每个网格段设置不同的材质"))
    TArray<UMaterialInterface*> ProceduralSectionMaterials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Materials", meta = (ToolTip = "为StaticMesh的每个网格段设置不同的材质"))
    TArray<UMaterialInterface*> StaticSectionMaterials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bUseAsyncCooking = true;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Component")
    UProceduralMeshComponent* ProceduralMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Component")
    UStaticMeshComponent* StaticMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bAutoGenerateStaticMesh = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bShowStaticMeshInEditor = true;
    // 设置静态网格组件可见性
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Component")
    void SetStaticMeshVisibility(bool bVisible);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bShowProceduralMeshInEditor = true;
    // 设置程序化网格组件可见性
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Component")
    void SetProceduralMeshVisibility(bool bVisible);

protected:
    //~ Begin Common Methods
    virtual void InitializeComponents();
    virtual void ApplyMaterial();
    virtual void SetupCollision();
    virtual void RegenerateMesh();
    
    virtual void GenerateMesh() {}

    virtual bool IsValid() const { return true; }

public:
    // 通用的网格信息获取方法
    UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMeshComponent; }
    UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
    bool ShouldGenerateCollision() const { return bGenerateCollision; }
    bool ShouldUseAsyncCooking() const { return bUseAsyncCooking; }
    
    // 通用的网格操作
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void ClearMesh();
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetProceduralMeshMaterial(UMaterialInterface* NewMaterial);
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetStaticMeshMaterial(UMaterialInterface* NewMaterial);
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetProceduralSectionMaterial(int32 SectionIndex, UMaterialInterface* NewMaterial);
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetStaticSectionMaterial(int32 SectionIndex, UMaterialInterface* NewMaterial);
    
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetCollisionEnabled(bool bEnable);


	UStaticMesh* ConvertProceduralMeshToStaticMesh();
    
    // 转换程序化网格为静态网格组件
    void ConvertToStaticMeshComponent();
    
    // 手动生成静态网格（蓝图可调用）
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|StaticMesh")
    void GenerateStaticMesh();
    
    // 刷新静态网格（当程序化网格改变后调用）
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|StaticMesh")
    void RefreshStaticMesh();

protected:
    // 清理当前静态网格组件的资源
    void CleanupCurrentStaticMesh();
    
    // 将材质应用到StaticMeshComponent
    void ApplyMaterialToStaticMesh();
};
