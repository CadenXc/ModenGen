// Copyright (c) 2024. All rights reserved.

#include "HollowPrism.h"
#include "HollowPrismBuilder.h"
#include "ModelGenMeshData.h"

AHollowPrism::AHollowPrism()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AHollowPrism::GenerateMesh()
{
    if (!IsValid())
    {
        return;
    }

    FHollowPrismBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    }
}

void AHollowPrism::RegenerateMeshBlueprint()
{
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->ClearAllMeshSections();
        GenerateMesh();
    }
}

bool AHollowPrism::IsValid() const
{
    return InnerRadius > 0.0f && OuterRadius > 0.0f && Height > 0.0f &&
           InnerRadius < OuterRadius &&
           InnerSides >= 3 && OuterSides >= 3 &&
           ArcAngle > 0.0f && ArcAngle <= 360.0f &&
           BevelRadius >= 0.0f && BevelSegments >= 1;
}

float AHollowPrism::GetWallThickness() const
{
    return OuterRadius - InnerRadius;
}

bool AHollowPrism::IsFullCircle() const
{
    return FMath::IsNearlyEqual(ArcAngle, 360.0f, 0.1f);
}

int32 AHollowPrism::CalculateVertexCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    const int32 BaseVertices = InnerSides + OuterSides;
    
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        BevelVertices = (InnerSides + OuterSides) * BevelSegments * 2;
    }
    
    int32 EndCapVertices = 0;
    if (!IsFullCircle())
    {
        EndCapVertices = InnerSides + OuterSides;
    }

    return BaseVertices + BevelVertices + EndCapVertices;
}

int32 AHollowPrism::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    const int32 BaseTriangles = InnerSides + OuterSides;
    
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangles = (InnerSides + OuterSides) * BevelSegments * 2;
    }
    
    int32 EndCapTriangles = 0;
    if (!IsFullCircle())
    {
        EndCapTriangles = InnerSides + OuterSides;
    }

    return BaseTriangles + BevelTriangles + EndCapTriangles;
}

bool AHollowPrism::operator==(const AHollowPrism& Other) const
{
    return InnerRadius == Other.InnerRadius &&
           OuterRadius == Other.OuterRadius &&
           Height == Other.Height &&
           InnerSides == Other.InnerSides &&
           OuterSides == Other.OuterSides &&
           ArcAngle == Other.ArcAngle &&
           BevelRadius == Other.BevelRadius &&
           BevelSegments == Other.BevelSegments;
}

bool AHollowPrism::operator!=(const AHollowPrism& Other) const
{
    return !(*this == Other);
}

// 设置参数函数实现
void AHollowPrism::SetInnerRadius(float NewInnerRadius)
{
    if (NewInnerRadius > 0.0f && NewInnerRadius < OuterRadius && NewInnerRadius != InnerRadius)
    {
        InnerRadius = NewInnerRadius;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AHollowPrism::SetOuterRadius(float NewOuterRadius)
{
    if (NewOuterRadius > 0.0f && NewOuterRadius > InnerRadius && NewOuterRadius != OuterRadius)
    {
        OuterRadius = NewOuterRadius;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AHollowPrism::SetHeight(float NewHeight)
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

void AHollowPrism::SetOuterSides(int32 NewOuterSides)
{
    if (NewOuterSides >= 3 && NewOuterSides <= 100 && NewOuterSides != OuterSides)
    {
        OuterSides = NewOuterSides;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AHollowPrism::SetInnerSides(int32 NewInnerSides)
{
    if (NewInnerSides >= 3 && NewInnerSides <= 100 && NewInnerSides != InnerSides)
    {
        InnerSides = NewInnerSides;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}

void AHollowPrism::SetArcAngle(float NewArcAngle)
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

void AHollowPrism::SetBevelRadius(float NewBevelRadius)
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

void AHollowPrism::SetBevelSegments(int32 NewBevelSegments)
{
    if (NewBevelSegments >= 1 && NewBevelSegments <= 20 && NewBevelSegments != BevelSegments)
    {
        BevelSegments = NewBevelSegments;
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
}