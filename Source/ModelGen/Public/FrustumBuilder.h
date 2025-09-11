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

    TArray<int32> SideBottomRing;
    TArray<int32> SideTopRing;
    TArray<int32> EndCapConnectionPoints;

    // [REVERTED] 移除了 TopCapRing 和 BottomCapRing，因为不再需要共享顶点

    TArray<int32> GenerateVertexRing(float Radius, float Z, int32 Sides);
    void GenerateCapGeometry(float Z, int32 Sides, float Radius, bool bIsTop);
    void GenerateBevelGeometry(bool bIsTop);
    float CalculateBentRadius(float BaseRadius, float HeightRatio);
    float CalculateBevelHeight(float Radius);
    float CalculateHeightRatio(float Z);
    float CalculateAngleStep(int32 Sides);
    void CalculateAngles();

    void CreateSideGeometry();
    void GenerateTopGeometry();
    void GenerateBottomGeometry();
    void GenerateTopBevelGeometry();
    void GenerateBottomBevelGeometry();
    void GenerateEndCaps();
    void GenerateEndCap(float Angle, const FVector& Normal, bool IsStart);
    void GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, bool IsStart, float Angle);

    void RecordEndCapConnectionPoint(int32 VertexIndex);
    void ClearEndCapConnectionPoints();
    const TArray<int32>& GetEndCapConnectionPoints() const;
    void Clear();

    // UV生成已移除 - 让UE4自动处理UV生成
};