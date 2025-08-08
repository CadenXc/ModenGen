// Copyright (c) 2024. All rights reserved.

/**
 * @file Frustum.h
 * @brief 可配置的程序化截锥体生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的截锥体生成器。
 * 支持以下特性：
 * - 可配置的顶部和底部半径
 * - 可调节的高度和分段数
 * - 支持倒角和弯曲效果
 * - 可选的端面生成
 * - 实时预览和更新
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

// Generated header must be the last include
#include "Frustum.generated.h"

/**
 * 截锥体网格数据结构
 * 用于存储和管理程序化生成的网格数据
 */
USTRUCT(BlueprintType)
struct FFrustumMeshData
{
    GENERATED_BODY()

public:
    //~ Begin Mesh Data Section
    /** 顶点位置数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector> Vertices;

    /** 三角形索引数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Triangles")
    TArray<int32> Triangles;

    /** 顶点法线数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector> Normals;

    /** 顶点UV坐标数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector2D> UVs;

    /** 顶点颜色数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FLinearColor> VertexColors;

    /** 顶点切线数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FProcMeshTangent> Tangents;

    /** 三角形材质索引数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Materials")
    TArray<int32> MaterialIndices;

    //~ Begin Statistics Section
    /** 顶点总数 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 VertexCount = 0;

    /** 三角形总数 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 TriangleCount = 0;

    /** 材质总数 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 MaterialCount = 0;

public:
    //~ Begin Public Methods
    /** 清除所有网格数据 */
    void Clear()
    {
        Vertices.Reset();
        Triangles.Reset();
        Normals.Reset();
        UVs.Reset();
        VertexColors.Reset();
        Tangents.Reset();
        MaterialIndices.Reset();
        
        VertexCount = 0;
        TriangleCount = 0;
        MaterialCount = 0;
    }

    /**
     * 预分配网格数据内存
     * @param InVertexCount - 预期的顶点数量
     * @param InTriangleCount - 预期的三角形数量
     */
    void Reserve(int32 InVertexCount, int32 InTriangleCount)
    {
        Vertices.Reserve(InVertexCount);
        Triangles.Reserve(InTriangleCount * 3);  // 每个三角形3个顶点
        Normals.Reserve(InVertexCount);
        UVs.Reserve(InVertexCount);
        VertexColors.Reserve(InVertexCount);
        Tangents.Reserve(InVertexCount);
        MaterialIndices.Reserve(InTriangleCount);
    }

    /**
     * 添加一个新顶点到网格
     * @param Position - 顶点位置
     * @param Normal - 顶点法线
     * @param UV - 顶点UV坐标
     * @param Color - 顶点颜色
     * @return int32 - 新顶点的索引
     */
    int32 AddVertex(const FVector& Position, 
                   const FVector& Normal = FVector::ZeroVector, 
                   const FVector2D& UV = FVector2D::ZeroVector, 
                   const FLinearColor& Color = FLinearColor::White)
    {
        const int32 Index = Vertices.Num();
        Vertices.Add(Position);
        Normals.Add(Normal);
        UVs.Add(UV);
        VertexColors.Add(Color);
        Tangents.Add(FProcMeshTangent());
        VertexCount++;
        return Index;
    }

    /**
     * 添加一个三角形到网格
     * @param V1,V2,V3 - 三角形的三个顶点索引
     * @param MaterialIndex - 三角形使用的材质索引
     */
    void AddTriangle(int32 V1, int32 V2, int32 V3, int32 MaterialIndex = 0)
    {
        // 按逆时针顺序添加顶点索引
        Triangles.Add(V1);
        Triangles.Add(V2);
        Triangles.Add(V3);
        MaterialIndices.Add(MaterialIndex);
        TriangleCount++;
    }

    /**
     * 添加一个四边形到网格（自动分解为两个三角形）
     * @param V1,V2,V3,V4 - 四边形的四个顶点索引（按逆时针顺序）
     * @param MaterialIndex - 四边形使用的材质索引
     */
    void AddQuad(int32 V1, int32 V2, int32 V3, int32 V4, int32 MaterialIndex = 0)
    {
        // 将四边形分解为两个三角形
        AddTriangle(V1, V2, V3, MaterialIndex);  // 第一个三角形
        AddTriangle(V1, V3, V4, MaterialIndex);  // 第二个三角形
    }

    /**
     * 获取顶点总数
     * @return int32 - 顶点数量
     */
    int32 GetVertexCount() const { return VertexCount; }
    
    /**
     * 获取三角形总数
     * @return int32 - 三角形数量
     */
    int32 GetTriangleCount() const { return TriangleCount; }

    /**
     * 验证网格数据的完整性
     * @return bool - 如果数据完整且有效则返回true
     */
    bool IsValid() const
    {
        // 检查顶点数量是否匹配
        const bool bValidVertexCount = Vertices.Num() == VertexCount;
        
        // 检查三角形索引数量是否正确
        const bool bValidTriangleCount = Triangles.Num() == TriangleCount * 3;
        
        // 检查材质索引数量是否匹配
        const bool bValidMaterialCount = MaterialIndices.Num() == TriangleCount;
        
        return bValidVertexCount && bValidTriangleCount && bValidMaterialCount;
    }
};

/**
 * 截锥体生成参数结构
 * 包含控制截锥体形状和外观的所有参数
 */
USTRUCT(BlueprintType)
struct FFrustumParameters
{
    GENERATED_BODY()

public:
    //~ Begin Basic Geometry Parameters
    /** 顶部半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "1000.0", 
        DisplayName = "Top Radius", 
        ToolTip = "截锥体顶部的半径"))
    float TopRadius = 50.0f;

    /** 底部半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "1000.0", 
        DisplayName = "Bottom Radius", 
        ToolTip = "截锥体底部的半径"))
    float BottomRadius = 100.0f;

    /** 高度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "1000.0", 
        DisplayName = "Height", 
        ToolTip = "截锥体的总高度"))
    float Height = 200.0f;

    //~ Begin Tessellation Parameters
    /** 横向分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", UIMin = "3", UIMax = "64", 
        DisplayName = "Side Segments", 
        ToolTip = "截锥体周围的分段数，影响圆形的平滑度"))
    int32 Sides = 16;

    /** 纵向分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "1", UIMin = "1", UIMax = "32", 
        DisplayName = "Height Segments", 
        ToolTip = "截锥体高度方向的分段数"))
    int32 HeightSegments = 4;

    //~ Begin Bevel Parameters
    /** 倒角半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "50.0", 
        DisplayName = "Bevel Radius", 
        ToolTip = "边缘倒角的半径"))
    float BevelRadius = 5.0f;

    /** 倒角分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "1", UIMin = "1", UIMax = "8", 
        DisplayName = "Bevel Segments", 
        ToolTip = "倒角的分段数，影响倒角的平滑度"))
    int32 BevelSections = 4;

    /** 倒角突出量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (UIMin = "0.0", UIMax = "1.0", 
        DisplayName = "Bevel Bulge", 
        ToolTip = "倒角的突出程度，0为直线，1为最大突出"))
    float BevelBulgeAmount = 0.2f;

    //~ Begin Deformation Parameters
    /** 弯曲程度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Deformation", 
        meta = (UIMin = "-1.0", UIMax = "1.0", 
        DisplayName = "Bend Amount", 
        ToolTip = "截锥体的弯曲程度，负值向一个方向弯曲，正值向相反方向弯曲"))
    float BendAmount = 0.0f;

    /** 最小弯曲半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Deformation", 
        meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0", 
        DisplayName = "Min Bend Radius", 
        ToolTip = "弯曲时的最小半径，防止过度弯曲"))
    float MinBendRadius = 1.0f;

    //~ Begin Shape Control Parameters
    /** 弧形角度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Shape", 
        meta = (UIMin = "0.0", UIMax = "360.0", 
        DisplayName = "Arc Angle", 
        ToolTip = "截锥体的弧形角度，360度为完整圆形"))
    float ArcAngle = 360.0f;

    /** 端面厚度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Shape", 
        meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "50.0", 
        DisplayName = "Cap Thickness", 
        ToolTip = "顶部和底部端面的厚度"))
    float CapThickness = 10.0f;

public:
    /** 验证参数的有效性 */
    bool IsValid() const
    {
        return TopRadius > 0.0f &&
               BottomRadius > 0.0f &&
               Height > 0.0f &&
               Sides >= 3 &&
               HeightSegments >= 1 &&
               BevelRadius >= 0.0f &&
               BevelSections >= 1 &&
               BevelBulgeAmount >= 0.0f &&
               BevelBulgeAmount <= 1.0f &&
               MinBendRadius > 0.0f &&
               ArcAngle >= 0.0f &&
               ArcAngle <= 360.0f &&
               CapThickness >= 0.0f;
    }
};

/**
 * 程序化生成的截锥体Actor
 * 支持实时参数调整和预览
 */
UCLASS(BlueprintType, meta=(DisplayName = "Frustum"))
class MODELGEN_API AFrustum : public AActor
{
    GENERATED_BODY()

public:
    //~ Begin Constructor Section
    AFrustum();

    //~ Begin Parameters Section
    /** 截锥体的生成参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Parameters",
        meta = (ShowOnSpawn = true))
    FFrustumParameters Parameters;

    //~ Begin Public Methods
    /** 重新生成截锥体几何体 */
    UFUNCTION(BlueprintCallable, Category = "Frustum|Generation",
        meta = (DisplayName = "Regenerate Frustum"))
    void Regenerate();

    /** 获取当前的网格数据 */
    UFUNCTION(BlueprintPure, Category = "Frustum|Data",
        meta = (DisplayName = "Get Mesh Data"))
    const FFrustumMeshData& GetMeshData() const { return MeshData; }

protected:
    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void PostLoad() override;
    virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    //~ Begin Components Section
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, Category = "Frustum|Components")
    UProceduralMeshComponent* MeshComponent;

    /** 网格数据缓存 */
    FFrustumMeshData MeshData;

    //~ Begin Geometry Generation Section
    /** 生成完整的几何体 */
    void GenerateGeometry();

    /** 验证并修正生成参数 */
    void ValidateAndClampParameters();

    /** 准备几何数据，包括清理和预分配内存 */
    void PrepareGeometryData();

    /** 计算预估的顶点数量 */
    int32 CalculateVertexCountEstimate() const;

    /** 计算预估的三角形数量 */
    int32 CalculateTriangleCountEstimate() const;

    /** 生成几何体的各个部分 */
    void GenerateGeometryParts(float StartZ, float EndZ);

    /** 验证生成的几何数据 */
    bool ValidateGeneratedData();

    /** 生成侧面几何体 */
    void CreateSideGeometry(float StartZ, float EndZ);

    /** 生成顶部几何体 */
    void CreateTopGeometry(float Z);

    /** 生成底部几何体 */
    void CreateBottomGeometry(float Z);

    /** 生成顶部倒角几何体 */
    void CreateTopBevelGeometry(float StartZ);

    /** 生成底部倒角几何体 */
    void CreateBottomBevelGeometry(float StartZ);

    /** 生成端面几何体 */
    void CreateEndCaps();

    /** 生成端面三角形 */
    void CreateEndCapTriangles(float Angle, const FVector& Normal, bool IsStart);

    /** 生成倒角弧形三角形 */
    void CreateBevelArcTriangles(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop);

    /** 生成带端面的倒角弧形三角形 */
    void CreateBevelArcTrianglesWithCaps(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop, int32 CenterVertex, int32 CapCenterVertex);

    //~ Begin Vertex Management Section
    /** 添加顶点到网格 */
    int32 AddVertex(const FVector& Position, 
                   const FVector& Normal = FVector::ZeroVector, 
                   const FVector2D& UV = FVector2D::ZeroVector, 
                   const FLinearColor& Color = FLinearColor::White);

    /** 添加四边形到网格 */
    void AddQuad(int32 V1, int32 V2, int32 V3, int32 V4, int32 MaterialIndex = 0);

    /** 添加三角形到网格 */
    void AddTriangle(int32 V1, int32 V2, int32 V3, int32 MaterialIndex = 0);

    //~ Begin Material Section
    /** 应用材质到网格 */
    void ApplyMaterial();

    //~ Begin Validation Section
    /** 法线验证统计信息 */
    struct FNormalValidationStats
    {
        int32 InvalidNormals = 0;
        int32 ZeroNormals = 0;
        int32 FlippedNormals = 0;
        FVector AverageNormal = FVector::ZeroVector;
    };

    /** 验证并修复法线 */
    void ValidateAndFixNormals();

    /** 验证法线数据的完整性 */
    bool ValidateNormalDataIntegrity() const;

    /** 验证并修复顶点法线 */
    void ValidateVertexNormals(FNormalValidationStats& Stats);

    /** 修复零向量法线 */
    bool FixZeroNormal(FVector& Normal, const FVector& Vertex, FNormalValidationStats& Stats);

    /** 标准化法线 */
    bool NormalizeNormal(FVector& Normal, FNormalValidationStats& Stats);

    /** 修正法线方向 */
    void FixNormalDirection(FVector& Normal, const FVector& Vertex, float HalfHeight, FNormalValidationStats& Stats);

    /** 修正侧面法线方向 */
    void FixSideNormal(FVector& Normal, const FVector& Vertex, FNormalValidationStats& Stats);

    /** 修正顶面法线方向 */
    void FixTopNormal(FVector& Normal, FNormalValidationStats& Stats);

    /** 修正底面法线方向 */
    void FixBottomNormal(FVector& Normal, FNormalValidationStats& Stats);

    /** 输出法线验证统计信息 */
    void LogNormalValidationStats(FNormalValidationStats& Stats);

    /** 验证三角形法线 */
    void ValidateTriangleNormals();

    /** 验证三角形索引的有效性 */
    bool ValidateTriangleIndices(int32 BaseIndex) const;

    /** 检查顶点法线与面法线的一致性 */
    bool CheckVertexNormalConsistency(int32 V0, int32 V1, int32 V2, const FVector& FaceNormal) const;

    /** 输出三角形验证结果 */
    void LogTriangleValidationResults(int32 InconsistentTriangles, int32 DegenerateTriangles) const;

    /** 测试法线验证 */
    void TestNormalValidation();

    /** 测试法线点积计算 */
    void TestNormalDotProducts(const FVector TestNormals[3]);

    /** 测试法线标准化 */
    void TestNormalNormalization();

    //~ Begin Bevel Generation Section
    /**
     * 倒角控制点结构
     * 用于生成平滑的倒角曲线
     */
    struct FBevelArcControlPoints
    {
        FVector StartPoint;    // 起点：侧边顶点
        FVector EndPoint;      // 终点：顶/底面顶点
        FVector ControlPoint;  // 控制点：原始顶点位置
    };

    /** 计算倒角控制点 */
    FBevelArcControlPoints CalculateBevelControlPoints(const FVector& SideVertex, const FVector& TopBottomVertex);

    /** 计算倒角曲线上的点 */
    FVector CalculateBevelArcPoint(const FBevelArcControlPoints& ControlPoints, float t);

    /** 计算倒角曲线的切线 */
    FVector CalculateBevelArcTangent(const FBevelArcControlPoints& ControlPoints, float t);

    //~ Begin Mesh Update Section
    /** 更新程序化网格组件 */
    void UpdateProceduralMeshComponent();

    /** 更新网格组件 */
    void UpdateMeshComponent();

    //~ Begin State Management
    /** 几何体是否需要重新生成 */
    bool bGeometryDirty = true;
};