// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrismBuilder.h
 * @brief 空心棱柱网格构建器
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

 // Forward declarations
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

    // 是否启用倒角：BevelRadius > 0 且 BevelSegments > 0
    bool bEnableBevel;
    
    // 倒角段数
    int32 BevelSegments;

    // 盖子顶点环记录 (由 Bevel/Wall 几何体生成)
    // 这些是最终与 Cap 三角形连接的边缘顶点
    TArray<int32> TopInnerCapRing;
    TArray<int32> TopOuterCapRing;
    TArray<int32> BottomInnerCapRing;
    TArray<int32> BottomOuterCapRing;

    // 墙体顶点环记录 (倒角与中间墙体连接的边缘顶点)
    TArray<int32> TopInnerWallRing;
    TArray<int32> TopOuterWallRing;
    TArray<int32> BottomInnerWallRing;
    TArray<int32> BottomOuterWallRing;
    
    // 端盖顶点索引记录（用于生成端盖）
    TArray<int32> StartOuterCapIndices;
    TArray<int32> StartInnerCapIndices;
    TArray<int32> EndOuterCapIndices;
    TArray<int32> EndInnerCapIndices;
    
    // 清空所有数据
    void Clear();

    //~ Begin Geometry Generation
    /**
     * @brief (新) 统一生成墙体和倒角几何体。
     * 此函数将填充 Top/Bottom CapRing 和 WallRing 数组。
     */
    void GenerateSideAndBevelGeometry(EInnerOuter InnerOuter);

    /**
     * @brief 生成墙体 (仅在 bEnableBevel=false 时使用)
     */
    void GenerateWalls(float Radius, int32 Sides, EInnerOuter InnerOuter);

    /**
     * @brief 使用提供的 Cap 环生成盖子三角形。
     */
    void GenerateCapTriangles(const TArray<int32>& InnerVertices,
        const TArray<int32>& OuterVertices,
        EHeightPosition HeightPosition);

    //~ Begin End Cap Generation (for partial circles)
    void GenerateEndCapColumn(float Angle,
        const FVector& Normal,
        TArray<int32>& OutOrderedVertices,
        EEndCapType EndCapType);
    void GenerateEndCapTriangles(const TArray<int32>& OrderedVertices,
        EEndCapType EndCapType);
    void GenerateEndCapWithBevel(EEndCapType EndCapType);

    //~ Begin Utility Functions
    float CalculateStartAngle() const;
    float CalculateAngleStep(int32 Sides) const;
    /**
     * @brief 计算顶点坐标 (Z值会向上偏移 HalfHeight)
     */
    FVector CalculateVertexPosition(float Radius, float Angle, float Z) const;

    //~ Begin UV Functions
    FVector2D CalculateWallUV(float Angle, float Z, EInnerOuter InnerOuter) const;
    FVector2D CalculateCapUV(float Angle, float Radius, EHeightPosition HeightPosition) const;
    FVector2D CalculateBevelUV(float Angle, float Alpha, EInnerOuter InnerOuter, EHeightPosition HeightPosition, float Radius, float Z) const;
    FVector2D CalculateEndCapUVWithRadius(float Angle, float Z, float Radius, EEndCapType EndCapType) const;
};