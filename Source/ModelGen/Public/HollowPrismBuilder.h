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
    
    /** 生成顶底面 */
    void GenerateTopCapWithTriangles();
    void GenerateBottomCapWithTriangles();
    
    /** 生成倒角几何 */
    void GenerateTopBevelGeometry();
    void GenerateBottomBevelGeometry();
    
    /** 生成端盖 */
    void GenerateEndCaps();
    void GenerateEndCap(float Angle, const FVector& Normal, bool IsStart);
    
    /** 端盖相关方法 */
    void GenerateEndCapVertices(float Angle, const FVector& Normal, bool IsStart,
                               TArray<int32>& OutOrderedVertices);
    void GenerateEndCapBevelVertices(float Angle, const FVector& Normal, bool IsStart,
                                    bool bIsTopBevel, TArray<int32>& OutVertices);
    void GenerateEndCapSideVertices(float Angle, const FVector& Normal, bool IsStart,
                                   TArray<int32>& OutVertices);
    void GenerateEndCapTriangles(const TArray<int32>& OrderedVertices, bool IsStart);
    void CalculateEndCapBevelHeights(float& OutTopBevelHeight, float& OutBottomBevelHeight) const;
    void CalculateEndCapZRange(float TopBevelHeight, float BottomBevelHeight, 
                              float& OutStartZ, float& OutEndZ) const;
    
    /** 顶底面生成方法 */
    void GenerateCapTriangles(const TArray<int32>& InnerVertices, 
                              const TArray<int32>& OuterVertices, 
                              bool bIsTopCap);
    void GenerateCapVertices(TArray<int32>& OutInnerVertices, 
                            TArray<int32>& OutOuterVertices, 
                            bool bIsTopCap);
    
    /** 倒角生成方法 */
    void GenerateBevelGeometry(bool bIsTop, bool bIsInner);
    void GenerateBevelRing(const TArray<int32>& PrevRing, 
                           TArray<int32>& OutCurrentRing,
                           bool bIsTop, bool bIsInner, 
                           int32 RingIndex, int32 TotalRings);
    FVector CalculateBevelNormal(float Angle, float Alpha, bool bIsInner, bool bIsTop) const;
    void ConnectBevelRings(const TArray<int32>& PrevRing, 
                           const TArray<int32>& CurrentRing, 
                           bool bIsInner, bool bIsTop);
    
    /** 通用计算辅助方法 */
    float CalculateStartAngle() const;
    float CalculateAngleStep(int32 Sides) const;
    float CalculateInnerRadius(bool bIncludeBevel) const;
    float CalculateOuterRadius(bool bIncludeBevel) const;
    FVector CalculateVertexPosition(float Radius, float Angle, float Z) const;
    
    /** 基于顶点位置的稳定UV映射 - 重写父类实现 */
    virtual FVector2D GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const override;
    
    /** 生成第二UV通道 - 传统UV系统 */
    FVector2D GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const;
    
    /** 使用双UV通道添加顶点 */
    int32 GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal);
    
};
