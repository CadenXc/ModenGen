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

    // 新增：预计算常量和数据结构
    float BaseRadius;                       // 底面半径
    float Height;                           // 金字塔高度
    int32 Sides;                            // 底面边数
    float BevelRadius;                      // 倒角半径
    float BevelTopRadius;                   // 倒角顶部半径
    
    // 预计算顶点数据
    TArray<FVector> BaseVertices;           // 底面顶点
    TArray<FVector> BevelBottomVertices;    // 倒角底部顶点
    TArray<FVector> BevelTopVertices;       // 倒角顶部顶点
    TArray<FVector> PyramidBaseVertices;    // 金字塔底部顶点
    FVector PyramidTopPoint;                // 金字塔顶点
    
    // 预计算UV数据
    TArray<FVector2D> BaseUVs;             // 底面UV
    TArray<FVector2D> SideUVs;             // 侧面UV
    TArray<FVector2D> BevelUVs;            // 倒角UV
    
    // 性能优化：预计算常用值
    TArray<float> AngleValues;              // 角度值 (0 到 2π)
    TArray<float> CosValues;                // 余弦值
    TArray<float> SinValues;                // 正弦值
    TArray<float> UVScaleValues;            // UV缩放值

    //~ Begin Private Methods
    // 新增：初始化函数
    void PrecomputeConstants();
    void PrecomputeVertices();
    void PrecomputeUVs();
    void PrecomputeTrigonometricValues();
    void PrecomputeUVScaleValues();
    
    void InitializeBaseVertices();
    void InitializeBevelVertices();
    void InitializePyramidVertices();
    
    // 原有的生成函数
    void GenerateBaseFace();
    void GenerateBevelSection();
    void GeneratePyramidSides();
    
    // 新增：分解复杂函数
    void GenerateBasePolygon();
    void GenerateBevelSideStrip();
    void GeneratePyramidSideTriangles();
    
    // 原有的辅助函数
    void GeneratePolygonFace(const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder = false, float UVOffsetZ = 0.0f);
    
    void GenerateSideStrip(const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal = false, float UVOffsetY = 0.0f, float UVScaleY = 1.0f);
    
    TArray<FVector> GenerateCircleVertices(float Radius, float Z, int32 NumSides) const;
    
    TArray<FVector> GenerateBevelVertices(float BottomRadius, float TopRadius, float Z, int32 NumSides) const;
    
    // 新增：优化后的辅助函数
    void GeneratePolygonFaceOptimized(const TArray<FVector>& Vertices, 
                                     const FVector& Normal, 
                                     const TArray<FVector2D>& UVs,
                                     bool bReverseOrder = false);
    
    void GenerateSideStripOptimized(const TArray<FVector>& BottomVerts, 
                                   const TArray<FVector>& TopVerts, 
                                   const TArray<FVector2D>& UVs,
                                   bool bReverseNormal = false);
    
    // 新增：UV生成函数
    TArray<FVector2D> GenerateCircularUVs(int32 NumSides, float UVOffsetZ = 0.0f) const;
    TArray<FVector2D> GenerateSideStripUVs(int32 NumSides, float UVOffsetY = 0.0f, float UVScaleY = 1.0f) const;
    
    // 新增：验证函数
    bool ValidatePrecomputedData() const;
};
