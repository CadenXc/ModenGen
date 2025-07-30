#include "CubeGenerator.h"
#include "KismetProceduralMeshLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"

ACubeGenerator::ACubeGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>("ProcMesh");
	RootComponent = ProcMesh;
	ProcMesh->CastShadow = true;
	ProcMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GenerateCube();
}

void ACubeGenerator::BeginPlay()
{
	Super::BeginPlay();
}

void ACubeGenerator::GenerateCube()
{
	FVector P0 = FVector(0.f, 0.f, 0.f);
	FVector P1 = FVector(0.f, 0.f, CubeSize);
	FVector P2 = FVector(CubeSize, 0.f, CubeSize);
	FVector P3 = FVector(CubeSize, 0.f, 0.f);
	FVector P4 = FVector(CubeSize, CubeSize, CubeSize);
	FVector P5 = FVector(CubeSize, CubeSize, 0.f);
	FVector P6 = FVector(0.f, CubeSize, CubeSize);
	FVector P7 = FVector(0.f, CubeSize, 0.f);

	// Front face
	CreateFace(P1, P2, P4, P6, 0);

	// Back face
	CreateFace(P3, P0, P7, P5, 1);

	// Left face
	CreateFace(P0, P1, P6, P7, 2);

	// Right face
	CreateFace(P2, P3, P5, P4, 3);

	// Top face
	CreateFace(P6, P4, P5, P7, 4);

	// Bottom face
	CreateFace(P0, P3, P2, P1, 5);
}

void ACubeGenerator::CreateFace(const FVector& BottomLeft, const FVector& BottomRight,
	const FVector& TopRight, const FVector& TopLeft, int32 SectionIndex)
{
	TArray<FVector> Vertices = { BottomLeft, TopLeft, TopRight, BottomRight };
	TArray<int32> Triangles = { 0,1,2, 0,2,3 };

	TArray<FVector> Normals;
	TArray<FColor> Colors;

	FVector InternalNormal = FVector::CrossProduct(Vertices[1] - Vertices[0], Vertices[2] - Vertices[0]).GetSafeNormal();

	FVector Normal = InternalNormal;
	const int32 VertexSize = 4;
	for (int32 i = 0; i < 4; i++)
	{
		Normals.Add(Normal);
		Colors.Add(FColor::White);
	}

	TArray<FVector2D> UVs;
	UVs.Add(FVector2D(0, 0));
	UVs.Add(FVector2D(1, 0));
	UVs.Add(FVector2D(1, 1));
	UVs.Add(FVector2D(0, 1));

	TArray<FProcMeshTangent> Tangents;
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

	ProcMesh->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
}
