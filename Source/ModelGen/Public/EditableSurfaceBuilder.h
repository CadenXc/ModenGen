// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "EditableSurface.h"

class AEditableSurface;

class MODELGEN_API FEditableSurfaceBuilder : public FModelGenMeshBuilder
{
public:
    explicit FEditableSurfaceBuilder(const AEditableSurface& InSurface);

    //~ Begin FModelGenMeshBuilder Interface
    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;
    //~ End FModelGenMeshBuilder Interface

    void Clear();

private:
    const AEditableSurface& Surface;

    // Configuration
    TArray<FSurfaceWaypoint> Waypoints;
    float SurfaceWidth;
    bool bEnableThickness;
    float ThicknessValue;
    int32 SideSmoothness;
    float RightSlopeLength;
    float RightSlopeGradient;
    float LeftSlopeLength;
    float LeftSlopeGradient;

    // 用于厚度生成的横截面数据
    TArray<TArray<int32>> FrontCrossSections;
    int32 FrontVertexStartIndex = 0;
    int32 FrontVertexCount = 0;

    // Internal helpers
    void GenerateSurfaceMesh();
    
    // 使用Catmull-Rom样条插值计算路径点
    FVector InterpolatePathPoint(float Alpha) const;
    
    // 计算路径的切线方向
    FVector GetPathTangent(float Alpha) const;
    
    // 计算路径点的宽度
    float GetPathWidth(float Alpha) const;
    
    // 生成横截面顶点
    TArray<int32> GenerateCrossSection(const FVector& Center, const FVector& Forward, 
                                       const FVector& Up, float Width, int32 Segments);
    
    // 生成厚度（如果启用）
    void GenerateThickness();
};

