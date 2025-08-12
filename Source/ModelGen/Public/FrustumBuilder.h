// Copyright (c) 2024. All rights reserved.

/**
 * @file FrustumBuilder.h
 * @brief 截锥体网格构建器
 *
 * 该类负责生成截锥体的几何数据，继承自FModelGenMeshBuilder。
 * 提供了完整的截锥体生成功能，包括侧面、顶底面和倒角处理。
 */

#pragma once

#include "CoreMinimal.h"
#include "FrustumParameters.h"
#include "ModelGenMeshBuilder.h"

/**
 * 截锥体网格构建器
 */
class MODELGEN_API FFrustumBuilder : public FModelGenMeshBuilder
{
public:
    explicit FFrustumBuilder(const FFrustumParameters& InParams);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    FFrustumParameters Params;

    // 侧边环的顶点索引，用于倒角弧线生成
    TArray<int32> SideBottomRing;
    TArray<int32> SideTopRing;

    // 通用方法：生成顶点环
    TArray<int32> GenerateVertexRing(float Radius, float Z, int32 Sides, float UVV);

    // 通用方法：生成顶底面几何体
    void GenerateCapGeometry(float Z, int32 Sides, float Radius, bool bIsTop);

    // 通用方法：生成倒角几何体
    void GenerateBevelGeometry(bool bIsTop);

    // 通用方法：计算UV坐标
    FVector2D CalculateUV(float SideIndex, float Sides, float HeightRatio);

    // 通用方法：计算弯曲后的半径
    float CalculateBentRadius(float BaseRadius, float HeightRatio);

    // 通用方法：计算倒角高度
    float CalculateBevelHeight(float Radius);

    // 通用方法：计算高度比例（用于UV坐标）
    float CalculateHeightRatio(float Z);

    // 通用方法：计算角度步长
    float CalculateAngleStep(int32 Sides);

    void GenerateBaseGeometry();
    void CreateSideGeometry();
    void GenerateTopGeometry();
    void GenerateBottomGeometry();
    void GenerateTopBevelGeometry();
    void GenerateBottomBevelGeometry();
    void GenerateEndCaps();
    void GenerateEndCapTriangles(float Angle, const FVector& Normal, bool IsStart);
    void GenerateBevelArcTriangles(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop);
    void GenerateBevelArcTrianglesWithCaps(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop,
        int32 CenterVertex, int32 CapCenterVertex);
    
    // 新增：端面重构相关方法
    void GenerateEndCapVertices(float Angle, const FVector& Normal, bool IsStart,
                               TArray<int32>& OutOrderedVertices);
    
    // 新增：生成端面倒角顶点
    void GenerateEndCapBevelVertices(float Angle, const FVector& Normal, bool IsStart,
                                    bool bIsTopBevel, TArray<int32>& OutVertices, float SideRadius);
    
    // 新增：生成端面侧边顶点
    void GenerateEndCapSideVertices(float Angle, const FVector& Normal, bool IsStart,
                                   TArray<int32>& OutVertices, float TopSideRadius, float BottomSideRadius);
    
    // 新增：生成端面三角形
    void GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, bool IsStart);
    
    // 新增：端面计算辅助方法
    void CalculateEndCapBevelHeights(float& OutTopBevelHeight, float& OutBottomBevelHeight) const;
    void CalculateEndCapZRange(float TopBevelHeight, float BottomBevelHeight, 
                              float& OutStartZ, float& OutEndZ) const;
    float CalculateEndCapRadiusAtHeight(float Z) const;
    
    // 新增：专门用于倒角的半径计算方法，不受MinBendRadius影响
    float CalculateBevelRadiusAtHeight(float Z) const;
};
