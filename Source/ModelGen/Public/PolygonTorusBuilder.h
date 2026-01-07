#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class APolygonTorus;

class MODELGEN_API FPolygonTorusBuilder : public FModelGenMeshBuilder
{
public:
    explicit FPolygonTorusBuilder(const APolygonTorus& InPolygonTorus);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const APolygonTorus& PolygonTorus;

    struct FCachedTrig
    {
        float Cos;
        float Sin;
    };
    TArray<FCachedTrig> MajorAngleCache;
    TArray<FCachedTrig> MinorAngleCache;

    TArray<int32> StartCapRingIndices;
    TArray<int32> EndCapRingIndices;

    void Clear();
    void PrecomputeMath();

    void GenerateTorusSurface();

    void GenerateEndCaps();
    void CreateCap(const TArray<int32>& RingIndices, bool bIsStart);
};