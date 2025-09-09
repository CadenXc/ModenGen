// Copyright (c) 2024. All rights reserved.

/**
 * @file PyramidBuilder.h
 * @brief 金字塔网格构建器
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class APyramid;

class MODELGEN_API FPyramidBuilder : public FModelGenMeshBuilder
{
public:
    explicit FPyramidBuilder(const APyramid& InPyramid);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const APyramid& Pyramid;

    float BaseRadius;
    float Height;
    int32 Sides;
    float BevelRadius;
    float BevelTopRadius;

    TArray<FVector> BaseVertices;
    TArray<FVector> BevelBottomVertices;
    TArray<FVector> BevelTopVertices;
    TArray<FVector> PyramidBaseVertices;
    FVector PyramidTopPoint;

    TArray<FVector2D> BaseUVs;
    TArray<FVector2D> PyramidSideUVs;
    TArray<FVector2D> BevelUVs;

    TArray<float> AngleValues;
    TArray<float> CosValues;
    TArray<float> SinValues;
    TArray<float> UVScaleValues;

    void PrecomputeVertices();
    void PrecomputeUVs();
    void PrecomputeTrigonometricValues();
    void PrecomputeUVScaleValues();

    void InitializeBaseVertices();
    void InitializeBevelVertices();
    void InitializePyramidVertices();

    void GenerateBaseFace();
    void GenerateBevelSection();
    void GeneratePyramidSides();

    void GeneratePolygonFaceOptimized(const TArray<FVector>& Vertices,
        const FVector& Normal);

    void GenerateSideStripOptimized(const TArray<FVector>& BottomVerts,
        const TArray<FVector>& TopVerts,
        const TArray<FVector2D>& BottomUVs,
        const TArray<FVector2D>& TopUVs);

    FVector2D GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const;

    // 新增：用于直接传入UV
    int32 GetOrAddVertexWithUV(const FVector& Pos, const FVector& Normal, const FVector2D& UV);

    // 保留原始 GetOrAddVertexWithDualUV，用于金字塔侧面和底面
    int32 GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal);
};