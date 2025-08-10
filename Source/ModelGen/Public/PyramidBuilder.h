// Copyright (c) 2024. All rights reserved.

/**
 * @file PyramidBuilder.h
 * @brief 金字塔网格构建器
 * 
 * 该类负责生成金字塔的几何数据，继承自FModelGenMeshBuilder。
 * 提供了完整的金字塔生成功能，包括底面、倒角部分和侧面。
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "PyramidParameters.h"

/**
 * 金字塔网格构建器
 */
class MODELGEN_API FPyramidBuilder : public FModelGenMeshBuilder
{
public:
    //~ Begin Constructor
    explicit FPyramidBuilder(const FPyramidParameters& InParams);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    //~ Begin Private Members
    /** 生成参数 */
    FPyramidParameters Params;

    //~ Begin Private Methods
    void GenerateBaseFace();
    
    void GenerateBevelSection();
    
    void GeneratePyramidSides();
    
    void GeneratePolygonFace(const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder = false, float UVOffsetZ = 0.0f);
    
    void GenerateSideStrip(const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal = false, float UVOffsetY = 0.0f, float UVScaleY = 1.0f);
    
    TArray<FVector> GenerateCircleVertices(float Radius, float Z, int32 NumSides) const;
    
    TArray<FVector> GenerateBevelVertices(float BottomRadius, float TopRadius, float Z, int32 NumSides) const;
};
