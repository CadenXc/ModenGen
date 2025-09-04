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
    if (!IsValid()) return;

    FBevelCubeBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    }
}

bool ABevelCube::IsValid() const
{
    return CubeSize > 0.0f && 
           BevelRadius >= 0.0f && BevelRadius < GetHalfSize() && 
           BevelSegments >= 1 && BevelSegments <= 10;
}

int32 ABevelCube::GetVertexCount() const
{
    // 简化的顶点数量预估：基础立方体 + 边缘倒角 + 角落倒角
    return 24 + 24 * (BevelSegments + 1) + 4 * (BevelSegments + 1) * (BevelSegments + 1);
}

int32 ABevelCube::GetTriangleCount() const
{
    // 简化的三角形数量预估：基础立方体 + 边缘倒角 + 角落倒角
    return 12 + 24 * BevelSegments + 8 * BevelSegments * BevelSegments;
}

// 设置参数函数实现
void ABevelCube::SetCubeSize(float NewCubeSize)
{
    if (NewCubeSize > 0.0f && NewCubeSize != CubeSize)
    {
        CubeSize = NewCubeSize;
        RegenerateMesh();
    }
}

void ABevelCube::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && NewBevelRadius < GetHalfSize() && NewBevelRadius != BevelRadius)
    {
        BevelRadius = NewBevelRadius;
        RegenerateMesh();
    }
}

void ABevelCube::SetBevelSegments(int32 NewBevelSegments)
{
    if (NewBevelSegments >= 1 && NewBevelSegments <= 10 && NewBevelSegments != BevelSegments)
    {
        BevelSegments = NewBevelSegments;
        RegenerateMesh();
    }
}