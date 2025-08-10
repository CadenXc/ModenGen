// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrismBuilder.h
 * @brief 空心棱柱网格构建器
 * 
 * 该类负责生成空心棱柱的几何数据，继承自FModelGenMeshBuilder。
 * 提供了完整的空心棱柱生成功能，包括侧面、顶底面和倒角处理。
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "HollowPrismParameters.h"

class MODELGEN_API FHollowPrismBuilder : public FModelGenMeshBuilder
{
public:
    //~ Begin Constructor
    explicit FHollowPrismBuilder(const FHollowPrismParameters& InParams);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    FHollowPrismParameters Params;

    /** 生成基础几何体 */
    void GenerateBaseGeometry();
    
    /** 生成侧面墙壁 */
    void GenerateSideWalls();
    void GenerateInnerWalls();
    void GenerateOuterWalls();
    
    void GenerateTopCapWithTriangles();
    void GenerateBottomCapWithTriangles();
    
    // 生成顶部倒角几何
    void GenerateTopBevelGeometry();
    
    //生成底部倒角几何
    void GenerateBottomBevelGeometry();
    
    // 生成倒角
    void GenerateTopInnerBevel();
    void GenerateTopOuterBevel();
    void GenerateBottomInnerBevel();
    void GenerateBottomOuterBevel();
    
    void GenerateEndCaps();
    
    void GenerateEndCap(float Angle, const FVector& Normal, bool IsStart);
    
    void CalculateRingVertices(float Radius, int32 Sides, float Z, float ArcAngle,
                              TArray<FVector>& OutVertices, TArray<FVector2D>& OutUVs, float UVScale = 1.0f);
};
