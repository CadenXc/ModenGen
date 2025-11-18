// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class ABevelCube;

struct FUnfoldedFace
{
    FVector Normal;
    FVector U_Axis;
    FVector V_Axis;
    FString Name;
};

class MODELGEN_API FBevelCubeBuilder : public FModelGenMeshBuilder
{
public:
    explicit FBevelCubeBuilder(const ABevelCube& InBevelCube);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const ABevelCube& BevelCube;

    bool bEnableBevel;

    float HalfSize;
    float InnerOffset;
    float BevelRadius;
    int32 BevelSegments;

    // --- Cache ---
    // 预计算的网格坐标（适用于 U 和 V 方向）
    // 包含了倒角细分和中间平面的跨度
    TArray<float> GridCoordinates;

    // --- Helpers ---
    void Clear();
    void PrecomputeGrid();

    /**
     * @brief 使用预计算网格生成单面几何体
     */
    void GenerateUnfoldedFace(const FUnfoldedFace& FaceDef);
};