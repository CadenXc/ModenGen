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
    
    // 生成端盖
    void GenerateCaps();
    
    // 生成横截断端盖（底部）
    void GenerateHorizontalCap(float Phi, bool bIsBottom);
    
    // 生成竖截断端盖
    void GenerateVerticalCap(float Theta, bool bIsStart);
    
    // 生成球面顶点
    FVector GetSpherePoint(float Theta, float Phi) const;
    
    // 计算球面法线
    FVector GetSphereNormal(float Theta, float Phi) const;
    
    // 计算UV坐标
    FVector2D GetSphereUV(float Theta, float Phi) const;
};

