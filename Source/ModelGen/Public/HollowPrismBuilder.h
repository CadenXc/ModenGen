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
    

    
    void GenerateEndCaps();
    
    void GenerateEndCap(float Angle, const FVector& Normal, bool IsStart);
    
    // 新增：端盖重构相关方法
    void GenerateEndCapVertices(float Angle, const FVector& Normal, bool IsStart,
                               TArray<int32>& OutOrderedVertices);
    
    // 新增：生成端盖倒角顶点
    void GenerateEndCapBevelVertices(float Angle, const FVector& Normal, bool IsStart,
                                    bool bIsTopBevel, TArray<int32>& OutVertices);
    
    // 新增：生成端盖侧边顶点
    void GenerateEndCapSideVertices(float Angle, const FVector& Normal, bool IsStart,
                                   TArray<int32>& OutVertices);
    
    // 新增：生成端盖三角形
    void GenerateEndCapTriangles(const TArray<int32>& OrderedVertices, bool IsStart);
    
    // 新增：端盖计算辅助方法
    void CalculateEndCapBevelHeights(float& OutTopBevelHeight, float& OutBottomBevelHeight) const;
    void CalculateEndCapZRange(float TopBevelHeight, float BottomBevelHeight, 
                              float& OutStartZ, float& OutEndZ) const;
    
    // 新增：公共三角化方法
    void GenerateCapTriangles(const TArray<int32>& InnerVertices, 
                              const TArray<int32>& OuterVertices, 
                              bool bIsTopCap);
    
    // 新增：生成单个端盖的顶点
    void GenerateCapVertices(TArray<int32>& OutInnerVertices, 
                            TArray<int32>& OutOuterVertices, 
                            bool bIsTopCap);
    
    // 新增：统一的倒角生成方法
    void GenerateBevelGeometry(bool bIsTop, bool bIsInner);
    
    // 新增：生成单个倒角环
    void GenerateBevelRing(const TArray<int32>& PrevRing, 
                           TArray<int32>& OutCurrentRing,
                           bool bIsTop, bool bIsInner, 
                           int32 RingIndex, int32 TotalRings);
    
    // 新增：计算倒角法线
    FVector CalculateBevelNormal(float Angle, float Alpha, bool bIsInner, bool bIsTop) const;
    
    // 新增：连接倒角环
    void ConnectBevelRings(const TArray<int32>& PrevRing, 
                           const TArray<int32>& CurrentRing, 
                           bool bIsInner, bool bIsTop);
    
    // 通用计算辅助方法
    float CalculateStartAngle() const;
    float CalculateAngleStep(int32 Sides) const;
    float CalculateInnerRadius(bool bIncludeBevel) const;
    float CalculateOuterRadius(bool bIncludeBevel) const;
    FVector CalculateVertexPosition(float Radius, float Angle, float Z) const;
};
