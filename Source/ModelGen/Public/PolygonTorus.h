// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PolygonTorusParameters.h"
#include "ModelGenMeshData.h"
#include "PolygonTorus.generated.h"

UCLASS(BlueprintType, meta=(DisplayName = "Polygon Torus"))
class MODELGEN_API APolygonTorus : public AActor
{
    GENERATED_BODY()

public:
    APolygonTorus();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Parameters")
    FPolygonTorusParameters Parameters;

    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Generation")
    void RegenerateMesh();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "PolygonTorus|Components")
    UProceduralMeshComponent* ProceduralMesh;
};
