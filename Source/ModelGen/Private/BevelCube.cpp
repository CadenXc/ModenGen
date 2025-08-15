// Copyright (c) 2024. All rights reserved.

#include "BevelCube.h"
#include "BevelCubeBuilder.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

ABevelCube::ABevelCube()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;

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
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
        
        if (Material)
        {
            ProceduralMesh->SetMaterial(0, Material);
        }
    }
}

void ABevelCube::RegenerateMesh()
{
    if (!ProceduralMesh || !IsValid())
    {
        return;
    }

    ProceduralMesh->ClearAllMeshSections();

    FBevelCubeBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData) && MeshData.IsValid())
    {
        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        
        if (Material)
        {
            ProceduralMesh->SetMaterial(0, Material);
        }
        
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void ABevelCube::GenerateBeveledCube(float InSize, float InBevelSize, int32 InSections)
{
    Size = InSize;
    BevelRadius = InBevelSize;
    BevelSegments = InSections;
    
    RegenerateMesh();
}

bool ABevelCube::IsValid() const
{
    const bool bValidSize = Size > 0.0f;
    const bool bValidBevelSize = BevelRadius >= 0.0f && BevelRadius < GetHalfSize();
    const bool bValidSections = BevelSegments >= 1 && BevelSegments <= 10;
    
    return bValidSize && bValidBevelSize && bValidSections;
}

int32 ABevelCube::GetVertexCount() const
{
    const int32 BaseVertexCount = 24;
    const int32 EdgeBevelVertexCount = 12 * (BevelSegments + 1) * 2;
    const int32 CornerBevelVertexCount = 8 * (BevelSegments + 1) * (BevelSegments + 1) / 2;
    
    return BaseVertexCount + EdgeBevelVertexCount + CornerBevelVertexCount;
}

int32 ABevelCube::GetTriangleCount() const
{
    const int32 BaseTriangleCount = 12;
    const int32 EdgeBevelTriangleCount = 12 * BevelSegments * 2;
    const int32 CornerBevelTriangleCount = 8 * BevelSegments * BevelSegments * 2;
    
    return BaseTriangleCount + EdgeBevelTriangleCount + CornerBevelTriangleCount;
}