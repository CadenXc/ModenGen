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
    // 网格分辨率，覆盖主面+两侧倒角。每条边有 (2 + 2*BevelSegments) 个顶点。
    const int32 GridSize = 2 + 2 * BevelSegments;

    // 创建一个二维数组来存储生成的顶点索引，方便后续连接成面
    TArray<TArray<int32>> VertexGrid;
    VertexGrid.SetNum(GridSize);
    for (int32 i = 0; i < GridSize; ++i)
    {
        VertexGrid[i].SetNum(GridSize);
    }

    // UV布局计算，假设整个UV空间被划分为4x4的网格
    const FVector2D UVBlockSize(1.0f / 4.0f, 1.0f / 4.0f);
    const FVector2D UVBaseOffset(FaceDef.UV_Grid_Offset.X * UVBlockSize.X, FaceDef.UV_Grid_Offset.Y * UVBlockSize.Y);

    // 遍历网格上的每个点，计算其 3D位置、法线 和 UV坐标
    for (int32 v_idx = 0; v_idx < GridSize; ++v_idx)
    {
        for (int32 u_idx = 0; u_idx < GridSize; ++u_idx)
        {
            // 1. 计算当前点在 [0,1] 区间内的标准化坐标 (alpha)
            const float u_alpha = static_cast<float>(u_idx) / (GridSize - 1);
            const float v_alpha = static_cast<float>(v_idx) / (GridSize - 1);

            // 2. 将 alpha 映射到面的局部2D坐标系 [-HalfSize, HalfSize]
            const float u = FMath::Lerp(-HalfSize, HalfSize, u_alpha);
            const float v = FMath::Lerp(-HalfSize, HalfSize, v_alpha);

            // 3. 计算UV坐标：基于alpha和面的网格偏移
            // V方向(v_alpha)需要翻转 (1.0f - ...)，以匹配常见的从上到下的纹理坐标系
            FVector2D UV(u_alpha * UVBlockSize.X + UVBaseOffset.X, (1.0f - v_alpha) * UVBlockSize.Y + UVBaseOffset.Y);

            // 4. 计算核心点 (CorePoint) 和表面法线 (SurfaceNormal)
            FVector CorePoint;
            FVector SurfaceNormal;
            FVector VertexPos;

            // 硬边模式：始终使用面的原始法线，只调整位置
            SurfaceNormal = FaceDef.Normal;
            
            // 判断当前点是否在主平面区域内
            const bool bInMainPlane = (FMath::Abs(u) <= InnerOffset) && (FMath::Abs(v) <= InnerOffset);
            
            if (bInMainPlane)
            {
                // 主平面区域：直接在立方体表面
                VertexPos = FaceDef.Normal * HalfSize + FaceDef.U_Axis * u + FaceDef.V_Axis * v;
            }
            else
            {
                // 倒角区域：计算倒角位置
                // 使用简单的倒角算法：从内部核心点向外延伸
                const FVector InnerPoint = FaceDef.Normal * InnerOffset +
                    FaceDef.U_Axis * FMath::Clamp(u, -InnerOffset, InnerOffset) +
                    FaceDef.V_Axis * FMath::Clamp(v, -InnerOffset, InnerOffset);
                
                // 计算从内部点到外部点的方向
                const FVector OuterPoint = FaceDef.Normal * HalfSize + FaceDef.U_Axis * u + FaceDef.V_Axis * v;
                const FVector Direction = (OuterPoint - InnerPoint).GetSafeNormal();
                
                // 沿着方向延伸BevelRadius距离
                VertexPos = InnerPoint + Direction * BevelRadius;
            }

            // 6. 添加顶点到网格数据，并记录其索引
            VertexGrid[v_idx][u_idx] = AddVertex(VertexPos, SurfaceNormal, UV);
        }
    }

    // 再次遍历网格，使用存储的顶点索引来生成四边形（两个三角形）
    for (int32 v_idx = 0; v_idx < GridSize - 1; ++v_idx)
    {
        for (int32 u_idx = 0; u_idx < GridSize - 1; ++u_idx)
        {
            const int32 V00 = VertexGrid[v_idx][u_idx];
            const int32 V10 = VertexGrid[v_idx + 1][u_idx];
            const int32 V01 = VertexGrid[v_idx][u_idx + 1];
            const int32 V11 = VertexGrid[v_idx + 1][u_idx + 1];

            AddQuad(V00, V10, V11, V01); // 注意顶点顺序以保证法线朝外
        }
    }
}