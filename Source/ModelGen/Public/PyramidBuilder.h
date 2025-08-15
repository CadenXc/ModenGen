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
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const APyramid& Pyramid;

    float BaseRadius;
    float Height;
    int32 Sides;
    float BevelRadius;
    float BevelTopRadius;
    
    TArray<FVector> BaseVertices;
    TArray<FVector> BevelBottomVertices;
    TArray<FVector> BevelTopVertices;
    TArray<FVector> PyramidBaseVertices;
    FVector PyramidTopPoint;
    
    TArray<FVector2D> BaseUVs;
    TArray<FVector2D> SideUVs;
    TArray<FVector2D> BevelUVs;
    
    TArray<float> AngleValues;
    TArray<float> CosValues;
    TArray<float> SinValues;
    TArray<float> UVScaleValues;

    void PrecomputeConstants();
    void PrecomputeVertices();
    void PrecomputeUVs();
    void PrecomputeTrigonometricValues();
    void PrecomputeUVScaleValues();
    
    void InitializeBaseVertices();
    void InitializeBevelVertices();
    void InitializePyramidVertices();
    
    void GenerateBaseFace();
    void GenerateBevelSection();
    void GeneratePyramidSides();
    
    void GenerateBasePolygon();
    void GenerateBevelSideStrip();
    void GeneratePyramidSideTriangles();
    
    void GeneratePolygonFace(const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder = false, float UVOffsetZ = 0.0f);
    void GenerateSideStrip(const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal = false, float UVOffsetY = 0.0f, float UVScaleY = 1.0f);
    
    TArray<FVector> GenerateCircleVertices(float Radius, float Z, int32 NumSides) const;
    TArray<FVector> GenerateBevelVertices(float BottomRadius, float TopRadius, float Z, int32 NumSides) const;
    
    void GeneratePolygonFaceOptimized(const TArray<FVector>& Vertices, 
                                     const FVector& Normal, 
                                     const TArray<FVector2D>& UVs,
                                     bool bReverseOrder = false);
    
    void GenerateSideStripOptimized(const TArray<FVector>& BottomVerts, 
                                   const TArray<FVector>& TopVerts, 
                                   const TArray<FVector2D>& UVs,
                                   bool bReverseNormal = false);
    
    TArray<FVector2D> GenerateCircularUVs(int32 NumSides, float UVOffsetZ = 0.0f) const;
    TArray<FVector2D> GenerateSideStripUVs(int32 NumSides, float UVOffsetY = 0.0f, float UVScaleY = 1.0f) const;
    
    virtual FVector2D GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const override;
    FVector2D GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const;
    int32 GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal);
    
    bool ValidatePrecomputedData() const;
};
