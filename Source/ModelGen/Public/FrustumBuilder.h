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
#include "ModelGenMeshBuilder.h"
#include "FrustumParameters.h"

/**
 * 截锥体网格构建器
 */
class MODELGEN_API FFrustumBuilder : public FModelGenMeshBuilder
{
public:
    //~ Begin Constructor
    explicit FFrustumBuilder(const FFrustumParameters& InParams);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    FFrustumParameters Params;

    /** 生成基础几何体 */
    void GenerateBaseGeometry();
    
    /** 生成侧面几何体 */
    void GenerateSideGeometry(float StartZ, float EndZ);
    
    /** 生成顶面几何体 */
    void GenerateTopGeometry(float Z);
    
    /** 生成底面几何体 */
    void GenerateBottomGeometry(float Z);
    
    /** 生成顶部倒角几何体 */
    void GenerateTopBevelGeometry(float StartZ);
    
    /** 生成底部倒角几何体 */
    void GenerateBottomBevelGeometry(float StartZ);
    
    /** 生成端面 */
    void GenerateEndCaps();
    
    /** 生成端面三角形 */
    void GenerateEndCapTriangles(float Angle, const FVector& Normal, bool IsStart);
    
    /** 生成倒角弧三角形 */
    void GenerateBevelArcTriangles(float Angle, const FVector& Normal, bool IsStart, 
                                    float Z1, float Z2, bool IsTop);
    
    /** 生成带端面的倒角弧三角形 */
    void GenerateBevelArcTrianglesWithCaps(float Angle, const FVector& Normal, bool IsStart, 
                                            float Z1, float Z2, bool IsTop, int32 CenterVertex, 
                                            int32 CapCenterVertex);

    //~ Begin Bevel Helper Structures
    /** 倒角几何体控制点结构 */
    struct FBevelArcControlPoints
    {
        FVector StartPoint;      // 起点：侧边顶点
        FVector EndPoint;        // 终点：顶/底面顶点
        FVector ControlPoint;    // 控制点：原始顶点位置
    };

    //~ Begin Bevel Helper Methods
    /** 计算倒角控制点 */
    FBevelArcControlPoints CalculateBevelControlPoints(const FVector& SideVertex, 
                                                          const FVector& TopBottomVertex);
    
    /** 计算倒角弧点 */
    FVector CalculateBevelArcPoint(const FBevelArcControlPoints& ControlPoints, float t);
    
    /** 计算倒角弧切线 */
    FVector CalculateBevelArcTangent(const FBevelArcControlPoints& ControlPoints, float t);
};
