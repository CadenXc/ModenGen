// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "EditableSurface.h"

class AEditableSurface;
class USplineComponent;

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
    USplineComponent* SplineComponent;
    int32 PathSampleCount;
    float SurfaceWidth;
    bool bEnableThickness;
    float ThicknessValue;
    int32 SideSmoothness;
    float RightSlopeLength;
    float RightSlopeGradient;
    float LeftSlopeLength;
    float LeftSlopeGradient;
    ESurfaceTextureMapping TextureMapping;

    // 用于厚度生成的横截面数据
    TArray<TArray<int32>> FrontCrossSections;
    int32 FrontVertexStartIndex = 0;
    int32 FrontVertexCount = 0;

    // Helper struct for path sampling
    struct FPathSampleInfo
    {
        FVector Location;
        FVector Tangent; // Forward
        FVector Normal;  // Up
        FVector Binormal; // Right
        float DistanceAlongSpline;
    };

    // Internal helpers
    void GenerateSurfaceMesh();

    FPathSampleInfo GetPathSample(float Alpha) const;

    TArray<int32> GenerateCrossSection(
        const FPathSampleInfo& SampleInfo, 
        float RoadHalfWidth,      // 路面半宽（原始宽度的一半，不要在外面乘Scale）
        int32 SlopeSegments, 
        float Alpha = 0.0f,
        const FVector& MiterDir = FVector::ZeroVector,  // 预计算好的斜切方向
        float MiterScale = 1.0f                        // 预计算好的缩放 (1 / dot)
    );

    void GenerateThickness();

    // 自适应采样：根据切线夹角误差生成采样点
    void GetAdaptiveSamplePoints(TArray<float>& OutAlphas, float AngleThresholdDeg = 5.0f) const;

    // 斜切角处理：计算角平分线并调整顶点位置
    FVector CalculateMiterDirection(const FPathSampleInfo& PrevInfo, const FPathSampleInfo& CurrInfo, const FPathSampleInfo& NextInfo, bool bIsLeft) const;
};