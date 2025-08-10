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
    PrimaryActorTick.bCanEverTick = true;

    // 初始化组件
    InitializeComponents();
    
    // 初始生成网格
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
    
    // 检查是否是Parameters结构中的属性
    if (PropertyNameStr.StartsWith("Parameters."))
    {
        // 减少日志输出以提高性能
        RegenerateMesh();
        return;
    }
    
    // 检查其他相关属性
    static const TArray<FName> RelevantProperties = {
        "Material", "bGenerateCollision", "bUseAsyncCooking"
    };

    if (RelevantProperties.Contains(PropertyName))
    {
        // 减少日志输出以提高性能
        RegenerateMesh();
    }
}

void AHollowPrism::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FString PropertyNameStr = PropertyName.ToString();
    
    // 检查是否是Parameters结构中的属性
    if (PropertyNameStr.StartsWith("Parameters."))
    {
        // 减少日志输出以提高性能
        RegenerateMesh();
        return;
    }
    
    // 检查其他相关属性
    static const TArray<FName> RelevantProperties = {
        "Material", "bGenerateCollision", "bUseAsyncCooking"
    };

    if (RelevantProperties.Contains(PropertyName))
    {
        // 减少日志输出以提高性能
        RegenerateMesh();
    }
}
#endif

void AHollowPrism::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AHollowPrism::InitializeComponents()
{
    // 创建程序化网格组件
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HollowPrismMesh"));
    if (ProceduralMesh)
    {
        RootComponent = ProceduralMesh;
        
        // 配置网格属性
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMesh->SetSimulatePhysics(false);
    }
}

void AHollowPrism::ApplyMaterial()
{
    if (!ProceduralMesh)
    {
        return;
    }

    if (Material)
    {
        ProceduralMesh->SetMaterial(0, Material);
    }
    else
    {
        // 使用默认材质 - 使用更安全的加载方式
        static UMaterial* DefaultMaterial = nullptr;
        if (!DefaultMaterial)
        {
            DefaultMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
        }
        
        if (DefaultMaterial)
        {
            ProceduralMesh->SetMaterial(0, DefaultMaterial);
        }
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
        UE_LOG(LogTemp, Warning, TEXT("HollowPrism::RegenerateMesh - ProceduralMesh is null"));
        return;
    }

    // 防抖：检查是否距离上次更新太近（减少延迟以提高响应性）
    static float LastUpdateTime = 0.0f;
    const float CurrentTime = FPlatformTime::Seconds();
    const float MinUpdateInterval = 0.05f; // 减少到50毫秒，提高响应性
    
    // 如果启用了禁用防抖选项，则跳过防抖检查
    if (!Parameters.bDisableDebounce && CurrentTime - LastUpdateTime < MinUpdateInterval)
    {
        // 不记录日志，减少性能开销
        return;
    }
    
    LastUpdateTime = CurrentTime;

    // 参数缓存：检查参数是否真的改变了
    static FHollowPrismParameters LastParameters;
    static bool bFirstGeneration = true;
    
    if (!bFirstGeneration && LastParameters == Parameters)
    {
        // 参数没有改变，跳过生成
        return;
    }
    
    LastParameters = Parameters;
    bFirstGeneration = false;

    UE_LOG(LogTemp, Log, TEXT("HollowPrism::RegenerateMesh - Starting mesh generation"));
    UE_LOG(LogTemp, Log, TEXT("HollowPrism::RegenerateMesh - Parameters: InnerRadius=%.2f, OuterRadius=%.2f, Height=%.2f, InnerSides=%d, OuterSides=%d"), 
           Parameters.InnerRadius, Parameters.OuterRadius, Parameters.Height, Parameters.InnerSides, Parameters.OuterSides);

    // 清除现有网格
    ProceduralMesh->ClearAllMeshSections();

    // 创建构建器并生成网格
    FHollowPrismBuilder Builder(Parameters);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        UE_LOG(LogTemp, Log, TEXT("HollowPrism::RegenerateMesh - Builder generated mesh successfully"));
        
        // 将网格数据应用到组件
        MeshData.ToProceduralMesh(ProceduralMesh, 0);

        // 应用材质和碰撞设置
        ApplyMaterial();
        SetupCollision();

        UE_LOG(LogTemp, Log, TEXT("HollowPrism generated successfully: %d vertices, %d triangles"), 
               MeshData.GetVertexCount(), MeshData.GetTriangleCount());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate hollow prism mesh"));
    }
}

void AHollowPrism::RegenerateMeshBlueprint()
{
    RegenerateMesh();
}