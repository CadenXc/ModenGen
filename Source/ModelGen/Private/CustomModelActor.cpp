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
    
    // 注意：不能在构造函数中调用GenerateMesh()，因为GetWorld()可能不可用
    // 网格生成将在BeginPlay()中进行
}

void ACustomModelActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    UE_LOG(LogTemp, Log, TEXT("OnConstruction: World = %s"), GetWorld() ? TEXT("可用") : TEXT("不可用"));
    
    // 检查World是否可用
    if (GetWorld())
    {
        UE_LOG(LogTemp, Log, TEXT("OnConstruction: 开始生成网格"));
        // 在OnConstruction中生成网格（编辑器可见）
        GenerateMesh();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("OnConstruction: World不可用，跳过网格生成"));
    }
}

void ACustomModelActor::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Log, TEXT("BeginPlay: 强制生成网格"));
    // 在BeginPlay中强制生成网格
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
    UE_LOG(LogTemp, Log, TEXT("World: %s"), GetWorld() ? TEXT("可用") : TEXT("不可用"));
    UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent: %s"), StaticMeshComponent ? TEXT("存在") : TEXT("不存在"));
    
    // 使用工厂创建模型Actor
    AActor* ModelActor = UCustomModelFactory::CreateModelActor(ModelTypeName, GetWorld(), GetActorLocation(), GetActorRotation());
    
    if (ModelActor)
    {
        UE_LOG(LogTemp, Log, TEXT("成功创建模型Actor: %s"), *ModelActor->GetClass()->GetName());
        
        // 如果创建的是ProceduralMeshActor，获取其StaticMesh
        if (AProceduralMeshActor* ProcMeshActor = Cast<AProceduralMeshActor>(ModelActor))
        {
            UE_LOG(LogTemp, Log, TEXT("转换为ProceduralMeshActor成功"));
            
            // 确保先生成网格数据
            ProcMeshActor->GenerateMesh();
            UE_LOG(LogTemp, Log, TEXT("调用ProcMeshActor->GenerateMesh()完成"));
            
            // 然后转换为StaticMesh
            UStaticMesh* GeneratedStaticMesh = ProcMeshActor->ConvertProceduralMeshToStaticMesh();
            UE_LOG(LogTemp, Log, TEXT("ConvertProceduralMeshToStaticMesh结果: %s"), GeneratedStaticMesh ? TEXT("成功") : TEXT("失败"));
            
            if (GeneratedStaticMesh && StaticMeshComponent)
            {
                StaticMeshComponent->SetStaticMesh(GeneratedStaticMesh);
                UE_LOG(LogTemp, Log, TEXT("StaticMesh 设置到组件成功"));
                
                // 设置材质
                if (StaticMeshMaterial)
                {
                    StaticMeshComponent->SetMaterial(0, StaticMeshMaterial);
                    UE_LOG(LogTemp, Log, TEXT("材质设置成功"));
                }
                
                UE_LOG(LogTemp, Log, TEXT("StaticMesh 设置成功"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("StaticMesh设置失败 - GeneratedStaticMesh: %s, StaticMeshComponent: %s"), 
                    GeneratedStaticMesh ? TEXT("存在") : TEXT("不存在"),
                    StaticMeshComponent ? TEXT("存在") : TEXT("不存在"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("无法转换为ProceduralMeshActor"));
        }
        
        // 销毁临时Actor
        ModelActor->Destroy();
        UE_LOG(LogTemp, Log, TEXT("临时Actor已销毁"));
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


void ACustomModelActor::UpdateStaticMesh()
{
    GenerateMesh();
}
