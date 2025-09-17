// Copyright (c) 2024. All rights reserved.

#include "CustomModelActor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshActor.h"

ACustomModelActor::ACustomModelActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建StaticMeshComponent
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
    RootComponent = StaticMeshComponent;

    // 设置默认属性
    if (StaticMeshComponent)
    {
        StaticMeshComponent->SetVisibility(bShowStaticMesh);
        StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        StaticMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
    }
}

void ACustomModelActor::BeginPlay()
{
    Super::BeginPlay();
    
    // 初始化模型
    InitializeModel();
    
    // 生成初始网格
    GenerateMesh();
}

void ACustomModelActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ACustomModelActor::GenerateMesh()
{
    UE_LOG(LogTemp, Log, TEXT("=== CustomModelActor::GenerateMesh 开始 ==="));
    UE_LOG(LogTemp, Log, TEXT("模型类型: %s"), *ModelTypeName);
    
    // 使用工厂创建模型Actor
    AActor* ModelActor = UCustomModelFactory::CreateModelActor(ModelTypeName, GetWorld(), GetActorLocation(), GetActorRotation());
    
    if (ModelActor)
    {
        // 如果创建的是ProceduralMeshActor，获取其StaticMesh
        if (AProceduralMeshActor* ProcMeshActor = Cast<AProceduralMeshActor>(ModelActor))
        {
            UStaticMesh* GeneratedStaticMesh = ProcMeshActor->ConvertProceduralMeshToStaticMesh();
            
            if (GeneratedStaticMesh && StaticMeshComponent)
            {
                StaticMeshComponent->SetStaticMesh(GeneratedStaticMesh);
                
                // 设置材质
                if (StaticMeshMaterial)
                {
                    StaticMeshComponent->SetMaterial(0, StaticMeshMaterial);
                }
                
                UE_LOG(LogTemp, Log, TEXT("StaticMesh 设置成功"));
            }
        }
        
        // 销毁临时Actor
        ModelActor->Destroy();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("无法创建模型: %s"), *ModelTypeName);
    }
    
    UE_LOG(LogTemp, Log, TEXT("=== CustomModelActor::GenerateMesh 完成 ==="));
}

void ACustomModelActor::SetModelType(const FString& NewModelTypeName)
{
    if (ModelTypeName != NewModelTypeName)
    {
        ModelTypeName = NewModelTypeName;
        UE_LOG(LogTemp, Log, TEXT("模型类型切换为: %s"), *ModelTypeName);
        
        // 重新生成网格
        GenerateMesh();
    }
}

void ACustomModelActor::InitializeModel()
{
    UE_LOG(LogTemp, Log, TEXT("初始化模型: %s"), *ModelTypeName);
    // 直接使用工厂，不需要策略
}

void ACustomModelActor::UpdateStaticMesh()
{
    GenerateMesh();
}
