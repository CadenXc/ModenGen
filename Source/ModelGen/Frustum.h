// Frustum.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Frustum.generated.h"

// 1. Frustum Parameters Structure
USTRUCT(BlueprintType)
struct FFrustumParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "0.01"))
    float TopRadius; // 上底半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "0.01"))
    float BottomRadius; // 下底半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "0.01"))
    float Height; // 高度

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "3"))
    int32 TopSides; // 上底边数 (至少3边)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "3"))
    int32 BottomSides; // 下底边数 (至少3边)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "0.0"))
    float ChamferRadius; // 倒角半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "1"))
    int32 ChamferSections; // 倒角分段数

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "1"))
    int32 ArcSegments; // 弧面段数 (侧面平行于底面的分段)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (UIMin = "-1.0", UIMax = "1.0"))
    float BendDegree; // 弯曲程度 (-1到1，向内到向外)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (ClampMin = "0.0"))
    float MinBendRadius; // 弯曲最小半径 (限制弯曲程度，防止自相交)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters", meta = (UIMin = "0.0", UIMax = "360.0"))
    float FrustumAngle; // 圆台角度 (0到360度)

    // Constructor to set default values
    FFrustumParameters()
        : TopRadius(50.0f)
        , BottomRadius(100.0f)
        , Height(200.0f)
        , TopSides(8)
        , BottomSides(16)
        , ChamferRadius(5.0f)
        , ChamferSections(2)
        , ArcSegments(4)
        , BendDegree(0.0f)
        , MinBendRadius(1.0f) // Should be greater than 0 to prevent division by zero or collapse
        , FrustumAngle(360.0f)
    {
    }
};

UCLASS()
class MODELGEN_API AFrustum : public AActor
{
    GENERATED_BODY()

public:
    AFrustum();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Parameters")
    FFrustumParameters FrustumParameters;

    UFUNCTION(BlueprintCallable, Category = "Frustum")
    void GenerateFrustum();

protected:
    virtual void BeginPlay() override;
    virtual void PostLoad() override;
    #if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    #endif

public:
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* ProceduralMesh;

    // Structure for storing mesh data
    struct FMeshData
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UV0;
        TArray<FLinearColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;
        // TMap<FVector, int32> UniqueVerticesMap; // Kept for reference, but not used for this procedural mesh for simplicity with hard edges/UVs
    };

    // Helper functions
    int32 AddVertexInternal(FMeshData& MeshData, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    void AddQuadInternal(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3, int32 V4);
    void AddTriangleInternal(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3);

    // Main geometry generation functions
    void GenerateGeometry(FMeshData& MeshData);
    void GenerateTopAndBottomFaces(FMeshData& MeshData);
    void GenerateSideFaces(FMeshData& MeshData); // Not currently implemented as a separate function, logic is in GenerateGeometry
    void GenerateChamfers(FMeshData& MeshData); // For top and bottom edges

    void SetupMaterial();
};