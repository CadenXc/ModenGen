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
    explicit FFrustumBuilder(const FFrustumParameters& InParams);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    FFrustumParameters Params;

    void GenerateBaseGeometry();
    
    void CreateSideGeometry();

    void GenerateTopGeometry(float Z);
    
    void GenerateBottomGeometry(float Z);
    
    void GenerateTopBevelGeometry(float StartZ);
    
    void GenerateBottomBevelGeometry(float StartZ);
    
    void GenerateEndCaps();
    
    void GenerateEndCapTriangles(float Angle, const FVector& Normal, bool IsStart);
    
    void GenerateBevelArcTriangles(float Angle, const FVector& Normal, bool IsStart, 
                                    float Z1, float Z2, bool IsTop);
    
    void GenerateBevelArcTrianglesWithCaps(float Angle, const FVector& Normal, bool IsStart, 
                                            float Z1, float Z2, bool IsTop, int32 CenterVertex, 
                                            int32 CapCenterVertex);

    struct FBevelArcControlPoints
    {
        FVector StartPoint;      // 起点：侧边顶点
        FVector EndPoint;        // 终点：顶/底面顶点
        FVector ControlPoint;    // 控制点：原始顶点位置
    };

    FBevelArcControlPoints CalculateBevelControlPoints(const FVector& SideVertex, 
                                                          const FVector& TopBottomVertex);
    
    FVector CalculateBevelArcPoint(const FBevelArcControlPoints& ControlPoints, float t);
    
    FVector CalculateBevelArcTangent(const FBevelArcControlPoints& ControlPoints, float t);
};
