// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class AFrustum;

class MODELGEN_API FFrustumBuilder : public FModelGenMeshBuilder
{
public:
    explicit FFrustumBuilder(const AFrustum& InFrustum);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

    void Clear();

private:
    const AFrustum& Frustum;

    // Configuration
    bool bEnableBevel;
    float StartAngle;
    float ArcAngleRadians;

    // --- Topology Cache ---
    TArray<int32> TopSideRing;
    TArray<int32> BottomSideRing;
    TArray<int32> TopCapRing;
    TArray<int32> BottomCapRing;

    TArray<int32> StartSliceIndices;
    TArray<int32> EndSliceIndices;

    // --- Internal Helpers ---
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

    // Helper to stitch two rings
    void StitchRings(const TArray<int32>& RingA, const TArray<int32>& RingB);

    // Helper to calculate 2D ring positions (Added this declaration)
    TArray<FVector2D> GetRingPos2D(float Radius, int32 Sides) const;

    TArray<int32> CreateVertexRing(const FRingContext& Context, float VCoord);
    void CreateCapDisk(float Z, const TArray<int32>& BoundaryRing, bool bIsTop);
    void CreateCutPlaneSurface(float Angle, const TArray<int32>& ProfileIndices, bool bIsStartFace);

    FVector ApplyBend(const FVector& BasePos, float BaseRadius, float HeightRatio) const;
};