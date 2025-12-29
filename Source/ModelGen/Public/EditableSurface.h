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

    /** 该路点的路面宽度。
     * - 如果值 <= 0 (如 -1.0)，则自动继承全局 SurfaceWidth。
     * - 如果值 > 0，则强制使用该宽度覆盖全局设置。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint", meta = (UIMin = "-1.0"))
    float Width = -1.0f; // 默认为 -1.0f，表示继承全局宽度

    FSurfaceWaypoint()
        : Position(FVector::ZeroVector), Width(-1.0f)
    {
    }

    FSurfaceWaypoint(const FVector& InPosition, float InWidth = -1.0f)
        : Position(InPosition), Width(InWidth)
    {
    }
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

    /** 获取指定索引的路点位置 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EditableSurface|Waypoints")
    FVector GetWaypointPosition(int32 Index) const;

    /** 设置指定索引的路点位置 */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Waypoints")
    bool SetWaypointPosition(int32 Index, const FVector& NewPosition);

    /** 添加一个新的路点 */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "添加新路点 (+100)"))
    void AddNewWaypoint();

    /** 要删除的路点索引 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Tools",
        meta = (ClampMin = "0", ClampMax = "20", UIMin = "0", UIMax = "20", DisplayName = "删除路点索引"))
    int32 RemoveWaypointIndex = 0;

    /** 删除指定位置的路点 */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "删除路点"))
    void RemoveWaypoint();

    /** 在编辑器中删除指定位置的路点 */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Tools", meta = (DisplayName = "删除路点(索引)"))
    void RemoveWaypointByIndex(int32 Index);

    /** 打印所有路点信息到日志 */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "打印路点信息"))
    void PrintWaypointInfo();

    /** 打印 EditableSurface 参数信息 */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "打印参数信息"))
    void PrintParametersInfo();

    /** 曲面宽度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "0.01", ClampMax = "2000", UIMin = "0.01", UIMax = "1000", DisplayName = "曲面宽度"))
    float SurfaceWidth = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetSurfaceWidth(float NewSurfaceWidth);

    /** 样条线采样步长（越小越平滑，但性能开销越大） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "10.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "500.0", DisplayName = "采样精度(步长)"))
    float SplineSampleStep = 50.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetSplineSampleStep(float NewStep);

    /** 几何去环阈值：用于检测和移除急转弯处的自交死结。
     * 值越大，去环越激进（会切掉更大的角）；值越小，保留的细节越多。
     * 对于护坡，实际使用的阈值会自动根据挤出长度动态放大，但此值为基础下限。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "1.0", ClampMax = "5000.0", UIMin = "10.0", UIMax = "1000.0", DisplayName = "去环检测距离"))
    float LoopRemovalThreshold = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetLoopRemovalThreshold(float NewValue);

    /** 曲面厚度开关 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Thickness",
        meta = (DisplayName = "曲面厚度"))
    bool bEnableThickness = false;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetEnableThickness(bool bNewEnableThickness);

    /** 曲面厚度值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Thickness",
        meta = (ClampMin = "0.01", ClampMax = "200", UIMin = "0.01", UIMax = "200",
            DisplayName = "曲面厚度值", EditCondition = "bEnableThickness"))
    float ThicknessValue = 20.0f;

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

    /** 从样条线更新路点 */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void UpdateWaypointsFromSpline();

    /** 通过字符串设置路点（格式：x1,y1,z1;x2,y2,z2;...） */
    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Waypoints")
    void SetWaypoints(const FString& WaypointsString);

    /** 安全设置：自动推开过近的路点，防止弯道自交 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Safety",
        meta = (DisplayName = "自动松弛路点"))
    bool bAutoRelaxWaypoints = true;

    /** 执行物理松弛算法 */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Safety")
    void RelaxWaypoints();

    virtual void GenerateMesh() override;

public:
    virtual bool IsValid() const override;

    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;

private:
    bool TryGenerateMeshInternal();
    void InitializeDefaultWaypoints();
};