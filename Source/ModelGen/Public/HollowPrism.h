// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrism.h
 * @brief 可配置的空心棱柱生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的空心棱柱生成器。
 * 支持以下特性：
 * - 可配置的内外半径和高度
 * - 可调节的边数和弧角
 * - 可选的倒角效果
 * - 支持部分弧段生成
 * - 实时预览和更新
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

// Generated header must be the last include
#include "HollowPrism.generated.h"

// Forward declarations
class UProceduralMeshComponent;

/**
 * 空心棱柱网格数据结构
 * 用于存储和管理程序化生成的网格数据
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FMeshSection
{
    GENERATED_BODY()

public:
    //~ Begin Mesh Data Section
    /** 顶点位置数组 */
    TArray<FVector> Vertices;
    
    /** 三角形索引数组 */
    TArray<int32> Triangles;
    
    /** 顶点法线数组 */
    TArray<FVector> Normals;
    
    /** 顶点UV坐标数组 */
    TArray<FVector2D> UVs;
    
    /** 顶点颜色数组 */
    TArray<FLinearColor> VertexColors;
    
    /** 顶点切线数组 */
    TArray<FProcMeshTangent> Tangents;

public:
    //~ Begin Public Methods
    /** 清除所有网格数据 */
    void Clear();
    
    /** 预分配内存以提高性能 */
    void Reserve(int32 VertexCount, int32 TriangleCount);
    
    /** 验证网格数据的完整性和有效性 */
    bool IsValid() const;
    
    /** 获取顶点总数 */
    int32 GetVertexCount() const { return Vertices.Num(); }
    
    /** 获取三角形总数 */
    int32 GetTriangleCount() const { return Triangles.Num() / 3; }
};

/**
 * 空心棱柱生成参数结构
 * 包含控制空心棱柱形状和外观的所有参数
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FPrismParameters
{
    GENERATED_BODY()

public:
    //~ Begin Basic Geometry Section
    /** 内半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Inner Radius", ToolTip = "空心棱柱的内半径"))
    float InnerRadius = 50.0f;

    /** 外半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Outer Radius", ToolTip = "空心棱柱的外半径"))
    float OuterRadius = 100.0f;

    /** 高度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Height", ToolTip = "空心棱柱的高度"))
    float Height = 200.0f;

    /** 边数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", 
        DisplayName = "Sides", ToolTip = "空心棱柱的边数"))
    int32 Sides = 8;

    /** 弧角（度） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "360.0", UIMax = "360.0", 
        DisplayName = "Arc Angle", ToolTip = "空心棱柱的弧角（度）"))
    float ArcAngle = 360.0f;

    //~ Begin Chamfer Section
    /** 倒角半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Chamfer", 
        meta = (ClampMin = "0.0", UIMin = "0.0", 
        DisplayName = "Chamfer Radius", ToolTip = "倒角的半径"))
    float ChamferRadius = 5.0f;

    /** 倒角分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Chamfer", 
        meta = (ClampMin = "1", UIMin = "1", ClampMax = "20", UIMax = "10", 
        DisplayName = "Chamfer Sections", ToolTip = "倒角的分段数"))
    int32 ChamferSections = 4;

    //~ Begin Generation Options
    /** 是否使用三角形方法生成顶底面 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Options", 
        meta = (DisplayName = "Use Triangle Method", ToolTip = "使用三角形方法生成顶底面"))
    bool bUseTriangleMethod = false;

public:
    /** 验证参数的有效性 */
    bool IsValid() const;
    
    /** 获取壁厚 */
    float GetWallThickness() const;
    
    /** 检查是否为完整圆环 */
    bool IsFullCircle() const;
};

/**
 * 程序化生成的空心棱柱Actor
 * 支持实时参数调整和预览
 */
UCLASS(BlueprintType, meta=(DisplayName = "Hollow Prism"))
class MODELGEN_API AHollowPrism : public AActor
{
    GENERATED_BODY()

public:
    //~ Begin Constructor Section
    AHollowPrism();

protected:
    //~ Begin AActor Interface
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
    //~ Begin Public Methods
    /** 重新生成空心棱柱几何体 */
    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Generation", 
        meta = (DisplayName = "Regenerate"))
    void Regenerate();

    //~ Begin Parameters Section
    /** 空心棱柱生成参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Parameters")
    FPrismParameters Parameters;

    //~ Begin Components Section
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HollowPrism|Components")
    UProceduralMeshComponent* MeshComponent;

private:
    //~ Begin Geometry Generation Methods
    /** 生成空心棱柱几何体 */
    void GenerateGeometry();

    /** 验证和修正参数 */
    bool ValidateAndClampParameters();

    /** 计算预估的顶点数量 */
    int32 CalculateVertexCountEstimate() const;

    /** 计算预估的三角形数量 */
    int32 CalculateTriangleCountEstimate() const;

    /** 生成侧面墙壁 */
    void GenerateSideWalls(FMeshSection& Section);

    /** 生成顶面（使用四边形） */
    void GenerateTopCapWithQuads(FMeshSection& Section);

    /** 生成底面（使用四边形） */
    void GenerateBottomCapWithQuads(FMeshSection& Section);

    /** 更新网格组件 */
    void UpdateMeshComponent(const FMeshSection& MeshData);

    //~ Begin Chamfer Generation Methods
    /** 生成顶部倒角几何 */
    void GenerateTopChamferGeometry(FMeshSection& Section, float StartZ);

    /** 生成顶部内环倒角 */
    void GenerateTopInnerChamfer(FMeshSection& Section, float HalfHeight);

    /** 生成顶部外环倒角 */
    void GenerateTopOuterChamfer(FMeshSection& Section, float HalfHeight);

    /** 生成底部内环倒角 */
    void GenerateBottomInnerChamfer(FMeshSection& Section, float HalfHeight);

    /** 生成底部外环倒角 */
    void GenerateBottomOuterChamfer(FMeshSection& Section, float HalfHeight);

    //~ Begin End Cap Generation Methods
    /** 生成端盖 */
    void GenerateEndCaps(FMeshSection& Section);

    /** 生成单个端盖 */
    void GenerateEndCap(FMeshSection& Section, float Angle, const FVector& Normal, bool IsStart);

    //~ Begin Helper Methods
    /** 计算环形顶点 */
    void CalculateRingVertices(float Radius, int32 Sides, float Z, float ArcAngle,
                              TArray<FVector>& OutVertices, TArray<FVector2D>& OutUVs, float UVScale = 1.0f);

    /** 添加顶点到网格 */
    int32 AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV);

    /** 添加四边形到网格 */
    void AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4);

    /** 添加三角形到网格 */
    void AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3);

    /** 计算顶点法线 */
    FVector CalculateVertexNormal(const FVector& Vertex);
};