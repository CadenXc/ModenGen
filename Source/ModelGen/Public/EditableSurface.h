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

/** 曲线类型枚举 */
UENUM(BlueprintType)
enum class ESurfaceCurveType : uint8
{
    /** 标准曲线：路段一定被曲线经过，且路段可以影响曲线进入进出方向 */
    Standard UMETA(DisplayName = "标准曲线"),
    /** 平滑曲线：路点不一定被曲线经过 */
    Smooth UMETA(DisplayName = "平滑曲线")
};

/** 纹理映射模式枚举 */
UENUM(BlueprintType)
enum class ESurfaceTextureMapping : uint8
{
    /** 默认：基于实际距离的UV映射 */
    Default UMETA(DisplayName = "默认"),
    /** 拉伸：基于归一化距离的UV映射，纹理会被拉伸以覆盖整个路径 */
    Stretch UMETA(DisplayName = "拉伸")
};

USTRUCT(BlueprintType)
struct FSurfaceWaypoint
{
    GENERATED_BODY()

        /** 路点位置（相对于Actor） */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint", meta = (MakeEditWidget = true))
        FVector Position = FVector::ZeroVector;

    /** 该点的曲面宽度 */
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

UCLASS(BlueprintType, meta = (DisplayName = "Editable Surface"))
class MODELGEN_API AEditableSurface : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    AEditableSurface();

    //~ Begin AActor Interface
    virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
    //~ End AActor Interface

    //~ Begin Geometry Parameters
    /** 路点数组（控制点） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Waypoints",
        meta = (DisplayName = "路点", MakeEditWidget = true))
        TArray<FSurfaceWaypoint> Waypoints;

    /** 添加路点数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "2", ClampMax = "20", UIMin = "2", UIMax = "20", DisplayName = "路点计数(重置用)"))
        int32 WaypointCount = 3;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetWaypointCount(int32 NewWaypointCount);

    /** * 在编辑器中添加一个新的路点
     * 位置为最后一个路点的沿切线方向延伸 100 单位
     */
    UFUNCTION(CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "添加新路点 (+100)"))
        void AddNewWaypoint();

    /** 曲面宽度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "曲面宽度"))
        float SurfaceWidth = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetSurfaceWidth(float NewSurfaceWidth);

    /** 曲面厚度开关 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Thickness",
        meta = (DisplayName = "曲面厚度"))
        bool bEnableThickness = true;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetEnableThickness(bool bNewEnableThickness);

    /** 曲面厚度值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Thickness",
        meta = (ClampMin = "0.01", ClampMax = "100", UIMin = "0.01", UIMax = "100",
            DisplayName = "曲面厚度值", EditCondition = "bEnableThickness"))
        float ThicknessValue = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetThicknessValue(float NewThicknessValue);

    /** 曲面两侧平滑度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Smoothing",
        meta = (ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "5", DisplayName = "曲面两侧平滑度"))
        int32 SideSmoothness = 0;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetSideSmoothness(int32 NewSideSmoothness);

    /** 护坡参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "右侧护坡长度", EditCondition = "SideSmoothness >= 1"))
        float RightSlopeLength = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetRightSlopeLength(float NewValue);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "-9.0", ClampMax = "9.0", UIMin = "-9.0", UIMax = "9.0", DisplayName = "右侧护坡斜率", EditCondition = "SideSmoothness >= 1"))
        float RightSlopeGradient = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetRightSlopeGradient(float NewValue);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "左侧护坡长度", EditCondition = "SideSmoothness >= 1"))
        float LeftSlopeLength = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetLeftSlopeLength(float NewValue);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "-9.0", ClampMax = "9.0", UIMin = "-9.0", UIMax = "9.0", DisplayName = "左侧护坡斜率", EditCondition = "SideSmoothness >= 1"))
        float LeftSlopeGradient = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetLeftSlopeGradient(float NewValue);

    /** 样条线组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EditableSurface|Spline",
        meta = (DisplayName = "样条线组件"))
        USplineComponent* SplineComponent;

    /** 路径采样分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "4", ClampMax = "200", UIMin = "4", UIMax = "200", DisplayName = "路径采样分段数"))
        int32 PathSampleCount = 20;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetPathSampleCount(int32 NewPathSampleCount);

    /** 曲线类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (DisplayName = "曲线类型"))
        ESurfaceCurveType CurveType = ESurfaceCurveType::Standard;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetCurveType(ESurfaceCurveType NewCurveType);

    /** 纹理映射模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (DisplayName = "纹理映射"))
        ESurfaceTextureMapping TextureMapping = ESurfaceTextureMapping::Default;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetTextureMapping(ESurfaceTextureMapping NewTextureMapping);

    /** 更新函数 */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void UpdateSplineFromWaypoints();

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void UpdateWaypointsFromSpline();

    virtual void GenerateMesh() override;

public:
    virtual bool IsValid() const override;

    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;

private:
    bool TryGenerateMeshInternal();
    void InitializeDefaultWaypoints();
    void InitializeSplineComponent();
};