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
    TryGenerateMeshInternal();
}

bool AFrustum::TryGenerateMeshInternal()
{
    if (!IsValid())
    {
        return false;
    }

    FFrustumBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData))
    {
        return false;
    }

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("AFrustum::TryGenerateMeshInternal - 生成的网格数据无效"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

bool AFrustum::IsValid() const
{
    return TopRadius > 0.0f && BottomRadius > 0.0f && Height > 0.0f &&
           TopSides >= 3 && TopSides <= 25 && BottomSides >= 3 && BottomSides <= 25 && HeightSegments >= 0 && HeightSegments <= 12 &&
           BevelRadius >= 0.0f && 
           MinBendRadius >= 0.0f && ArcAngle > 0.0f && ArcAngle <= 360.0f;
}

int32 AFrustum::CalculateVertexCountEstimate() const
{
    if (!IsValid()) return 0;

    const int32 MaxSides = FMath::Max(TopSides, BottomSides);
    const int32 BaseVertices = TopSides + BottomSides;
    // 中间分段数 = HeightSegments，实际分段数 = HeightSegments + 1（0表示只有顶部和底部）
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
    // 实际分段数 = HeightSegments + 1（0表示只有顶部和底部）
    const int32 SideTriangles = HeightSegments * MaxSides * 2;  // 中间分段数 = HeightSegments
    
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangles = (TopSides + BottomSides + HeightSegments * MaxSides) * 2;
    }

    return BaseTriangles + SideTriangles + BevelTriangles;
}

void AFrustum::SetTopRadius(float NewTopRadius)
{
    if (NewTopRadius > 0.0f && !FMath::IsNearlyEqual(NewTopRadius, TopRadius))
    {
        float OldTopRadius = TopRadius;
        TopRadius = NewTopRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                TopRadius = OldTopRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetTopRadius: 网格生成失败，参数已恢复为 %f"), OldTopRadius);
            }
        }
    }
}

void AFrustum::SetBottomRadius(float NewBottomRadius)
{
    if (NewBottomRadius > 0.0f && !FMath::IsNearlyEqual(NewBottomRadius, BottomRadius))
    {
        float OldBottomRadius = BottomRadius;
        BottomRadius = NewBottomRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                BottomRadius = OldBottomRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetBottomRadius: 网格生成失败，参数已恢复为 %f"), OldBottomRadius);
            }
        }
    }
}

void AFrustum::SetHeight(float NewHeight)
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
                UE_LOG(LogTemp, Warning, TEXT("SetHeight: 网格生成失败，参数已恢复为 %f"), OldHeight);
            }
        }
    }
}

void AFrustum::SetTopSides(int32 NewTopSides)
{
	if (NewTopSides > BottomSides)
	{
		return;
	}
    
    if (NewTopSides >= 3 && NewTopSides <= 25 && NewTopSides != TopSides)
    {
        int32 OldTopSides = TopSides;
        TopSides = NewTopSides;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                TopSides = OldTopSides;
                UE_LOG(LogTemp, Warning, TEXT("SetTopSides: 网格生成失败，参数已恢复为 %d"), OldTopSides);
            }
        }
    }
}

void AFrustum::SetBottomSides(int32 NewBottomSides)
{
    if (NewBottomSides >= 3 && NewBottomSides <= 25 && NewBottomSides != BottomSides)
    {
        int32 OldBottomSides = BottomSides;
        int32 OldTopSides = TopSides;
        
        BottomSides = NewBottomSides;
        if (BottomSides < TopSides)
        {
            TopSides = BottomSides;
        }
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                BottomSides = OldBottomSides;
                TopSides = OldTopSides;
                UE_LOG(LogTemp, Warning, TEXT("SetBottomSides: 网格生成失败，参数已恢复"));
            }
        }
    }
}

void AFrustum::SetHeightSegments(int32 NewHeightSegments)
{
    if (NewHeightSegments >= 0 && NewHeightSegments <= 12 && NewHeightSegments != HeightSegments)
    {
        int32 OldHeightSegments = HeightSegments;
        HeightSegments = NewHeightSegments;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                HeightSegments = OldHeightSegments;
                UE_LOG(LogTemp, Warning, TEXT("SetHeightSegments: 网格生成失败，参数已恢复为 %d"), OldHeightSegments);
            }
        }
    }
}

void AFrustum::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && !FMath::IsNearlyEqual(NewBevelRadius, BevelRadius))
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

void AFrustum::SetBendAmount(float NewBendAmount)
{
    if (NewBendAmount >= -1.0f && NewBendAmount <= 1.0f && !FMath::IsNearlyEqual(NewBendAmount, BendAmount))
    {
        float OldBendAmount = BendAmount;
        BendAmount = NewBendAmount;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                BendAmount = OldBendAmount;
                UE_LOG(LogTemp, Warning, TEXT("SetBendAmount: 网格生成失败，参数已恢复为 %f"), OldBendAmount);
            }
        }
    }
}

void AFrustum::SetMinBendRadius(float NewMinBendRadius)
{
    if (NewMinBendRadius >= 0.0f && !FMath::IsNearlyEqual(NewMinBendRadius, MinBendRadius))
    {
        float OldMinBendRadius = MinBendRadius;
        MinBendRadius = NewMinBendRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                MinBendRadius = OldMinBendRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetMinBendRadius: 网格生成失败，参数已恢复为 %f"), OldMinBendRadius);
            }
        }
    }
}

void AFrustum::SetArcAngle(float NewArcAngle)
{
    if (NewArcAngle > 0.0f && NewArcAngle <= 360.0f && !FMath::IsNearlyEqual(NewArcAngle, ArcAngle))
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