// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class ASphere;

class MODELGEN_API FSphereBuilder : public FModelGenMeshBuilder
{
public:
    explicit FSphereBuilder(const ASphere& InSphere);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

    void Clear();

private:
    const ASphere& Sphere;

    // Configuration
    float Radius;
    int32 Sides;
    float HorizontalCut;
    float VerticalCut;
    float ZOffset;

    // Internal helpers
    void GenerateSphereMesh();
    void GenerateCaps();
    void GenerateHorizontalCap(float Phi, bool bIsBottom);
    void GenerateVerticalCap(float Theta, bool bIsStart);

    // Geometry helpers
    FVector GetSpherePoint(float Theta, float Phi) const;
    FVector GetSphereNormal(float Theta, float Phi) const;

    // --- 安全构建辅助函数 ---
    // 检查并添加非退化三角形/四边形
    void SafeAddTriangle(int32 V0, int32 V1, int32 V2);
    void SafeAddQuad(int32 V0, int32 V1, int32 V2, int32 V3);
};