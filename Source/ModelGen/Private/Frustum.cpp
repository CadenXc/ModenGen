// Copyright (c) 2024. All rights reserved.

#include "Frustum.h"
#include "FrustumBuilder.h"
#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "HAL/PlatformTime.h"

AFrustum::AFrustum()
{
    PrimaryActorTick.bCanEverTick = false;

    // 初始化组件
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
    
    // 检查是否是Parameters结构中的属性
    if (PropertyNameStr.StartsWith("Parameters."))
    {
        RegenerateMesh();
        return;
    }
    
    // 检查材质相关属性
    if (PropertyName == "Material")
    {
        ApplyMaterial();
    }
    else if (PropertyName == "bGenerateCollision" || PropertyName == "bUseAsyncCooking")
    {
        RegenerateMesh();
    }
}

void AFrustum::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FString PropertyNameStr = PropertyName.ToString();
    
    // 检查是否是Parameters结构中的属性
    if (PropertyNameStr.StartsWith("Parameters."))
    {
        RegenerateMesh();
        return;
    }
    
    // 检查材质相关属性
    if (PropertyName == "Material")
    {
        ApplyMaterial();
    }
    else if (PropertyName == "bGenerateCollision" || PropertyName == "bUseAsyncCooking")
    {
        RegenerateMesh();
    }
}
#endif


void AFrustum::InitializeComponents()
{
    // 创建程序化网格组件
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FrustumMesh"));
    if (ProceduralMesh)
    {
        RootComponent = ProceduralMesh;

        // 配置网格属性
        ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMesh->SetSimulatePhysics(false);
        
        // 应用材质和碰撞设置
        ApplyMaterial();
        SetupCollision();
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
        UE_LOG(LogTemp, Warning, TEXT("Frustum::RegenerateMesh - ProceduralMesh is null"));
        return;
    }

    // 验证参数
    if (!Parameters.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Frustum parameters are invalid"));
        return;
    }

    // 防抖检查
    static float LastUpdateTime = 0.0f;
    const float CurrentTime = FPlatformTime::Seconds();
    const float MinUpdateInterval = 0.05f;
    
    if (!Parameters.bDisableDebounce && CurrentTime - LastUpdateTime < MinUpdateInterval)
    {
        return;
    }
    
    LastUpdateTime = CurrentTime;

    // 参数缓存检查
    static FFrustumParameters LastParameters;
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

    // 清除现有网格
    ProceduralMesh->ClearAllMeshSections();

    // 创建构建器并生成网格
    FFrustumBuilder Builder(Parameters);
    FModelGenMeshData MeshData;

    if (Builder.Generate(MeshData))
    {
        UE_LOG(LogTemp, Log, TEXT("Frustum::RegenerateMesh - Builder generated mesh successfully"));
        
        // 将网格数据应用到组件
        MeshData.ToProceduralMesh(ProceduralMesh, 0);
        
        // 应用材质和碰撞设置
        ApplyMaterial();
        SetupCollision();
        
        if (bMaterialChanged && !bParametersChanged)
        {
            UE_LOG(LogTemp, Log, TEXT("Frustum::RegenerateMesh - Material changed, reapplying material"));
            ApplyMaterial();
        }
        
        UE_LOG(LogTemp, Log, TEXT("Frustum generated successfully: %d vertices, %d triangles"), 
               MeshData.GetVertexCount(), MeshData.GetTriangleCount());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate frustum mesh"));
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