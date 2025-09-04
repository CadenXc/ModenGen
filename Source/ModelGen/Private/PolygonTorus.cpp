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
    if (!IsValid())
    {
        return;
    }
    
    FPolygonTorusBuilder Builder(*this);
    FModelGenMeshData MeshData;
    
    if (Builder.Generate(MeshData))
    {
        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    }
}


bool APolygonTorus::IsValid() const
{
    return MajorRadius > 0.0f &&
           MinorRadius > 0.0f && MinorRadius <= MajorRadius * 0.9f &&
           MajorSegments >= 3 && MajorSegments <= 256 &&
           MinorSegments >= 3 && MinorSegments <= 256 &&
           TorusAngle >= 1.0f && TorusAngle <= 360.0f;
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

// 设置参数函数实现
void APolygonTorus::SetMajorRadius(float NewMajorRadius)
{
    if (NewMajorRadius > 0.0f && NewMajorRadius != MajorRadius)
    {
        MajorRadius = NewMajorRadius;
        RegenerateMesh();
    }
}

void APolygonTorus::SetMinorRadius(float NewMinorRadius)
{
    if (NewMinorRadius > 0.0f && NewMinorRadius <= MajorRadius * 0.9f && NewMinorRadius != MinorRadius)
    {
        MinorRadius = NewMinorRadius;
        RegenerateMesh();
    }
}

void APolygonTorus::SetMajorSegments(int32 NewMajorSegments)
{
    if (NewMajorSegments >= 3 && NewMajorSegments <= 256 && NewMajorSegments != MajorSegments)
    {
        MajorSegments = NewMajorSegments;
        RegenerateMesh();
    }
}

void APolygonTorus::SetMinorSegments(int32 NewMinorSegments)
{
    if (NewMinorSegments >= 3 && NewMinorSegments <= 256 && NewMinorSegments != MinorSegments)
    {
        MinorSegments = NewMinorSegments;
        RegenerateMesh();
    }
}

void APolygonTorus::SetTorusAngle(float NewTorusAngle)
{
    if (NewTorusAngle >= 1.0f && NewTorusAngle <= 360.0f && NewTorusAngle != TorusAngle)
    {
        TorusAngle = NewTorusAngle;
        RegenerateMesh();
    }
}

void APolygonTorus::SetSmoothCrossSection(bool bNewSmoothCrossSection)
{
    if (bNewSmoothCrossSection != bSmoothCrossSection)
    {
        bSmoothCrossSection = bNewSmoothCrossSection;
        RegenerateMesh();
    }
}

void APolygonTorus::SetSmoothVerticalSection(bool bNewSmoothVerticalSection)
{
    if (bNewSmoothVerticalSection != bSmoothVerticalSection)
    {
        bSmoothVerticalSection = bNewSmoothVerticalSection;
        RegenerateMesh();
    }
}
