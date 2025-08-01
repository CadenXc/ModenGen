// Fill out your copyright notice in the Description page of Project Settings.

#include "RegularPolygonActor.h"

// Sets default values
ARegularPolygonActor::ARegularPolygonActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>("ProcMesh");
	RootComponent = ProcMesh;

    GeneratePolygon();
}

// Called when the game starts or when spawned
void ARegularPolygonActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARegularPolygonActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ARegularPolygonActor::GeneratePolygon()
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
    TArray<FLinearColor> Colors;

    // 中心点
    Vertices.Add(FVector::ZeroVector);
    Normals.Add(FVector::UpVector);
    UVs.Add(FVector2D(0.5f, 0.5f));

    // 生成外围顶点
    for (int32 i = 0; i < Sides; ++i) {
        float Angle = 2 * PI * i / Sides;
        Vertices.Add(FVector(
            Radius * FMath::Cos(Angle),
            Radius * FMath::Sin(Angle),
            0));
        Normals.Add(FVector::UpVector);
        UVs.Add(FVector2D(
            0.5f + 0.5f * FMath::Cos(Angle),
            0.5f + 0.5f * FMath::Sin(Angle)));
    }

    // 构建三角形（逆时针方向）
    for (int32 i = 1; i < Sides; ++i) {
        Triangles.Add(i + 1);
        Triangles.Add(i);
        Triangles.Add(0);
    }
    // 闭合多边形
    Triangles.Add(0);
    Triangles.Add(Sides);
    Triangles.Add(1);

    ProcMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
}

