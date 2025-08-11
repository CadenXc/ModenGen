// Copyright (c) 2024. All rights reserved.

/**
 * @file ModelGenBuilderUtils.h
 * @brief 面向各个网格构建器的通用几何辅助函数（纯头文件，无外部依赖）
 *
 * 这些工具以自由函数形式提供，入参携带 FModelGenMeshBuilder 引用，
 * 以便直接复用已有的 AddTriangle/AddQuad/GetOrAddVertex 能力，
 * 而无需改动现有派生构建器类。
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

// 数学常量 - 使用UE4内置的PI常量
constexpr float PI = UE_PI;

namespace ModelGen
{
namespace BuilderUtils
{

// 生成矩形的四个顶点（从法线方向看逆时针顺序）
inline TArray<FVector> MakeRectangleVertices(const FVector& Center,
                                             const FVector& SizeX,
                                             const FVector& SizeY)
{
    TArray<FVector> vertices;
    vertices.Reserve(4);
    vertices.Add(Center - SizeX - SizeY); // 左下
    vertices.Add(Center - SizeX + SizeY); // 左上
    vertices.Add(Center + SizeX + SizeY); // 右上
    vertices.Add(Center + SizeX - SizeY); // 右下
    return vertices;
}

// 按角添加一个三角形（每个角可有不同的 Normal/UV）。
// 返回创建的三个顶点索引（如需）。
inline void AddTriangleWithCorners(FModelGenMeshBuilder& builder,
                                   const FVector pos[3],
                                   const FVector nrm[3],
                                   const FVector2D uv[3])
{
    const int32 v0 = builder.AddVertexNoDedup(pos[0], nrm[0], uv[0]);
    const int32 v1 = builder.AddVertexNoDedup(pos[1], nrm[1], uv[1]);
    const int32 v2 = builder.AddVertexNoDedup(pos[2], nrm[2], uv[2]);
    builder.AddTriangle(v0, v1, v2);
}

// 以四个顶点+统一法线+UV 组成一个四边形（两个三角形）
inline void AddQuadFromVerts(FModelGenMeshBuilder& builder,
                             const TArray<FVector>& verts4,
                             const FVector& normal,
                             const TArray<FVector2D>& uvs4)
{
    if (verts4.Num() != 4 || uvs4.Num() != 4)
    {
        return;
    }

    const int32 v0 = builder.GetOrAddVertex(verts4[0], normal, uvs4[0]);
    const int32 v1 = builder.GetOrAddVertex(verts4[1], normal, uvs4[1]);
    const int32 v2 = builder.GetOrAddVertex(verts4[2], normal, uvs4[2]);
    const int32 v3 = builder.GetOrAddVertex(verts4[3], normal, uvs4[3]);
    builder.AddQuad(v0, v1, v2, v3);
}

// 在两条等长的顶点环之间生成四边形条带（ringA[i]→ringB[i]→ringB[i+1]→ringA[i+1]）
inline void BuildQuadStripBetweenRings(FModelGenMeshBuilder& builder,
                                       const TArray<int32>& ringA,
                                       const TArray<int32>& ringB,
                                       bool bCloseLoop)
{
    const int32 countA = ringA.Num();
    const int32 countB = ringB.Num();
    if (countA < 2 || countA != countB)
    {
        return;
    }

    const int32 last = bCloseLoop ? countA : countA - 1;
    for (int32 i = 0; i < last - 1; ++i)
    {
        const int32 iNext = i + 1;
        builder.AddQuad(ringA[i], ringB[i], ringB[iNext], ringA[iNext]);
    }

    // 首尾闭合
    if (bCloseLoop)
    {
        builder.AddQuad(ringA[last - 1], ringB[last - 1], ringB[0], ringA[0]);
    }
}

// 从中心点到一圈顶点生成三角扇
inline void BuildTriangleFan(FModelGenMeshBuilder& builder,
                             int32 centerVertex,
                             const TArray<int32>& ring,
                             bool bCloseLoop)
{
    const int32 n = ring.Num();
    if (n < 2)
    {
        return;
    }

    for (int32 i = 0; i < n - 1; ++i)
    {
        builder.AddTriangle(centerVertex, ring[i], ring[i + 1]);
    }

    if (bCloseLoop)
    {
        builder.AddTriangle(centerVertex, ring[n - 1], ring[0]);
    }
}

// 根据二维网格（如经纬网格/倒角角点网格）生成三角形
inline void BuildGridTriangles(FModelGenMeshBuilder& builder,
                               const TArray<TArray<int32>>& grid,
                               bool bSpecialOrder = false)
{
    const int32 rows = grid.Num();
    if (rows < 2)
    {
        return;
    }

    for (int32 r = 0; r < rows - 1; ++r)
    {
        const int32 colsThis = grid[r].Num();
        const int32 colsNext = grid[r + 1].Num();
        const int32 cols = FMath::Min(colsThis, colsNext);
        for (int32 c = 0; c < cols - 1; ++c)
        {
            const int32 v00 = grid[r][c];
            const int32 v10 = grid[r + 1][c];
            const int32 v01 = grid[r][c + 1];
            const int32 v11 = grid[r + 1][c + 1];

            if (bSpecialOrder)
            {
                builder.AddTriangle(v00, v01, v10);
                builder.AddTriangle(v10, v01, v11);
            }
            else
            {
                builder.AddTriangle(v00, v10, v01);
                builder.AddTriangle(v10, v11, v01);
            }
        }
    }
}

// 对两个法线进行插值并单位化
inline FVector LerpNormal(const FVector& n1, const FVector& n2, float alpha)
{
    FVector n = FMath::Lerp(n1, n2, alpha);
    n.Normalize();
    return n;
}

// 生成圆形一圈顶点（位于 Z 平面）
inline TArray<FVector> MakeCircularRingPositions(float radius, float z, int32 numSides)
{
    TArray<FVector> positions;
    positions.Reserve(numSides);
    for (int32 i = 0; i < numSides; ++i)
    {
        const float angle = 2.0f * PI * static_cast<float>(i) / FMath::Max(1, numSides);
        positions.Add(FVector(radius * FMath::Cos(angle), radius * FMath::Sin(angle), z));
    }
    return positions;
}

// 生成弧形一圈顶点（起始到结束角，角度单位：度）
// 默认以 -arcAngle/2 为起点，+arcAngle/2 为终点；可选择是否包含终点
inline TArray<FVector> MakeArcRingPositions(float radius,
                                            float z,
                                            int32 numSides,
                                            float arcAngleDegrees,
                                            float startAngleOffsetDegrees = TNumericLimits<float>::Lowest(),
                                            bool bIncludeEndPoint = true)
{
    TArray<FVector> positions;
    if (numSides <= 0 || arcAngleDegrees <= 0.0f)
    {
        return positions;
    }

    const float startDeg = (startAngleOffsetDegrees == TNumericLimits<float>::Lowest())
        ? -arcAngleDegrees * 0.5f
        : startAngleOffsetDegrees;
    const float endDeg = startDeg + arcAngleDegrees;
    const int32 steps = bIncludeEndPoint ? numSides : (numSides - 1);

    positions.Reserve(bIncludeEndPoint ? (numSides + 1) : numSides);
    for (int32 i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / numSides;
        const float deg = FMath::Lerp(startDeg, endDeg, t);
        const float rad = FMath::DegreesToRadians(deg);
        positions.Add(FVector(radius * FMath::Cos(rad), radius * FMath::Sin(rad), z));
    }
    return positions;
}

// 从一圈位置点生成多边形面（使用首点扇形三角剖分，UV采用极坐标映射）
inline void AddPolygonFaceFanFromPositions(FModelGenMeshBuilder& builder,
                                           const TArray<FVector>& positions,
                                           const FVector& normal,
                                           bool bReverseOrder,
                                           float uvOffsetZ = 0.0f)
{
    const int32 n = positions.Num();
    if (n < 3)
    {
        return;
    }

    TArray<int32> indices;
    indices.Reserve(n);

    for (int32 i = 0; i < n; ++i)
    {
        const float angle = 2.0f * PI * static_cast<float>(i) / n;
        const float u = 0.5f + 0.5f * FMath::Cos(angle);
        const float v = 0.5f + 0.5f * FMath::Sin(angle) + uvOffsetZ;
        indices.Add(builder.GetOrAddVertex(positions[i], normal, FVector2D(u, v)));
    }

    for (int32 i = 1; i < n - 1; ++i)
    {
        const int32 v0 = indices[0];
        const int32 v1 = indices[i];
        const int32 v2 = indices[i + 1];
        if (bReverseOrder)
        {
            builder.AddTriangle(v0, v1, v2);
        }
        else
        {
            builder.AddTriangle(v0, v2, v1);
        }
    }
}

// 极坐标UV（用于圆形/多边形端盖）：返回 (u,v) ∈ [0,1]
inline FVector2D MakePolarUV(int32 index, int32 count, float uvCenterU = 0.5f, float uvCenterV = 0.5f,
                             float uvRadiusScale = 0.5f, float uvOffsetV = 0.0f)
{
    if (count <= 0)
    {
        return FVector2D(uvCenterU, uvCenterV + uvOffsetV);
    }
    const float angle = 2.0f * PI * static_cast<float>(index) / count;
    const float u = uvCenterU + uvRadiusScale * FMath::Cos(angle);
    const float v = uvCenterV + uvRadiusScale * FMath::Sin(angle) + uvOffsetV;
    return FVector2D(u, v);
}

// 柱面UV（环方向映射到U，纵向映射到V）
inline FVector2D MakeCylindricalUV(int32 ringIndex, int32 ringCount, float vAlpha)
{
    const float u = (ringCount > 0) ? static_cast<float>(ringIndex) / ringCount : 0.0f;
    const float v = FMath::Clamp(vAlpha, 0.0f, 1.0f);
    return FVector2D(u, v);
}

// 根据弯曲量修正法线（适配截锥体的 Bend 逻辑）：alpha ∈ [0,1]
inline FVector MakeBentNormalFromRadial(const FVector& radialNormal,
                                        float bendAmount,
                                        float alpha)
{
    FVector n = radialNormal;
    if (!FMath::IsNearlyZero(bendAmount))
    {
        const float normalZ = -bendAmount * FMath::Cos(alpha * PI);
        n = (n + FVector(0, 0, normalZ)).GetSafeNormal();
    }
    return n;
}

// 用于规则索引格网（rows x cols），将相邻四点转为两个三角形
// indexAt(r,c) 返回顶点索引；r ∈ [0, rows-1], c ∈ [0, cols-1]
template <typename IndexAtFn>
inline void BuildIndexedGridQuads(FModelGenMeshBuilder& builder,
                                  int32 rows,
                                  int32 cols,
                                  IndexAtFn indexAt,
                                  bool bReverse = false)
{
    if (rows < 2 || cols < 2)
    {
        return;
    }
    for (int32 r = 0; r < rows - 1; ++r)
    {
        for (int32 c = 0; c < cols - 1; ++c)
        {
            const int32 v00 = indexAt(r, c);
            const int32 v10 = indexAt(r + 1, c);
            const int32 v01 = indexAt(r, c + 1);
            const int32 v11 = indexAt(r + 1, c + 1);
            if (bReverse)
            {
                builder.AddQuad(v00, v01, v11, v10);
            }
            else
            {
                builder.AddQuad(v00, v10, v11, v01);
            }
        }
    }
}

// 依据上下两圈位置点生成侧壁条带（自动计算法线，支持反转，带基础UV）
inline void AddSideStripFromRings(FModelGenMeshBuilder& builder,
                                  const TArray<FVector>& bottom,
                                  const TArray<FVector>& top,
                                  bool bReverseNormal = false,
                                  float uvOffsetY = 0.0f,
                                  float uvScaleY = 1.0f)
{
    const int32 n = bottom.Num();
    if (n < 2 || n != top.Num())
    {
        return;
    }

    for (int32 i = 0; i < n; ++i)
    {
        const int32 nextI = (i + 1) % n;

        FVector edge1 = bottom[nextI] - bottom[i];
        FVector edge2 = top[i] - bottom[i];
        FVector sideNormal = FVector::CrossProduct(edge1, edge2).GetSafeNormal();

        const FVector center = (bottom[i] + bottom[nextI] + top[i] + top[nextI]) * 0.25f;
        if (FVector::DotProduct(sideNormal, center) < 0)
        {
            sideNormal = -sideNormal;
        }

        if (bReverseNormal)
        {
            sideNormal = -sideNormal;
        }

        const float u0 = static_cast<float>(i) / n;
        const float u1 = static_cast<float>(nextI) / n;
        const float v0 = uvOffsetY;
        const float v1 = uvOffsetY + uvScaleY;

        const int32 v1Idx = builder.GetOrAddVertex(bottom[i],    sideNormal, FVector2D(u0, v0));
        const int32 v2Idx = builder.GetOrAddVertex(bottom[nextI], sideNormal, FVector2D(u1, v0));
        const int32 v3Idx = builder.GetOrAddVertex(top[nextI],    sideNormal, FVector2D(u1, v1));
        const int32 v4Idx = builder.GetOrAddVertex(top[i],        sideNormal, FVector2D(u0, v1));

        if (bReverseNormal)
        {
            builder.AddTriangle(v1Idx, v3Idx, v2Idx);
            builder.AddTriangle(v1Idx, v4Idx, v3Idx);
        }
        else
        {
            builder.AddTriangle(v1Idx, v2Idx, v3Idx);
            builder.AddTriangle(v1Idx, v3Idx, v4Idx);
        }
    }
}

} // namespace BuilderUtils
} // namespace ModelGen

