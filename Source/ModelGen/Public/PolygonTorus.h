// Copyright (c) 2024. All rights reserved.

/**
 * @file PolygonTorus.h
 * @brief 可配置的多边形圆环生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的多边形圆环生成器。
 * 支持以下特性：
 * - 可配置的主半径和次半径
 * - 可调节的主次分段数
 * - 多种光滑模式
 * - 多种UV映射模式
 * - 端盖生成选项
 * - 实时预览和更新
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

// Generated header must be the last include
#include "PolygonTorus.generated.h"

/**
 * 圆环端面填充类型
 * 定义端盖的填充方式
 */
UENUM(BlueprintType)
enum class ETorusFillType : uint8
{
    /* 不填充 */
    None        UMETA(DisplayName = "None"),
    
    /* 多边形填充 */
    NGon        UMETA(DisplayName = "NGon"),
    
    /* 三角形填充 */
    Triangles   UMETA(DisplayName = "Triangles")
};

/**
 * 圆环UV映射模式
 * 定义纹理坐标的生成方式
 */
UENUM(BlueprintType)
enum class ETorusUVMode : uint8
{
    /* 标准UV映射 */
    Standard    UMETA(DisplayName = "Standard"),
    
    /* 柱面UV映射 */
    Cylindrical UMETA(DisplayName = "Cylindrical"),
    
    /* 球面UV映射 */
    Spherical   UMETA(DisplayName = "Spherical")
};

/**
 * 圆环光滑模式
 * 定义法线光滑的计算方式
 */
UENUM(BlueprintType)
enum class ETorusSmoothMode : uint8
{
    /* 不平滑 */
    None        UMETA(DisplayName = "None"),
    
    /* 仅横截面光滑 */
    Cross       UMETA(DisplayName = "Cross Section Only"),
    
    /* 仅垂直截面光滑 */
    Vertical    UMETA(DisplayName = "Vertical Section Only"),
    
    /* 两个方向都光滑 */
    Both        UMETA(DisplayName = "Both Sections"),
    
    /* 自动检测 */
    Auto        UMETA(DisplayName = "Auto Detect")
};

/**
 * 程序化生成的多边形圆环Actor
 * 支持实时参数调整和预览
 */
UCLASS(BlueprintType, meta=(DisplayName = "Polygon Torus"))
class MODELGEN_API APolygonTorus : public AActor
{
    GENERATED_BODY()

public:
    //~ Begin Constructor Section
    APolygonTorus();

    //~ Begin Torus Parameters Section
    /** 主半径（圆环中心到截面中心的距离） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = "1.0", UIMin = "1.0", 
        DisplayName = "Major Radius", ToolTip = "Distance from torus center to section center"))
    float MajorRadius = 100.0f;

    /** 次半径（截面半径） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = "1.0", UIMin = "1.0", 
        DisplayName = "Minor Radius", ToolTip = "Section radius"))
    float MinorRadius = 25.0f;

    /** 主分段数（圆环分段数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = 3, UIMin = 3, ClampMax = 256, UIMax = 100, 
        DisplayName = "Major Segments", ToolTip = "Number of torus segments"))
    int32 MajorSegments = 8;

    /** 次分段数（截面分段数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = 3, UIMin = 3, ClampMax = 256, UIMax = 100, 
        DisplayName = "Minor Segments", ToolTip = "Number of section segments"))
    int32 MinorSegments = 4;

    /** 圆环角度（度数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = "1.0", UIMin = "1.0", ClampMax = "360.0", UIMax = "360.0", 
        DisplayName = "Torus Angle", ToolTip = "Torus angle in degrees"))
    float TorusAngle = 360.0f;

    //~ Begin Smoothing Section
    /** 光滑模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Mode", ToolTip = "Normal smoothing calculation method"))
    ETorusSmoothMode SmoothMode = ETorusSmoothMode::Both;

    /** 横切面光滑（向后兼容） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Cross Section", ToolTip = "Smooth cross sections"))
    bool bSmoothCrossSection = true;

    /** 竖面光滑（向后兼容） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Vertical Section", ToolTip = "Smooth vertical sections"))
    bool bSmoothVerticalSection = true;

    /** 生成光滑组 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Generate Smooth Groups", ToolTip = "Generate smooth groups"))
    bool bGenerateSmoothGroups = true;

    /** 生成硬边标记 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Generate Hard Edges", ToolTip = "Generate hard edge markers"))
    bool bGenerateHardEdges = true;

    /** 光滑角度阈值（度数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "180.0", UIMax = "180.0", 
        DisplayName = "Smoothing Angle", ToolTip = "Normal smoothing angle threshold"))
    float SmoothingAngle = 30.0f;

    //~ Begin Generation Options Section
    /** 端面填充类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "Fill Type", ToolTip = "End cap fill method"))
    ETorusFillType FillType = ETorusFillType::NGon;

    /** UV映射模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "UV Mode", ToolTip = "Texture coordinate generation method"))
    ETorusUVMode UVMode = ETorusUVMode::Standard;

    /** 是否生成UV坐标 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "Generate UVs", ToolTip = "Generate UV coordinates"))
    bool bGenerateUVs = true;

    /** 是否生成法线 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "Generate Normals", ToolTip = "Generate normal vectors"))
    bool bGenerateNormals = true;

    /** 是否生成切线 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "Generate Tangents", ToolTip = "Generate tangent vectors"))
    bool bGenerateTangents = true;

    //~ Begin End Caps Section
    /** 是否生成起始端盖 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|End Caps", 
        meta = (DisplayName = "Generate Start Cap", ToolTip = "Generate start end cap"))
    bool bGenerateStartCap = true;

    /** 是否生成结束端盖 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|End Caps", 
        meta = (DisplayName = "Generate End Cap", ToolTip = "Generate end end cap"))
    bool bGenerateEndCap = true;

    /** 端盖分段数（用于圆形端盖） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|End Caps", 
        meta = (ClampMin = 3, UIMin = 3, ClampMax = 64, UIMax = 32, 
        DisplayName = "Cap Segments", ToolTip = "Number of cap segments"))
    int32 CapSegments = 16;

    /** 是否使用圆形端盖 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|End Caps", 
        meta = (DisplayName = "Use Circular Caps", ToolTip = "Use circular end caps"))
    bool bUseCircularCaps = false;

    //~ Begin Public Methods Section
    /**
     * 生成多边形圆环
     * @param MajorRad - 主半径
     * @param MinorRad - 次半径
     * @param MajorSegs - 主分段数
     * @param MinorSegs - 次分段数
     * @param Angle - 圆环角度
     * @param bSmoothCross - 是否横切面光滑
     * @param bSmoothVertical - 是否竖面光滑
     */
    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Generation", 
        meta = (DisplayName = "Generate Polygon Torus"))
    void GeneratePolygonTorus(float MajorRad, float MinorRad, int32 MajorSegs, int32 MinorSegs, 
                             float Angle, bool bSmoothCross, bool bSmoothVertical);

    /**
     * 生成优化的圆环
     */
    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Generation", 
        meta = (DisplayName = "Generate Optimized Torus"))
    void GenerateOptimizedTorus();

    /**
     * 使用高级光滑控制生成圆环
     * @param InSmoothMode - 光滑模式
     * @param InSmoothingAngle - 光滑角度阈值
     */
    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Generation", 
        meta = (DisplayName = "Generate Torus With Smoothing"))
    void GenerateTorusWithSmoothing(ETorusSmoothMode InSmoothMode, float InSmoothingAngle = 30.0f);

    /**
     * 仅生成端盖
     */
    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|End Caps", 
        meta = (DisplayName = "Generate End Caps Only"))
    void GenerateEndCapsOnly();

    /**
     * 重新生成端盖
     */
    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|End Caps", 
        meta = (DisplayName = "Regenerate End Caps"))
    void RegenerateEndCaps();

protected:
    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    //~ Begin Components Section
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, Category = "PolygonTorus|Components")
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
