// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "RegularPolygonActor.generated.h"

UCLASS()
class MODELGEN_API ARegularPolygonActor : public AActor
{
	GENERATED_BODY()
	
public:	

	// Sets default values for this actor's properties
	ARegularPolygonActor();

	UPROPERTY(EditAnywhere, Category = "Polygon", meta = (ClampMin = "3"))
		int32 Sides = 8;

	UPROPERTY(EditAnywhere, Category = "Polygon")
		float Radius = 100.f;

	UPROPERTY(VisibleAnywhere)
		UProceduralMeshComponent* ProcMesh;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void GeneratePolygon();

};
