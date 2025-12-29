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
        // 只有在数据极度异常（如负数）时才报错，
        // 常规的半径过大问题已经在 Setter 中被自动修复了。
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
    // 基础验证：尺寸必须大于0，且段数合法
    // 注意：我们将半径的强校验移到了 Setter 中自动处理，
    // 这里只做最后的底线检查，允许半径暂时等于0或极小。
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
    // 1. 基础合法性过滤
    if (NewSize.X <= 0.0f || NewSize.Y <= 0.0f || NewSize.Z <= 0.0f || NewSize.Equals(Size))
    {
        return;
    }

    // 2. 智能适配逻辑
    // 计算新尺寸下，理论上允许的最大倒角半径（最小边的一半）
    float MinDim = FMath::Min3(NewSize.X, NewSize.Y, NewSize.Z);
    float MaxAllowedRadius = MinDim * 0.5f;

    // 留一点微小的余量，防止浮点数精度问题导致重合
    MaxAllowedRadius = FMath::Max(0.0f, MaxAllowedRadius - KINDA_SMALL_NUMBER);

    // 如果当前的倒角半径对于新尺寸来说太大了，就自动把它缩小
    // 这样用户就可以随意把盒子改小，而不会被倒角卡住
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
    // 1. 计算当前尺寸允许的最大半径
    float MinHalfSize = FMath::Min3(Size.X, Size.Y, Size.Z) * 0.5f;
    float MaxAllowed = FMath::Max(0.0f, MinHalfSize - KINDA_SMALL_NUMBER);

    // 2. 钳制输入值
    // 如果用户拖动超过了极限，就停在极限值，而不是拒绝操作
    float ClampedRadius = FMath::Clamp(NewBevelRadius, 0.0f, MaxAllowed);

    // 保留两位小数精度（可选，防止UI抖动）
    // ClampedRadius = FMath::RoundToFloat(ClampedRadius * 100.0f) / 100.0f;

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