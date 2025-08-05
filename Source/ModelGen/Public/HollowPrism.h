#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "HollowPrism.generated.h"

USTRUCT(BlueprintType)
struct FPrismParameters
{
    GENERATED_BODY()

        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float InnerRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float OuterRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float Height = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Polygons", meta = (ClampMin = "3"))
        int32 InnerSides = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Polygons", meta = (ClampMin = "3"))
        int32 OuterSides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angular", meta = (UIMin = "0.0", UIMax = "360.0"))
        float ArcAngle = 360.0f;
};

// 端面顶点结构体
struct FEndCapVertices
{
    int32 InnerTop;
    int32 InnerBottom;
    int32 OuterTop;
    int32 OuterBottom;
};

UCLASS()
class MODELGEN_API AHollowPrism : public AActor
{
    GENERATED_BODY()

public:
    AHollowPrism();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism")
        FPrismParameters Parameters;

    UFUNCTION(BlueprintCallable, Category = "Prism")
        void Regenerate();

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    UPROPERTY(VisibleAnywhere)
        UProceduralMeshComponent* MeshComponent;

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
    };

    // 主要几何生成函数
    void GenerateGeometry();
    void ValidateAndClampParameters();
    void ReserveMeshMemory(FMeshSection& MeshData);
    void CreateMeshFromData(const FMeshSection& MeshData);

    // 顶点和面片操作
    int32 AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV);
    void AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4);
    void AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3);

    // 侧面几何生成
    void GenerateSideGeometry(FMeshSection& Section);
    void GenerateVertexRings(FMeshSection& Section, float HalfHeight, float ArcRad,
        TArray<int32>& InnerRingTop, TArray<int32>& InnerRingBottom,
        TArray<int32>& OuterRingTop, TArray<int32>& OuterRingBottom);
    void GenerateSideQuads(FMeshSection& Section, 
        const TArray<int32>& InnerRingTop, const TArray<int32>& InnerRingBottom,
        const TArray<int32>& OuterRingTop, const TArray<int32>& OuterRingBottom);

    // 顶面和底面几何生成
    void GenerateTopGeometry(FMeshSection& Section);
    void GenerateBottomGeometry(FMeshSection& Section);
    void GenerateRingVertices(FMeshSection& Section, float Height, float ArcRad, 
        const FVector& Normal, TArray<int32>& InnerRing, TArray<int32>& OuterRing);
    void ConnectRingsWithTriangles(FMeshSection& Section, const TArray<int32>& InnerRing, 
        const TArray<int32>& OuterRing, bool bReverse);

    // 端面几何生成
    void GenerateEndCaps(FMeshSection& Section);

    // 材质应用
    void ApplyMaterial();
};