// Copyright (c) 2024. All rights reserved.

#include "BevelCubeBuilder.h"
#include "BevelCube.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"

FBevelCubeBuilder::FBevelCubeBuilder(const ABevelCube & InBevelCube)
    : BevelCube(InBevelCube)
{
    Clear();
}

void FBevelCubeBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    GridX.Empty();
    GridY.Empty();
    GridZ.Empty();
}

bool FBevelCubeBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!BevelCube.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    // 获取各轴尺寸
    HalfSize = BevelCube.GetHalfSize();
    InnerOffset = BevelCube.GetInnerOffset();

    BevelRadius = BevelCube.BevelRadius;
    BevelSegments = BevelCube.BevelSegments;

    // 只要有倒角段数且半径有效，并且所有轴向的中间平面都存在，则启用倒角
    bEnableBevel = (BevelSegments > 0) && (BevelRadius > KINDA_SMALL_NUMBER) &&
        (HalfSize.X > InnerOffset.X + KINDA_SMALL_NUMBER) &&
        (HalfSize.Y > InnerOffset.Y + KINDA_SMALL_NUMBER) &&
        (HalfSize.Z > InnerOffset.Z + KINDA_SMALL_NUMBER);

    PrecomputeGrids();

    // 定义面和它们对应的轴向
    // 0=X, 1=Y, 2=Z

    // Front (+Y): Right=X(0), Down=Z(2)  (注：Down是V轴方向，但在 Grid 传递时我们只关心数据源)
    GenerateUnfoldedFace({ FVector(0, 1, 0),  FVector(1, 0, 0), FVector(0, 0,-1), TEXT("Front") }, GridX, GridZ, 0, 2);

    // Back (-Y): Right=-X(0), Down=Z(2)
    GenerateUnfoldedFace({ FVector(0,-1, 0),  FVector(-1, 0, 0), FVector(0, 0,-1), TEXT("Back") }, GridX, GridZ, 0, 2);

    // Top (+Z): Right=X(0), Down=-Y(1) (修正后是 V=+Y) -> 这里要小心
    // Top: U=+X, V=+Y. 所以用 GridX 和 GridY.
    GenerateUnfoldedFace({ FVector(0, 0, 1),  FVector(1, 0, 0), FVector(0, 1, 0), TEXT("Top") }, GridX, GridY, 0, 1);

    // Bottom (-Z): Right=X(0), Down=Y(1) (V=-Y)
    GenerateUnfoldedFace({ FVector(0, 0,-1),  FVector(1, 0, 0), FVector(0,-1, 0), TEXT("Bottom") }, GridX, GridY, 0, 1);

    // Right (+X): Right=Y(1) (-Y actually), Down=Z(2)
    // Right Face: Normal(1,0,0). U is (0,-1,0) [Y axis], V is (0,0,-1) [Z axis]. 
    GenerateUnfoldedFace({ FVector(1, 0, 0),  FVector(0,-1, 0), FVector(0, 0,-1), TEXT("Right") }, GridY, GridZ, 1, 2);

    // Left (-X): Right=Y(1), Down=Z(2)
    GenerateUnfoldedFace({ FVector(-1, 0, 0),  FVector(0, 1, 0), FVector(0, 0,-1), TEXT("Left") }, GridY, GridZ, 1, 2);

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

void FBevelCubeBuilder::PrecomputeGrids()
{
    ComputeSingleAxisGrid(HalfSize.X, InnerOffset.X, GridX);
    ComputeSingleAxisGrid(HalfSize.Y, InnerOffset.Y, GridY);
    ComputeSingleAxisGrid(HalfSize.Z, InnerOffset.Z, GridZ);
}

void FBevelCubeBuilder::ComputeSingleAxisGrid(float AxisHalfSize, float AxisInnerOffset, TArray<float>& OutGrid)
{
    OutGrid.Empty();
    if (!bEnableBevel)
    {
        OutGrid = { -AxisHalfSize, AxisHalfSize };
    }
    else
    {
        // 左/下 半边 (-Half -> -Inner)
        const float Range = AxisHalfSize - AxisInnerOffset;
        const float Step = Range / BevelSegments;

        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            OutGrid.Add(-AxisHalfSize + i * Step);
        }
        // 右/上 半边 (Inner -> Half)
        // 注意：中间平面区域是 (-InnerOffset 到 InnerOffset)
        // 上面的循环结束于 -InnerOffset。
        // 我们从 InnerOffset 开始画另一边的倒角。
        // 中间会自动形成一个大的 Quad (从 -InnerOffset 到 InnerOffset)。

        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            OutGrid.Add(AxisInnerOffset + i * Step);
        }
    }
}

void FBevelCubeBuilder::GenerateUnfoldedFace(const FUnfoldedFace& FaceDef, const TArray<float>& GridU, const TArray<float>& GridV, int32 AxisIndexU, int32 AxisIndexV)
{
    const int32 NumU = GridU.Num();
    const int32 NumV = GridV.Num();

    if (NumU < 2 || NumV < 2) return;

    TArray<int32> VertIndices;
    VertIndices.SetNumUninitialized(NumU * NumV);

    // 获取当前面对应的内缩偏移量，用于判断是否在平面中心区域
    float OffsetU = InnerOffset[AxisIndexU];
    float OffsetV = InnerOffset[AxisIndexV];

    // 1. 生成顶点
    for (int32 v_idx = 0; v_idx < NumV; ++v_idx)
    {
        const float local_v = GridV[v_idx];
        const bool bInMainPlaneV = (FMath::Abs(local_v) <= OffsetV + KINDA_SMALL_NUMBER);

        for (int32 u_idx = 0; u_idx < NumU; ++u_idx)
        {
            const float local_u = GridU[u_idx];
            const bool bInMainPlaneU = (FMath::Abs(local_u) <= OffsetU + KINDA_SMALL_NUMBER);

            FVector VertexPos;
            FVector Normal;

            // 如果没有倒角，或者点位于中间平坦区域 (Plane U 和 Plane V 交叉区)
            if (!bEnableBevel || (bInMainPlaneU && bInMainPlaneV))
            {
                Normal = FaceDef.Normal;
                // 注意：这里需要加上 Z 轴偏移吗？原始代码有 + HalfSize.Z
                // 原始逻辑：VertexPos += FVector(0, 0, HalfSize);
                // 这意味着原点在底面中心。
                // 我们这里先算出相对于物体中心的坐标
                VertexPos = FaceDef.Normal * HalfSize[0] * FMath::Abs(FaceDef.Normal.X) +
                    FaceDef.Normal * HalfSize[1] * FMath::Abs(FaceDef.Normal.Y) +
                    FaceDef.Normal * HalfSize[2] * FMath::Abs(FaceDef.Normal.Z) +
                    FaceDef.U_Axis * local_u +
                    FaceDef.V_Axis * local_v;
            }
            else
            {
                // 计算该点在内缩盒子上的投影位置
                const float ClampedU = FMath::Clamp(local_u, -OffsetU, OffsetU);
                const float ClampedV = FMath::Clamp(local_v, -OffsetV, OffsetV);

                // 根据法线方向确定 Box 的厚度维度
                FVector NormalOffset = FaceDef.Normal * InnerOffset[0] * FMath::Abs(FaceDef.Normal.X) +
                    FaceDef.Normal * InnerOffset[1] * FMath::Abs(FaceDef.Normal.Y) +
                    FaceDef.Normal * InnerOffset[2] * FMath::Abs(FaceDef.Normal.Z);

                // 内盒表面点
                const FVector InnerPoint = NormalOffset +
                    FaceDef.U_Axis * ClampedU +
                    FaceDef.V_Axis * ClampedV;

                // 外盒表面点（如果不倒角的位置）
                FVector OuterNormalOffset = FaceDef.Normal * HalfSize[0] * FMath::Abs(FaceDef.Normal.X) +
                    FaceDef.Normal * HalfSize[1] * FMath::Abs(FaceDef.Normal.Y) +
                    FaceDef.Normal * HalfSize[2] * FMath::Abs(FaceDef.Normal.Z);

                const FVector OuterPoint = OuterNormalOffset +
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

            // 【保留原逻辑】：将原点从几何中心移动到底部中心
            // 原始代码: VertexPos += FVector(0, 0, HalfSize); 
            // 现在 HalfSize 是向量，应该是移动 Z 轴的一半高度
            VertexPos += FVector(0, 0, HalfSize.Z);

            // --- 【修改开始】UV 计算 ---
            // 目标：将 local_u/local_v 转换为物理 UV 坐标。
            // 1. local_u 的范围是 [-HalfSize, HalfSize]。
            // 2. 为了让纹理从角落开始平铺，我们加上 HalfSize，使其从 [0, Size] 开始。
            // 3. 乘以 GLOBAL_UV_SCALE (通常是 0.01) 来匹配世界单位。

            float PhysicalU = (local_u + HalfSize[AxisIndexU]) * ModelGenConstants::GLOBAL_UV_SCALE;
            float PhysicalV = (local_v + HalfSize[AxisIndexV]) * ModelGenConstants::GLOBAL_UV_SCALE;

            FVector2D UV(PhysicalU, PhysicalV);
            // --- 【修改结束】 ---

            VertIndices[v_idx * NumU + u_idx] = AddVertex(VertexPos, Normal, UV);
        }
    }

    // 2. 生成面 (Quads)
    for (int32 v = 0; v < NumV - 1; ++v)
    {
        for (int32 u = 0; u < NumU - 1; ++u)
        {
            int32 V00 = VertIndices[v * NumU + u];
            int32 V10 = VertIndices[v * NumU + (u + 1)];
            int32 V01 = VertIndices[(v + 1) * NumU + u];
            int32 V11 = VertIndices[(v + 1) * NumU + (u + 1)];

            // 保持逆时针顺序
            AddQuad(V00, V01, V11, V10);
        }
    }
}
