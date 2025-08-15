// Copyright (c) 2024. All rights reserved.

#include "PolygonTorus.h"
#include "PolygonTorusBuilder.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

APolygonTorus::APolygonTorus()
{
    PrimaryActorTick.bCanEverTick = false;

    InitializeComponents();
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

#if WITH_EDITOR
void APolygonTorus::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FString PropertyNameStr = PropertyName.ToString();
    
    if (PropertyNameStr.StartsWith("MajorRadius") || PropertyNameStr.StartsWith("MinorRadius") || 
        PropertyNameStr.StartsWith("MajorSegments") || PropertyNameStr.StartsWith("MinorSegments") || 
        PropertyNameStr.StartsWith("TorusAngle") || PropertyNameStr.StartsWith("bSmoothCrossSection") || 
        PropertyNameStr.StartsWith("bSmoothVerticalSection") || PropertyName == "bGenerateCollision" || 
        PropertyName == "bUseAsyncCooking")
    {
        RegenerateMesh();
    }
    else if (PropertyName == "Material")
    {
        ApplyMaterial();
    }
}

void APolygonTorus::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FString PropertyNameStr = PropertyName.ToString();
    
    if (PropertyNameStr.StartsWith("MajorRadius") || PropertyNameStr.StartsWith("MinorRadius") || 
        PropertyNameStr.StartsWith("MajorSegments") || PropertyNameStr.StartsWith("MinorSegments") || 
        PropertyNameStr.StartsWith("TorusAngle") || PropertyNameStr.StartsWith("bSmoothCrossSection") || 
        PropertyNameStr.StartsWith("bSmoothVerticalSection") || PropertyName == "bGenerateCollision" || 
        PropertyName == "bUseAsyncCooking")
    {
        RegenerateMesh();
    }
    else if (PropertyName == "Material")
    {
        return;
    }
}
#endif

void APolygonTorus::InitializeComponents()
{
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    if (ProceduralMesh)
    {
        RootComponent = ProceduralMesh;
        
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMesh->SetSimulatePhysics(false);
        
        SetupCollision();
        ApplyMaterial();
    }
}

void APolygonTorus::ApplyMaterial()
{
    if (ProceduralMesh && Material)
    {
        ProceduralMesh->SetMaterial(0, Material);
    }
    else if (ProceduralMesh && !Material)
    {
        // 如果没有设置材质，使用默认材质
        static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
        if (MaterialFinder.Succeeded())
        {
            ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
        }
    }
}

void APolygonTorus::SetupCollision()
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

void APolygonTorus::RegenerateMesh()
{
    if (!ProceduralMesh)
    {
        return;
    }

    ProceduralMesh->ClearAllMeshSections();

    if (!IsValid())
    {
        return;
    }
    
    FPolygonTorusBuilder Builder(*this);
    FModelGenMeshData MeshData;
    
    if (Builder.Generate(MeshData))
    {
        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        
        ApplyMaterial();
        SetupCollision();
    }
}

void APolygonTorus::SetMaterial(UMaterialInterface* NewMaterial)
{
    if (Material != NewMaterial)
    {
        Material = NewMaterial;
        ApplyMaterial();
    }
}

bool APolygonTorus::IsValid() const
{
    const bool bValidMajorRadius = MajorRadius > 0.0f;
    const bool bValidMinorRadius = MinorRadius > 0.0f && MinorRadius <= MajorRadius * 0.9f;
    const bool bValidMajorSegments = MajorSegments >= 3 && MajorSegments <= 256;
    const bool bValidMinorSegments = MinorSegments >= 3 && MinorSegments <= 256;
    const bool bValidTorusAngle = TorusAngle >= 1.0f && TorusAngle <= 360.0f;
    
    return bValidMajorRadius && bValidMinorRadius && bValidMajorSegments && 
           bValidMinorSegments && bValidTorusAngle;
}

int32 APolygonTorus::CalculateVertexCountEstimate() const
{
    int32 BaseVertexCount = MajorSegments * MinorSegments;
    
    int32 CapVertexCount = 0;
    if (TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CapVertexCount = MinorSegments * 2;
    }
    
    return BaseVertexCount + CapVertexCount;
}

int32 APolygonTorus::CalculateTriangleCountEstimate() const
{
    int32 BaseTriangleCount = MajorSegments * MinorSegments * 2;
    
    int32 CapTriangleCount = 0;
    if (TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CapTriangleCount = MinorSegments * 2;
    }
    
    return BaseTriangleCount + CapTriangleCount;
}
