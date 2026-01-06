#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class AFrustum;

class MODELGEN_API FFrustumBuilder : public FModelGenMeshBuilder
{
public:
    explicit FFrustumBuilder(const AFrustum& InFrustum);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

    void Clear();

private:
    const AFrustum& Frustum;

    bool bEnableBevel;
    float StartAngle;
    float ArcAngleRadians;

    TArray<int32> TopSideRing;
    TArray<int32> BottomSideRing;
    TArray<int32> TopCapRing;
    TArray<int32> BottomCapRing;

    TArray<int32> StartSliceIndices;
    TArray<int32> EndSliceIndices;

    struct FRingContext
    {
        float Radius;
        float Z;
        int32 Sides;
    };

    void CalculateCommonParams();
    void GenerateSides();
    void GenerateBevels();
    void GenerateCaps();
    void GenerateCutPlanes();

    void StitchRings(const TArray<int32>& RingA, const TArray<int32>& RingB);

    TArray<FVector2D> GetRingPos2D(float Radius, int32 Sides) const;

    TArray<int32> CreateVertexRing(const FRingContext& Context, float VCoord);

    TArray<int32> CreateBevelRing(const FRingContext& Context, float VCoord, float NormalAlpha, bool bIsTopBevel, float OverrideRadius = 0.0f);

    void CreateCapDisk(float Z, const TArray<int32>& BoundaryRing, bool bIsTop);
    void CreateCutPlaneSurface(float Angle, const TArray<int32>& ProfileIndices, bool bIsStartFace, const FVector& InnerNormal);

    FVector ApplyBend(const FVector& BasePos, float BaseRadius, float HeightRatio) const;
    float CalculateBevelHeight(float Radius) const;
};