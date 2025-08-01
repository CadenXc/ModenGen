#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h" 
#include "ChamferPrism.generated.h"

UCLASS()
class MODELGEN_API AChamferPrism : public AActor
{
    GENERATED_BODY()

public:
    AChamferPrism();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism Parameters")
        float BottomRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism Parameters")
        float TopRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism Parameters")
        float Height = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism Parameters", meta = (ClampMin = "3"))
        int32 Sides = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism Parameters")
        float ChamferSize = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism Parameters", meta = (ClampMin = "1"))
        int32 ChamferSections = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prism Parameters")
        float RotationOffset = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Prism")
        void GeneratePrism(float BottomRad, float TopRad, float H, int32 NumSides, float Chamfer, int32 ChamferSects, float RotationOff = 0.0f);

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere)
        UProceduralMeshComponent* ProceduralMesh;

    int32 AddVertexInternal(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UV0,
        TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents,
        const FVector& Pos,
        const FVector& Normal,
        const FVector2D& UV
    );

    void AddQuadInternal(
        TArray<int32>& Triangles,
        int32 V1,
        int32 V2,
        int32 V3,
        int32 V4
    );

    void AddTriangleInternal(
        TArray<int32>& Triangles,
        int32 V1,
        int32 V2,
        int32 V3
    );

    int32 GetOrAddVertex(
        TMap<FVector, int32>& UniqueVerticesMap,
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UV0,
        TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents,
        const FVector& Pos,
        const FVector& Normal,
        const FVector2D& UV
    );

    void GeneratePolygonCap(
        TMap<FVector, int32>& UniqueVerticesMap,
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UV0,
        TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& Triangles,
        float ZHeight,
        float Radius,
        int32 NumSides,
        bool bIsTop,
        float InChamferSize,
        int32 InChamferSections,
        TArray<FVector>& OutCapPoints
    );

    void GenerateSides(
        TMap<FVector, int32>& UniqueVerticesMap,
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UV0,
        TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& Triangles,
        const TArray<FVector>& BottomPoints,
        const TArray<FVector>& TopPoints,
        float InChamferSize,
        int32 InChamferSections
    );
};