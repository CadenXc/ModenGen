// Copyright (c) 2024. All rights reserved.

/**
 * @file BevelCubeBuilder.h
 * @brief 圆角立方体网格构建器
 * 
 * 该类负责生成圆角立方体的几何数据，继承自FModelGenMeshBuilder。
 * 提供了完整的圆角立方体生成功能，包括主要面、边缘倒角和角落倒角。
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "BevelCubeParameters.h"

/**
 * 圆角立方体网格构建器
 */
class MODELGEN_API FBevelCubeBuilder : public FModelGenMeshBuilder
{
public:
    //~ Begin Constructor
    explicit FBevelCubeBuilder(const FBevelCubeParameters& InParams);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    //~ Begin Private Members
    /** 生成参数 */
    FBevelCubeParameters Params;

    //~ Begin Private Methods
    /** 计算立方体的核心点（倒角起始点） */
    TArray<FVector> CalculateCorePoints() const;
    
    /** 生成主要面 */
    void GenerateMainFaces();
    
    /** 生成边缘倒角 */
    void GenerateEdgeBevels(const TArray<FVector>& CorePoints);
    
    /** 生成角落倒角 */
    void GenerateCornerBevels(const TArray<FVector>& CorePoints);
    
    /** 生成边缘条带 */
    void GenerateEdgeStrip(const TArray<FVector>& CorePoints, int32 Core1Idx, int32 Core2Idx, 
                          const FVector& Normal1, const FVector& Normal2);
    
    /** 生成角落三角形 */
    void GenerateCornerTriangles(const TArray<TArray<int32>>& CornerVerticesGrid, 
                                int32 Lat, int32 Lon, bool bSpecialOrder);
    
    /** 生成矩形顶点 */
    TArray<FVector> GenerateRectangleVertices(const FVector& Center, const FVector& SizeX, const FVector& SizeY) const;
    
    /** 生成四边形面 */
    void GenerateQuadSides(const TArray<FVector>& Verts, const FVector& Normal, const TArray<FVector2D>& UVs);
    
    /** 生成边缘顶点 */
    TArray<FVector> GenerateEdgeVertices(const FVector& CorePoint1, const FVector& CorePoint2, 
                                        const FVector& Normal1, const FVector& Normal2, float Alpha) const;
    
    /** 生成角落顶点 */
    TArray<FVector> GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX, 
                                          const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const;
};
