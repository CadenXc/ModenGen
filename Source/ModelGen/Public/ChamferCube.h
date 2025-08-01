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
		float CubeSize = 100.0f; // 立方体总尺寸

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters")
		float CubeChamferSize = 10.0f; // 倒角大小

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters")
		int32 ChamferSections = 1; // 倒角分段数

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

    // 提取的顶点管理函数
    int32 GetOrAddVertex(
        TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors,
        TArray<FProcMeshTangent>& Tangents, const FVector& Pos, const FVector& Normal, const FVector2D& UV );

    // 拆分的生成逻辑
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