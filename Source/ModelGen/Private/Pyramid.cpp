// Copyright (c) 2024. All rights reserved.

/**
 * @file Pyramid.cpp
 * @brief 可配置的程序化金字塔生成器的实现
 */

#include "Pyramid.h"
#include "PyramidBuilder.h"
#include "PyramidParameters.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

APyramid::APyramid()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // 创建程序化网格组件
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;
    
    // 初始化默认参数
    Parameters.BaseRadius = 100.0f;
    Parameters.Height = 200.0f;
    Parameters.Sides = 4;
    Parameters.BevelRadius = 0.0f;
    
    // 初始化组件
    InitializeComponents();
}

void APyramid::BeginPlay()
{
    Super::BeginPlay();
    
    // 在游戏开始时生成网格
    RegenerateMesh();
}

void APyramid::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    // 在编辑器中构建时生成网格
    RegenerateMesh();
}

void APyramid::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void APyramid::InitializeComponents()
{
    if (ProceduralMesh)
    {
        // 设置程序化网格组件的默认属性
        ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProceduralMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        
        ApplyMaterial();
        
        SetupCollision();
    }
}

void APyramid::ApplyMaterial()
{
    if (ProceduralMesh && Material)
    {
        ProceduralMesh->SetMaterial(0, Material);
    }
}

void APyramid::SetupCollision()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
    }
}

void APyramid::RegenerateMesh()
{
    UE_LOG(LogTemp, Log, TEXT("APyramid::RegenerateMesh - Starting mesh regeneration"));
    
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMesh component is null"));
        return;
    }
    
    // 清除之前的网格数据
    ProceduralMesh->ClearAllMeshSections();
    UE_LOG(LogTemp, Log, TEXT("Cleared all mesh sections"));
    
    // 验证参数
    if (!Parameters.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid Pyramid parameters: BaseRadius=%f, Height=%f, Sides=%d, BevelRadius=%f"),
               Parameters.BaseRadius, Parameters.Height, Parameters.Sides, Parameters.BevelRadius);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Starting Pyramid generation with parameters: BaseRadius=%f, Height=%f, Sides=%d, BevelRadius=%f"),
           Parameters.BaseRadius, Parameters.Height, Parameters.Sides, Parameters.BevelRadius);
    UE_LOG(LogTemp, Log, TEXT("BevelRadius > 0: %s"), Parameters.BevelRadius > 0.0f ? TEXT("true") : TEXT("false"));
    
    // 创建构建器并生成网格数据
    FPyramidBuilder Builder(Parameters);
    FModelGenMeshData MeshData;
    
    if (Builder.Generate(MeshData))
    {
        // 验证生成的网格数据
        if (!MeshData.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Generated mesh data is invalid"));
            UE_LOG(LogTemp, Error, TEXT("Mesh data details: Vertices=%d, Triangles=%d, Normals=%d, UVs=%d, Tangents=%d"),
                   MeshData.Vertices.Num(), MeshData.Triangles.Num(), MeshData.Normals.Num(), 
                   MeshData.UVs.Num(), MeshData.Tangents.Num());
            return;
        }
        
        UE_LOG(LogTemp, Log, TEXT("Mesh data generated successfully: %d vertices, %d triangles"),
               MeshData.GetVertexCount(), MeshData.GetTriangleCount());
        
        // 将网格数据应用到程序化网格组件
        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        
        ApplyMaterial();
        
        SetupCollision();
        
        UE_LOG(LogTemp, Log, TEXT("Pyramid generated successfully: %d vertices, %d triangles"),
               MeshData.GetVertexCount(), MeshData.GetTriangleCount());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate Pyramid mesh"));
    }
}

void APyramid::GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides)
{
    // 更新参数
    Parameters.BaseRadius = InBaseRadius;
    Parameters.Height = InHeight;
    Parameters.Sides = InSides;
    
    // 重新生成网格
    RegenerateMesh();
}
