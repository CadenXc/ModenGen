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
    
    if (PropertyNameStr.StartsWith("TopRadius") || PropertyNameStr.StartsWith("BottomRadius") || 
        PropertyNameStr.StartsWith("Height") || PropertyNameStr.StartsWith("TopSides") || 
        PropertyNameStr.StartsWith("BottomSides") || PropertyNameStr.StartsWith("HeightSegments") || 
        PropertyNameStr.StartsWith("BevelRadius") || PropertyNameStr.StartsWith("BevelSegments") || 
        PropertyNameStr.StartsWith("BendAmount") || PropertyNameStr.StartsWith("MinBendRadius") || 
        PropertyNameStr.StartsWith("ArcAngle") || PropertyName == "bGenerateCollision" || 
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
    
    if (PropertyNameStr.StartsWith("TopRadius") || PropertyNameStr.StartsWith("BottomRadius") || 
        PropertyNameStr.StartsWith("Height") || PropertyNameStr.StartsWith("TopSides") || 
        PropertyNameStr.StartsWith("BottomSides") || PropertyNameStr.StartsWith("HeightSegments") || 
        PropertyNameStr.StartsWith("BevelRadius") || PropertyNameStr.StartsWith("BevelSegments") || 
        PropertyNameStr.StartsWith("BendAmount") || PropertyNameStr.StartsWith("MinBendRadius") || 
        PropertyNameStr.StartsWith("ArcAngle") || PropertyName == "bGenerateCollision" || 
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

    if (!IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid Frustum parameters: TopRadius=%f, BottomRadius=%f, Height=%f, TopSides=%d, BottomSides=%d"), 
               TopRadius, BottomRadius, Height, TopSides, BottomSides);
        return;
    }

    ProceduralMesh->ClearAllMeshSections();

    FFrustumBuilder Builder(*this);
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

void AFrustum::GenerateFrustum(float InTopRadius, float InBottomRadius, float InHeight, int32 InSides)
{
    TopRadius = InTopRadius;
    BottomRadius = InBottomRadius;
    Height = InHeight;
    TopSides = InSides;
    BottomSides = InSides;
    
    RegenerateMesh();
}

void AFrustum::GenerateFrustumWithDifferentSides(float InTopRadius, float InBottomRadius, float InHeight, 
                                                 int32 InTopSides, int32 InBottomSides)
{
    TopRadius = InTopRadius;
    BottomRadius = InBottomRadius;
    Height = InHeight;
    TopSides = InTopSides;
    BottomSides = InBottomSides;
    
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

bool AFrustum::IsValid() const
{
    if (TopRadius <= 0.0f || BottomRadius <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    if (TopSides < 3 || BottomSides < 3 || HeightSegments < 1)
    {
        return false;
    }

    if (BevelRadius < 0.0f || BevelSegments < 1)
    {
        return false;
    }

    if (MinBendRadius < 0.0f)
    {
        return false;
    }

    if (ArcAngle <= 0.0f || ArcAngle > 360.0f)
    {
        return false;
    }

    return true;
}

int32 AFrustum::CalculateVertexCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    int32 BaseVertices = TopSides + BottomSides;
    int32 MaxSides = FMath::Max(TopSides, BottomSides);
    int32 SideVertices = HeightSegments * MaxSides;
    
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        BevelVertices = TopSides * BevelSegments + BottomSides * BevelSegments;
        BevelVertices += HeightSegments * MaxSides * BevelSegments;
    }

    return BaseVertices + SideVertices + BevelVertices;
}

int32 AFrustum::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    int32 BaseTriangles = TopSides + BottomSides;
    int32 MaxSides = FMath::Max(TopSides, BottomSides);
    int32 SideTriangles = HeightSegments * MaxSides * 2;
    
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangles = TopSides * BevelSegments * 2 + BottomSides * BevelSegments * 2;
        BevelTriangles += HeightSegments * MaxSides * BevelSegments * 2;
    }

    return BaseTriangles + SideTriangles + BevelTriangles;
}

void AFrustum::PostEditChangeProperty(const FName& PropertyName)
{
}

bool AFrustum::operator==(const AFrustum& Other) const
{
    return TopRadius == Other.TopRadius && BottomRadius == Other.BottomRadius && Height == Other.Height &&
           TopSides == Other.TopSides && BottomSides == Other.BottomSides && HeightSegments == Other.HeightSegments &&
           BevelRadius == Other.BevelRadius && BevelSegments == Other.BevelSegments && BendAmount == Other.BendAmount &&
           MinBendRadius == Other.MinBendRadius && ArcAngle == Other.ArcAngle;
}

bool AFrustum::operator!=(const AFrustum& Other) const
{
    return !(*this == Other);
}