#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "Components/SplineComponent.h"
#include "EditableSurface.generated.h"

class FEditableSurfaceBuilder;
struct FModelGenMeshData;

UENUM(BlueprintType)
enum class ESurfaceCurveType : uint8
{
    Standard UMETA(DisplayName = "标准曲线"),
    Smooth UMETA(DisplayName = "平滑曲线")
};

UENUM(BlueprintType)
enum class ESurfaceTextureMapping : uint8
{
    Default UMETA(DisplayName = "默认"),
    Stretch UMETA(DisplayName = "拉伸")
};

USTRUCT(BlueprintType)
struct FSurfaceWaypoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint", meta = (MakeEditWidget = true))
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint", meta = (UIMin = "-1.0"))
    float Width = -1.0f;

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

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Waypoints",
        meta = (DisplayName = "路点", MakeEditWidget = true))
    TArray<FSurfaceWaypoint> Waypoints;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EditableSurface|Waypoints")
    FVector GetWaypointPosition(int32 Index) const;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Waypoints")
    bool SetWaypointPosition(int32 Index, const FVector& NewPosition);

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Waypoints")
	void SetWaypointWidth(int32 Index, float NewWidth);

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Tools", meta = (DisplayName = "设置新增路点距离"))
	void SetNextWaypointDistance(float NewDistance);

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "添加新路点 (延伸)"))
	void AddNewWaypoint();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Tools",
        meta = (ClampMin = "0", ClampMax = "20", UIMin = "0", UIMax = "20", DisplayName = "删除路点索引"))
    int32 RemoveWaypointIndex = 0;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "删除路点"))
    void RemoveWaypoint();

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Tools", meta = (DisplayName = "删除路点(索引)"))
    void RemoveWaypointByIndex(int32 Index);

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "打印路点信息"))
    void PrintWaypointInfo();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "EditableSurface|Tools", meta = (DisplayName = "打印参数信息"))
    void PrintParametersInfo();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "0.01", ClampMax = "2000", UIMin = "0.01", UIMax = "1000", DisplayName = "曲面宽度"))
    float SurfaceWidth = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetSurfaceWidth(float NewSurfaceWidth);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "10.0", ClampMax = "1000.0", UIMin = "10.0", UIMax = "500.0", DisplayName = "采样精度(步长)"))
    float SplineSampleStep = 10.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetSplineSampleStep(float NewStep);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (ClampMin = "1.0", ClampMax = "5000.0", UIMin = "10.0", UIMax = "1000.0", DisplayName = "去环检测距离"))
    float LoopRemovalThreshold = 10.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetLoopRemovalThreshold(float NewValue);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Thickness",
        meta = (DisplayName = "曲面厚度"))
    bool bEnableThickness = false;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetEnableThickness(bool bNewEnableThickness);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Thickness",
        meta = (ClampMin = "0.01", ClampMax = "200", UIMin = "0.01", UIMax = "200",
            DisplayName = "曲面厚度值", EditCondition = "bEnableThickness"))
    float ThicknessValue = 20.0f;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetThicknessValue(float NewThicknessValue);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Smoothing",
        meta = (ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "5", DisplayName = "曲面两侧平滑度"))
    int32 SideSmoothness = 1;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetSideSmoothness(int32 NewSideSmoothness);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Slope",
        meta = (ClampMin = "0.01", ClampMax = "2000", UIMin = "0.01", UIMax = "1000", DisplayName = "右侧护坡长度", EditCondition = "SideSmoothness >= 1"))
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EditableSurface|Spline",
        meta = (DisplayName = "样条线组件"))
    USplineComponent* SplineComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (DisplayName = "曲线类型"))
    ESurfaceCurveType CurveType = ESurfaceCurveType::Smooth;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetCurveType(ESurfaceCurveType NewCurveType);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EditableSurface|Geometry",
        meta = (DisplayName = "纹理映射"))
    ESurfaceTextureMapping TextureMapping = ESurfaceTextureMapping::Default;

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void SetTextureMapping(ESurfaceTextureMapping NewTextureMapping);

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void UpdateSplineFromWaypoints();

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Parameters")
    void UpdateWaypointsFromSpline();

    UFUNCTION(BlueprintCallable, Category = "EditableSurface|Waypoints")
    void SetWaypoints(const FString& WaypointsString);

    virtual void GenerateMesh() override;

public:
    virtual bool IsValid() const override;

    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;

private:
    bool TryGenerateMeshInternal();
    void InitializeDefaultWaypoints();
    void RebuildSplineData();

    float NextWaypointDistance = 200.0f;

};