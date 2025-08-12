// Copyright (c) 2024. All rights reserved.

/**
 * @file PolygonTorusBuilder.h
 * @brief 圆环生成器
 * 
 * 该文件定义了圆环生成器类，负责根据参数生成圆环几何体。
 */

#pragma once

#include "ModelGenMeshBuilder.h"
#include "PolygonTorusParameters.h"

/**
 * 圆环生成器
 * 负责根据参数生成圆环几何体
 */
class MODELGEN_API FPolygonTorusBuilder : public FModelGenMeshBuilder
{
public:
    /** 构造函数 */
    FPolygonTorusBuilder(const FPolygonTorusParameters& InParams);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

private:
    /** 生成参数 */
    FPolygonTorusParameters Params;

    /** 生成顶点 */
    void GenerateVertices();
    
    /** 生成三角形 */
    void GenerateTriangles();
    
    /** 生成端盖 */
    void GenerateEndCaps();
    
    /** 生成端盖 */
    void GenerateAdvancedEndCaps();
    
    /** 应用平滑光照 */
    void ApplySmoothing();
    
    /** 应用横面平滑光照 */
    void ApplyHorizontalSmoothing();
    
    /** 应用竖面平滑光照 */
    void ApplyVerticalSmoothing();
    
    /** 验证和修正参数 */
    void ValidateAndClampParameters();
    
    /** 记录网格统计信息 */
    void LogMeshStatistics();
};
