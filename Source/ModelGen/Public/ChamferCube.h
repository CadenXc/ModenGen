#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h" 
#include "Materials/MaterialInstanceDynamic.h"
#include "ChamferCube.generated.h"

UCLASS()
class MODELGEN_API AChamferCube : public AActor
{
	GENERATED_BODY()

public:
	AChamferCube();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters")
		float CubeSize = 100.0f; // 立方体总尺寸

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters")
		float CubeChamferSize = 10.0f; // 倒角大小

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters")
		int32 ChamferSections = 1; // 倒角分段数

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
		UMaterialInstanceDynamic* DynamicMaterial;

	UFUNCTION(BlueprintCallable, Category = "ChamferCube")
		void GenerateChamferedCube(float Size, float ChamferSize, int32 Sections);

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

	// Helper function to add a quad (two triangles)
	void AddQuadInternal(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3, int32 V4);


	// New functions for specific geometry generation parts
	void GenerateMainFaces(float InnerSize,
		TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents,
		const FVector InnerCorner[8]);

	void GenerateEdgeChamfers(float Size, float ChamferSize, int32 Sections,
		TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents,
		const FVector InnerCorner[8]);

	void GenerateCornerChamfers(float ChamferSize, int32 Sections,
		TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents,
		const FVector InnerCorner[8]);
};