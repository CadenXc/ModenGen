// Copyright (c) 2024. All rights reserved.

#include "PolygonTorus.h"
#include "PolygonTorusBuilder.h"
#include "PolygonTorusParameters.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

APolygonTorus::APolygonTorus()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;
    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (MaterialFinder.Succeeded())
    {
        ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
    }

    RegenerateMesh();
}

void APolygonTorus::BeginPlay()
{
    Super::BeginPlay();
    RegenerateMesh();
}

void APolygonTorus::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RegenerateMesh();
}

void APolygonTorus::RegenerateMesh()
{
    ProceduralMesh->ClearAllMeshSections();

    if (!Parameters.IsValid())
    {
        return;
    }
    
    FPolygonTorusBuilder Builder(Parameters);
    FModelGenMeshData MeshData;
    
    if (Builder.Generate(MeshData))
    {
        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        
        if (ProceduralMesh->GetMaterial(0) == nullptr)
        {
            static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
            if (MaterialFinder.Succeeded())
            {
                ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
            }
        }
    
        ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}
