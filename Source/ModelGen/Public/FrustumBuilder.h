// Copyright (c) 2024. All rights reserved.

/**
 * @file FrustumBuilder.h
 * @brief 截锥体网格构建器
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

 // Forward declarations
class AFrustum;

class MODELGEN_API FFrustumBuilder : public FModelGenMeshBuilder
{
public:
    explicit FFrustumBuilder(const AFrustum& InFrustum);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const AFrustum& Frustum;

    // 是否启用倒角：BevelRadius > 0
    bool bEnableBevel;

    float StartAngle;
    float EndAngle;
    float ArcAngleRadians;

    TArray<int32> EndCapConnectionPoints;

    // 保存侧边顶点环索引，用于倒角顶点复用
    TArray<int32> TopSideRing;      // 顶部侧边环（倒角连接点）
    TArray<int32> BottomSideRing;    // 底部侧边环（倒角连接点）

    // 保存端盖边缘顶点环索引，用于倒角顶点复用
    TArray<int32> TopCapRing;       // 顶部端盖边缘环（倒角连接点）
    TArray<int32> BottomCapRing;    // 底部端盖边缘环（倒角连接点）

    TArray<int32> GenerateVertexRing(float Radius, float Z, int32 Sides);
    TArray<int32> GenerateVertexRing(float Radius, float Z, int32 Sides, float VCoord, const FVector2D& UVOffset, const FVector2D& UVScale);
    void GenerateCapGeometry(float Z, int32 Sides, float Radius, EHeightPosition HeightPosition);
    void GenerateCapTrianglesFromRing(const TArray<int32>& CapRing, EHeightPosition HeightPosition);
    void GenerateBevelGeometry(EHeightPosition HeightPosition);
    void CalculateBevelPosition(bool bIsTop, int32 RingIndex, int32 BevelSections, float Theta, 
        float CenterZ, float CenterRadius, float BevelRadius, int32 SideIndex, float AngleStep,
        const TArray<int32>& SideRing, FVector& OutPosition);
    FVector CalculateBevelNormal(bool bIsTop, int32 RingIndex, int32 BevelSections, 
        const FVector& Position, float CenterZ, float CenterRadius, int32 SideIndex, float AngleStep,
        const TArray<int32>& SideRing);
    int32 CreateBevelVertex(bool bIsTop, int32 RingIndex, int32 BevelSections, 
        const FVector& Position, const FVector& Normal, float Alpha, float Radius, float HalfHeight,
        EHeightPosition HeightPosition, const TArray<int32>& SideRing, int32 SideIndex);
    float CalculateBentRadius(float BaseRadius, float HeightRatio);
    float CalculateBevelHeight(float Radius) const;
    float CalculateHeightRatio(float Z);
    float CalculateAngleStep(int32 Sides);
    void CalculateAngles();
    
    //~ Begin UV Functions
    FVector2D CalculateWallUV(float Angle, float Z) const;
    FVector2D CalculateCapUV(float Angle, float Radius, EHeightPosition HeightPosition) const;
    FVector2D CalculateBevelUV(float Angle, float Alpha, EHeightPosition HeightPosition, float Radius, float Z) const;

    void CreateSideGeometry();
    void GenerateEndCaps();
    void GenerateEndCap(float Angle, EEndCapType EndCapType);
    void GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, EEndCapType EndCapType, float Angle);

    void RecordEndCapConnectionPoint(int32 VertexIndex);
    void ClearEndCapConnectionPoints();
    const TArray<int32>& GetEndCapConnectionPoints() const;
    void Clear();

};