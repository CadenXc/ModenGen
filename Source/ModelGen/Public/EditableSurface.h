// Copyright (c) 2024. All rights reserved.

/**
 * @file EditableSurface.h
 * @brief 可配置的程序化可编辑曲面生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "Components/SplineComponent.h"
#include "EditableSurface.generated.h"

class FEditableSurfaceBuilder;
struct FModelGenMeshData;

USTRUCT(BlueprintType)
struct FSurfaceWaypoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
    float Width = 5.0f;

    FSurfaceWaypoint()
        : Position(FVector::ZeroVector)
        , Width(5.0f)
    {}

    FSurfaceWaypoint(const FVector& InPosition, float InWidth)
        : Position(InPosition)
        , Width(InWidth)
    {}
};

UCLASS(BlueprintType, meta=(DisplayName = "Editable Surface"))
class MODELGEN_API AEditableSurface : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    AEditableSurface();

    //~ Begin Geometry Parameters
    /** 路点数组（控制点） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Waypoints",
        meta = (DisplayName = "路点"))
    TArray<FSurfaceWaypoint> Waypoints;

    /** 添加路点数量（2-20，默认3） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "2", ClampMax = "20", UIMin = "2", UIMax = "20", DisplayName = "添加路点"))
    int32 WaypointCount = 3;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetWaypointCount(int32 NewWaypointCount);

    /** 曲面宽度（0.01-1000，默认100） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "曲面宽度"))
    float SurfaceWidth = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetSurfaceWidth(float NewSurfaceWidth);

    /** 曲面厚度开关（默认开启，生成薄的长方体） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Thickness",
        meta = (DisplayName = "曲面厚度"))
    bool bEnableThickness = true;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetEnableThickness(bool bNewEnableThickness);

    /** 曲面厚度值（0.01-100，默认1.0，仅在厚度打开后可用） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Thickness",
        meta = (ClampMin = "0.01", ClampMax = "100", UIMin = "0.01", UIMax = "100", 
        DisplayName = "曲面厚度值", EditCondition = "bEnableThickness"))
    float ThicknessValue = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetThicknessValue(float NewThicknessValue);

    /** 曲面两侧平滑度（0-5，默认0，整数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Smoothing",
        meta = (ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "5", DisplayName = "曲面两侧平滑度"))
    int32 SideSmoothness = 0;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetSideSmoothness(int32 NewSideSmoothness);

    /** 右侧护坡长度（0.01-1000，默认100，需要平滑度≥1） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", 
        DisplayName = "右侧护坡长度", EditCondition = "SideSmoothness >= 1"))
    float RightSlopeLength = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetRightSlopeLength(float NewRightSlopeLength);

    /** 右侧护坡斜率（-9到9，默认1） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "-9.0", ClampMax = "9.0", UIMin = "-9.0", UIMax = "9.0", 
        DisplayName = "右侧护坡斜率", EditCondition = "SideSmoothness >= 1"))
    float RightSlopeGradient = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetRightSlopeGradient(float NewRightSlopeGradient);

    /** 左侧护坡长度（0.01-1000，默认100） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", 
        DisplayName = "左侧护坡长度", EditCondition = "SideSmoothness >= 1"))
    float LeftSlopeLength = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetLeftSlopeLength(float NewLeftSlopeLength);

    /** 左侧护坡斜率（-9到9，默认1） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "-9.0", ClampMax = "9.0", UIMin = "-9.0", UIMax = "9.0", 
        DisplayName = "左侧护坡斜率", EditCondition = "SideSmoothness >= 1"))
    float LeftSlopeGradient = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetLeftSlopeGradient(float NewLeftSlopeGradient);

    virtual void GenerateMesh() override;

    /** 样条线组件（用于定义路径） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EditableSurface|Spline",
        meta = (DisplayName = "样条线组件"))
    USplineComponent* SplineComponent;

    /** 路径采样分段数（用于从样条线生成网格，默认20） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "4", ClampMax = "200", UIMin = "4", UIMax = "200", DisplayName = "路径采样分段数"))
    int32 PathSampleCount = 20;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetPathSampleCount(int32 NewPathSampleCount);

    /** 从路点数组更新样条线 */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void UpdateSplineFromWaypoints();

    /** 从样条线更新路点数组 */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void UpdateWaypointsFromSpline();

private:
    bool TryGenerateMeshInternal();
    
    // 初始化默认路点
    void InitializeDefaultWaypoints();
    
    // 初始化样条线组件
    void InitializeSplineComponent();

public:
    virtual bool IsValid() const override;
    
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
};

