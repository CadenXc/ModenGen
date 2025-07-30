#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubeGenerator.generated.h"

class UProceduralMeshComponent;

UCLASS()
class MODELGEN_API ACubeGenerator : public AActor
{
	GENERATED_BODY()

public:
	ACubeGenerator();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube")
		float CubeSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube")
		int32 ChamferDegree = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cube")
		float ChamferRadius = 0.1f;

protected:
	virtual void BeginPlay() override;

private:
	UProceduralMeshComponent* ProcMesh;

	void GenerateCube();
	void CreateFace(const FVector& BottomLeft, const FVector& BottomRight,
		const FVector& TopRight, const FVector& TopLeft, int32 SectionIndex);
};


