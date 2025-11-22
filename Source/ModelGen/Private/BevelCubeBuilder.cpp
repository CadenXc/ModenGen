// Copyright (c) 2024. All rights reserved.

#include "BevelCubeBuilder.h"
#include "BevelCube.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"

FBevelCubeBuilder::FBevelCubeBuilder(const ABevelCube& InBevelCube)
    : BevelCube(InBevelCube)
{
    Clear();
}

void FBevelCubeBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    GridCoordinates.Empty();
}

bool FBevelCubeBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!BevelCube.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    HalfSize = BevelCube.GetHalfSize();
    InnerOffset = BevelCube.GetInnerOffset();
    BevelRadius = BevelCube.BevelRadius;
    BevelSegments = BevelCube.BevelSegments;

    bEnableBevel = (BevelSegments > 0) && (BevelRadius > KINDA_SMALL_NUMBER) && (HalfSize > InnerOffset + KINDA_SMALL_NUMBER);

    PrecomputeGrid();

    // 【核心修复】：修正 Top 面 V 轴方向，使其符合右手定则 (U x V = Normal)
    // Top: U(+X) x V(+Y) = Normal(+Z). 之前 V 是 -Y 导致法线向下(-Z)从而渲染反向。
    const TArray<FUnfoldedFace> FacesToGenerate = {
        // Name      Normal      U_Axis (Right)    V_Axis (Down)
        { FVector(0, 1, 0),  FVector(1, 0, 0), FVector(0, 0,-1), TEXT("Front") }, // +Y
        { FVector(0,-1, 0),  FVector(-1, 0, 0), FVector(0, 0,-1), TEXT("Back") },  // -Y
        { FVector(0, 0, 1),  FVector(1, 0, 0), FVector(0, 1, 0), TEXT("Top") },   // +Z (修改处：V改为 +Y)
        { FVector(0, 0,-1),  FVector(1, 0, 0), FVector(0,-1, 0), TEXT("Bottom")}, // -Z
        { FVector(1, 0, 0),  FVector(0,-1, 0), FVector(0, 0,-1), TEXT("Right") }, // +X
        { FVector(-1, 0, 0),  FVector(0, 1, 0), FVector(0, 0,-1), TEXT("Left") }   // -X
    };

    for (const FUnfoldedFace& FaceDef : FacesToGenerate)
    {
        GenerateUnfoldedFace(FaceDef);
    }

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;
    return true;
}

int32 FBevelCubeBuilder::CalculateVertexCountEstimate() const
{
    return BevelCube.GetVertexCount();
}

int32 FBevelCubeBuilder::CalculateTriangleCountEstimate() const
{
    return BevelCube.GetTriangleCount();
}

void FBevelCubeBuilder::PrecomputeGrid()
{
    if (!bEnableBevel)
    {
        GridCoordinates = { -HalfSize, HalfSize };
    }
    else
    {
        const float Range = HalfSize - InnerOffset;
        const float Step = Range / BevelSegments;

        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            GridCoordinates.Add(-HalfSize + i * Step);
        }
        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            GridCoordinates.Add(InnerOffset + i * Step);
        }
    }
}

void FBevelCubeBuilder::GenerateUnfoldedFace(const FUnfoldedFace& FaceDef)
{
    const int32 GridSize = GridCoordinates.Num();
    if (GridSize < 2) return;

    TArray<int32> VertIndices;
    VertIndices.SetNumUninitialized(GridSize * GridSize);

    // 1. 生成顶点
    for (int32 v_idx = 0; v_idx < GridSize; ++v_idx)
    {
        const float local_v = GridCoordinates[v_idx];
        const bool bInMainPlaneV = (FMath::Abs(local_v) <= InnerOffset + KINDA_SMALL_NUMBER);

        for (int32 u_idx = 0; u_idx < GridSize; ++u_idx)
        {
            const float local_u = GridCoordinates[u_idx];
            const bool bInMainPlaneU = (FMath::Abs(local_u) <= InnerOffset + KINDA_SMALL_NUMBER);

            FVector VertexPos;
            FVector Normal;

            if (!bEnableBevel || (bInMainPlaneU && bInMainPlaneV))
            {
                Normal = FaceDef.Normal;
                VertexPos = FaceDef.Normal * HalfSize + FaceDef.U_Axis * local_u + FaceDef.V_Axis * local_v;
            }
            else
            {
                const float ClampedU = FMath::Clamp(local_u, -InnerOffset, InnerOffset);
                const float ClampedV = FMath::Clamp(local_v, -InnerOffset, InnerOffset);

                const FVector InnerPoint = FaceDef.Normal * InnerOffset +
                    FaceDef.U_Axis * ClampedU +
                    FaceDef.V_Axis * ClampedV;

                const FVector OuterPoint = FaceDef.Normal * HalfSize +
                    FaceDef.U_Axis * local_u +
                    FaceDef.V_Axis * local_v;

                FVector Dir = (OuterPoint - InnerPoint);
                if (!Dir.IsNearlyZero())
                {
                    Normal = Dir.GetSafeNormal();
                    VertexPos = InnerPoint + Normal * BevelRadius;
                }
                else
                {
                    Normal = FaceDef.Normal;
                    VertexPos = OuterPoint;
                }
            }

            VertexPos += FVector(0, 0, HalfSize);

            // 【修改前】：依赖物理尺寸和全局缩放，UV值随尺寸变化
            // float WorldU = (local_u + HalfSize);
            // float WorldV = (local_v + HalfSize);
            // FVector2D UV(WorldU * ModelGenConstants::GLOBAL_UV_SCALE, WorldV * ModelGenConstants::GLOBAL_UV_SCALE);

            // 【修改后】：强制归一化到 [0, 1]
            // local_u 和 local_v 的范围是 [-HalfSize, HalfSize]
            // 下面的公式将其映射为 [0, 1]，固定为 1 倍平铺
            float NormalizedU = (local_u + HalfSize) / (2.0f * HalfSize);
            float NormalizedV = (local_v + HalfSize) / (2.0f * HalfSize);
            FVector2D UV(NormalizedU, NormalizedV);

            VertIndices[v_idx * GridSize + u_idx] = AddVertex(VertexPos, Normal, UV);
        }
    }

    // 2. 生成面 (Quads)
    for (int32 v = 0; v < GridSize - 1; ++v)
    {
        for (int32 u = 0; u < GridSize - 1; ++u)
        {
            int32 V00 = VertIndices[v * GridSize + u];
            int32 V10 = VertIndices[v * GridSize + (u + 1)];
            int32 V01 = VertIndices[(v + 1) * GridSize + u];
            int32 V11 = VertIndices[(v + 1) * GridSize + (u + 1)];

            // 【核心修复】：修改连接顺序为 V00 -> V01 -> V11 -> V10
            // 路径: Down(V01) -> Right(V11) -> Up(V10) -> Left(V00)
            // 这在屏幕上是逆时针 (CCW)，即正面
            AddQuad(V00, V01, V11, V10);
        }
    }
}