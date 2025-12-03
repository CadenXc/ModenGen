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

    FSurfaceWaypoint()
        : Position(FVector::ZeroVector)
    {}

    FSurfaceWaypoint(const FVector& InPosition)
        : Position(InPosition)
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
    //~ End AActor Interface

    //~ Begin Geometry Parameters
    /** 路点数组（控制点） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Waypoints",
        meta = (DisplayName = "路点", MakeEditWidget = true))
    TArray<FSurfaceWaypoint> Waypoints;


    /** 获取指定索引的路点位置
     * @param Index 路点索引（0 开始）
     * @return 路点位置（相对于Actor），如果索引无效则返回零向量
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EditableSurface|Waypoints")
    FVector GetWaypointPosition(int32 Index) const;

    /** 设置指定索引的路点位置
     * @param Index 路点索引（0 开始）
     * @param NewPosition 新的路点位置（相对于Actor）
     * @return 是否设置成功（索引有效时返回true）
     */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Waypoints")
    bool SetWaypointPosition(int32 Index, const FVector& NewPosition);

    /** * 添加一个新的路点
     * 位置为最后一个路点的沿切线方向延伸 100 单位
     * 运行时可通过蓝图或C++调用
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "添加新路点 (+100)"))
        void AddNewWaypoint();

    /** 要删除的路点索引（0 开始，用于删除路点功能） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Tools",
        meta = (ClampMin = "0", ClampMax = "20", UIMin = "0", UIMax = "20", DisplayName = "删除路点索引"))
        int32 RemoveWaypointIndex = 0;

    /** * 删除指定位置的路点
     * 使用 RemoveWaypointIndex 属性作为要删除的路点索引
     * 运行时可通过蓝图或C++调用
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "删除路点"))
        void RemoveWaypoint();

    /** * 在编辑器中删除指定位置的路点（带参数版本，用于蓝图调用）
     * @param Index 要删除的路点索引（0 开始）
     */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Tools", meta = (DisplayName = "删除路点(索引)"))
        void RemoveWaypointByIndex(int32 Index);

    /** * 打印所有路点信息到日志
     * 运行时可通过蓝图或C++调用
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "打印路点信息"))
        void PrintWaypointInfo();

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
    int32 SideSmoothness = 1;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetSideSmoothness(int32 NewSideSmoothness);

    /** 护坡参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "右侧护坡长度", EditCondition = "SideSmoothness >= 1"))
    float RightSlopeLength = 30.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetRightSlopeLength(float NewValue);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "-9.0", ClampMax = "9.0", UIMin = "-9.0", UIMax = "9.0", DisplayName = "右侧护坡斜率", EditCondition = "SideSmoothness >= 1"))
    float RightSlopeGradient = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
        void SetRightSlopeGradient(float NewValue);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "左侧护坡长度", EditCondition = "SideSmoothness >= 1"))
    float LeftSlopeLength = 30.0f;

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

    /** 采样间距（单位：厘米）
     * 用于动态计算采样分段数，基于样条线长度自动调整分段密度
     * 较小的值会产生更密集的采样点，改善急转弯处的几何质量
     * 值越小 = 网格越密（越平滑），值越大 = 网格越稀疏（性能越好）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "10.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "1000.0", DisplayName = "采样间距"))
    float SampleSpacing = 50.0f;

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

    virtual void GenerateMesh() override;

public:
    virtual bool IsValid() const override;
    
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;

private:
    bool TryGenerateMeshInternal();
    void InitializeDefaultWaypoints();
};