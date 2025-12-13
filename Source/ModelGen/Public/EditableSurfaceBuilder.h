// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "EditableSurface.h"

class AEditableSurface;
class USplineComponent;

/**
 * [新增] 路径采样点结构 
 * 对应参考代码中的采样逻辑，存储样条线上的离散点信息
 */
struct FSurfaceSamplePoint
{
    FVector Location;       // 世界/局部位置
    FVector RightVector;    // 样条线右向量 (用于宽度扩展)
    FVector Normal;         // 样条线上向量 (用于护坡垂直偏移)
    FVector Tangent;        // 切线方向
    float Distance;         // 沿样条线的距离
    float Alpha;            // 归一化进度 (0~1)
};

/**
 * [新增] 横截面定义结构
 * 用于预计算路面+护坡的形状，相当于 Grid 的"列"定义
 */
struct FProfilePoint
{
    float OffsetH;  // 水平偏移量 (相对中心线，左负右正)
    float OffsetV;  // 垂直偏移量 (相对路面，向下为负)
    float U;        // 预计算的纹理 U 坐标
    bool bIsSlope;  // 是否属于护坡部分 (用于材质区分或平滑处理)
};

/**
 * [新增] 拐角几何数据结构
 * 用于存储每个采样点的 Miter 向量和缩放系数
 */
struct FCornerData
{
    FVector MiterVector;    // 真正的几何扩边方向（标准化）
    float MiterScale;       // 扩边缩放系数 (1/sin(angle/2))，用于路面
    bool bIsConvexLeft;     // 拐角朝向：true表示向左弯，false表示向右弯
    
    // 【新增】护坡专用缩放（内外弯补偿）
    float LeftSlopeMiterScale;   // 左侧护坡缩放
    float RightSlopeMiterScale;  // 右侧护坡缩放
    
    // --- [新增] 扇形塌缩核心数据 ---
    float TurnRadius;        // 瞬时转弯半径
    FVector RotationCenter;  // 转弯圆心 (塌缩的目标点)
    bool bIsSharpTurn;       // 是否是急弯
    
    // 默认构造函数，确保新字段被正确初始化
    FCornerData()
        : MiterVector(FVector::ZeroVector)
        , MiterScale(1.0f)
        , bIsConvexLeft(false)
        , LeftSlopeMiterScale(1.0f)
        , RightSlopeMiterScale(1.0f)
        , TurnRadius(FLT_MAX)          // 默认无限大
        , RotationCenter(FVector::ZeroVector)
        , bIsSharpTurn(false)
    {
    }
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
    bool bEnableThickness;
    float ThicknessValue;
    int32 SideSmoothness;
    float RightSlopeLength;
    float RightSlopeGradient;
    float LeftSlopeLength;
    float LeftSlopeGradient;
    ESurfaceTextureMapping TextureMapping;

    // --- [新增] 核心数据缓存 ---
    
    // 1. 路径采样数据 (Grid 的"行")
    TArray<FSurfaceSamplePoint> SampledPath;
    
    // 2. 横截面形状定义 (Grid 的"列")
    TArray<FProfilePoint> ProfileDefinition;
    
    // 3. 预计算的拐角数据 (大小与 SampledPath 一致)
    TArray<FCornerData> PathCornerData;

    // --- [新增] Grid 结构信息（用于厚度生成） ---
    int32 GridStartIndex = 0;  // Grid 网格的起始顶点索引
    int32 GridNumRows = 0;     // Grid 行数（路径采样点数）
    int32 GridNumCols = 0;     // Grid 列数（横截面点数，当前只有路面是2）
    
    // --- [保留] 旧数据成员（将在后续步骤中删除） ---
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

    // --- [新增] 内部核心流程函数 ---

    // 步骤 1: 沿样条线等距采样，填充 SampledPath
    void SampleSplinePath();

    // 步骤 2: 根据路宽和护坡参数，计算横截面形状，填充 ProfileDefinition
    void BuildProfileDefinition();
    
    // 步骤 2.5: 计算拐角几何 (核心 Miter 算法)
    void CalculateCornerGeometry();

    // 步骤 3: 双重循环 (Row * Col) 生成网格表面
    void GenerateGridMesh();

    // --- [保留] 旧函数（将在后续步骤中删除） ---
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

    // 递归函数：计算自适应采样点（基于曲率的递归细分）
    void RecursiveAdaptiveSampling(
        float StartDist, 
        float EndDist, 
        const FVector& StartTan, 
        const FVector& EndTan, 
        float AngleThresholdCos, 
        float MinStepLen,
        TArray<float>& OutDistanceSamples
    ) const;
};