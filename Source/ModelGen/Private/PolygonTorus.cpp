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

void APolygonTorus::SetMaterial(UMaterialInterface* NewMaterial)
{
    // 调用父类的方法设置材质
    Super::SetMaterial(NewMaterial);
}

bool APolygonTorus::IsValid() const
{
    const bool bValidMajorRadius = MajorRadius > 0.0f;
    const bool bValidMinorRadius = MinorRadius > 0.0f && MinorRadius <= MajorRadius * 0.9f;
    const bool bValidMajorSegments = MajorSegments >= 3 && MajorSegments <= 256;
    const bool bValidMinorSegments = MinorSegments >= 3 && MinorSegments <= 256;
    const bool bValidTorusAngle = TorusAngle >= 1.0f && TorusAngle <= 360.0f;
    
    return bValidMajorRadius && bValidMinorRadius && bValidMajorSegments && 
           bValidMinorSegments && bValidTorusAngle;
}

int32 APolygonTorus::CalculateVertexCountEstimate() const
{
    int32 BaseVertexCount = MajorSegments * MinorSegments;
    
    int32 CapVertexCount = 0;
    if (TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CapVertexCount = MinorSegments * 2;
    }
    
    return BaseVertexCount + CapVertexCount;
}

int32 APolygonTorus::CalculateTriangleCountEstimate() const
{
    int32 BaseTriangleCount = MajorSegments * MinorSegments * 2;
    
    int32 CapTriangleCount = 0;
    if (TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CapTriangleCount = MinorSegments * 2;
    }
    
    return BaseTriangleCount + CapTriangleCount;
}
