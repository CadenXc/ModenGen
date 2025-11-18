// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class AHollowPrism;

class MODELGEN_API FHollowPrismBuilder : public FModelGenMeshBuilder
{
public:
    explicit FHollowPrismBuilder(const AHollowPrism& InHollowPrism);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

private:
    const AHollowPrism& HollowPrism;

    // Configuration
    bool bEnableBevel;
    int32 BevelSegments;

    float StartAngle;
    float ArcAngleRadians;

    // --- Cache Data ---
    struct FCachedTrig
    {
        float Cos;
        float Sin;
    };
    TArray<FCachedTrig> InnerAngleCache;
    TArray<FCachedTrig> OuterAngleCache;

    // Vertical Profile Point Definition
    struct FVerticalProfilePoint
    {
        float Z;
        float Radius;
        float V;           // World Space V
        FVector Normal;    // Local Profile Normal
        bool bIsWallEdge;
    };

    // --- Topology Cache ---
    // Ring indices for stitching caps
    TArray<int32> TopInnerCapRing;
    TArray<int32> TopOuterCapRing;
    TArray<int32> BottomInnerCapRing;
    TArray<int32> BottomOuterCapRing;

    // End Cap Indices (Separate for Inner/Outer walls to form the cut plane)
    TArray<int32> StartInnerCapIndices;
    TArray<int32> StartOuterCapIndices;
    TArray<int32> EndInnerCapIndices;
    TArray<int32> EndOuterCapIndices;

    // --- Internal Helpers ---
    void Clear();
    void PrecomputeMath();

    void ComputeVerticalProfile(EInnerOuter InnerOuter, TArray<FVerticalProfilePoint>& OutProfile);

    void GenerateSideGeometry(EInnerOuter InnerOuter);

    void GenerateCaps();
    void CreateCapDisk(const TArray<int32>& InnerRing, const TArray<int32>& OuterRing, bool bIsTop);

    void GenerateCutPlanes();
    // Helper to create a specific cut plane surface
    void CreateCutPlane(float Angle, const TArray<int32>& InnerIndices, const TArray<int32>& OuterIndices, bool bIsStartFace);
};