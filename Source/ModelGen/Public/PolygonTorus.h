#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PolygonTorus.generated.h"

UENUM(BlueprintType)
enum class ETorusFillType : uint8
{
    None        UMETA(DisplayName = "None"),
    NGon        UMETA(DisplayName = "NGon"),
    Triangles   UMETA(DisplayName = "Triangles")
};

UENUM(BlueprintType)
enum class ETorusUVMode : uint8
{
    Standard    UMETA(DisplayName = "Standard"),
    Cylindrical UMETA(DisplayName = "Cylindrical"),
    Spherical   UMETA(DisplayName = "Spherical")
};

UENUM(BlueprintType)
enum class ETorusSmoothMode : uint8
{
    None        UMETA(DisplayName = "None"),
    Cross       UMETA(DisplayName = "Cross Section Only"),
    Vertical    UMETA(DisplayName = "Vertical Section Only"),
    Both        UMETA(DisplayName = "Both Sections"),
    Auto        UMETA(DisplayName = "Auto Detect")
};

UCLASS()
class MODELGEN_API APolygonTorus : public AActor
{
    GENERATED_BODY()

public:
    APolygonTorus();

    // 圆环参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        float MajorRadius = 100.0f;  // 圆环中心到截面中心的距离

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        float MinorRadius = 25.0f;   // 截面半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        int32 MajorSegments = 8;     // 圆环分段数（多边形边数）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        int32 MinorSegments = 4;     // 截面分段数（多边形边数）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        float TorusAngle = 360.0f;   // 圆环角度（度数）

    // 光滑控制参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing")
        ETorusSmoothMode SmoothMode = ETorusSmoothMode::Both;  // 光滑模式

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing")
        bool bSmoothCrossSection = true;  // 横切面光滑（向后兼容）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing")
        bool bSmoothVerticalSection = true;  // 竖面光滑（向后兼容）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing")
        bool bGenerateSmoothGroups = true;  // 生成光滑组

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing")
        bool bGenerateHardEdges = true;  // 生成硬边标记

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing")
        float SmoothingAngle = 30.0f;  // 光滑角度阈值（度数）

    // 新增参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        ETorusFillType FillType = ETorusFillType::NGon;  // 端面填充类型

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        ETorusUVMode UVMode = ETorusUVMode::Standard;  // UV映射模式

    // 端盖控制参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "End Caps")
        bool bGenerateStartCap = true;  // 是否生成起始端盖

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "End Caps")
        bool bGenerateEndCap = true;  // 是否生成结束端盖

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "End Caps")
        int32 CapSegments = 16;  // 端盖分段数（用于圆形端盖）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "End Caps")
        bool bUseCircularCaps = false;  // 是否使用圆形端盖

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        bool bGenerateUVs = true;  // 是否生成UV坐标

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        bool bGenerateNormals = true;  // 是否生成法线

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        bool bGenerateTangents = true;  // 是否生成切线

    // 网格生成函数
    UFUNCTION(BlueprintCallable, Category = "Torus")
        void GeneratePolygonTorus(float MajorRad, float MinorRad, int32 MajorSegs, int32 MinorSegs, 
                                 float Angle, bool bSmoothCross, bool bSmoothVertical);

    // 优化的生成函数
    UFUNCTION(BlueprintCallable, Category = "Torus")
        void GenerateOptimizedTorus();

    // 高级光滑控制函数
    UFUNCTION(BlueprintCallable, Category = "Torus")
        void GenerateTorusWithSmoothing(ETorusSmoothMode InSmoothMode, float InSmoothingAngle = 30.0f);

    // 端盖生成函数
    UFUNCTION(BlueprintCallable, Category = "End Caps")
        void GenerateEndCapsOnly();

    // 重新生成端盖
    UFUNCTION(BlueprintCallable, Category = "End Caps")
        void RegenerateEndCaps();

    // 引擎事件
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere)
        UProceduralMeshComponent* ProceduralMesh;

    // 优化的辅助函数
    void GenerateOptimizedVertices(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        float MajorRad,
        float MinorRad,
        int32 MajorSegs,
        int32 MinorSegs,
        float AngleRad
    );

    void GenerateOptimizedTriangles(
        TArray<int32>& Triangles,
        int32 MajorSegs,
        int32 MinorSegs,
        bool bIsFullCircle
    );

    void GenerateEndCapsOptimized(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& Triangles,
        float MajorRad,
        float MinorRad,
        float AngleRad,
        int32 MajorSegs,
        int32 MinorSegs,
        ETorusFillType InFillType
    );

    // 高级端盖生成函数
    void GenerateAdvancedEndCaps(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& Triangles,
        float MajorRad,
        float MinorRad,
        float AngleRad,
        int32 MajorSegs,
        int32 MinorSegs,
        ETorusFillType InFillType,
        bool bGenerateInnerCaps,
        bool bGenerateOuterCaps
    );

    // 生成圆形端盖（用于特殊形状）
    void GenerateCircularEndCaps(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& Triangles,
        float MajorRad,
        float MinorRad,
        float AngleRad,
        int32 MajorSegs,
        int32 MinorSegs,
        int32 InCapSegments,
        bool bInGenerateStartCap,
        bool bInGenerateEndCap
    );

    void CalculateOptimizedNormals(
        TArray<FVector>& Normals,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        float MajorRad,
        float MinorRad,
        int32 MajorSegs,
        int32 MinorSegs,
        bool bSmoothCross,
        bool bSmoothVertical
    );

    // 新增的高级光滑计算函数
    void CalculateAdvancedSmoothing(
        TArray<FVector>& Normals,
        TArray<int32>& SmoothGroups,
        TArray<bool>& HardEdges,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        float MajorRad,
        float MinorRad,
        int32 MajorSegs,
        int32 MinorSegs,
        ETorusSmoothMode InSmoothMode,
        float InSmoothingAngle
    );

    void GenerateSmoothGroups(
        TArray<int32>& SmoothGroups,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        int32 MajorSegs,
        int32 MinorSegs,
        ETorusSmoothMode InSmoothMode
    );

    void GenerateHardEdges(
        TArray<bool>& HardEdges,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        int32 MajorSegs,
        int32 MinorSegs,
        ETorusSmoothMode InSmoothMode,
        float InSmoothingAngle
    );

    void GenerateUVsOptimized(
        TArray<FVector2D>& UVs,
        int32 MajorSegs,
        int32 MinorSegs,
        float AngleRad,
        ETorusUVMode InUVMode
    );

    // 参数验证
    void ValidateAndClampParameters(float& MajorRad, float& MinorRad, int32& MajorSegs, 
                                  int32& MinorSegs, float& Angle);

    // 调试和验证函数
    void ValidateMeshTopology(const TArray<FVector>& Vertices, const TArray<int32>& Triangles);
    void LogMeshStatistics(const TArray<FVector>& Vertices, const TArray<int32>& Triangles);

    // 旧版本函数（保持兼容性）
    void GeneratePolygonVertices(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        const FVector& Center,
        const FVector& Direction,
        const FVector& UpVector,
        float Radius,
        int32 Segments,
        float UOffset
    );

    void AddQuad(
        TArray<int32>& Triangles,
        int32 V0, int32 V1, int32 V2, int32 V3
    );

    void ValidateParameters(float& MajorRad, float& MinorRad, int32& MajorSegs, int32& MinorSegs, float& Angle);
    
    void GenerateSectionVertices(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& SectionStartIndices,
        float MajorRad,
        float MinorRad,
        int32 MajorSegs,
        int32 MinorSegs,
        float AngleStep
    );
    
    void GenerateSideTriangles(
        TArray<int32>& Triangles,
        const TArray<int32>& SectionStartIndices,
        int32 MajorSegs,
        int32 MinorSegs
    );
    
    void GenerateEndCaps(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& Triangles,
        const TArray<int32>& SectionStartIndices,
        float MajorRad,
        float AngleRad,
        int32 MajorSegs,
        int32 MinorSegs
    );
    
    void CalculateSmoothNormals(
        TArray<FVector>& Normals,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        const TArray<int32>& SectionStartIndices,
        float MajorRad,
        float AngleStep,
        int32 MajorSegs,
        int32 MinorSegs,
        bool bSmoothCross,
        bool bSmoothVertical
    );
    
    void ValidateNormalDirections(
        const TArray<FVector>& Vertices,
        const TArray<FVector>& Normals,
        const TArray<int32>& SectionStartIndices,
        float MajorRad,
        int32 MajorSegs,
        int32 MinorSegs
    );
};
