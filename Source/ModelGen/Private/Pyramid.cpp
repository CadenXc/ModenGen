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
    
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;
    
    Parameters.BaseRadius = 100.0f;
    Parameters.Height = 200.0f;
    Parameters.Sides = 4;
    Parameters.BevelRadius = 0.0f;
    
    InitializeComponents();
}

void APyramid::BeginPlay()
{
    Super::BeginPlay();
    
    RegenerateMesh();
}

void APyramid::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    RegenerateMesh();
}

void APyramid::InitializeComponents()
{
    if (ProceduralMesh)
    {
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
    if (!ProceduralMesh)
    {
        return;
    }
    
    ProceduralMesh->ClearAllMeshSections();
    
    if (!Parameters.IsValid())
    {
        return;
    }
    
    FPyramidBuilder Builder(Parameters);
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

void APyramid::GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides)
{
    Parameters.BaseRadius = InBaseRadius;
    Parameters.Height = InHeight;
    Parameters.Sides = InSides;
    
    RegenerateMesh();
}
