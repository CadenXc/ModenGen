// Copyright (c) 2024. All rights reserved.

#include "HollowPrism.h"
#include "HollowPrismBuilder.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/PlatformTime.h"

AHollowPrism::AHollowPrism()
{
    PrimaryActorTick.bCanEverTick = false;

    InitializeComponents();
    RegenerateMesh();
}

void AHollowPrism::BeginPlay()
{
    Super::BeginPlay();
    RegenerateMesh();
}

void AHollowPrism::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RegenerateMesh();
}

#if WITH_EDITOR
void AHollowPrism::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FString PropertyNameStr = PropertyName.ToString();
    
    if (PropertyNameStr.StartsWith("Parameters."))
    {
        RegenerateMesh();
        return;
    }
    
    static const TArray<FName> RelevantProperties = {
        "Material", "bGenerateCollision", "bUseAsyncCooking"
    };

    if (RelevantProperties.Contains(PropertyName))
    {
        if (PropertyName == "Material")
        {
            ApplyMaterial();
        }
        else
        {
            RegenerateMesh();
        }
    }
}

void AHollowPrism::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FString PropertyNameStr = PropertyName.ToString();
    
    if (PropertyNameStr.StartsWith("Parameters."))
    {
        RegenerateMesh();
        return;
    }
    
    static const TArray<FName> RelevantProperties = {
        "Material", "bGenerateCollision", "bUseAsyncCooking"
    };

    if (RelevantProperties.Contains(PropertyName))
    {
        if (PropertyName == "Material")
        {
            ApplyMaterial();
        }
        else
        {
            RegenerateMesh();
        }
    }
}
#endif

void AHollowPrism::InitializeComponents()
{
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HollowPrismMesh"));
    if (ProceduralMesh)
    {
        RootComponent = ProceduralMesh;
        
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMesh->SetSimulatePhysics(false);
        
        ApplyMaterial();
        SetupCollision();
    }
}

void AHollowPrism::ApplyMaterial()
{
    if (ProceduralMesh && Material)
    {
        ProceduralMesh->SetMaterial(0, Material);
    }
}

void AHollowPrism::SetupCollision()
{
    if (!ProceduralMesh)
    {
        return;
    }

    if (bGenerateCollision)
    {
        ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProceduralMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
    }
    else
    {
        ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void AHollowPrism::RegenerateMesh()
{
    if (!ProceduralMesh)
    {
        return;
    }

    static float LastUpdateTime = 0.0f;
    const float CurrentTime = FPlatformTime::Seconds();
    const float MinUpdateInterval = 0.05f;
    
    if (!Parameters.bDisableDebounce && CurrentTime - LastUpdateTime < MinUpdateInterval)
    {
        return;
    }
    
    LastUpdateTime = CurrentTime;

    static FHollowPrismParameters LastParameters;
    static UMaterialInterface* LastMaterial = nullptr;
    static bool bFirstGeneration = true;
    
    bool bParametersChanged = bFirstGeneration || LastParameters != Parameters;
    bool bMaterialChanged = bFirstGeneration || LastMaterial != Material;
    
    if (!bFirstGeneration && !bParametersChanged && !bMaterialChanged)
    {
        return;
    }
    
    LastParameters = Parameters;
    LastMaterial = Material;
    bFirstGeneration = false;

    ProceduralMesh->ClearAllMeshSections();

    FHollowPrismBuilder Builder(Parameters);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        MeshData.ToProceduralMesh(ProceduralMesh, 0);

        ApplyMaterial();
        SetupCollision();
        
        if (bMaterialChanged && !bParametersChanged)
        {
            ApplyMaterial();
        }
    }
}

void AHollowPrism::RegenerateMeshBlueprint()
{
    RegenerateMesh();
}

void AHollowPrism::SetMaterial(UMaterialInterface* NewMaterial)
{
    if (Material != NewMaterial)
    {
        Material = NewMaterial;
        ApplyMaterial();
    }
}