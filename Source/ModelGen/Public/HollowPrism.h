#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "HollowPrism.generated.h"

// 前向声明
class UProceduralMeshComponent;

// 网格数据结构
struct FMeshSection
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    void Clear()
    {
        Vertices.Reset();
        Triangles.Reset();
        Normals.Reset();
        UVs.Reset();
        VertexColors.Reset();
        Tangents.Reset();
    }

    void Reserve(int32 VertexCount, int32 TriangleCount)
    {
        Vertices.Reserve(VertexCount);
        Triangles.Reserve(TriangleCount);
        Normals.Reserve(VertexCount);
        UVs.Reserve(VertexCount);
        VertexColors.Reserve(VertexCount);
        Tangents.Reserve(VertexCount);
    }

    bool IsValid() const
    {
        return Vertices.Num() > 0 && Triangles.Num() > 0;
    }
};

USTRUCT(BlueprintType)
struct FPrismParameters
{
    GENERATED_BODY()

    /** 内半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism", meta = (ClampMin = "0.01"))
    float InnerRadius = 50.0f;

    /** 外半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism", meta = (ClampMin = "0.01"))
    float OuterRadius = 100.0f;

    /** 高度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism", meta = (ClampMin = "0.01"))
    float Height = 200.0f;

    /** 边数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism", meta = (ClampMin = "3"))
    int32 Sides = 8;

    /** 弧角（度） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism", meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float ArcAngle = 360.0f;

    /** 倒角半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "0.0"))
    float ChamferRadius = 5.0f;

    /** 倒角分段数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "1"))
    int32 ChamferSections = 4;

    /** 是否使用三角形方法生成顶底面 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism")
    bool bUseTriangleMethod = false;
};

UCLASS()
class MODELGEN_API AHollowPrism : public AActor
{
    GENERATED_BODY()

public:
    AHollowPrism();

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
    /** 重新生成几何 */
    UFUNCTION(BlueprintCallable, Category = "Prism")
    void Regenerate();

    /** 参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism")
    FPrismParameters Parameters;

    /** 网格组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProceduralMeshComponent* MeshComponent;

private:
    /** 生成几何 */
    void GenerateGeometry();

    /** 生成侧面 */
    void GenerateSideWalls(FMeshSection& Section);

    /** 生成顶面 */
    void GenerateTopCapWithQuads(FMeshSection& Section);

    /** 生成底面 */
    void GenerateBottomCapWithQuads(FMeshSection& Section);

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

    /** 生成端盖 */
    void GenerateEndCaps(FMeshSection& Section);

    /** 生成单个端盖 */
    void GenerateEndCap(FMeshSection& Section, float Angle, const FVector& Normal, bool IsStart);

    /** 计算环形顶点 */
    void CalculateRingVertices(float Radius, int32 Sides, float Z, float ArcAngle,
                              TArray<FVector>& OutVertices, TArray<FVector2D>& OutUVs, float UVScale = 1.0f);

    /** 添加顶点 */
    int32 AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV);

    /** 添加四边形 */
    void AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4);

    /** 添加三角形 */
    void AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3);

    /** 计算顶点法线 */
    FVector CalculateVertexNormal(const FVector& Vertex);
};