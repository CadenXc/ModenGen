// Copyright (c) 2024. All rights reserved.

#include "BevelCube.h"
#include "BevelCubeBuilder.h"
#include "BevelCubeParameters.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

ABevelCube::ABevelCube()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;

    Parameters.Size = 100.0f;
    Parameters.BevelSize = 10.0f;
    Parameters.BevelSections = 3;

    InitializeComponents();
}

void ABevelCube::BeginPlay()
{
    Super::BeginPlay();
    
    RegenerateMesh();
}

void ABevelCube::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    RegenerateMesh();
}

void ABevelCube::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ABevelCube::InitializeComponents()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProceduralMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        
        ApplyMaterial();
        
        SetupCollision();
    }
}

void ABevelCube::ApplyMaterial()
{
    if (ProceduralMesh && Material)
    {
        ProceduralMesh->SetMaterial(0, Material);
    }
}

void ABevelCube::SetupCollision()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
    }
}

void ABevelCube::RegenerateMesh()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMesh component is null"));
        return;
    }

    ProceduralMesh->ClearAllMeshSections();
    UE_LOG(LogTemp, Log, TEXT("Cleared all mesh sections"));

    if (!Parameters.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid BevelCube parameters: Size=%f, BevelSize=%f, BevelSections=%d"), 
               Parameters.Size, Parameters.BevelSize, Parameters.BevelSections);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Starting BevelCube generation with parameters: Size=%f, BevelSize=%f, BevelSections=%d"), 
           Parameters.Size, Parameters.BevelSize, Parameters.BevelSections);

    // 创建构建器并生成网格数据
    FBevelCubeBuilder Builder(Parameters);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        // 验证生成的网格数据
        if (!MeshData.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Generated mesh data is invalid"));
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("Mesh data generated successfully: %d vertices, %d triangles"), 
               MeshData.GetVertexCount(), MeshData.GetTriangleCount());

        // 将网格数据应用到程序化网格组件
        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        
        ApplyMaterial();
        
        SetupCollision();
        
        UE_LOG(LogTemp, Log, TEXT("BevelCube generated successfully: %d vertices, %d triangles"), 
               MeshData.GetVertexCount(), MeshData.GetTriangleCount());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate BevelCube mesh"));
    }
}

void ABevelCube::GenerateBeveledCube(float Size, float BevelSize, int32 Sections)
{
    Parameters.Size = Size;
    Parameters.BevelSize = BevelSize;
    Parameters.BevelSections = Sections;
    
    RegenerateMesh();
}