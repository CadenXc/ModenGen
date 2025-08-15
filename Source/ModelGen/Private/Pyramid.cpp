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
    
    BaseRadius = 100.0f;
    Height = 200.0f;
    Sides = 4;
    BevelRadius = 0.0f;
    
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
    
    if (!IsValid())
    {
        return;
    }
    
    FPyramidBuilder Builder(*this);
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
    BaseRadius = InBaseRadius;
    Height = InHeight;
    Sides = InSides;
    
    RegenerateMesh();
}

bool APyramid::IsValid() const
{
    const bool bValidBaseRadius = BaseRadius > 0.0f;
    const bool bValidHeight = Height > 0.0f;
    const bool bValidSides = Sides >= 3 && Sides <= 100;
    const bool bValidBevelRadius = BevelRadius >= 0.0f && BevelRadius < Height;
    
    return bValidBaseRadius && bValidHeight && bValidSides && bValidBevelRadius;
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
