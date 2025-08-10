// Copyright (c) 2024. All rights reserved.

/**
 * @file BevelBuilderUtils.h
 * @brief 倒角（Bevel）生成的通用工具（边倒角、角倒角、环形倒角），面向各 Builder 复用。
 *
 * 注意：本文件提供为自由函数，需传入 FModelGenMeshBuilder 引用以写入网格，
 * 不改动任何现有类。调用方可逐步用这些函数替换各类 Builder 中的重复倒角代码。
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "ModelGenBuilderUtils.h"

namespace ModelGen
{
namespace Bevel
{

// 1) 边倒角：在两端核心点 core1/core2 之间，根据两侧法线 normal1/normal2 与分段数生成条带
//    适配：BevelCube 的边倒角
inline void GenerateEdgeBevelStrip(FModelGenMeshBuilder& builder,
                                   const FVector& core1,
                                   const FVector& core2,
                                   const FVector& normal1,
                                   const FVector& normal2,
                                   float bevelSize,
                                   int32 sections)
{
    int32 prevStart = INDEX_NONE;
    int32 prevEnd = INDEX_NONE;

    for (int32 s = 0; s <= sections; ++s)
    {
        const float alpha = sections > 0 ? static_cast<float>(s) / sections : 0.0f;
        const FVector n = BuilderUtils::LerpNormal(normal1, normal2, alpha);

        const FVector posStart = core1 + n * bevelSize;
        const FVector posEnd   = core2 + n * bevelSize;

        const FVector2D uvStart(alpha, 0.0f);
        const FVector2D uvEnd  (alpha, 1.0f);

        const int32 vStart = builder.GetOrAddVertexPublic(posStart, n, uvStart);
        const int32 vEnd   = builder.GetOrAddVertexPublic(posEnd,   n, uvEnd);

        if (s > 0 && prevStart != INDEX_NONE && prevEnd != INDEX_NONE)
        {
            builder.AddQuadPublic(prevStart, prevEnd, vEnd, vStart);
        }

        prevStart = vStart;
        prevEnd = vEnd;
    }
}

// 2) 角倒角（四分之一球）：基于核心点与三轴方向生成顶点网格并三角化
//    适配：BevelCube 的角倒角
inline void GenerateCornerBevelQuarterSphere(FModelGenMeshBuilder& builder,
                                             const FVector& corePoint,
                                             const FVector& axisX,
                                             const FVector& axisY,
                                             const FVector& axisZ,
                                             float bevelSize,
                                             int32 sections,
                                             bool bSpecialOrder = false)
{
    // 顶点索引网格：行数 = sections + 1；第 r 行列数 = sections + 1 - r
    TArray<TArray<int32>> grid;
    grid.SetNum(sections + 1);
    for (int32 lat = 0; lat <= sections; ++lat)
    {
        grid[lat].SetNum(sections + 1 - lat);
    }

    for (int32 lat = 0; lat <= sections; ++lat)
    {
        for (int32 lon = 0; lon <= sections - lat; ++lon)
        {
            const float latAlpha = sections > 0 ? static_cast<float>(lat) / sections : 0.0f;
            const float lonAlpha = sections > 0 ? static_cast<float>(lon) / sections : 0.0f;

            FVector n = axisX * (1.0f - latAlpha - lonAlpha)
                      + axisY * latAlpha
                      + axisZ * lonAlpha;
            n.Normalize();

            const FVector pos = corePoint + n * bevelSize;
            const FVector2D uv(lonAlpha, latAlpha);
            grid[lat][lon] = builder.GetOrAddVertexPublic(pos, n, uv);
        }
    }

    BuilderUtils::BuildGridTriangles(builder, grid, bSpecialOrder);
}

// 3) 环形倒角：在两圈半径/高度之间生成一圈倒角侧壁
//    适配：Frustum/HollowPrism 顶/底倒角的圆环过渡
inline void GenerateRingBevel(FModelGenMeshBuilder& builder,
                              float radiusFrom,
                              float radiusTo,
                              float zFrom,
                              float zTo,
                              int32 sides,
                              bool bReverseNormal = false,
                              float uvOffsetY = 0.0f,
                              float uvScaleY = 1.0f)
{
    const TArray<FVector> bottom = BuilderUtils::MakeCircularRingPositions(radiusFrom, zFrom, sides);
    const TArray<FVector> top    = BuilderUtils::MakeCircularRingPositions(radiusTo,   zTo,   sides);
    BuilderUtils::AddSideStripFromRings(builder, bottom, top, bReverseNormal, uvOffsetY, uvScaleY);
}

} // namespace Bevel
} // namespace ModelGen

