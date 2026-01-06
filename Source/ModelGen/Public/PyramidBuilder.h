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

    TArray<float> CosValues;
    TArray<float> SinValues;

    FVector TopPoint;

    void Clear();
    void PrecomputeMath();

    void GenerateBase();
    void GenerateSlices();

    FVector GetRingPos(int32 Index, float Radius, float Z) const;
};