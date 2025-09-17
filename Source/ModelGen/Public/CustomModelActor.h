// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "ModelStrategyFactory.h"
#include "CustomModelActor.generated.h"

UCLASS(BlueprintType, meta=(DisplayName = "Custom Model Actor"))
class MODELGEN_API ACustomModelActor : public AActor
{
    GENERATED_BODY()
    
public:    
    ACustomModelActor();

protected:
    virtual void BeginPlay() override;

public:    
    virtual void Tick(float DeltaTime) override;

    // StaticMesh组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CustomModel|Component")
    UStaticMeshComponent* StaticMeshComponent;

    // 材质设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomModel|Materials")
    UMaterialInterface* StaticMeshMaterial = nullptr;

    // 可见性控制
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomModel|Display")
    bool bShowStaticMesh = true;

    // 模型类型选择
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomModel|Model", 
        meta = (DisplayName = "模型类型"))
    FString ModelTypeName = TEXT("BevelCube");

    // 生成StaticMesh的方法
    UFUNCTION(BlueprintCallable, Category = "CustomModel|Model", 
        meta = (DisplayName = "Generate Mesh", CallInEditor = "true"))
    void GenerateMesh();

    // 切换模型类型
    UFUNCTION(BlueprintCallable, Category = "CustomModel|Model", 
        meta = (DisplayName = "Set Model Type"))
    void SetModelType(const FString& NewModelTypeName);

protected:
    // 初始化模型
    void InitializeModel();
    
    // 更新StaticMesh
    void UpdateStaticMesh();
};
