// Copyright (c) 2024. All rights reserved.

#include "ProceduralMeshActor.h"

#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "MeshDescriptionBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "Materials/Material.h"

AProceduralMeshActor::AProceduralMeshActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
    
    RootComponent = ProceduralMeshComponent;
    StaticMeshComponent->SetupAttachment(RootComponent);
    StaticMeshComponent->SetRelativeTransform(FTransform::Identity);
    StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
    StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    ProceduralMeshComponent->SetVisibility(bShowProceduralMeshInEditor);

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
    
    if (PropertyChangedEvent.Property)
    {
        const FName PropertyName = PropertyChangedEvent.Property->GetFName();
        
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bShowStaticMeshInEditor))
        {
            if (StaticMeshComponent)
            {
                StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
                
                if (bShowStaticMeshInEditor && !StaticMeshComponent->GetStaticMesh() && bAutoGenerateStaticMesh)
                {
                    if (ProceduralMeshComponent && ProceduralMeshComponent->GetNumSections() > 0)
                    {
                        GenerateStaticMesh();
                    }
                }
            }
            return;
        }
        
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bShowProceduralMeshInEditor))
        {
            if (ProceduralMeshComponent)
            {
                ProceduralMeshComponent->SetVisibility(bShowProceduralMeshInEditor);
            }
            return;
        }
        
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bAutoGenerateStaticMesh))
        {
            if (bAutoGenerateStaticMesh && StaticMeshComponent && ProceduralMeshComponent && ProceduralMeshComponent->GetNumSections() > 0)
            {
                GenerateStaticMesh();
            }
            return;
        }
        
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, ProceduralMeshMaterial) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, StaticMeshMaterial) ||
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
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProceduralMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        ProceduralMeshComponent->bUseAsyncCooking = bUseAsyncCooking;
        
        if (ProceduralMeshMaterial)
        {
            ProceduralMeshComponent->SetMaterial(0, ProceduralMeshMaterial);
        }
    }
    
    if (bAutoGenerateStaticMesh && StaticMeshComponent && ProceduralMeshComponent)
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            if (ProceduralMeshComponent && ProceduralMeshComponent->GetNumSections() > 0)
            {
                GenerateStaticMesh();
            }
        });
    }
}

void AProceduralMeshActor::ApplyMaterial()
{
    if (ProceduralMeshComponent && ProceduralMeshMaterial)
    {
        ProceduralMeshComponent->SetMaterial(0, ProceduralMeshMaterial);
    }
}

void AProceduralMeshActor::SetupCollision()
{
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void AProceduralMeshActor::RegenerateMesh()
{
    if (!ProceduralMeshComponent || !IsValid())
    {
        return;
    }

    ProceduralMeshComponent->ClearAllMeshSections();
    GenerateMesh();
    ApplyMaterial();
    SetupCollision();
    
    if (bAutoGenerateStaticMesh && StaticMeshComponent && ProceduralMeshComponent->GetNumSections() > 0)
    {
        UStaticMesh* NewStaticMesh = ConvertProceduralMeshToStaticMesh();
        if (NewStaticMesh)
        {
            StaticMeshComponent->SetStaticMesh(NewStaticMesh);
            
            // 强制重新构建静态网格以确保材质正确应用
            NewStaticMesh->Build();
            
            // 设置材质
            UMaterialInterface* MaterialToUse = nullptr;
            if (StaticMeshMaterial && ::IsValid(StaticMeshMaterial))
            {
                MaterialToUse = StaticMeshMaterial;
            }
            else if (ProceduralMeshComponent->GetNumSections() > 0)
            {
                UMaterialInterface* ProcMaterial = ProceduralMeshComponent->GetMaterial(0);
                if (ProcMaterial && ::IsValid(ProcMaterial))
                {
                    MaterialToUse = ProcMaterial;
                }
            }
            
            if (MaterialToUse)
            {
                // 确保材质正确应用到所有材质槽
                for (int32 i = 0; i < NewStaticMesh->StaticMaterials.Num(); i++)
                {
                    StaticMeshComponent->SetMaterial(i, MaterialToUse);
                }
                
                // 强制更新渲染状态
                StaticMeshComponent->MarkRenderStateDirty();
                StaticMeshComponent->RecreateRenderState_Concurrent();
            }
            
            StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
        }
    }
}

void AProceduralMeshActor::ClearMesh()
{
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->ClearAllMeshSections();
    }

    if (StaticMeshComponent) 
    {
	    CleanupCurrentStaticMesh();
        StaticMeshComponent->SetStaticMesh(nullptr);
    }
}

void AProceduralMeshActor::SetProceduralMeshMaterial(UMaterialInterface* NewMaterial)
{
    ProceduralMeshMaterial = NewMaterial;
    ApplyMaterial();
}

void AProceduralMeshActor::SetStaticMeshMaterial(UMaterialInterface* NewMaterial)
{
    StaticMeshMaterial = NewMaterial;
    
    // 如果StaticMeshComponent已经存在，也要更新其材质
    if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
    {
        ApplyMaterialToStaticMesh();
    }
}

void AProceduralMeshActor::SetProceduralSectionMaterial(int32 SectionIndex, UMaterialInterface* NewMaterial)
{
    if (SectionIndex < 0)
    {
        return;
    }
    
    // 确保ProceduralSectionMaterials数组足够大
    if (SectionIndex >= ProceduralSectionMaterials.Num())
    {
        ProceduralSectionMaterials.SetNum(SectionIndex + 1);
    }
    
    ProceduralSectionMaterials[SectionIndex] = NewMaterial;
    
    // 应用到ProceduralMeshComponent
    if (ProceduralMeshComponent && SectionIndex < ProceduralMeshComponent->GetNumSections())
    {
        ProceduralMeshComponent->SetMaterial(SectionIndex, NewMaterial);
    }
}

void AProceduralMeshActor::SetStaticSectionMaterial(int32 SectionIndex, UMaterialInterface* NewMaterial)
{
    if (SectionIndex < 0)
    {
        return;
    }
    
    // 确保StaticSectionMaterials数组足够大
    if (SectionIndex >= StaticSectionMaterials.Num())
    {
        StaticSectionMaterials.SetNum(SectionIndex + 1);
    }
    
    StaticSectionMaterials[SectionIndex] = NewMaterial;
    
    // 如果StaticMeshComponent已经存在，也要更新其材质
    if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
    {
        ApplyMaterialToStaticMesh();
    }
}

void AProceduralMeshActor::SetCollisionEnabled(bool bEnable)
{
    bGenerateCollision = bEnable;
    SetupCollision();
}

UStaticMesh* AProceduralMeshActor::ConvertProceduralMeshToStaticMesh() 
{
    // 输入参数验证
  if (!ProceduralMeshComponent) {
    UE_LOG(LogTemp, Error, TEXT("ConvertProceduralMeshToStaticMesh: 程序化网格组件指针为空"));
    return nullptr;
  }
  
    if (ProceduralMeshComponent->GetNumSections() == 0) {
    UE_LOG(LogTemp, Warning, TEXT("ConvertProceduralMeshToStaticMesh: 程序化网格组件没有网格段"));
    return nullptr;
  }

  UProceduralMeshComponent* ProcMeshComp = ProceduralMeshComponent;
  FString ActorName = ProceduralMeshComponent->GetOwner()->GetName();
  FString LevelName = ProceduralMeshComponent->GetWorld()->GetMapName();
    FString AssetName = FString(TEXT("SM_")) + LevelName + FString(TEXT("_") + ActorName);
  FString PathName = FString(TEXT("/Game/WebEZMeshes/"));
  FString PackageName = PathName + AssetName;


    // 使用UE4内置的BuildMeshDescription函数
    FMeshDescription MeshDescription = BuildMeshDescription(ProcMeshComp);
    
    // 检查是否有有效的几何体数据
    if (MeshDescription.Polygons().Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("没有有效的几何体数据"));
        return nullptr;
    }

    // 创建包和静态网格对象
  UPackage* Package = CreatePackage(*PackageName);
  if (!Package) {
    UE_LOG(LogTemp, Error, TEXT("无法创建包: %s"), *PackageName);
    return nullptr;
  }
  
    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
  if (!StaticMesh) {
    UE_LOG(LogTemp, Error, TEXT("无法创建静态网格对象"));
    return nullptr;
  }
  
    // 初始化StaticMesh
    StaticMesh->InitResources();
    StaticMesh->LightingGuid = FGuid::NewGuid();

    // 添加源模型到新的StaticMesh
    FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
    SrcModel.BuildSettings.bRecomputeNormals = false;
    SrcModel.BuildSettings.bRecomputeTangents = false;
    SrcModel.BuildSettings.bRemoveDegenerates = false;
    SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
    SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
    SrcModel.BuildSettings.bGenerateLightmapUVs = true;
    SrcModel.BuildSettings.SrcLightmapIndex = 0;
    SrcModel.BuildSettings.DstLightmapIndex = 1;
    
    StaticMesh->CreateMeshDescription(0, MoveTemp(MeshDescription));
    StaticMesh->CommitMeshDescription(0);

    // 处理材质 - 优先级：StaticSectionMaterials > StaticMeshMaterial > ProceduralMesh材质 > 默认材质
    TSet<UMaterialInterface*> UniqueMaterials;
    const int32 NumSections = ProcMeshComp->GetNumSections();
    for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++) {
        UMaterialInterface* SectionMaterial = nullptr;
        
        // 优先级1：使用StaticSectionMaterials数组中的材质
        if (SectionIdx < StaticSectionMaterials.Num() && StaticSectionMaterials[SectionIdx] && ::IsValid(StaticSectionMaterials[SectionIdx])) {
            SectionMaterial = StaticSectionMaterials[SectionIdx];
        }
        // 优先级2：使用指定的StaticMeshMaterial
        else if (StaticMeshMaterial && ::IsValid(StaticMeshMaterial)) {
            SectionMaterial = StaticMeshMaterial;
        }
        // 优先级3：使用ProceduralMesh的材质
        else {
            SectionMaterial = ProcMeshComp->GetMaterial(SectionIdx);
            if (!SectionMaterial || !::IsValid(SectionMaterial)) {
                SectionMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
            }
        }
        
        UniqueMaterials.Add(SectionMaterial);
    }
    
    // 复制材质到新网格
    for (auto* SectionMaterial : UniqueMaterials) {
        StaticMesh->StaticMaterials.Add(FStaticMaterial(SectionMaterial));
    }

    // 设置导入版本
    StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

    // 构建网格 - 关键：使用false参数
    StaticMesh->Build(false);
    StaticMesh->PostEditChange();

    // 通知资源注册表
      FAssetRegistryModule::AssetCreated(StaticMesh);

  
  return StaticMesh;
}

void AProceduralMeshActor::ConvertToStaticMeshComponent()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        return;
    }

    // 清理组件中之前的静态网格
    CleanupCurrentStaticMesh();

    // 使用现有的转换方法创建静态网格
    UStaticMesh* NewStaticMesh = ConvertProceduralMeshToStaticMesh();
    
    if (NewStaticMesh)
    {
        // 设置静态网格组件
        StaticMeshComponent->SetStaticMesh(NewStaticMesh);
        
        // 强制重新构建静态网格以确保材质正确应用
        NewStaticMesh->Build();
        
        // 复制材质
        UMaterialInterface* MaterialToUse = nullptr;
        if (StaticMeshMaterial && ::IsValid(StaticMeshMaterial))
        {
            MaterialToUse = StaticMeshMaterial;
        }
        else if (ProceduralMeshComponent->GetNumSections() > 0)
        {
            UMaterialInterface* ProcMaterial = ProceduralMeshComponent->GetMaterial(0);
            if (ProcMaterial && ::IsValid(ProcMaterial))
            {
                MaterialToUse = ProcMaterial;
            }
        }
        
        if (MaterialToUse)
        {
            // 确保材质正确应用到所有材质槽
            for (int32 i = 0; i < NewStaticMesh->StaticMaterials.Num(); i++)
            {
                StaticMeshComponent->SetMaterial(i, MaterialToUse);
            }
            
            // 强制更新渲染状态
            StaticMeshComponent->MarkRenderStateDirty();
            StaticMeshComponent->RecreateRenderState_Concurrent();
        }
        
        // 设置显示模式为静态网格
        ProceduralMeshComponent->SetVisibility(false);
        StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
        ProceduralMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StaticMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void AProceduralMeshActor::GenerateStaticMesh()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent || ProceduralMeshComponent->GetNumSections() == 0)
    {
        return;
    }
    
    // 清理组件中之前的静态网格
    CleanupCurrentStaticMesh();
    
    UStaticMesh* NewStaticMesh = ConvertProceduralMeshToStaticMesh();
    if (NewStaticMesh)
    {
        StaticMeshComponent->SetStaticMesh(NewStaticMesh);
        
        // 强制重新构建静态网格以确保材质正确应用
        NewStaticMesh->Build();
        
        // 设置材质
        UMaterialInterface* MaterialToUse = nullptr;
        if (StaticMeshMaterial && ::IsValid(StaticMeshMaterial))
        {
            MaterialToUse = StaticMeshMaterial;
        }
        else if (ProceduralMeshComponent->GetNumSections() > 0)
        {
            UMaterialInterface* ProcMaterial = ProceduralMeshComponent->GetMaterial(0);
            if (ProcMaterial && ::IsValid(ProcMaterial))
            {
                MaterialToUse = ProcMaterial;
            }
        }
        
        if (MaterialToUse)
        {
            // 确保材质正确应用到所有材质槽
            for (int32 i = 0; i < NewStaticMesh->StaticMaterials.Num(); i++)
            {
                StaticMeshComponent->SetMaterial(i, MaterialToUse);
            }
            
            // 强制更新渲染状态
            StaticMeshComponent->MarkRenderStateDirty();
            StaticMeshComponent->RecreateRenderState_Concurrent();
        }
        
        StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
    }
}

void AProceduralMeshActor::RefreshStaticMesh()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        return;
    }
    
    // 如果静态网格组件已经有内容，则刷新它
    if (StaticMeshComponent->GetStaticMesh())
    {
        GenerateStaticMesh();
    }
}

void AProceduralMeshActor::SetProceduralMeshVisibility(bool bVisible)
{
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetVisibility(bVisible);
    }
}

void AProceduralMeshActor::SetStaticMeshVisibility(bool bVisible)
{
    if (StaticMeshComponent)
    {
        StaticMeshComponent->SetVisibility(bVisible);
    }
}

void AProceduralMeshActor::CleanupCurrentStaticMesh()
{
    if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
    {
        UStaticMesh* OldStaticMesh = StaticMeshComponent->GetStaticMesh();
        UPackage* Package = OldStaticMesh->GetPackage();
        if (Package)
        {
            Package->RemoveFromRoot();
            Package->ConditionalBeginDestroy();
        }
        OldStaticMesh->ConditionalBeginDestroy();
        StaticMeshComponent->SetStaticMesh(nullptr);
    }
}

void AProceduralMeshActor::ApplyMaterialToStaticMesh()
{
    if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
    {
        return;
    }
    
    UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
    
    // 确定要使用的材质
    UMaterialInterface* MaterialToUse = nullptr;
    if (StaticMeshMaterial && ::IsValid(StaticMeshMaterial))
    {
        MaterialToUse = StaticMeshMaterial;
    }
    else if (ProceduralMeshComponent && ProceduralMeshComponent->GetNumSections() > 0)
    {
        UMaterialInterface* ProcMaterial = ProceduralMeshComponent->GetMaterial(0);
        if (ProcMaterial && ::IsValid(ProcMaterial))
        {
            MaterialToUse = ProcMaterial;
        }
    }
    
    if (MaterialToUse)
    {
        // 确保材质正确应用到所有材质槽
        int32 NumMaterialSlots = StaticMesh->StaticMaterials.Num();
        for (int32 i = 0; i < NumMaterialSlots; i++)
        {
            StaticMeshComponent->SetMaterial(i, MaterialToUse);
        }
        
        // 强制更新渲染状态
        StaticMeshComponent->MarkRenderStateDirty();
        StaticMeshComponent->RecreateRenderState_Concurrent();
    }
}

void AProceduralMeshActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    
    // 清理组件中的静态网格
    CleanupCurrentStaticMesh();
}
