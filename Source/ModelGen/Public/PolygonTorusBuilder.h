// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class APolygonTorus;

class MODELGEN_API FPolygonTorusBuilder : public FModelGenMeshBuilder
{
public:
    explicit FPolygonTorusBuilder(const APolygonTorus& InPolygonTorus);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

private:
    const APolygonTorus& PolygonTorus;

    // --- Cache Data ---
    struct FCachedTrig
    {
        float Cos;
        float Sin;
    };
    TArray<FCachedTrig> MajorAngleCache;
    TArray<FCachedTrig> MinorAngleCache;

    // 记录端盖的边缘索引
    TArray<int32> StartCapRingIndices;
    TArray<int32> EndCapRingIndices;

    // --- Internal Helpers ---
    void Clear();
    void PrecomputeMath();

    void GenerateTorusSurface();

    void GenerateEndCaps();
    void CreateCap(const TArray<int32>& RingIndices, bool bIsStart);

    void ValidateAndClampParameters();
};