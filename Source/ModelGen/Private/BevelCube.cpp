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
        return false;
    }

    FBevelCubeBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData))
    {
        return false;
    }

    if (!MeshData.IsValid())
    {
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

bool ABevelCube::IsValid() const
{
    return Size.X > 0.0f && Size.Y > 0.0f && Size.Z > 0.0f &&
        BevelRadius >= 0.0f &&
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

void ABevelCube::SetSize(FVector NewSize)
{
    if (NewSize.X <= 0.0f || NewSize.Y <= 0.0f || NewSize.Z <= 0.0f || NewSize.Equals(Size))
    {
        return;
    }

    float MinDim = FMath::Min3(NewSize.X, NewSize.Y, NewSize.Z);
    float MaxAllowedRadius = MinDim * 0.5f;

    MaxAllowedRadius = FMath::Max(0.0f, MaxAllowedRadius - KINDA_SMALL_NUMBER);

    if (BevelRadius > MaxAllowedRadius)
    {
        BevelRadius = MaxAllowedRadius;
    }

    // 3. 应用并生成
    Size = NewSize;

    if (ProceduralMeshComponent)
    {
        TryGenerateMeshInternal();
    }
}

void ABevelCube::SetBevelRadius(float NewBevelRadius)
{
    float MinHalfSize = FMath::Min3(Size.X, Size.Y, Size.Z) * 0.5f;
    float MaxAllowed = FMath::Max(0.0f, MinHalfSize - KINDA_SMALL_NUMBER);

    float ClampedRadius = FMath::Clamp(NewBevelRadius, 0.0f, MaxAllowed);


    if (!FMath::IsNearlyEqual(ClampedRadius, BevelRadius))
    {
        BevelRadius = ClampedRadius;

        if (ProceduralMeshComponent)
        {
            TryGenerateMeshInternal();
        }
    }
}

void ABevelCube::SetBevelSegments(int32 NewBevelSegments)
{
    if (NewBevelSegments >= 0 && NewBevelSegments <= 10 && NewBevelSegments != BevelSegments)
    {
        BevelSegments = NewBevelSegments;

        if (ProceduralMeshComponent)
        {
            TryGenerateMeshInternal();
        }
    }
}