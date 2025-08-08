// Copyright (c) 2024. All rights reserved.

/**
 * @file Pyramid.h
 * @brief 可配置的程序化金字塔生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的金字塔生成器。
 * 支持以下特性：
 * - 可配置的底面半径和高度
 * - 可调节的边数（3-100边）
 * - 可选的底部倒角效果
 * - 实时预览和更新
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

// Generated header must be the last include
#include "Pyramid.generated.h"

// Forward declarations
class FPyramidBuilder;
struct FPyramidGeometry;

/**
 * 金字塔几何数据结构
 * 用于存储和管理程序化生成的网格数据
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FPyramidGeometry
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
    TArray<FVector2D> UV0;
    
    /** 顶点颜色数组 */
    TArray<FColor> VertexColors;
    
    /** 顶点切线数组 */
    TArray<FProcMeshTangent> Tangents;

public:
    //~ Begin Public Methods
    /** 清除所有几何数据 */
    void Clear();
    
    /** 验证几何数据的完整性和有效性 */
    bool IsValid() const;
    
    /** 获取顶点总数 */
    int32 GetVertexCount() const { return Vertices.Num(); }
    
    /** 获取三角形总数 */
    int32 GetTriangleCount() const { return Triangles.Num() / 3; }
};

/**
 * 金字塔生成参数结构
 * 包含控制金字塔形状和外观的所有参数
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FPyramidBuildParameters
{
    GENERATED_BODY()

public:
    /** 底面半径 */
    float BaseRadius = 100.0f;
    
    /** 金字塔高度 */
    float Height = 200.0f;
    
    /** 底面边数 */
    int32 Sides = 4;
    
    /** 底部倒角半径 */
    float BevelRadius = 0.0f;

public:
    /** 验证参数的有效性 */
    bool IsValid() const;
    
    /** 获取倒角顶部半径 */
    float GetBevelTopRadius() const;
    
    /** 获取总高度 */
    float GetTotalHeight() const;
    
    /** 获取金字塔底面半径 */
    float GetPyramidBaseRadius() const;
    
    /** 获取金字塔底面高度 */
    float GetPyramidBaseHeight() const;
};

/**
 * 金字塔几何体生成器
 * 负责生成金字塔的几何数据，包括顶点、三角形、UV等
 */
class MODELGEN_API FPyramidBuilder
{
public:
    //~ Begin Constructor
    explicit FPyramidBuilder(const FPyramidBuildParameters& InParams);
    
    //~ Begin Public Methods
    /** 生成金字塔的几何体 */
    bool Generate(FPyramidGeometry& OutGeometry);

private:
    //~ Begin Private Members
    /** 生成参数 */
    FPyramidBuildParameters Params;

    //~ Begin Core Generation Methods
    /** 生成底部棱柱部分（用于倒角） */
    void GeneratePrismSection(FPyramidGeometry& Geometry);
    
    /** 生成金字塔锥形部分 */
    void GeneratePyramidSection(FPyramidGeometry& Geometry);
    
    /** 生成底面 */
    void GenerateBottomFace(FPyramidGeometry& Geometry);

    //~ Begin Vertex Management
    /** 添加顶点到几何体 */
    int32 GetOrAddVertex(FPyramidGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    
    /** 添加三角形到几何体 */
    void AddTriangle(FPyramidGeometry& Geometry, int32 V1, int32 V2, int32 V3);

    //~ Begin Geometry Helper Methods
    /** 生成圆形顶点 */
    TArray<FVector> GenerateCircleVertices(float Radius, float Z, int32 NumSides);
    
    /** 生成棱柱侧面 */
    void GeneratePrismSides(FPyramidGeometry& Geometry, const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal = false, float UVOffsetY = 0.0f, float UVScaleY = 1.0f);
    
    /** 生成多边形面 */
    void GeneratePolygonFace(FPyramidGeometry& Geometry, const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder = false, float UVOffsetZ = 0.0f);
};

/**
 * 程序化生成的金字塔Actor
 * 支持实时参数调整和预览
 */
UCLASS(BlueprintType, meta=(DisplayName = "Pyramid"))
class MODELGEN_API APyramid : public AActor
{
    GENERATED_BODY()

public:
    //~ Begin Constructor Section
    APyramid();

    //~ Begin Shape Parameters
    /** 底面半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters", 
        meta = (ClampMin = "1.0", ClampMax = "500.0", UIMin = "1.0", UIMax = "500.0", 
        DisplayName = "Base Radius", ToolTip = "金字塔底面的半径"))
    float BaseRadius = 100.0f;

    /** 金字塔高度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters", 
        meta = (ClampMin = "1.0", ClampMax = "500.0", UIMin = "1.0", UIMax = "500.0", 
        DisplayName = "Height", ToolTip = "金字塔的总高度"))
    float Height = 200.0f;

    /** 底面边数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters", 
        meta = (ClampMin = 3, UIMin = 3, ClampMax = 100, UIMax = 25, 
        DisplayName = "Sides", ToolTip = "金字塔底面的边数"))
    int32 Sides = 4;

    /** 底部倒角半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Parameters", 
        meta = (ClampMin = "0.0", UIMin = "0.0", 
        DisplayName = "Bevel Radius", ToolTip = "底部倒角的半径"))
    float BevelRadius = 0.0f;

    //~ Begin Material Section
    /** 应用于金字塔的材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Materials")
    UMaterialInterface* Material;

    //~ Begin Collision Section
    /** 是否生成碰撞体 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid|Collision")
    bool bGenerateCollision = true;

    //~ Begin Public Methods
    /** 重新生成金字塔几何体 */
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Generation", 
        meta = (DisplayName = "Regenerate Mesh"))
    void RegenerateMesh();

    /** 获取程序化网格组件 */
    UFUNCTION(BlueprintPure, Category = "Pyramid|Components", 
        meta = (DisplayName = "Get Procedural Mesh"))
    UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMesh; }

    /**
     * 使用指定参数生成金字塔
     * @param InBaseRadius - 底面半径
     * @param InHeight - 高度
     * @param InSides - 边数
     */
    UFUNCTION(BlueprintCallable, Category = "Pyramid|Generation", 
        meta = (DisplayName = "Generate Pyramid"))
    void GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides);

protected:
    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    //~ Begin Components Section
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, Category = "Pyramid|Components")
    UProceduralMeshComponent* ProceduralMesh;

    //~ Begin Private Methods
    /** 内部网格生成函数 */
    bool GenerateMeshInternal();
    
    /** 设置材质 */
    void SetupMaterial();
    
    /** 设置碰撞 */
    void SetupCollision();

    //~ Begin Private Members
    /** 几何体生成器 */
    TUniquePtr<FPyramidBuilder> Builder;
    
    /** 当前几何数据 */
    FPyramidGeometry CurrentGeometry;
};