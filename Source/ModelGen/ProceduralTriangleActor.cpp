// Fill out your copyright notice in the Description page of Project Settings.


#include "ProceduralTriangleActor.h"
#include "proceduralMeshComponent.h"

// Sets default values
AProceduralTriangleActor::AProceduralTriangleActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PMC = CreateDefaultSubobject<UProceduralMeshComponent>("ProcMesh");
	RootComponent = PMC;

	GenerateTriangle();
}

// Called when the game starts or when spawned
void AProceduralTriangleActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AProceduralTriangleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProceduralTriangleActor::GenerateTriangle()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;

	Vertices.Add(FVector(0.0f, 0.0f, 0.0f));
	Vertices.Add(FVector(0.0f, 1000.0f, 0.0f));
	Vertices.Add(FVector(1000.0f, 0.0f, 0.0f));

	Triangles.Add(0);
	Triangles.Add(1);
	Triangles.Add(2);

	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;

	PMC->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
}

