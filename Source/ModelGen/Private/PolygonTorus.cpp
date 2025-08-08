// Copyright (c) 2024. All rights reserved.

/**
 * @file PolygonTorus.cpp
 * @brief 可配置的多边形圆环生成器的实现
 */

#include "PolygonTorus.h"
#include "PolygonTorusBuilder.h"
#include "PolygonTorusParameters.h"
#include "ModelGenMeshData.h"

// Engine includes
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

// 日志分类定义
DEFINE_LOG_CATEGORY_STATIC(LogPolygonTorus, Log, All);

/**
 * 构造函数
 * 初始化组件和默认设置
 */
APolygonTorus::APolygonTorus()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建程序化网格组件
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;
    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 设置默认材质
    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (MaterialFinder.Succeeded())
    {
        ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
    }
    else
    {
        UE_LOG(LogPolygonTorus, Warning, TEXT("无法加载默认材质"));
    }

    // 初始生成圆环
    RegenerateMesh();
}

/**
 * 游戏开始时调用
 * 确保圆环已正确生成
 */
void APolygonTorus::BeginPlay()
{
    Super::BeginPlay();
    RegenerateMesh();
}

/**
 * 编辑器构造时调用
 * 实时更新圆环以反映参数变化
 */
void APolygonTorus::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RegenerateMesh();
}

/**
 * 重新生成网格
 * 使用新的参数结构和生成器
 */
void APolygonTorus::RegenerateMesh()
{
    UE_LOG(LogPolygonTorus, Log, TEXT("APolygonTorus::RegenerateMesh - Starting mesh regeneration"));
    
    // 清除之前的网格数据
    ProceduralMesh->ClearAllMeshSections();

    // 验证参数
    if (!Parameters.IsValid())
    {
        UE_LOG(LogPolygonTorus, Error, TEXT("无效的圆环参数"));
        return;
    }

    UE_LOG(LogPolygonTorus, Log, TEXT("APolygonTorus::RegenerateMesh - Parameters validated successfully"));
    
    // 创建生成器并生成网格
    FPolygonTorusBuilder Builder(Parameters);
    FModelGenMeshData MeshData;
    
    if (Builder.Generate(MeshData))
    {
        UE_LOG(LogPolygonTorus, Log, TEXT("APolygonTorus::RegenerateMesh - Mesh generation successful"));
        
        // 应用网格数据到组件
        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        
        // 设置材质
        if (ProceduralMesh->GetMaterial(0) == nullptr)
        {
            static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
            if (MaterialFinder.Succeeded())
            {
                ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
            }
        }
    
    // 设置碰撞
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        
        UE_LOG(LogPolygonTorus, Log, TEXT("APolygonTorus::RegenerateMesh - Mesh applied successfully: %d vertices, %d triangles"), 
               MeshData.GetVertexCount(), MeshData.GetTriangleCount());
        }
        else
        {
        UE_LOG(LogPolygonTorus, Error, TEXT("圆环网格生成失败"));
    }
}
