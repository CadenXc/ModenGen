// Copyright (c) 2024. All rights reserved.

#include "HollowPrism.h"
#include "HollowPrismBuilder.h"
#include "ModelGenMeshBuilder.h"

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

    if (Builder.Generate(MeshData) && MeshData.IsValid())
    {
        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    }
}

void AHollowPrism::RegenerateMeshBlueprint()
{
    // 调用父类的 RegenerateMesh 方法
    Super::RegenerateMesh();
}

void AHollowPrism::SetMaterial(UMaterialInterface* NewMaterial)
{
    // 调用父类的方法设置材质
    Super::SetMaterial(NewMaterial);
}

bool AHollowPrism::IsValid() const
{
    if (InnerRadius <= 0.0f || OuterRadius <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    if (InnerRadius >= OuterRadius)
    {
        return false;
    }

    if (InnerSides < 3 || OuterSides < 3)
    {
        return false;
    }

    if (ArcAngle <= 0.0f || ArcAngle > 360.0f)
    {
        return false;
    }

    if (BevelRadius < 0.0f || BevelSegments < 1)
    {
        return false;
    }

    return true;
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

    int32 BaseVertices = InnerSides + OuterSides;
    
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

    int32 BaseTriangles = InnerSides + OuterSides;
    
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
           BevelSegments == Other.BevelSegments &&
           bUseTriangleMethod == Other.bUseTriangleMethod &&
           bFlipNormals == Other.bFlipNormals;
}

bool AHollowPrism::operator!=(const AHollowPrism& Other) const
{
    return !(*this == Other);
}