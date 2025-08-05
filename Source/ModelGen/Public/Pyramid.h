// Pyramid.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Pyramid.generated.h"

// ============================================================================
// 前向声明
// ============================================================================

class FPyramidBuilder;
struct FPyramidGeometry;

// ============================================================================
// 几何数据结构
// ============================================================================

struct MODELGEN_API FPyramidGeometry
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    void Clear();
    bool IsValid() const;
};

// ============================================================================
// 构建参数结构
// ============================================================================

struct MODELGEN_API FPyramidBuildParameters
{
    float BaseRadius = 100.0f;
    float Height = 200.0f;
    int32 Sides = 4;
    bool bCreateBottom = true;
    float BevelRadius = 0.0f;

    bool IsValid() const;
    float GetBevelTopRadius() const;
    float GetTotalHeight() const;
    float GetPyramidBaseRadius() const;
    float GetPyramidBaseHeight() const;
};

// ============================================================================
// 几何构建器
// ============================================================================

class MODELGEN_API FPyramidBuilder
{
public:
    FPyramidBuilder(const FPyramidBuildParameters& InParams);
    
    bool Generate(FPyramidGeometry& OutGeometry);

private:
    FPyramidBuildParameters Params;

    // 核心生成函数
    void GeneratePrismSection(FPyramidGeometry& Geometry);
    void GeneratePyramidSection(FPyramidGeometry& Geometry);
    void GenerateBottomFace(FPyramidGeometry& Geometry);
    
    // 辅助函数
    int32 GetOrAddVertex(FPyramidGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    void AddTriangle(FPyramidGeometry& Geometry, int32 V1, int32 V2, int32 V3);
    TArray<FVector> GenerateCircleVertices(float Radius, float Z, int32 NumSides);
    
    // 面生成函数
    void GeneratePrismSides(FPyramidGeometry& Geometry, const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal = false, float UVOffsetY = 0.0f, float UVScaleY = 1.0f);
    void GeneratePolygonFace(FPyramidGeometry& Geometry, const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder = false, float UVOffsetZ = 0.0f);
};

// ============================================================================
// 主Actor类
// ============================================================================

UCLASS()
class MODELGEN_API APyramid : public AActor
{
    GENERATED_BODY()

public:
    APyramid();

    // 可调节参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
    float BaseRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
    float Height = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters", meta = (ClampMin = 3, UIMin = 3))
    int32 Sides = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters")
    bool bCreateBottom = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters", meta = (ClampMin = "0.0"))
    float BevelRadius = 0.0f;

    // 材质和碰撞设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Collision")
    bool bGenerateCollision = true;

    // 蓝图接口
    UFUNCTION(BlueprintCallable, Category = "Pyramid")
    void RegenerateMesh();

    UFUNCTION(BlueprintPure, Category = "Pyramid")
    UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMesh; }

    UFUNCTION(BlueprintCallable, Category = "Pyramid")
    void GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides, bool bInCreateBottom);

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* ProceduralMesh;

    // 内部生成函数
    bool GenerateMeshInternal();
    void SetupMaterial();
    void SetupCollision();
    
    // 几何数据
    TUniquePtr<FPyramidBuilder> Builder;
    FPyramidGeometry CurrentGeometry;
};