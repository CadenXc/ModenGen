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
    TryGenerateMeshInternal();
}

bool AHollowPrism::TryGenerateMeshInternal()
{
    if (!IsValid())
    {
        return false;
    }

    FHollowPrismBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData))
    {
        return false;
    }

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("AHollowPrism::TryGenerateMeshInternal - 生成的网格数据无效"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

void AHollowPrism::RegenerateMeshBlueprint()
{
    if (ProceduralMeshComponent)
    {
        TryGenerateMeshInternal();
    }
}

bool AHollowPrism::IsValid() const
{
    return InnerRadius > 0.0f && OuterRadius > 0.0f && Height > 0.0f &&
           InnerRadius < OuterRadius &&
           InnerSides >= 3 && OuterSides >= 3 &&
           ArcAngle > 0.0f && ArcAngle <= 360.0f &&
           BevelRadius >= 0.0f && BevelSegments >= 0;
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
    if (BevelRadius > 0.0f && BevelSegments > 0)
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
    if (BevelRadius > 0.0f && BevelSegments > 0)
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

void AHollowPrism::SetInnerRadius(float NewInnerRadius)
{
    if (NewInnerRadius >= OuterRadius)
    {
        return;
    }
    if (NewInnerRadius > 0.0f && NewInnerRadius < OuterRadius && NewInnerRadius != InnerRadius)
    {
        float OldInnerRadius = InnerRadius;
        InnerRadius = NewInnerRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                InnerRadius = OldInnerRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetInnerRadius: 网格生成失败，参数已恢复为 %f"), OldInnerRadius);
            }
        }
    }
}

void AHollowPrism::SetOuterRadius(float NewOuterRadius)
{
    if (NewOuterRadius > 0.0f && NewOuterRadius > InnerRadius && NewOuterRadius != OuterRadius)
    {
        float OldOuterRadius = OuterRadius;
        float OldInnerRadius = InnerRadius;
        
        OuterRadius = NewOuterRadius;
        if (OuterRadius < InnerRadius)
        {
            InnerRadius = OuterRadius - KINDA_SMALL_NUMBER;
        }
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                OuterRadius = OldOuterRadius;
                InnerRadius = OldInnerRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetOuterRadius: 网格生成失败，参数已恢复"));
            }
        }
    }
}

void AHollowPrism::SetHeight(float NewHeight)
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

void AHollowPrism::SetOuterSides(int32 NewOuterSides)
{
    if (NewOuterSides >= 3 && NewOuterSides <= 100 && NewOuterSides != OuterSides)
    {
        int32 OldOuterSides = OuterSides;
        int32 OldInnerSides = InnerSides;
        
        OuterSides = NewOuterSides;
        if (OuterSides < InnerSides)
        {
            InnerSides = OuterSides;
        }

        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                OuterSides = OldOuterSides;
                InnerSides = OldInnerSides;
                UE_LOG(LogTemp, Warning, TEXT("SetOuterSides: 网格生成失败，参数已恢复"));
            }
        }
    }
}

void AHollowPrism::SetInnerSides(int32 NewInnerSides)
{
    if (NewInnerSides > OuterSides)
    {
        return;
    }

    if (NewInnerSides >= 3 && NewInnerSides <= 100 && NewInnerSides != InnerSides)
    {
        int32 OldInnerSides = InnerSides;
        InnerSides = NewInnerSides;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                InnerSides = OldInnerSides;
                UE_LOG(LogTemp, Warning, TEXT("SetInnerSides: 网格生成失败，参数已恢复为 %d"), OldInnerSides);
            }
        }
    }
}

void AHollowPrism::SetArcAngle(float NewArcAngle)
{
    if (NewArcAngle > 0.0f && NewArcAngle <= 360.0f && NewArcAngle != ArcAngle)
    {
        float OldArcAngle = ArcAngle;
        ArcAngle = NewArcAngle;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                ArcAngle = OldArcAngle;
                UE_LOG(LogTemp, Warning, TEXT("SetArcAngle: 网格生成失败，参数已恢复为 %f"), OldArcAngle);
            }
        }
    }
}

void AHollowPrism::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && NewBevelRadius != BevelRadius)
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

void AHollowPrism::SetBevelSegments(int32 NewBevelSegments)
{
    if (NewBevelSegments >= 0 && NewBevelSegments <= 20 && NewBevelSegments != BevelSegments)
    {
        int32 OldBevelSegments = BevelSegments;
        BevelSegments = NewBevelSegments;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                BevelSegments = OldBevelSegments;
                UE_LOG(LogTemp, Warning, TEXT("SetBevelSegments: 网格生成失败，参数已恢复为 %d"), OldBevelSegments);
            }
        }
    }
}