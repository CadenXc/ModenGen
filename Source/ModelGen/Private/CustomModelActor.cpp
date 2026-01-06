// Copyright (c) 2024. All rights reserved.

#include "CustomModelActor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshActor.h"

ACustomModelActor::ACustomModelActor()
{
    PrimaryActorTick.bCanEverTick = true;

    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
    RootComponent = StaticMeshComponent;

    if (StaticMeshComponent)
    {
        StaticMeshComponent->SetVisibility(bShowStaticMesh);
        StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        StaticMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
    }
}

void ACustomModelActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    if (GetWorld())
    {
        GenerateMesh();
    }
}

void ACustomModelActor::BeginPlay()
{
    Super::BeginPlay();
    GenerateMesh();
}

void ACustomModelActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ACustomModelActor::GenerateMesh()
{
    TMap<FString, FString> EmptyParameters;
    AActor* ModelActor = UCustomModelFactory::CreateModelActorWithParams(ModelTypeName, EmptyParameters, GetWorld(), GetActorLocation(), GetActorRotation());
    
    if (ModelActor)
    {
        if (AProceduralMeshActor* ProcMeshActor = Cast<AProceduralMeshActor>(ModelActor))
        {
            ProcMeshActor->GenerateMesh();
            UStaticMesh* GeneratedStaticMesh = ProcMeshActor->ConvertProceduralMeshToStaticMesh();
            
            if (GeneratedStaticMesh && StaticMeshComponent)
            {
                StaticMeshComponent->SetStaticMesh(GeneratedStaticMesh);
                StaticMeshComponent->StreamingDistanceMultiplier = 10.0f;
                
                if (StaticMeshMaterial)
                {
                    StaticMeshComponent->SetMaterial(0, StaticMeshMaterial);
                }
            }
        }
        
        ModelActor->Destroy();
    }
}

void ACustomModelActor::SetModelType(const FString& NewModelTypeName)
{
    if (ModelTypeName != NewModelTypeName)
    {
        ModelTypeName = NewModelTypeName;
        GenerateMesh();
    }
}


void ACustomModelActor::UpdateStaticMesh()
{
    GenerateMesh();
}
