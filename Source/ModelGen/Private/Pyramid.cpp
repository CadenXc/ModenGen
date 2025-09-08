// Copyright (c) 2024. All rights reserved.

/**
 * @file Pyramid.cpp
 * @brief 可配置的程序化金字塔生成器的实现
 */

#include "Pyramid.h"
#include "PyramidBuilder.h"
#include "ModelGenMeshData.h"

APyramid::APyramid()
{
    PrimaryActorTick.bCanEverTick = false;
}

void APyramid::GenerateMesh()
{
    if (!IsValid())
    {
        return;
    }
    
    FPyramidBuilder Builder(*this);
    FModelGenMeshData MeshData;
    
    if (Builder.Generate(MeshData))
    {
        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    }
}

void APyramid::GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides)
{
    BaseRadius = InBaseRadius;
    Height = InHeight;
    Sides = InSides;
    
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->ClearAllMeshSections();
        GenerateMesh();
    }
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
    
    const float ScaleFactor = 1.0f - (BevelRadius / Height);
    return FMath::Max(0.0f, BaseRadius * ScaleFactor);
}

int32 APyramid::CalculateVertexCountEstimate() const
{
    const int32 BaseVertexCount = Sides;
    const int32 BevelVertexCount = (BevelRadius > 0.0f) ? Sides * 2 : 0;
    const int32 PyramidVertexCount = Sides + 1;
    
    return BaseVertexCount + BevelVertexCount + PyramidVertexCount;
}

int32 APyramid::CalculateTriangleCountEstimate() const
{
    const int32 BaseTriangleCount = Sides - 2;
    const int32 BevelTriangleCount = (BevelRadius > 0.0f) ? Sides * 2 : 0;
    const int32 PyramidTriangleCount = Sides;
    
    return BaseTriangleCount + BevelTriangleCount + PyramidTriangleCount;
}

// 设置参数函数实现
void APyramid::SetBaseRadius(float NewBaseRadius)
{
    if (NewBaseRadius > 0.0f && NewBaseRadius != BaseRadius)
    {
        BaseRadius = NewBaseRadius;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void APyramid::SetHeight(float NewHeight)
{
    if (NewHeight > 0.0f && NewHeight != Height)
    {
        Height = NewHeight;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void APyramid::SetSides(int32 NewSides)
{
    if (NewSides >= 3 && NewSides <= 100 && NewSides != Sides)
    {
        Sides = NewSides;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void APyramid::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && NewBevelRadius < Height && NewBevelRadius != BevelRadius)
    {
        BevelRadius = NewBevelRadius;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}
