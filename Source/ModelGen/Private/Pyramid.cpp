// Copyright (c) 2024. All rights reserved.

/**
 * @file Pyramid.cpp
 * @brief 可配置的程序化金字塔生成器的实现
 */

#include "Pyramid.h"
#include "PyramidBuilder.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

APyramid::APyramid()
{
    PrimaryActorTick.bCanEverTick = false;
    
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;
    
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
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
        
        if (Material)
        {
            ProceduralMesh->SetMaterial(0, Material);
        }
    }
}

void APyramid::RegenerateMesh()
{
    if (!ProceduralMesh || !IsValid())
    {
        return;
    }
    
    ProceduralMesh->ClearAllMeshSections();
    
    FPyramidBuilder Builder(*this);
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

void APyramid::GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides)
{
    BaseRadius = InBaseRadius;
    Height = InHeight;
    Sides = InSides;
    RegenerateMesh();
}

bool APyramid::IsValid() const
{
    return BaseRadius > 0.0f && Height > 0.0f && 
           Sides >= 3 && Sides <= 100 && 
           BevelRadius >= 0.0f && BevelRadius < Height;
}

float APyramid::GetBevelTopRadius() const
{
    if (BevelRadius <= 0.0f) 
    {
        return BaseRadius;
    }
    
    float ScaleFactor = 1.0f - (BevelRadius / Height);
    return FMath::Max(0.0f, BaseRadius * ScaleFactor);
}

int32 APyramid::CalculateVertexCountEstimate() const
{
    int32 BaseVertexCount = Sides;
    int32 BevelVertexCount = (BevelRadius > 0.0f) ? Sides * 2 : 0;
    int32 PyramidVertexCount = Sides + 1;
    
    return BaseVertexCount + BevelVertexCount + PyramidVertexCount;
}

int32 APyramid::CalculateTriangleCountEstimate() const
{
    int32 BaseTriangleCount = Sides - 2;
    int32 BevelTriangleCount = (BevelRadius > 0.0f) ? Sides * 2 : 0;
    int32 PyramidTriangleCount = Sides;
    
    return BaseTriangleCount + BevelTriangleCount + PyramidTriangleCount;
}
