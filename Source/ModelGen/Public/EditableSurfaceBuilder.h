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
};