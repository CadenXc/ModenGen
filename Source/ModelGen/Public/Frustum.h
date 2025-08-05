// Frustum.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Frustum.generated.h"

// 前向声明
class FMeshBuilder;
class FGeometryGenerator;

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
    int32 TopSides = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (ClampMin = "3"))
    int32 BottomSides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (ClampMin = "1"))
    int32 HeightSegments = 4;



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
        
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caps")
    bool bEnableEndCaps = true;
        
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caps", meta = (ClampMin = "0.0"))
    float CapUVScale = 1.0f;
        
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caps")
    bool bFullCapCoverage = true;

    // 倒角参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "0.0"))
    float ChamferRadius = 5.0f;
        
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "1"))
    int32 ChamferSegments = 3;
        
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer")
    bool bEnableChamfer = true;
};

// 顶点数据结构
struct FVertex
{
    FVector Position;
    FVector Normal;
    FVector2D UV;
    FLinearColor Color;
    FProcMeshTangent Tangent;

    FVertex() : Position(FVector::ZeroVector), Normal(FVector::UpVector), UV(FVector2D::ZeroVector), Color(FLinearColor::White) {}
    FVertex(const FVector& InPosition, const FVector& InNormal, const FVector2D& InUV) 
        : Position(InPosition), Normal(InNormal), UV(InUV), Color(FLinearColor::White) {}
};

// 三角形数据结构
struct FTriangle
{
    int32 Indices[3];
    
    FTriangle() { Indices[0] = Indices[1] = Indices[2] = -1; }
    FTriangle(int32 A, int32 B, int32 C) { Indices[0] = A; Indices[1] = B; Indices[2] = C; }
};

// 四边形数据结构
struct FQuad
{
    int32 Indices[4];
    
    FQuad() { Indices[0] = Indices[1] = Indices[2] = Indices[3] = -1; }
    FQuad(int32 A, int32 B, int32 C, int32 D) { Indices[0] = A; Indices[1] = B; Indices[2] = C; Indices[3] = D; }
};

// 几何网格构建器
class FMeshBuilder
{
public:
    FMeshBuilder();
    ~FMeshBuilder();

    // 顶点管理
    int32 AddVertex(const FVector& Position, const FVector& Normal, const FVector2D& UV);
    int32 AddVertex(const FVertex& Vertex);
    
    // 图元管理
    void AddTriangle(int32 A, int32 B, int32 C);
    void AddQuad(int32 A, int32 B, int32 C, int32 D);
    void AddTriangle(const FTriangle& Triangle);
    void AddQuad(const FQuad& Quad);
    
    // 批量操作
    void AddTriangles(const TArray<FTriangle>& Triangles);
    void AddQuads(const TArray<FQuad>& Quads);
    
    // 网格构建
    void BuildMesh(UProceduralMeshComponent* MeshComponent);
    void Clear();
    
    // 内存管理
    void Reserve(int32 VertexCount, int32 TriangleCount);
    int32 GetVertexCount() const { return Vertices.Num(); }
    int32 GetTriangleCount() const { return Triangles.Num(); }

private:
    TArray<FVertex> Vertices;
    TArray<FTriangle> Triangles;
    
    void CalculateTangents();
};

// 几何生成器基类
class FGeometryGenerator
{
public:
    virtual ~FGeometryGenerator() = default;
    virtual void Generate(FMeshBuilder& Builder, const FFrustumParameters& Params) = 0;
};

// 侧面几何生成器
class FSideGeometryGenerator : public FGeometryGenerator
{
public:
    virtual void Generate(FMeshBuilder& Builder, const FFrustumParameters& Params) override;
    
private:
    void GenerateVertexRing(FMeshBuilder& Builder, const FFrustumParameters& Params, float Height, float Alpha);
    float CalculateRadius(const FFrustumParameters& Params, float Alpha);
    FVector CalculateNormal(const FFrustumParameters& Params, float Alpha, const FVector& Position);
};

// 顶部几何生成器
class FTopGeometryGenerator : public FGeometryGenerator
{
public:
    virtual void Generate(FMeshBuilder& Builder, const FFrustumParameters& Params) override;
};

// 底部几何生成器
class FBottomGeometryGenerator : public FGeometryGenerator
{
public:
    virtual void Generate(FMeshBuilder& Builder, const FFrustumParameters& Params) override;
};



// 端面几何生成器
class FEndCapGeometryGenerator : public FGeometryGenerator
{
public:
    virtual void Generate(FMeshBuilder& Builder, const FFrustumParameters& Params) override;
    
private:
    void GenerateEndCap(FMeshBuilder& Builder, const FFrustumParameters& Params, float Angle, bool bIsStart);
};

// 倒角几何生成器
class FChamferGeometryGenerator : public FGeometryGenerator
{
public:
    virtual void Generate(FMeshBuilder& Builder, const FFrustumParameters& Params) override;
    
private:
    // 主面生成
    void GenerateShrunkTopSurface(FMeshBuilder& Builder, const FFrustumParameters& Params);
    void GenerateShrunkBottomSurface(FMeshBuilder& Builder, const FFrustumParameters& Params);
    
    // 倒角弧面生成
    void GenerateTopChamferArc(FMeshBuilder& Builder, const FFrustumParameters& Params);
    void GenerateBottomChamferArc(FMeshBuilder& Builder, const FFrustumParameters& Params);
};

// 几何管理器
class FGeometryManager
{
public:
    FGeometryManager();
    ~FGeometryManager();
    
    void GenerateFrustum(FMeshBuilder& Builder, const FFrustumParameters& Params);
    
private:
    TArray<TUniquePtr<FGeometryGenerator>> Generators;
    void InitializeGenerators();
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

    // 几何管理器
    TUniquePtr<FGeometryManager> GeometryManager;
    
    // 网格构建器
    TUniquePtr<FMeshBuilder> MeshBuilder;

    // 几何生成
    void GenerateGeometry();
    void ApplyMaterial();

    // 内部状态
    bool bGeometryDirty = true;
};