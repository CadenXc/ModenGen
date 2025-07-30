
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "CubeGenerator.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class MODELGEN_API ACubeGenerator : public AActor
{
    GENERATED_BODY()

public:
    ACubeGenerator();

    UPROPERTY(VisibleAnywhere)
        UProceduralMeshComponent* ProcMesh;

    UPROPERTY(EditAnywhere, Category = "Cube Settings")
        float Size = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Cube Settings", meta = (ClampMin = "0.1", ClampMax = "50.0"))
        float BevelRadius = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Cube Settings", meta = (ClampMin = "1", ClampMax = "8"))
        int32 BevelSegments = 4;

    UFUNCTION(BlueprintCallable, Category = "Generation")
        void GenerateBeveledCube();

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    void AddQuad(TArray<FVector>& Vertices, TArray<int32>& Triangles,
        const FVector& A, const FVector& B, const FVector& C, const FVector& D);
    void AddCorner(TArray<FVector>& Vertices, TArray<int32>& Triangles,
        const FVector& Center, const FVector& X, const FVector& Y, const FVector& Z);
};

