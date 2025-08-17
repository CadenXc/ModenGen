// Copyright (c) 2024. All rights reserved.

#include "BevelCube.h"
#include "BevelCubeBuilder.h"
#include "ModelGenMeshData.h"

ABevelCube::ABevelCube()
{
    PrimaryActorTick.bCanEverTick = false;
}





void ABevelCube::GenerateMesh()
{
    if (!IsValid())
    {
        return;
    }

    FBevelCubeBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData) && MeshData.IsValid())
    {
        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    }
}

void ABevelCube::GenerateBeveledCube(float InSize, float InBevelSize, int32 InSections)
{
    Size = InSize;
    BevelRadius = InBevelSize;
    BevelSegments = InSections;
    
    // 调用父类的方法重新生成网格
    RegenerateMesh();
}

bool ABevelCube::IsValid() const
{
    const bool bValidSize = Size > 0.0f;
    const bool bValidBevelSize = BevelRadius >= 0.0f && BevelRadius < GetHalfSize();
    const bool bValidSections = BevelSegments >= 1 && BevelSegments <= 10;
    
    return bValidSize && bValidBevelSize && bValidSections;
}

int32 ABevelCube::GetVertexCount() const
{
    const int32 BaseVertexCount = 24;
    const int32 EdgeBevelVertexCount = 12 * (BevelSegments + 1) * 2;
    const int32 CornerBevelVertexCount = 8 * (BevelSegments + 1) * (BevelSegments + 1) / 2;
    
    return BaseVertexCount + EdgeBevelVertexCount + CornerBevelVertexCount;
}

int32 ABevelCube::GetTriangleCount() const
{
    const int32 BaseTriangleCount = 12;
    const int32 EdgeBevelTriangleCount = 12 * BevelSegments * 2;
    const int32 CornerBevelTriangleCount = 8 * BevelSegments * BevelSegments * 2;
    
    return BaseTriangleCount + EdgeBevelTriangleCount + CornerBevelTriangleCount;
}