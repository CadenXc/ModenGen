// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class APyramid;

class MODELGEN_API FPyramidBuilder : public FModelGenMeshBuilder
{
public:
    explicit FPyramidBuilder(const APyramid& InPyramid);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

private:
    const APyramid& Pyramid;

    // Geometry Params
    float BaseRadius;
    float Height;
    int32 Sides;
    float BevelRadius;
    float BevelTopRadius;

    // Cache
    TArray<float> CosValues;
    TArray<float> SinValues;

    // Internal States
    FVector TopPoint;

    // Helpers
    void Clear();
    void PrecomputeMath();

    void GenerateBase();
    void GenerateSlices(); // Replaces GenerateBevel & GenerateSides

    // Utility
    FVector GetRingPos(int32 Index, float Radius, float Z) const;
};