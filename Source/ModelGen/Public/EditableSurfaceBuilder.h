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

struct FProfilePoint
{
    float OffsetH;
    float OffsetV;
    float U;
    bool bIsSlope;
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

    /** 初始化采样数据（用于调试） */
    void InitializeForDebug();

    /** 打印防穿模调试信息 */
    void PrintAntiPenetrationDebugInfo();

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
    TArray<FProfilePoint> ProfileDefinition;
    TArray<FCornerData> PathCornerData;

    // Grid 结构信息（用于厚度生成）
    int32 GridStartIndex = 0;
    int32 GridNumRows = 0;
    int32 GridNumCols = 0;

    // 内部核心流程函数
    void SampleSplinePath();
    void BuildProfileDefinition();
    void CalculateCornerGeometry();
    void GenerateGridMesh();
    void GenerateThickness();
    void GenerateZipperThickness(); // [新增] 适配拉链法的厚度生成函数

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

    // 拉链法核心函数
    void GenerateZipperRoadMesh(); // 替代 GenerateGridMesh 的入口
    void BuildRawRails();          // 1. 构建原始边线
    void ResampleRails();          // 2. 根据物理长度重采样
    void StitchRails();            // 3. 缝合左右边线 (Zipper) - 保留兼容性
    void StitchRailsInternal(int32 LeftStartIdx, int32 RightStartIdx, int32 LeftCount, int32 RightCount, bool bReverseWinding = false); // 内部缝合函数
    
    // 拉链法：护坡生成
    void GenerateZipperSlopes(int32 LeftRoadStartIdx, int32 RightRoadStartIdx);
    void BuildSingleSideSlopeVertices(const TArray<FRailPoint>& InputPoints, bool bIsRightSide);
    
    // 基于参考轨道，向外挤出并生成清洗后的下一层轨道（逐层拉链法）
    void BuildNextSlopeRail(const TArray<FRailPoint>& ReferenceRail, TArray<FRailPoint>& OutSlopeRail, bool bIsRightSide, float OffsetH, float OffsetV);

    // 辅助：对一条点序列进行重采样
    void ResampleSingleRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float SegmentLength);
    
    // 辅助：简化边线（合并靠得太近的点）
    void SimplifyRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float MergeThreshold);
    
    // 辅助：去除几何环/尾巴（Path Shortcutting / Loop Removal）
    void RemoveGeometricLoops(TArray<FRailPoint>& InPoints, float Threshold);
};