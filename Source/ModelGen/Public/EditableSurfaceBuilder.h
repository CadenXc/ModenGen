// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "EditableSurface.h"

class AEditableSurface;
class USplineComponent;

struct FSurfaceSamplePoint
{
    FVector Location;
    FVector RightVector;
    FVector Normal;
    FVector Tangent;
    float Distance;
    float Alpha;
    float InterpolatedWidth; // 当前采样点的宽度
};

struct FCornerData
{
    FVector MiterVector;
    float MiterScale;
    bool bIsConvexLeft;
    float LeftSlopeMiterScale;
    float RightSlopeMiterScale;
    float TurnRadius;
    FVector RotationCenter;
    
    FCornerData()
        : MiterVector(FVector::ZeroVector)
        , MiterScale(1.0f)
        , bIsConvexLeft(false)
        , LeftSlopeMiterScale(1.0f)
        , RightSlopeMiterScale(1.0f)
        , TurnRadius(FLT_MAX)
        , RotationCenter(FVector::ZeroVector)
    {
    }
};

// 用于拉链法的边线点结构
struct FRailPoint
{
    FVector Position;
    FVector Normal;
    FVector Tangent;
    FVector2D UV;
    float DistanceAlongRail; // 沿着这条边线的累计物理距离

    FRailPoint() 
        : Position(FVector::ZeroVector), Normal(FVector::UpVector), Tangent(FVector::ForwardVector), UV(FVector2D::ZeroVector), DistanceAlongRail(0.f) 
    {}
};

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
    float SurfaceWidth;
    float SplineSampleStep;
    float LoopRemovalThreshold;
    bool bEnableThickness;
    float ThicknessValue;
    int32 SideSmoothness;
    float RightSlopeLength;
    float RightSlopeGradient;
    float LeftSlopeLength;
    float LeftSlopeGradient;
    ESurfaceTextureMapping TextureMapping;

    // 核心数据缓存
    TArray<FSurfaceSamplePoint> SampledPath;
    TArray<FCornerData> PathCornerData;

    // 内部核心流程函数
    void SampleSplinePath();
    void CalculateCornerGeometry();

    // 拉链法核心数据
    TArray<FRailPoint> LeftRailRaw;   // 原始左侧边线（基于样条采样）
    TArray<FRailPoint> RightRailRaw;  // 原始右侧边线（基于样条采样）
    TArray<FRailPoint> LeftRailResampled;  // 重采样后的左侧边线（基于物理长度）
    TArray<FRailPoint> RightRailResampled; // 重采样后的右侧边线（基于物理长度）
    
    // [新增] 用于追踪最终的最外侧轨道（用于生成侧壁）
    TArray<FRailPoint> FinalLeftRail;
    TArray<FRailPoint> FinalRightRail;
    
    // [新增] 用于记录起点和终点横截面的顶点索引（从左到右排序）
    TArray<int32> TopStartIndices;
    TArray<int32> TopEndIndices;

    // 核心生成函数
    void GenerateRoadSurface();
    void GenerateSlopes(int32 LeftRoadStartIdx, int32 RightRoadStartIdx);
    void GenerateThickness();
    
    // 结构化辅助函数
    FVector CalculateRawPointPosition(const FSurfaceSamplePoint& Sample, const FCornerData& Corner, float HalfWidth, bool bIsRightSide);
    FVector CalculateSurfaceNormal(const FRailPoint& Pt, bool bIsRightSide) const;
    void GenerateSingleSideSlope(const TArray<FRailPoint>& BaseRail, float Length, float Gradient, int32 StartIndex, bool bIsRightSide);
    void BuildSideWall(const TArray<FRailPoint>& Rail, bool bIsRightSide);
    void BuildCap(const TArray<int32>& Indices, bool bIsStartCap);
    
    // 辅助函数
    void BuildRawRails();
    void StitchRailsInternal(int32 LeftStartIdx, int32 RightStartIdx, int32 LeftCount, int32 RightCount, bool bReverseWinding = false);
    
    // 基于参考轨道，向外挤出并生成清洗后的下一层轨道（逐层拉链法）
    void BuildNextSlopeRail(const TArray<FRailPoint>& ReferenceRail, TArray<FRailPoint>& OutSlopeRail, bool bIsRightSide, float OffsetH, float OffsetV);

    // 辅助：对一条点序列进行重采样
    void ResampleSingleRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float SegmentLength);
    
    // 辅助：简化边线（合并靠得太近的点）
    void SimplifyRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float MergeThreshold);
    
    // 辅助：去除几何环/尾巴（Path Shortcutting / Loop Removal）
    void RemoveGeometricLoops(TArray<FRailPoint>& InPoints, float Threshold);
    
    // 辅助：应用单调性约束（防止顶点倒车）
    void ApplyMonotonicityConstraint(FVector& Pos, const FVector& LastValidPos, const FVector& Tangent);
};