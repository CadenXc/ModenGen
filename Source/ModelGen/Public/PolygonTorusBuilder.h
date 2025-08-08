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

    /** 生成优化的顶点 */
    void GenerateOptimizedVertices();
    
    /** 生成优化的三角形 */
    void GenerateOptimizedTriangles();
    
    /** 生成端盖 */
    void GenerateEndCaps();
    
    /** 生成高级端盖 */
    void GenerateAdvancedEndCaps();
    
    /** 生成圆形端盖 */
    void GenerateCircularEndCaps();
    
    /** 计算优化的法线 */
    void CalculateOptimizedNormals();
    
    /** 计算高级光滑 */
    void CalculateAdvancedSmoothing();
    
    /** 生成光滑组 */
    void GenerateSmoothGroups();
    
    /** 生成硬边标记 */
    void GenerateHardEdges();
    
    /** 生成优化的UV坐标 */
    void GenerateUVsOptimized();
    
    /** 验证和修正参数 */
    void ValidateAndClampParameters();
    
    /** 验证网格拓扑 */
    void ValidateMeshTopology();
    
    /** 记录网格统计信息 */
    void LogMeshStatistics();
};
