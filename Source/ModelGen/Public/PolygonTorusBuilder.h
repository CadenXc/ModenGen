// Copyright (c) 2024. All rights reserved.

/**
 * @file PolygonTorusBuilder.h
 * @brief 圆环生成器
 */

#pragma once

#include "ModelGenMeshBuilder.h"

// Forward declarations
class APolygonTorus;

class MODELGEN_API FPolygonTorusBuilder : public FModelGenMeshBuilder
{
public:
    FPolygonTorusBuilder(const APolygonTorus& InPolygonTorus);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

private:
    const APolygonTorus& PolygonTorus;

    void GenerateVertices();
    void GenerateTriangles();
    void GenerateEndCaps();
    void GenerateAdvancedEndCaps();
    void ApplySmoothing();
    void ApplyHorizontalSmoothing();
    void ApplyVerticalSmoothing();
    void ValidateAndClampParameters();
    void LogMeshStatistics();
    
    virtual FVector2D GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const override;
    FVector2D GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const;
    int32 GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal);
    
    
};
