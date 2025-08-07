// Frustum.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Frustum.generated.h"

// 简化的网格数据结构，兼容UE4.26.2
USTRUCT(BlueprintType)
struct FFrustumMeshData
{
    GENERATED_BODY()

    // 顶点数据
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    TArray<FVector> Vertices;

    // 三角形索引
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    TArray<int32> Triangles;

    // 法线数据
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    TArray<FVector> Normals;

    // UV坐标
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    TArray<FVector2D> UVs;

    // 顶点颜色
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    TArray<FLinearColor> VertexColors;

    // 切线数据
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    TArray<FProcMeshTangent> Tangents;

    // 材质索引 (简化版)
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    TArray<int32> MaterialIndices;

    // 统计信息
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    int32 VertexCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    int32 TriangleCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data")
    int32 MaterialCount = 0;

    // 清理函数
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

    // 预分配内存
    void Reserve(int32 InVertexCount, int32 InTriangleCount)
    {
        Vertices.Reserve(InVertexCount);
        Triangles.Reserve(InTriangleCount * 3);
        Normals.Reserve(InVertexCount);
        UVs.Reserve(InVertexCount);
        VertexColors.Reserve(InVertexCount);
        Tangents.Reserve(InVertexCount);
        MaterialIndices.Reserve(InTriangleCount);
    }

    // 添加顶点
    int32 AddVertex(const FVector& Position, const FVector& Normal = FVector::ZeroVector, 
                   const FVector2D& UV = FVector2D::ZeroVector, 
                   const FLinearColor& Color = FLinearColor::White)
    {
        int32 Index = Vertices.Num();
        Vertices.Add(Position);
        Normals.Add(Normal);
        UVs.Add(UV);
        VertexColors.Add(Color);
        Tangents.Add(FProcMeshTangent());
        VertexCount++;
        return Index;
    }

    // 添加三角形
    void AddTriangle(int32 V1, int32 V2, int32 V3, int32 MaterialIndex = 0)
    {
        Triangles.Add(V1);
        Triangles.Add(V2);
        Triangles.Add(V3);
        MaterialIndices.Add(MaterialIndex);
        TriangleCount++;
    }

    // 添加四边形 (分解为两个三角形)
    void AddQuad(int32 V1, int32 V2, int32 V3, int32 V4, int32 MaterialIndex = 0)
    {
        // 第一个三角形
        AddTriangle(V1, V2, V3, MaterialIndex);
        // 第二个三角形
        AddTriangle(V1, V3, V4, MaterialIndex);
    }

    // 验证数据完整性
    bool IsValid() const
    {
        return Vertices.Num() == VertexCount &&
               Triangles.Num() == TriangleCount * 3 &&
               MaterialIndices.Num() == TriangleCount;
    }
};

USTRUCT(BlueprintType)
struct FFrustumParameters
{
    GENERATED_BODY()

        // 基础几何参数
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float TopRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float BottomRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float Height = 200.0f;

    // 细分控制
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (ClampMin = "3"))
        int32 Sides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (ClampMin = "1"))
        int32 HeightSegments = 4;

    // 倒角参数 (基于Blender的BevelModifierData)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "0.0"))
        float ChamferRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "1"))
        int32 ChamferSections = 4;

    // 倒角突出参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (UIMin = "0.0", UIMax = "1.0"))
        float ChamferBulgeAmount = 0.2f;

    // 弯曲参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bending", meta = (UIMin = "-1.0", UIMax = "1.0"))
        float BendAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bending", meta = (ClampMin = "0.0"))
        float MinBendRadius = 1.0f;

    // 角度控制
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angular", meta = (UIMin = "0.0", UIMax = "360.0"))
        float ArcAngle = 360.0f;

    // 端面控制
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caps", meta = (ClampMin = "0.0"))
        float CapThickness = 10.0f;
};

UCLASS()
class MODELGEN_API AFrustum : public AActor
{
    GENERATED_BODY()

public:
    AFrustum();

    // 主要参数集
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum")
        FFrustumParameters Parameters;

    // 蓝图可调用生成函数
    UFUNCTION(BlueprintCallable, Category = "Frustum")
        void Regenerate();

    // 获取网格数据
    UFUNCTION(BlueprintPure, Category = "Frustum")
        const FFrustumMeshData& GetMeshData() const { return MeshData; }

protected:
    virtual void BeginPlay() override;
    virtual void PostLoad() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    virtual void Tick(float DeltaTime) override;

private:
    // 网格组件
    UPROPERTY(VisibleAnywhere)
        UProceduralMeshComponent* MeshComponent;

    // 简化的网格数据结构
    FFrustumMeshData MeshData;

    // 几何生成模块
    void GenerateGeometry();
    void CreateSideGeometry(float StartZ, float EndZ);
    void CreateTopGeometry(float Z);
    void CreateBottomGeometry(float Z);
    void CreateTopChamferGeometry(float StartZ);
    void CreateBottomChamferGeometry(float StartZ);
    void CreateEndCaps();
    void CreateEndCapTriangles(float Angle, const FVector& Normal, bool IsStart);
    void CreateChamferArcTriangles(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop);
    void CreateChamferArcTrianglesWithCaps(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop, int32 CenterVertex, int32 CapCenterVertex);

    // 顶点管理
    int32 AddVertex(const FVector& Position, const FVector& Normal = FVector::ZeroVector, 
                   const FVector2D& UV = FVector2D::ZeroVector, 
                   const FLinearColor& Color = FLinearColor::White);
    void AddQuad(int32 V1, int32 V2, int32 V3, int32 V4, int32 MaterialIndex = 0);
    void AddTriangle(int32 V1, int32 V2, int32 V3, int32 MaterialIndex = 0);

    // 材质应用
    void ApplyMaterial();

    // 法线验证和修复
    void ValidateAndFixNormals();
    void ValidateTriangleNormals();
    void TestNormalValidation();

    // 倒角几何体控制点结构
    struct FChamferArcControlPoints
    {
        FVector StartPoint;      // 起点：侧边顶点
        FVector EndPoint;        // 终点：顶/底面顶点
        FVector ControlPoint;    // 控制点：原始顶点位置
    };

    // 倒角几何体辅助函数
    FChamferArcControlPoints CalculateChamferControlPoints(const FVector& SideVertex, const FVector& TopBottomVertex);
    FVector CalculateChamferArcPoint(const FChamferArcControlPoints& ControlPoints, float t);
    FVector CalculateChamferArcTangent(const FChamferArcControlPoints& ControlPoints, float t);

    // 网格更新
    void UpdateProceduralMeshComponent();

    // 内部状态
    bool bGeometryDirty = true;
};