// Copyright (c) 2024. All rights reserved.

#include "PolygonTorus.h"
#include "PolygonTorusBuilder.h"
#include "ModelGenMeshData.h"

APolygonTorus::APolygonTorus()
{
    PrimaryActorTick.bCanEverTick = false;
}

void APolygonTorus::GenerateMesh()
{
    TryGenerateMeshInternal();
}

bool APolygonTorus::TryGenerateMeshInternal()
{
    if (!IsValid())
    {
        return false;
    }

    FPolygonTorusBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData))
    {
        return false;
    }

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("APolygonTorus::TryGenerateMeshInternal - 生成的网格数据无效"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

bool APolygonTorus::IsValid() const
{
    return MajorRadius > 0.0f &&
           MinorRadius > 0.0f && MinorRadius <= MajorRadius * 0.9f &&
           MajorSegments >= 3 && MajorSegments <= 25 &&
           MinorSegments >= 3 && MinorSegments <= 25 &&
           TorusAngle >= 0.0f && TorusAngle <= 360.0f;
}

int32 APolygonTorus::CalculateVertexCountEstimate() const
{
    const int32 BaseVertexCount = MajorSegments * MinorSegments;
    const int32 CapVertexCount = (TorusAngle < 360.0f - KINDA_SMALL_NUMBER) ? MinorSegments * 2 : 0;
    
    return BaseVertexCount + CapVertexCount;
}

int32 APolygonTorus::CalculateTriangleCountEstimate() const
{
    const int32 BaseTriangleCount = MajorSegments * MinorSegments * 2;
    const int32 CapTriangleCount = (TorusAngle < 360.0f - KINDA_SMALL_NUMBER) ? MinorSegments * 2 : 0;
    
    return BaseTriangleCount + CapTriangleCount;
}

void APolygonTorus::SetMajorRadius(float NewMajorRadius)
{
    if (NewMajorRadius > 0.0f && !FMath::IsNearlyEqual(NewMajorRadius, MajorRadius))
    {
        float OldMajorRadius = MajorRadius;
        MajorRadius = NewMajorRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                MajorRadius = OldMajorRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetMajorRadius: 网格生成失败，参数已恢复为 %f"), OldMajorRadius);
            }
        }
    }
}

void APolygonTorus::SetMinorRadius(float NewMinorRadius)
{
    if (NewMinorRadius > 0.0f && NewMinorRadius <= MajorRadius * 0.9f && !FMath::IsNearlyEqual(NewMinorRadius, MinorRadius))
    {
        float OldMinorRadius = MinorRadius;
        MinorRadius = NewMinorRadius;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                MinorRadius = OldMinorRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetMinorRadius: 网格生成失败，参数已恢复为 %f"), OldMinorRadius);
            }
        }
    }
}

void APolygonTorus::SetMajorSegments(int32 NewMajorSegments)
{
    if (NewMajorSegments >= 3 && NewMajorSegments <= 25 && NewMajorSegments != MajorSegments)
    {
        int32 OldMajorSegments = MajorSegments;
        MajorSegments = NewMajorSegments;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                MajorSegments = OldMajorSegments;
                UE_LOG(LogTemp, Warning, TEXT("SetMajorSegments: 网格生成失败，参数已恢复为 %d"), OldMajorSegments);
            }
        }
    }
}

void APolygonTorus::SetMinorSegments(int32 NewMinorSegments)
{
    if (NewMinorSegments >= 3 && NewMinorSegments <= 25 && NewMinorSegments != MinorSegments)
    {
        int32 OldMinorSegments = MinorSegments;
        MinorSegments = NewMinorSegments;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                MinorSegments = OldMinorSegments;
                UE_LOG(LogTemp, Warning, TEXT("SetMinorSegments: 网格生成失败，参数已恢复为 %d"), OldMinorSegments);
            }
        }
    }
}

void APolygonTorus::SetTorusAngle(float NewTorusAngle)
{
    if (NewTorusAngle >= 0.0f && NewTorusAngle <= 360.0f && !FMath::IsNearlyEqual(NewTorusAngle, TorusAngle))
    {
        float OldTorusAngle = TorusAngle;
        TorusAngle = NewTorusAngle;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                TorusAngle = OldTorusAngle;
                UE_LOG(LogTemp, Warning, TEXT("SetTorusAngle: 网格生成失败，参数已恢复为 %f"), OldTorusAngle);
            }
        }
    }
}

void APolygonTorus::SetSmoothCrossSection(bool bNewSmoothCrossSection)
{
    if (bNewSmoothCrossSection != bSmoothCrossSection)
    {
        bool OldSmoothCrossSection = bSmoothCrossSection;
        bSmoothCrossSection = bNewSmoothCrossSection;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                bSmoothCrossSection = OldSmoothCrossSection;
                UE_LOG(LogTemp, Warning, TEXT("SetSmoothCrossSection: 网格生成失败，参数已恢复"));
            }
        }
    }
}

void APolygonTorus::SetSmoothVerticalSection(bool bNewSmoothVerticalSection)
{
    if (bNewSmoothVerticalSection != bSmoothVerticalSection)
    {
        bool OldSmoothVerticalSection = bSmoothVerticalSection;
        bSmoothVerticalSection = bNewSmoothVerticalSection;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                bSmoothVerticalSection = OldSmoothVerticalSection;
                UE_LOG(LogTemp, Warning, TEXT("SetSmoothVerticalSection: 网格生成失败，参数已恢复"));
            }
        }
    }
}
