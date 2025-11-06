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
    
    // 端盖顶点索引记录 - 拆分为4个独立数组
    TArray<int32> StartOuterCapIndices;  // 起始端盖外壁顶点
    TArray<int32> StartInnerCapIndices;  // 起始端盖内壁顶点
    TArray<int32> EndOuterCapIndices;    // 结束端盖外壁顶点
    TArray<int32> EndInnerCapIndices;    // 结束端盖内壁顶点
    
    // 倒角连接顶点记录
    TArray<int32> TopInnerBevelVertices;   // 顶部内环倒角连接顶点
    TArray<int32> TopOuterBevelVertices;   // 顶部外环倒角连接顶点
    TArray<int32> BottomInnerBevelVertices; // 底部内环倒角连接顶点
    TArray<int32> BottomOuterBevelVertices; // 底部外环倒角连接顶点
    
    //~ Begin Wall Generation
    void GenerateWalls(float Radius, int32 Sides, EInnerOuter InnerOuter);

    //~ Begin Cap Generation
    void GenerateCapVertices(TArray<int32>& OutInnerVertices, 
                            TArray<int32>& OutOuterVertices, 
                            EHeightPosition HeightPosition);
    void GenerateCapTriangles(const TArray<int32>& InnerVertices,
                             const TArray<int32>& OuterVertices,
                             EHeightPosition HeightPosition);

    //~ Begin Bevel Generation
    void GenerateBevelGeometry(EHeightPosition HeightPosition, EInnerOuter InnerOuter);
    void GenerateBevelRing(TArray<int32>& OutCurrentRing,
                          EHeightPosition HeightPosition,
                          EInnerOuter InnerOuter,
                          int32 RingIndex,
                          int32 TotalRings);
    void ConnectBevelRings(const TArray<int32>& PrevRing,
                          const TArray<int32>& CurrentRing,
                          EInnerOuter InnerOuter,
                          EHeightPosition HeightPosition);

    //~ Begin End Cap Generation (for partial circles)
    void GenerateEndCapColumn(float Angle,
                             const FVector& Normal,
                             TArray<int32>& OutOrderedVertices,
                             EEndCapType EndCapType);
    void GenerateEndCapTriangles(const TArray<int32>& OrderedVertices,
                                EEndCapType EndCapType);
    void GenerateEndCapWithBevel(EEndCapType EndCapType);
    void GenerateEndCapWithBevelVertices(const TArray<int32>& RecordedVertices,
                                        EEndCapType EndCapType);
    
    //~ Begin Utility Functions
    float CalculateStartAngle() const;
    float CalculateAngleStep(int32 Sides) const;
    FVector CalculateVertexPosition(float Radius, float Angle, float Z) const;
    FVector CalculateBevelNormal(float Angle, float Alpha, EInnerOuter InnerOuter, EHeightPosition HeightPosition) const;
    
    //~ Begin UV Functions
    FVector2D CalculateWallUV(float Angle, float Z, EInnerOuter InnerOuter) const;
    FVector2D CalculateCapUV(float Angle, float Radius, EHeightPosition HeightPosition) const;
    FVector2D CalculateBevelUV(float Angle, float Alpha, EInnerOuter InnerOuter, EHeightPosition HeightPosition) const;
    FVector2D CalculateEndCapUVWithRadius(float Angle, float Z, float Radius, EEndCapType EndCapType) const;
};