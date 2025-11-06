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

    TArray<FVector> BottomVertices;
    TArray<FVector> TopVertices;
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

    void InitializeVertices();

    void GenerateBaseFace();
    void GenerateBevelSection();
    void GeneratePyramidSides();
};