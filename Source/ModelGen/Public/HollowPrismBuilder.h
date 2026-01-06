#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class AHollowPrism;

class MODELGEN_API FHollowPrismBuilder : public FModelGenMeshBuilder
{
public:
    explicit FHollowPrismBuilder(const AHollowPrism& InHollowPrism);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const AHollowPrism& HollowPrism;

    bool bEnableBevel;
    int32 BevelSegments;

    float StartAngle;
    float ArcAngleRadians;
    struct FCachedTrig
    {
        float Cos;
        float Sin;
    };
    TArray<FCachedTrig> InnerAngleCache;
    TArray<FCachedTrig> OuterAngleCache;

    struct FVerticalProfilePoint
    {
        float Z;
        float Radius;
        float V;
        FVector Normal;
        bool bIsWallEdge;
    };

    TArray<int32> TopInnerCapRing;
    TArray<int32> TopOuterCapRing;
    TArray<int32> BottomInnerCapRing;
    TArray<int32> BottomOuterCapRing;

    TArray<int32> StartInnerCapIndices;
    TArray<int32> StartOuterCapIndices;
    TArray<int32> EndInnerCapIndices;
    TArray<int32> EndOuterCapIndices;

    void Clear();
    void PrecomputeMath();

    void ComputeVerticalProfile(EInnerOuter InnerOuter, TArray<FVerticalProfilePoint>& OutProfile);

    void GenerateSideGeometry(EInnerOuter InnerOuter);

    void GenerateCaps();
    void CreateCapDisk(const TArray<int32>& InnerRing, const TArray<int32>& OuterRing, bool bIsTop);

    void GenerateCutPlanes();
    void CreateCutPlane(float Angle, const TArray<int32>& InnerIndices, const TArray<int32>& OuterIndices, bool bIsStartFace);
};