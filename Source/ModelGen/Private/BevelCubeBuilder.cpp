// Copyright (c) 2024. All rights reserved.

#include "BevelCubeBuilder.h"
#include "BevelCube.h"
#include "ModelGenMeshData.h"

FBevelCubeBuilder::FBevelCubeBuilder(const ABevelCube& InBevelCube)
    : BevelCube(InBevelCube)
{
    Clear();

    // 缓存来自BevelCube的几何参数
    HalfSize = BevelCube.GetHalfSize();
    InnerOffset = BevelCube.GetInnerOffset();
    BevelRadius = BevelCube.BevelRadius;
    BevelSegments = BevelCube.BevelSegments;
}

bool FBevelCubeBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!BevelCube.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    // 定义6个面的展开信息，构成一个4x4的UV网格布局（十字架形）
    //      [Top]
    // [Left][Front][Right][Back]
    //      [Bottom]
    TArray<FUnfoldedFace> FacesToGenerate = {
        // U/V轴的定义确保了在UV展开图中所有面的方向是一致的
        { FVector(0, 1, 0),  FVector(1, 0, 0), FVector(0, 0, -1), FIntPoint(1, 1), TEXT("Front") }, // +Y
        { FVector(0, -1, 0), FVector(-1, 0, 0),FVector(0, 0, -1), FIntPoint(3, 1), TEXT("Back") },  // -Y
        { FVector(0, 0, 1),  FVector(1, 0, 0), FVector(0, 1, 0),  FIntPoint(1, 0), TEXT("Top") },   // +Z
        { FVector(0, 0, -1), FVector(1, 0, 0), FVector(0, -1, 0), FIntPoint(1, 2), TEXT("Bottom")},// -Z
        { FVector(1, 0, 0),  FVector(0, -1, 0),FVector(0, 0, -1), FIntPoint(2, 1), TEXT("Right") }, // +X
        { FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, 0, -1), FIntPoint(0, 1), TEXT("Left") }  // -X
    };

    // 循环为每个面生成网格
    for (const FUnfoldedFace& FaceDef : FacesToGenerate)
    {
        GenerateUnfoldedFace(FaceDef);
    }

    if (!ValidateGeneratedData())
    {
        return false;
    }

    // 计算正确的切线（用于法线贴图等）
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

void FBevelCubeBuilder::GenerateUnfoldedFace(const FUnfoldedFace& FaceDef)
{
    // 处理特殊情况：无倒角时，只生成主面的4个顶点（一个大正方形）
    TArray<float> UPositions;
    TArray<float> VPositions;
    if (BevelSegments <= 0 || HalfSize <= InnerOffset)
    {
        UPositions = { -HalfSize, HalfSize };
        VPositions = { -HalfSize, HalfSize };
    }
    else
    {
        const float Step = (HalfSize - InnerOffset) / static_cast<float>(BevelSegments);
        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            const float PosU = -HalfSize + static_cast<float>(i) * Step;
            UPositions.Add(PosU);
        }
        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            const float PosU = InnerOffset + static_cast<float>(i) * Step;
            UPositions.Add(PosU);
        }

        // V方向使用相同的非均匀分布
        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            const float PosV = -HalfSize + static_cast<float>(i) * Step;
            VPositions.Add(PosV);
        }
        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            const float PosV = InnerOffset + static_cast<float>(i) * Step;
            VPositions.Add(PosV);
        }
    }

    const int32 NumU = UPositions.Num();
    const int32 NumV = VPositions.Num();

    // 创建一个二维数组来存储生成的顶点索引，方便后续连接成面
    TArray<TArray<int32>> VertexGrid;
    VertexGrid.SetNum(NumV);
    for (int32 i = 0; i < NumV; ++i)
    {
        VertexGrid[i].SetNum(NumU);
    }

    // UV布局计算，假设整个UV空间被划分为4x4的网格
    const FVector2D UVBlockSize(1.0f / 4.0f, 1.0f / 4.0f);
    const FVector2D UVBaseOffset(FaceDef.UV_Grid_Offset.X * UVBlockSize.X, FaceDef.UV_Grid_Offset.Y * UVBlockSize.Y);

    // 遍历非均匀网格上的每个点，计算其 3D位置、法线 和 UV坐标
    for (int32 v_idx = 0; v_idx < NumV; ++v_idx)
    {
        const float local_v = VPositions[v_idx];
        const float v_alpha = (local_v + HalfSize) / (2.0f * HalfSize);

        for (int32 u_idx = 0; u_idx < NumU; ++u_idx)
        {
            const float local_u = UPositions[u_idx];
            const float u_alpha = (local_u + HalfSize) / (2.0f * HalfSize);

            // 计算UV坐标：基于物理位置的alpha，确保纹理均匀分布
            // V方向(v_alpha)需要翻转 (1.0f - ...)，以匹配常见的从上到下的纹理坐标系
            FVector2D UV(u_alpha * UVBlockSize.X + UVBaseOffset.X, (1.0f - v_alpha) * UVBlockSize.Y + UVBaseOffset.Y);

            // 计算核心点 (CorePoint) 和表面法线 (SurfaceNormal)
            FVector SurfaceNormal;
            FVector VertexPos;

            // 判断是否有倒角：BevelRadius > 0 时为软边，BevelRadius = 0 时为硬边
            const bool bHasBevel = BevelRadius > 0.0f;
            const bool bInMainPlane = (FMath::Abs(local_u) <= InnerOffset) && (FMath::Abs(local_v) <= InnerOffset);

            if (bInMainPlane)
            {
                // 主平面区域：始终使用面的原始法线
                SurfaceNormal = FaceDef.Normal;
                VertexPos = FaceDef.Normal * HalfSize + FaceDef.U_Axis * local_u + FaceDef.V_Axis * local_v;
            }
            else
            {
                if (bHasBevel)
                {
                    // 有倒角时：软边，使用平滑法线
                    const FVector InnerPoint = FaceDef.Normal * InnerOffset +
                        FaceDef.U_Axis * FMath::Clamp(local_u, -InnerOffset, InnerOffset) +
                        FaceDef.V_Axis * FMath::Clamp(local_v, -InnerOffset, InnerOffset);

                    const FVector OuterPoint = FaceDef.Normal * HalfSize + FaceDef.U_Axis * local_u + FaceDef.V_Axis * local_v;
                    const FVector Direction = (OuterPoint - InnerPoint).GetSafeNormal();

                    VertexPos = InnerPoint + Direction * BevelRadius;
                    SurfaceNormal = Direction;
                }
                else
                {
                    // 无倒角时：硬边，使用面的原始法线
                    SurfaceNormal = FaceDef.Normal;
                    VertexPos = FaceDef.Normal * HalfSize + FaceDef.U_Axis * local_u + FaceDef.V_Axis * local_v;
                }
            }

            // 添加顶点到网格数据，并记录其索引
            VertexGrid[v_idx][u_idx] = AddVertex(VertexPos, SurfaceNormal, UV);
        }
    }

    // 再次遍历网格，使用存储的顶点索引来生成四边形（两个三角形）
    for (int32 v_idx = 0; v_idx < NumV - 1; ++v_idx)
    {
        for (int32 u_idx = 0; u_idx < NumU - 1; ++u_idx)
        {
            const int32 V00 = VertexGrid[v_idx][u_idx];
            const int32 V10 = VertexGrid[v_idx + 1][u_idx];
            const int32 V01 = VertexGrid[v_idx][u_idx + 1];
            const int32 V11 = VertexGrid[v_idx + 1][u_idx + 1];

            AddQuad(V00, V10, V11, V01); // 注意顶点顺序以保证法线朝外
        }
    }
}