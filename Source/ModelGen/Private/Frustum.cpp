// Copyright (c) 2024. All rights reserved.

#include "Frustum.h"
#include "FrustumBuilder.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "HAL/PlatformTime.h"

AFrustum::AFrustum()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FrustumMesh"));
    RootComponent = ProceduralMesh;

    InitializeComponents();
}

void AFrustum::BeginPlay()
{
    Super::BeginPlay();
    RegenerateMesh();
}

void AFrustum::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RegenerateMesh();
}

#if WITH_EDITOR
void AFrustum::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FString PropertyNameStr = PropertyName.ToString();
    
    if (PropertyNameStr.StartsWith("Parameters.") || 
        PropertyName == "bGenerateCollision" || 
        PropertyName == "bUseAsyncCooking")
    {
        RegenerateMesh();
    }
    else if (PropertyName == "Material")
    {
        ApplyMaterial();
    }
}

void AFrustum::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FString PropertyNameStr = PropertyName.ToString();
    
    if (PropertyNameStr.StartsWith("Parameters.") || 
        PropertyName == "bGenerateCollision" || 
        PropertyName == "bUseAsyncCooking")
    {
        RegenerateMesh();
    }
    else if (PropertyName == "Material")
    {
        ApplyMaterial();
    }
}
#endif

void AFrustum::InitializeComponents()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMesh->SetSimulatePhysics(false);
        
        SetupCollision();
        ApplyMaterial();
    }
}

void AFrustum::ApplyMaterial()
{
    if (ProceduralMesh && Material)
    {
        ProceduralMesh->SetMaterial(0, Material);
    }
}

void AFrustum::SetupCollision()
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

void AFrustum::RegenerateMesh()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMesh component is null"));
        return;
    }

    if (!Parameters.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid Frustum parameters: TopRadius=%f, BottomRadius=%f, Height=%f, TopSides=%d, BottomSides=%d"), 
               Parameters.TopRadius, Parameters.BottomRadius, Parameters.Height, Parameters.TopSides, Parameters.BottomSides);
        return;
    }

    ProceduralMesh->ClearAllMeshSections();

    FFrustumBuilder Builder(Parameters);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        if (!MeshData.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Generated mesh data is invalid"));
            return;
        }

        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        ApplyMaterial();
        SetupCollision();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate Frustum mesh"));
    }
}

void AFrustum::GenerateFrustum(float TopRadius, float BottomRadius, float Height, int32 Sides)
{
    Parameters.TopRadius = TopRadius;
    Parameters.BottomRadius = BottomRadius;
    Parameters.Height = Height;
    Parameters.TopSides = Sides;
    Parameters.BottomSides = Sides;
    
    RegenerateMesh();
}

void AFrustum::GenerateFrustumWithDifferentSides(float TopRadius, float BottomRadius, float Height, 
                                                 int32 TopSides, int32 BottomSides)
{
    Parameters.TopRadius = TopRadius;
    Parameters.BottomRadius = BottomRadius;
    Parameters.Height = Height;
    Parameters.TopSides = TopSides;
    Parameters.BottomSides = BottomSides;
    
    RegenerateMesh();
}

void AFrustum::RegenerateMeshBlueprint()
{
    RegenerateMesh();
}

void AFrustum::SetMaterial(UMaterialInterface* NewMaterial)
{
    if (Material != NewMaterial)
    {
        Material = NewMaterial;
        ApplyMaterial();
    }
}