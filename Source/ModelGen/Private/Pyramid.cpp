// Copyright (c) 2024. All rights reserved.

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
        }
    }
}

bool APyramid::IsValid() const
{
    return BaseRadius > 0.0f && Height > 0.0f &&
           Sides >= 3 && Sides <= 25 &&
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
    if (NewBaseRadius > 0.0f && !FMath::IsNearlyEqual(NewBaseRadius, BaseRadius))
    {
        float OldBaseRadius = BaseRadius;
        BaseRadius = NewBaseRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                BaseRadius = OldBaseRadius;
            }
        }
    }
}

void APyramid::SetHeight(float NewHeight)
{
    if (NewHeight > 0.0f && !FMath::IsNearlyEqual(NewHeight, Height))
    {
        float OldHeight = Height;
        Height = NewHeight;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                Height = OldHeight;
            }
        }
    }
}

void APyramid::SetSides(int32 NewSides)
{
    if (NewSides >= 3 && NewSides <= 25 && NewSides != Sides)
    {
        int32 OldSides = Sides;
        Sides = NewSides;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                Sides = OldSides;
            }
        }
    }
}

void APyramid::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && NewBevelRadius < Height && !FMath::IsNearlyEqual(NewBevelRadius, BevelRadius))
    {
        float OldBevelRadius = BevelRadius;
        BevelRadius = NewBevelRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                BevelRadius = OldBevelRadius;
            }
        }
    }
}

void APyramid::SetSmoothSides(bool bNewSmoothSides)
{
    if (bNewSmoothSides != bSmoothSides)
    {
        bool OldSmoothSides = bSmoothSides;
        bSmoothSides = bNewSmoothSides;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                bSmoothSides = OldSmoothSides;
            }
        }
    }
}
