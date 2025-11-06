// Copyright (c) 2024. All rights reserved.

#include "BevelCube.h"
#include "ModelGenMeshData.h"
#include "BevelCubeBuilder.h"


ABevelCube::ABevelCube()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ABevelCube::GenerateMesh()
{
    TryGenerateMeshInternal();
}

bool ABevelCube::TryGenerateMeshInternal()
{
    if (!IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BevelCube::TryGenerateMeshInternal - 参数验证失败"));
        return false;
    }

    FBevelCubeBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData))
    {
        UE_LOG(LogTemp, Error, TEXT("BevelCube::TryGenerateMeshInternal - Builder.Generate 失败"));
        return false;
    }

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BevelCube::TryGenerateMeshInternal - 生成的网格数据无效"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

bool ABevelCube::IsValid() const
{
    return CubeSize > 0.0f && 
           BevelRadius >= 0.0f && BevelRadius < GetHalfSize() && 
           BevelSegments >= 0 && BevelSegments <= 10;
}

int32 ABevelCube::GetVertexCount() const
{
    if (BevelSegments == 0)
    {
        return 24;
    }
    return 24 + 24 * (BevelSegments + 1) + 4 * (BevelSegments + 1) * (BevelSegments + 1);
}

int32 ABevelCube::GetTriangleCount() const
{
    if (BevelSegments == 0)
    {
        return 12;
    }
    return 12 + 24 * BevelSegments + 8 * BevelSegments * BevelSegments;
}

void ABevelCube::SetCubeSize(float NewCubeSize)
{
    if (NewCubeSize > 0.0f && NewCubeSize != CubeSize)
    {
        float OldCubeSize = CubeSize;
        CubeSize = NewCubeSize;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                CubeSize = OldCubeSize;
                UE_LOG(LogTemp, Warning, TEXT("SetCubeSize: 网格生成失败，参数已恢复为 %f"), OldCubeSize);
            }
        }
    }
}

void ABevelCube::SetBevelRadius(float NewBevelRadius)
{
    if (NewBevelRadius >= 0.0f && NewBevelRadius < GetHalfSize() && NewBevelRadius != BevelRadius)
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

void ABevelCube::SetBevelSegments(int32 NewBevelSegments)
{
    if (NewBevelSegments >= 0 && NewBevelSegments <= 10 && NewBevelSegments != BevelSegments)
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
