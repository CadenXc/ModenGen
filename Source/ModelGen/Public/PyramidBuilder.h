// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class APyramid;

class MODELGEN_API FPyramidBuilder : public FModelGenMeshBuilder
{
public:
    explicit FPyramidBuilder(const APyramid& InPyramid);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

private:
    const APyramid& Pyramid;

    // Geometry Params
    float BaseRadius;
    float Height;
    int32 Sides;
    float BevelRadius;
    float BevelTopRadius; // 倒角结束处的半径（即金字塔主体底部的半径）

    // Cache
    TArray<float> CosValues;
    TArray<float> SinValues;

    // Internal States
    FVector TopPoint; // 金字塔尖端

    // Helpers
    void Clear();
    void PrecomputeMath();

    // 几何生成
    void GenerateBase();   // 底面
    void GenerateBevel();  // 底部倒角/裙边 (Skirt)
    void GenerateSides();  // 金字塔主体

    // 工具函数：根据索引获取环上的位置
    FVector GetRingPos(int32 Index, float Radius, float Z) const;
};