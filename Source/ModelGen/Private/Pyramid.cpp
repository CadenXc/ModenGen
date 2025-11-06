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
    TryGenerateMeshInternal();
}

bool APyramid::TryGenerateMeshInternal()
{
    if (!IsValid())
    {
        return false;
    }

    FPyramidBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData))
    {
        return false;
    }

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("APyramid::TryGenerateMeshInternal - 生成的网格数据无效"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

void APyramid::GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides)
{
    float OldBaseRadius = BaseRadius;
    float OldHeight = Height;
    int32 OldSides = Sides;
    
    BaseRadius = InBaseRadius;
    Height = InHeight;
    Sides = InSides;
    
    if (ProceduralMeshComponent)
    {
        if (!TryGenerateMeshInternal())
        {
            BaseRadius = OldBaseRadius;
            Height = OldHeight;
            Sides = OldSides;
            UE_LOG(LogTemp, Warning, TEXT("GeneratePyramid: 网格生成失败，参数已恢复"));
        }
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

void APyramid::SetBaseRadius(float NewBaseRadius)
{
    if (NewBaseRadius > 0.0f && NewBaseRadius != BaseRadius)
    {
        float OldBaseRadius = BaseRadius;
        BaseRadius = NewBaseRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                BaseRadius = OldBaseRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetBaseRadius: 网格生成失败，参数已恢复为 %f"), OldBaseRadius);
            }
        }
    }
}

void APyramid::SetHeight(float NewHeight)
{
    if (NewHeight > 0.0f && NewHeight != Height)
    {
        float OldHeight = Height;
        Height = NewHeight;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                Height = OldHeight;
                UE_LOG(LogTemp, Warning, TEXT("SetHeight: 网格生成失败，参数已恢复为 %f"), OldHeight);
            }
        }
    }
}

void APyramid::SetSides(int32 NewSides)
{
    if (NewSides >= 3 && NewSides <= 100 && NewSides != Sides)
    {
        int32 OldSides = Sides;
        Sides = NewSides;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                Sides = OldSides;
                UE_LOG(LogTemp, Warning, TEXT("SetSides: 网格生成失败，参数已恢复为 %d"), OldSides);
            }
        }
    }
}

void APyramid::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && NewBevelRadius < Height && NewBevelRadius != BevelRadius)
    {
        float OldBevelRadius = BevelRadius;
        BevelRadius = NewBevelRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                BevelRadius = OldBevelRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetBevelRadius: 网格生成失败，参数已恢复为 %f"), OldBevelRadius);
            }
        }
    }
}
