// Copyright (c) 2024. All rights reserved.

#include "Frustum.h"
#include "FrustumBuilder.h"
#include "ModelGenMeshData.h"

AFrustum::AFrustum()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AFrustum::GenerateMesh()
{
    if (!IsValid()) return;

    FFrustumBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    }
}

bool AFrustum::IsValid() const
{
    return TopRadius > 0.0f && BottomRadius > 0.0f && Height > 0.0f &&
           TopSides >= 3 && BottomSides >= 3 && HeightSegments >= 1 &&
           BevelRadius >= 0.0f && 
           MinBendRadius >= 0.0f && ArcAngle > 0.0f && ArcAngle <= 360.0f;
}

int32 AFrustum::CalculateVertexCountEstimate() const
{
    if (!IsValid()) return 0;

    const int32 MaxSides = FMath::Max(TopSides, BottomSides);
    const int32 BaseVertices = TopSides + BottomSides;
    const int32 SideVertices = HeightSegments * MaxSides;
    
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        BevelVertices = (TopSides + BottomSides + HeightSegments * MaxSides);
    }

    return BaseVertices + SideVertices + BevelVertices;
}

int32 AFrustum::CalculateTriangleCountEstimate() const
{
    if (!IsValid()) return 0;

    const int32 MaxSides = FMath::Max(TopSides, BottomSides);
    const int32 BaseTriangles = TopSides + BottomSides;
    const int32 SideTriangles = HeightSegments * MaxSides * 2;
    
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangles = (TopSides + BottomSides + HeightSegments * MaxSides) * 2;
    }

    return BaseTriangles + SideTriangles + BevelTriangles;
}

// 设置参数函数实现
void AFrustum::SetTopRadius(float NewTopRadius)
{
    if (NewTopRadius > 0.0f && NewTopRadius != TopRadius)
    {
        TopRadius = NewTopRadius;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AFrustum::SetBottomRadius(float NewBottomRadius)
{
    if (NewBottomRadius > 0.0f && NewBottomRadius != BottomRadius)
    {
        BottomRadius = NewBottomRadius;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AFrustum::SetHeight(float NewHeight)
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

void AFrustum::SetTopSides(int32 NewTopSides)
{
	if (NewTopSides > BottomSides)
	{
		return;
	}
    
    if (NewTopSides >= 3 && NewTopSides != TopSides)
    {
        TopSides = NewTopSides;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AFrustum::SetBottomSides(int32 NewBottomSides)
{
    if (NewBottomSides >= 3 && NewBottomSides != BottomSides)
    {
        BottomSides = NewBottomSides;
        if (BottomSides < TopSides)
        {
            TopSides = BottomSides;
        }
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AFrustum::SetHeightSegments(int32 NewHeightSegments)
{
    if (NewHeightSegments >= 1 && NewHeightSegments != HeightSegments)
    {
        HeightSegments = NewHeightSegments;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AFrustum::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && NewBevelRadius != BevelRadius)
    {
        BevelRadius = NewBevelRadius;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AFrustum::SetBendAmount(float NewBendAmount)
{
    if (NewBendAmount >= -1.0f && NewBendAmount <= 1.0f && NewBendAmount != BendAmount)
    {
        BendAmount = NewBendAmount;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AFrustum::SetMinBendRadius(float NewMinBendRadius)
{
    if (NewMinBendRadius >= 0.0f && NewMinBendRadius != MinBendRadius)
    {
        MinBendRadius = NewMinBendRadius;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AFrustum::SetArcAngle(float NewArcAngle)
{
    if (NewArcAngle > 0.0f && NewArcAngle <= 360.0f && NewArcAngle != ArcAngle)
    {
        ArcAngle = NewArcAngle;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}