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

    // 【修改】：改为 FVector 以存储各轴数据
    FVector HalfSize;
    FVector InnerOffset;

    float BevelRadius;
    int32 BevelSegments;

    // --- Cache ---
    // 【修改】：拆分为三个轴向的网格缓存
    TArray<float> GridX;
    TArray<float> GridY;
    TArray<float> GridZ;

    // --- Helpers ---
    void Clear();
    void PrecomputeGrids(); // 重命名为 PrecomputeGrids

    // 辅助函数：计算单轴网格
    void ComputeSingleAxisGrid(float AxisHalfSize, float AxisInnerOffset, TArray<float>& OutGrid);

    /**
     * @brief 生成单面几何体
     * @param FaceDef 面定义
     * @param GridU 水平方向使用的网格数据
     * @param GridV 垂直方向使用的网格数据
     * @param AxisIndexU U轴对应的索引 (0=X, 1=Y, 2=Z)，用于 UV 计算
     * @param AxisIndexV V轴对应的索引 (0=X, 1=Y, 2=Z)，用于 UV 计算
     */
    void GenerateUnfoldedFace(const FUnfoldedFace& FaceDef, const TArray<float>& GridU, const TArray<float>& GridV, int32 AxisIndexU, int32 AxisIndexV);
};