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
    int32 Sides = 8;  // 内外多边形使用相同的边数

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angular", meta = (UIMin = "0.0", UIMax = "360.0"))
    float ArcAngle = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triangulation")
    bool bUseTriangleMethod = true;  // 是否使用三角形方法连接内外多边形

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "0.0"))
    float ChamferRadius = 5.0f;  // 倒角半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer")
    bool bUseChamfer = false;  // 是否启用倒角

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "1"))
    int32 ChamferSections = 3;  // 倒角分段数，用于创建圆滑边缘

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "1"))
    int32 ChamferSegments = 5;  // 倒角段数
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

    void GenerateGeometry();
    void GenerateSideWalls(FMeshSection& Section);
    void GenerateTopCap(FMeshSection& Section);
    void GenerateBottomCap(FMeshSection& Section);
    void GenerateEndCaps(FMeshSection& Section);

    // 新增的三角形方法函数
    void GenerateTopCapWithTriangles(FMeshSection& Section);
    void GenerateBottomCapWithTriangles(FMeshSection& Section);
    void GenerateTopCapWithQuads(FMeshSection& Section);
    void GenerateBottomCapWithQuads(FMeshSection& Section);

    int32 AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV);
    void AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4);
    void AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3);

    // 计算圆环顶点
    void CalculateRingVertices(float Radius, int32 Sides, float Z, float ArcAngle, TArray<FVector>& OutVertices, TArray<FVector2D>& OutUVs, float UOffset = 0.0f);
    
    // 倒角相关函数
    void GenerateChamferedGeometry(FMeshSection& Section);
    void GenerateChamferedSideWalls(FMeshSection& Section);
    void GenerateChamferedTopCap(FMeshSection& Section);
    void GenerateChamferedBottomCap(FMeshSection& Section);
    void GenerateEdgeChamfers(FMeshSection& Section);
    void GenerateCornerChamfers(FMeshSection& Section);
    void GenerateEdgeChamfer(FMeshSection& Section, const FVector& RadialDir, 
                            float OriginalRadius, float ChamferedRadius, 
                            float OriginalZ, float ChamferedZ, 
                            bool bIsTop, bool bIsOuter, int32 SideIndex);
    void GenerateCornerChamfer(FMeshSection& Section, const FVector& RadialDir, 
                              float OriginalRadius, float ChamferedRadius, 
                              float OriginalZ, float ChamferedZ, 
                              bool bIsTop, bool bIsOuter, int32 SideIndex);
};