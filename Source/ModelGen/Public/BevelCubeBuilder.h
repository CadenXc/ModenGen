#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class ABevelCube;

struct FUnfoldedFace
{
    FVector Normal;
    FVector U_Axis;
    FVector V_Axis;
    FString Name;
};

class MODELGEN_API FBevelCubeBuilder : public FModelGenMeshBuilder
{
public:
    explicit FBevelCubeBuilder(const ABevelCube& InBevelCube);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const ABevelCube& BevelCube;

    bool bEnableBevel;

    FVector HalfSize;
    FVector InnerOffset;

    float BevelRadius;
    int32 BevelSegments;

    TArray<float> GridX;
    TArray<float> GridY;
    TArray<float> GridZ;

    void Clear();
    void PrecomputeGrids();

    void ComputeSingleAxisGrid(float AxisHalfSize, float AxisInnerOffset, TArray<float>& OutGrid);

    void GenerateUnfoldedFace(const FUnfoldedFace& FaceDef, const TArray<float>& GridU, const TArray<float>& GridV, int32 AxisIndexU, int32 AxisIndexV);
};