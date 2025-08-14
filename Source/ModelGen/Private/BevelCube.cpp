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
    Parameters.BevelRadius = 10.0f;
    Parameters.BevelSegments = 3;

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
        return;
    }

    ProceduralMesh->ClearAllMeshSections();

    if (!Parameters.IsValid())
    {
        return;
    }

    // 创建构建器并生成网格数据
    FBevelCubeBuilder Builder(Parameters);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        if (!MeshData.IsValid())
        {
            return;
        }

        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        
        ApplyMaterial();
        
        SetupCollision();
    }
}

void ABevelCube::GenerateBeveledCube(float Size, float BevelSize, int32 Sections)
{
    Parameters.Size = Size;
    Parameters.BevelRadius = BevelSize;
    Parameters.BevelSegments = Sections;
    
    RegenerateMesh();
}