#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h" 
#include "ChamferCube.generated.h"

UCLASS()
class MODELGEN_API AChamferCube : public AActor
{
	GENERATED_BODY()

public:
	AChamferCube();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters")
		float CubeSize = 100.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters")
		float CubeChamferSize = 10.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters")
		int32 ChamferSections = 1;

	UFUNCTION(BlueprintCallable, Category = "ChamferCube")
		void GenerateChamferedCube(float Size, float ChamferSize, int32 Sections);

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere)
		UProceduralMeshComponent* ProceduralMesh;

	// Helper function to add a vertex and its attributes
	int32 AddVertexInternal(TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents,
		const FVector& Pos, const FVector& Normal, const FVector2D& UV);

	void AddQuadInternal(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3, int32 V4);

    int32 GetOrAddVertex(
        TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents, const FVector& Pos, const FVector& Normal, const FVector2D& UV );

    void GenerateMainFaces(
        TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents, TArray<int32>& Triangles, float HalfSize, float InnerOffset );

    void GenerateEdgeChamfers(
        TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents, TArray<int32>& Triangles, const TArray<FVector>& CorePoints, float ChamferSize, int32 Sections );

    void GenerateCornerChamfers(
        TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents, TArray<int32>& Triangles, const TArray<FVector>& CorePoints, float ChamferSize, int32 Sections
    );
};