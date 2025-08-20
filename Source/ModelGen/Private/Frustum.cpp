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

void AFrustum::GenerateFrustum(float InTopRadius, float InBottomRadius, float InHeight, int32 InSides)
{
    TopRadius = InTopRadius;
    BottomRadius = InBottomRadius;
    Height = InHeight;
    TopSides = InSides;
    BottomSides = InSides;
    
    RegenerateMesh();
}

void AFrustum::GenerateFrustumWithDifferentSides(float InTopRadius, float InBottomRadius, float InHeight, 
                                                 int32 InTopSides, int32 InBottomSides)
{
    TopRadius = InTopRadius;
    BottomRadius = InBottomRadius;
    Height = InHeight;
    TopSides = InTopSides;
    BottomSides = InBottomSides;
    
    RegenerateMesh();
}

void AFrustum::RegenerateMeshBlueprint()
{
    RegenerateMesh();
}

void AFrustum::SetMaterial(UMaterialInterface* NewMaterial)
{
    Super::SetMaterial(NewMaterial);
}

bool AFrustum::IsValid() const
{
    return TopRadius > 0.0f && BottomRadius > 0.0f && Height > 0.0f &&
           TopSides >= 3 && BottomSides >= 3 && HeightSegments >= 1 &&
           BevelRadius >= 0.0f && BevelSegments >= 1 &&
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
        BevelVertices = (TopSides + BottomSides + HeightSegments * MaxSides) * BevelSegments;
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
        BevelTriangles = (TopSides + BottomSides + HeightSegments * MaxSides) * BevelSegments * 2;
    }

    return BaseTriangles + SideTriangles + BevelTriangles;
}

bool AFrustum::operator==(const AFrustum& Other) const
{
    return TopRadius == Other.TopRadius && BottomRadius == Other.BottomRadius && Height == Other.Height &&
           TopSides == Other.TopSides && BottomSides == Other.BottomSides && HeightSegments == Other.HeightSegments &&
           BevelRadius == Other.BevelRadius && BevelSegments == Other.BevelSegments && BendAmount == Other.BendAmount &&
           MinBendRadius == Other.MinBendRadius && ArcAngle == Other.ArcAngle;
}

bool AFrustum::operator!=(const AFrustum& Other) const
{
    return !(*this == Other);
}

// 设置参数函数实现
void AFrustum::SetTopRadius(float NewTopRadius)
{
    if (NewTopRadius > 0.0f && NewTopRadius != TopRadius)
    {
        TopRadius = NewTopRadius;
        RegenerateMesh();
    }
}

void AFrustum::SetBottomRadius(float NewBottomRadius)
{
    if (NewBottomRadius > 0.0f && NewBottomRadius != BottomRadius)
    {
        BottomRadius = NewBottomRadius;
        RegenerateMesh();
    }
}

void AFrustum::SetHeight(float NewHeight)
{
    if (NewHeight > 0.0f && NewHeight != Height)
    {
        Height = NewHeight;
        RegenerateMesh();
    }
}

void AFrustum::SetTopSides(int32 NewTopSides)
{
    if (NewTopSides >= 3 && NewTopSides != TopSides)
    {
        TopSides = NewTopSides;
        RegenerateMesh();
    }
}

void AFrustum::SetBottomSides(int32 NewBottomSides)
{
    if (NewBottomSides >= 3 && NewBottomSides != BottomSides)
    {
        BottomSides = NewBottomSides;
        RegenerateMesh();
    }
}

void AFrustum::SetHeightSegments(int32 NewHeightSegments)
{
    if (NewHeightSegments >= 1 && NewHeightSegments != HeightSegments)
    {
        HeightSegments = NewHeightSegments;
        RegenerateMesh();
    }
}

void AFrustum::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && NewBevelRadius != BevelRadius)
    {
        BevelRadius = NewBevelRadius;
        RegenerateMesh();
    }
}

void AFrustum::SetBevelSegments(int32 NewBevelSegments)
{
    if (NewBevelSegments >= 1 && NewBevelSegments != BevelSegments)
    {
        BevelSegments = NewBevelSegments;
        RegenerateMesh();
    }
}

void AFrustum::SetBendAmount(float NewBendAmount)
{
    if (NewBendAmount >= -1.0f && NewBendAmount <= 1.0f && NewBendAmount != BendAmount)
    {
        BendAmount = NewBendAmount;
        RegenerateMesh();
    }
}

void AFrustum::SetMinBendRadius(float NewMinBendRadius)
{
    if (NewMinBendRadius >= 0.0f && NewMinBendRadius != MinBendRadius)
    {
        MinBendRadius = NewMinBendRadius;
        RegenerateMesh();
    }
}

void AFrustum::SetArcAngle(float NewArcAngle)
{
    if (NewArcAngle > 0.0f && NewArcAngle <= 360.0f && NewArcAngle != ArcAngle)
    {
        ArcAngle = NewArcAngle;
        RegenerateMesh();
    }
}