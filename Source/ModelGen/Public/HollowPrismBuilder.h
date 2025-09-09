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
    //~ Begin Constructor
    explicit FHollowPrismBuilder(const AHollowPrism& InHollowPrism);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const AHollowPrism& HollowPrism;

    void GenerateSideWalls();
    void GenerateInnerWalls();
    void GenerateOuterWalls();
    
    void GenerateTopCapWithTriangles();
    void GenerateBottomCapWithTriangles();
    
    void GenerateTopBevelGeometry();
    void GenerateBottomBevelGeometry();
    
    void GenerateEndCaps();
    void GenerateEndCap(float Angle, const FVector& Normal, bool IsStart);
    
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
    
    void GenerateCapTriangles(const TArray<int32>& InnerVertices, 
                              const TArray<int32>& OuterVertices, 
                              bool bIsTopCap);
    void GenerateCapVertices(TArray<int32>& OutInnerVertices, 
                            TArray<int32>& OutOuterVertices, 
                            bool bIsTopCap);
    
    void GenerateBevelGeometry(bool bIsTop, bool bIsInner);
    void GenerateBevelRing(const TArray<int32>& PrevRing, 
                           TArray<int32>& OutCurrentRing,
                           bool bIsTop, bool bIsInner, 
                           int32 RingIndex, int32 TotalRings);
    FVector CalculateBevelNormal(float Angle, float Alpha, bool bIsInner, bool bIsTop) const;
    void ConnectBevelRings(const TArray<int32>& PrevRing, 
                           const TArray<int32>& CurrentRing, 
                           bool bIsInner, bool bIsTop);
    
    float CalculateStartAngle() const;
    float CalculateAngleStep(int32 Sides) const;
    float CalculateInnerRadius(bool bIncludeBevel) const;
    float CalculateOuterRadius(bool bIncludeBevel) const;
    FVector CalculateVertexPosition(float Radius, float Angle, float Z) const;
    
    virtual FVector2D GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const override;
    
    FVector2D GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const;
    
    int32 GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal);
    
};
