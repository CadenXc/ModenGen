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
    
    if (PropertyNameStr.StartsWith("InnerRadius") || PropertyNameStr.StartsWith("OuterRadius") || 
        PropertyNameStr.StartsWith("Height") || PropertyNameStr.StartsWith("Sides") || 
        PropertyNameStr.StartsWith("OuterSides") || PropertyNameStr.StartsWith("InnerSides") || 
        PropertyNameStr.StartsWith("ArcAngle") || PropertyNameStr.StartsWith("BevelRadius") || 
        PropertyNameStr.StartsWith("BevelSegments") || PropertyNameStr.StartsWith("bUseTriangleMethod") || 
        PropertyNameStr.StartsWith("bFlipNormals") || PropertyNameStr.StartsWith("bDisableDebounce"))
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
    
    if (PropertyNameStr.StartsWith("InnerRadius") || 
        PropertyNameStr.StartsWith("OuterRadius") || 
        PropertyNameStr.StartsWith("Height") || 
        PropertyNameStr.StartsWith("Sides") || 
        PropertyNameStr.StartsWith("OuterSides") || 
        PropertyNameStr.StartsWith("InnerSides") || 
        PropertyNameStr.StartsWith("ArcAngle") || 
        PropertyNameStr.StartsWith("BevelRadius") || 
        PropertyNameStr.StartsWith("BevelSegments") || 
        PropertyNameStr.StartsWith("bUseTriangleMethod") || 
        PropertyNameStr.StartsWith("bFlipNormals") || 
        PropertyNameStr.StartsWith("bDisableDebounce"))
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
    
    if (!bDisableDebounce && CurrentTime - LastUpdateTime < MinUpdateInterval)
    {
        return;
    }
    
    LastUpdateTime = CurrentTime;

    static AHollowPrism* LastHollowPrism = nullptr;
    static UMaterialInterface* LastMaterial = nullptr;
    static bool bFirstGeneration = true;
    
    bool bParametersChanged = bFirstGeneration || LastHollowPrism != this;
    bool bMaterialChanged = bFirstGeneration || LastMaterial != Material;
    
    if (!bFirstGeneration && !bParametersChanged && !bMaterialChanged)
    {
        return;
    }
    
    LastHollowPrism = this;
    LastMaterial = Material;
    bFirstGeneration = false;

    ProceduralMesh->ClearAllMeshSections();

    FHollowPrismBuilder Builder(*this);
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

bool AHollowPrism::IsValid() const
{
    if (InnerRadius <= 0.0f || OuterRadius <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    if (InnerRadius >= OuterRadius)
    {
        return false;
    }

    if (Sides < 3 || InnerSides < 3 || OuterSides < 3)
    {
        return false;
    }

    if (ArcAngle <= 0.0f || ArcAngle > 360.0f)
    {
        return false;
    }

    if (BevelRadius < 0.0f || BevelSegments < 1)
    {
        return false;
    }

    return true;
}

float AHollowPrism::GetWallThickness() const
{
    return OuterRadius - InnerRadius;
}

bool AHollowPrism::IsFullCircle() const
{
    return FMath::IsNearlyEqual(ArcAngle, 360.0f, 0.1f);
}

int32 AHollowPrism::CalculateVertexCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    int32 BaseVertices = InnerSides + OuterSides;
    
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        BevelVertices = (InnerSides + OuterSides) * BevelSegments * 2;
    }
    
    int32 EndCapVertices = 0;
    if (!IsFullCircle())
    {
        EndCapVertices = InnerSides + OuterSides;
    }

    return BaseVertices + BevelVertices + EndCapVertices;
}

int32 AHollowPrism::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    int32 BaseTriangles = InnerSides + OuterSides;
    
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangles = (InnerSides + OuterSides) * BevelSegments * 2;
    }
    
    int32 EndCapTriangles = 0;
    if (!IsFullCircle())
    {
        EndCapTriangles = InnerSides + OuterSides;
    }

    return BaseTriangles + BevelTriangles + EndCapTriangles;
}

bool AHollowPrism::operator==(const AHollowPrism& Other) const
{
    return InnerRadius == Other.InnerRadius &&
           OuterRadius == Other.OuterRadius &&
           Height == Other.Height &&
           Sides == Other.Sides &&
           InnerSides == Other.InnerSides &&
           OuterSides == Other.OuterSides &&
           ArcAngle == Other.ArcAngle &&
           BevelRadius == Other.BevelRadius &&
           BevelSegments == Other.BevelSegments &&
           bUseTriangleMethod == Other.bUseTriangleMethod &&
           bFlipNormals == Other.bFlipNormals &&
           bDisableDebounce == Other.bDisableDebounce;
}

bool AHollowPrism::operator!=(const AHollowPrism& Other) const
{
    return !(*this == Other);
}