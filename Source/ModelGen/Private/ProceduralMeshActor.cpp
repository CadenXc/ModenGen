// Copyright (c) 2024. All rights reserved.

#include "ProceduralMeshActor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

AProceduralMeshActor::AProceduralMeshActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMesh;

    InitializeComponents();
}

void AProceduralMeshActor::BeginPlay()
{
    Super::BeginPlay();
    
    RegenerateMesh();
}

void AProceduralMeshActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    RegenerateMesh();
}

#if WITH_EDITOR
void AProceduralMeshActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // 当属性改变时重新生成网格
    if (PropertyChangedEvent.Property)
    {
        const FName PropertyName = PropertyChangedEvent.Property->GetFName();
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, Material) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bGenerateCollision) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bUseAsyncCooking))
        {
            RegenerateMesh();
        }
    }
}

void AProceduralMeshActor::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);
    
    // 当链式属性改变时重新生成网格
    if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
    {
        const FName PropertyName = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName();
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, Material) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bGenerateCollision) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bUseAsyncCooking))
        {
            RegenerateMesh();
        }
    }
}
#endif

void AProceduralMeshActor::InitializeComponents()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProceduralMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
        
        if (Material)
        {
            ProceduralMesh->SetMaterial(0, Material);
        }
    }
}

void AProceduralMeshActor::ApplyMaterial()
{
    if (ProceduralMesh && Material)
    {
        ProceduralMesh->SetMaterial(0, Material);
    }
}

void AProceduralMeshActor::SetupCollision()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void AProceduralMeshActor::RegenerateMesh()
{
    if (!ProceduralMesh || !IsValid())
    {
        return;
    }

    ProceduralMesh->ClearAllMeshSections();
    
    // 调用子类实现的网格生成方法
    GenerateMesh();
    
    // 应用材质和碰撞设置
    ApplyMaterial();
    SetupCollision();
}

void AProceduralMeshActor::ClearMesh()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->ClearAllMeshSections();
    }
}

void AProceduralMeshActor::SetMaterial(UMaterialInterface* NewMaterial)
{
    Material = NewMaterial;
    ApplyMaterial();
}

void AProceduralMeshActor::SetCollisionEnabled(bool bEnable)
{
    bGenerateCollision = bEnable;
    SetupCollision();
}
