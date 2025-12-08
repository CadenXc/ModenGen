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

private:
    // 辅助函数：创建并初始化 StaticMesh 对象
    UStaticMesh* CreateStaticMeshObject() const;
    
    // 辅助函数：从 ProceduralMeshComponent 构建 MeshDescription
    bool BuildMeshDescriptionFromPMC(FMeshDescription& OutMeshDescription, UStaticMesh* StaticMesh) const;
    
    // 辅助函数：从 ProceduralMeshComponent 构建 StaticMesh 几何体
    bool BuildStaticMeshGeometryFromProceduralMesh(UStaticMesh* StaticMesh) const;
    
    // 辅助函数：初始化 StaticMesh 的 RenderData
    bool InitializeStaticMeshRenderData(UStaticMesh* StaticMesh) const;
    
    // 辅助函数：设置 BodySetup 的基本属性
    void SetupBodySetupProperties(UBodySetup* BodySetup) const;
    
    // 辅助函数：生成简单碰撞（ConvexElems 或 BoxElems）
    bool GenerateSimpleCollision(UBodySetup* BodySetup, UStaticMesh* StaticMesh) const;
    
    // 辅助函数：手动创建所有 ConvexMesh
    int32 CreateConvexMeshesManually(UBodySetup* BodySetup, IPhysXCookingModule* PhysXCookingModule) const;
    
    // 辅助函数：从 ProceduralMeshComponent 提取 TriMesh 数据
    bool ExtractTriMeshDataFromPMC(TArray<FVector>& OutVertices, TArray<FTriIndices>& OutIndices) const;
    
    // 辅助函数：从 RenderData 提取 TriMesh 数据
    bool ExtractTriMeshDataFromRenderData(UStaticMesh* StaticMesh, TArray<FVector>& OutVertices, TArray<FTriIndices>& OutIndices) const;
    
    // 辅助函数：生成复杂碰撞（TriMeshes）
    bool GenerateComplexCollision(UBodySetup* BodySetup, IPhysXCookingModule* PhysXCookingModule) const;
    
    // 辅助函数：记录碰撞统计信息
    void LogCollisionStatistics(UBodySetup* BodySetup, UStaticMesh* StaticMesh) const;
    
    // 辅助函数：创建 BodySetup 并生成碰撞（简单碰撞和复杂碰撞）
    void SetupBodySetupAndCollision(UStaticMesh* StaticMesh) const;
    
    // 辅助函数：手动计算切线（用于解决移动端引擎计算失败的问题）
    void GenerateTangentsManually(FMeshDescription& MeshDescription) const;
};
