// Copyright (c) 2024. All rights reserved.

/**
 * @file FrustumBuilder.h
 * @brief 截锥体网格构建器
 *
 * 该类负责生成截锥体的几何数据，继承自FModelGenMeshBuilder。
 * 提供了完整的截锥体生成功能，包括侧面、顶底面和倒角处理。
 */

#pragma once

#include "CoreMinimal.h"
#include "FrustumParameters.h"
#include "ModelGenMeshBuilder.h"

/**
 * 截锥体网格构建器
 */
class MODELGEN_API FFrustumBuilder : public FModelGenMeshBuilder
{
public:
    explicit FFrustumBuilder(const FFrustumParameters& InParams);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    FFrustumParameters Params;

    // 角度相关的成员变量，避免重复计算
    float StartAngle;  // 起始角度（0°）
    float EndAngle;    // 结束角度（ArcAngle）
    float ArcAngleRadians;  // ArcAngle的弧度值

    // 侧边环的顶点索引，用于倒角弧线生成
    TArray<int32> SideBottomRing;
    TArray<int32> SideTopRing;

    // 新增：存储倒角和侧面的起点位置信息，避免重复计算
    TArray<int32> EndCapConnectionPoints;    // 存储所有端面连接点的位置信息

    // 通用方法：生成顶点环
    TArray<int32> GenerateVertexRing(float Radius, float Z, int32 Sides, float UVV);

    // 通用方法：生成顶底面几何体
    void GenerateCapGeometry(float Z, int32 Sides, float Radius, bool bIsTop);

    // 通用方法：生成倒角几何体
    void GenerateBevelGeometry(bool bIsTop);



    // 通用方法：计算弯曲后的半径
    float CalculateBentRadius(float BaseRadius, float HeightRatio);

    // 通用方法：计算倒角高度
    float CalculateBevelHeight(float Radius);

    // 通用方法：计算高度比例（用于UV坐标）
    float CalculateHeightRatio(float Z);

    // 通用方法：计算角度步长
    float CalculateAngleStep(int32 Sides);

    // 计算并设置角度相关的成员变量
    void CalculateAngles();

    void GenerateBaseGeometry();
    void CreateSideGeometry();
    void GenerateTopGeometry();
    void GenerateBottomGeometry();
    void GenerateTopBevelGeometry();
    void GenerateBottomBevelGeometry();
    void GenerateEndCaps();

    // 统一的端面生成函数，整合所有EndCap相关逻辑
    void GenerateEndCap(float Angle, const FVector& Normal, bool IsStart);

    // 端面三角形生成
    void GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, bool IsStart, float Angle);



    // 端面连接点管理
    void RecordEndCapConnectionPoint(int32 VertexIndex);
    void ClearEndCapConnectionPoints();
    const TArray<int32>& GetEndCapConnectionPoints() const;

    // 重写基类的Clear方法
    void Clear();
    
    /** 基于顶点位置的稳定UV映射 - 重写父类实现 */
    virtual FVector2D GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const override;
};
