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
    void GenerateBevelGeometry(EHeightPosition HeightPosition);
    float CalculateBentRadius(float BaseRadius, float HeightRatio);
    float CalculateBevelHeight(float Radius);
    float CalculateHeightRatio(float Z);
    float CalculateAngleStep(int32 Sides);
    void CalculateAngles();

    void CreateSideGeometry();
    void GenerateEndCaps();
    void GenerateEndCap(float Angle, EEndCapType EndCapType);
    void GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, EEndCapType EndCapType, float Angle);

    void RecordEndCapConnectionPoint(int32 VertexIndex);
    void ClearEndCapConnectionPoints();
    const TArray<int32>& GetEndCapConnectionPoints() const;
    void Clear();

};