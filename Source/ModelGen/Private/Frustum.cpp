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
    if (!IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid Frustum parameters: TopRadius=%f, BottomRadius=%f, Height=%f, TopSides=%d, BottomSides=%d"), 
               TopRadius, BottomRadius, Height, TopSides, BottomSides);
        return;
    }

    FFrustumBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        if (!MeshData.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Generated mesh data is invalid"));
            return;
        }

        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate Frustum mesh"));
    }
}

void AFrustum::GenerateFrustum(float InTopRadius, float InBottomRadius, float InHeight, int32 InSides)
{
    TopRadius = InTopRadius;
    BottomRadius = InBottomRadius;
    Height = InHeight;
    TopSides = InSides;
    BottomSides = InSides;
    
    // 调用父类的方法重新生成网格
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
    
    // 调用父类的方法重新生成网格
    RegenerateMesh();
}

void AFrustum::RegenerateMeshBlueprint()
{
    // 调用父类的方法重新生成网格
    RegenerateMesh();
}

void AFrustum::SetMaterial(UMaterialInterface* NewMaterial)
{
    // 调用父类的方法设置材质
    Super::SetMaterial(NewMaterial);
}

bool AFrustum::IsValid() const
{
    if (TopRadius <= 0.0f || BottomRadius <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    if (TopSides < 3 || BottomSides < 3 || HeightSegments < 1)
    {
        return false;
    }

    if (BevelRadius < 0.0f || BevelSegments < 1)
    {
        return false;
    }

    if (MinBendRadius < 0.0f)
    {
        return false;
    }

    if (ArcAngle <= 0.0f || ArcAngle > 360.0f)
    {
        return false;
    }

    return true;
}

int32 AFrustum::CalculateVertexCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    int32 BaseVertices = TopSides + BottomSides;
    int32 MaxSides = FMath::Max(TopSides, BottomSides);
    int32 SideVertices = HeightSegments * MaxSides;
    
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        BevelVertices = TopSides * BevelSegments + BottomSides * BevelSegments;
        BevelVertices += HeightSegments * MaxSides * BevelSegments;
    }

    return BaseVertices + SideVertices + BevelVertices;
}

int32 AFrustum::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    int32 BaseTriangles = TopSides + BottomSides;
    int32 MaxSides = FMath::Max(TopSides, BottomSides);
    int32 SideTriangles = HeightSegments * MaxSides * 2;
    
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangles = TopSides * BevelSegments * 2 + BottomSides * BevelSegments * 2;
        BevelTriangles += HeightSegments * MaxSides * BevelSegments * 2;
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