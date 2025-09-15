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
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

private:
    const APolygonTorus& PolygonTorus;

    // 用于存储封盖边缘顶点索引的数组
    TArray<int32> StartCapIndices;
    TArray<int32> EndCapIndices;

    void GenerateTriangles();
    void GenerateEndCaps();
    void GenerateAdvancedEndCaps();
    void ValidateAndClampParameters();

};