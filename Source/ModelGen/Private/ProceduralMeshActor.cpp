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
        
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bAutoGenerateStaticMesh))
        {
            if (bAutoGenerateStaticMesh && StaticMeshComponent && ProceduralMeshComponent && ProceduralMeshComponent->GetNumSections() > 0)
            {
                GenerateStaticMesh();
            }
            return;
        }
        
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
    
    // 链式属性变化通常不需要特殊处理，因为PostEditChangeProperty已经处理了
    // 如果需要特殊处理，可以在这里添加逻辑
}
#endif

void AProceduralMeshActor::InitializeComponents()
{
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProceduralMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        ProceduralMeshComponent->bUseAsyncCooking = bUseAsyncCooking;
        
        if (Material)
        {
            ProceduralMeshComponent->SetMaterial(0, Material);
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
    if (ProceduralMeshComponent && Material)
    {
        ProceduralMeshComponent->SetMaterial(0, Material);
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
        UStaticMesh* NewStaticMesh = ConvertProceduralMeshToStaticMesh(ProceduralMeshComponent);
        if (NewStaticMesh)
        {
            StaticMeshComponent->SetStaticMesh(NewStaticMesh);
            if (Material)
            {
                StaticMeshComponent->SetMaterial(0, Material);
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

UStaticMesh* AProceduralMeshActor::ConvertProceduralMeshToStaticMesh(
    UProceduralMeshComponent* ProcMesh) 
{
  if (!ProcMesh || ProcMesh->GetNumSections() == 0) 
  {
    UE_LOG(LogTemp, Warning, TEXT("ConvertProceduralMeshToStaticMesh: 无效的程序化网格组件或没有网格段"));
    return nullptr;
  }

  UProceduralMeshComponent* ProcMeshComp = ProcMesh;
  FString ActorName = ProcMesh->GetOwner()->GetName();
  FString LevelName = ProcMesh->GetWorld()->GetMapName();
  FString AssetName =
      FString(TEXT("SM_")) + LevelName + FString(TEXT("_") + ActorName);
  FString PathName = FString(TEXT("/Game/WebEZMeshes/"));
  FString PackageName = PathName + AssetName;

  UE_LOG(LogTemp, Log, TEXT("正在创建静态网格: %s"), *AssetName);

  // Create package and asset
  UPackage* Package = CreatePackage(*PackageName);
  if (!Package) {
    UE_LOG(LogTemp, Error, TEXT("无法创建包: %s"), *PackageName);
    return nullptr;
  }
  Package->FullyLoad();

  UStaticMesh* StaticMesh =
      NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
  if (!StaticMesh) {
    UE_LOG(LogTemp, Error, TEXT("无法创建静态网格对象"));
    return nullptr;
  }

  // Initialize the StaticMesh
  StaticMesh->NeverStream = true;

  // We need one SourceModel per LOD. We're only creating LOD0 here.
  FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();

  // Create a new MeshDescription
  FMeshDescription MeshDescription;
  FStaticMeshAttributes Attributes(MeshDescription);
  Attributes.Register();

  // Use a builder to help populate the MeshDescription
  FMeshDescriptionBuilder MeshBuilder;
  MeshBuilder.SetMeshDescription(&MeshDescription);
  MeshBuilder.EnablePolyGroups();
  MeshBuilder.SetNumUVLayers(1);

  // Process each section of the procedural mesh
  TArray<UMaterialInterface*> MeshMaterials;
  int32 TotalVertices = 0;
  int32 TotalTriangles = 0;

  for (int32 SectionIndex = 0; SectionIndex < ProcMeshComp->GetNumSections();
       SectionIndex++) {
    FProcMeshSection* ProcSection =
        ProcMeshComp->GetProcMeshSection(SectionIndex);
    if (!ProcSection || ProcSection->ProcVertexBuffer.Num() == 0 ||
        ProcSection->ProcIndexBuffer.Num() == 0) {
      UE_LOG(LogTemp, Warning, TEXT("跳过无效的网格段 %d"), SectionIndex);
      continue;
    }

    // 验证索引数据的有效性
    int32 NumVertices = ProcSection->ProcVertexBuffer.Num();
    int32 NumIndices = ProcSection->ProcIndexBuffer.Num();
    
    // 检查索引是否在有效范围内
    bool bValidIndices = true;
    for (int32 i = 0; i < NumIndices; ++i)
    {
        uint32 IndexValue = ProcSection->ProcIndexBuffer[i];
        if (IndexValue >= (uint32)NumVertices)
        {
            UE_LOG(LogTemp, Error, TEXT("无效的索引 %d: %u (顶点数量: %d)"), i, IndexValue, NumVertices);
            bValidIndices = false;
            break;
        }
    }
    
    if (!bValidIndices)
    {
        UE_LOG(LogTemp, Error, TEXT("网格段 %d 包含无效索引，跳过"), SectionIndex);
        continue;
    }

    // 确保索引数量是3的倍数（三角形）
    if (NumIndices % 3 != 0) {
      UE_LOG(LogTemp, Warning, TEXT("网格段 %d 的索引数量不是3的倍数: %d，跳过"), SectionIndex, NumIndices);
      continue;
    }

    UE_LOG(LogTemp, Log, TEXT("处理网格段 %d: %d 顶点, %d 三角形"), 
           SectionIndex, NumVertices, NumIndices / 3);

    // Map for storing created vertex IDs for this section
    TArray<FVertexID> VertexIDs;
    VertexIDs.SetNum(NumVertices);

    // 1. Create Vertices
    for (int32 VertIdx = 0; VertIdx < NumVertices; VertIdx++) {
      const FProcMeshVertex& ProcVert = ProcSection->ProcVertexBuffer[VertIdx];
      
      // 验证顶点数据
      if (!FMath::IsFinite(ProcVert.Position.X) || !FMath::IsFinite(ProcVert.Position.Y) || !FMath::IsFinite(ProcVert.Position.Z))
      {
          UE_LOG(LogTemp, Warning, TEXT("顶点 %d 位置无效，使用默认位置"), VertIdx);
          FVertexID VertexID = MeshBuilder.AppendVertex(FVector::ZeroVector);
          VertexIDs[VertIdx] = VertexID;
      }
      else
      {
          FVertexID VertexID = MeshBuilder.AppendVertex(ProcVert.Position);
          VertexIDs[VertIdx] = VertexID;
      }
    }

    // Create a Polygon Group for this section
    FPolygonGroupID PolygonGroupID = MeshBuilder.AppendPolygonGroup();
    
    // Store the material for this group
    UMaterialInterface* SectionMaterial = ProcMeshComp->GetMaterial(SectionIndex);
    MeshMaterials.Add(SectionMaterial);

    int32 NumTriangles = NumIndices / 3;
    for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++) {
      uint32 Index0 = ProcSection->ProcIndexBuffer[TriIdx * 3 + 0];
      uint32 Index1 = ProcSection->ProcIndexBuffer[TriIdx * 3 + 1];
      uint32 Index2 = ProcSection->ProcIndexBuffer[TriIdx * 3 + 2];

      // 再次验证索引
      if (Index0 >= (uint32)NumVertices || 
          Index1 >= (uint32)NumVertices || 
          Index2 >= (uint32)NumVertices)
      {
          UE_LOG(LogTemp, Warning, TEXT("三角形 %d 包含无效索引: %u, %u, %u，跳过"), TriIdx, Index0, Index1, Index2);
          continue;
      }

      // 检查退化三角形（三个顶点相同）
      if (Index0 == Index1 || Index1 == Index2 || Index0 == Index2)
      {
          UE_LOG(LogTemp, Warning, TEXT("三角形 %d 是退化三角形，跳过"), TriIdx);
          continue;
      }

      const FProcMeshVertex& V0 = ProcSection->ProcVertexBuffer[(int32)Index0];
      const FProcMeshVertex& V1 = ProcSection->ProcVertexBuffer[(int32)Index1];
      const FProcMeshVertex& V2 = ProcSection->ProcVertexBuffer[(int32)Index2];

      // Create Vertex Instances
      FVertexInstanceID InstanceID0 = MeshBuilder.AppendInstance(VertexIDs[(int32)Index0]);
      FVertexInstanceID InstanceID1 = MeshBuilder.AppendInstance(VertexIDs[(int32)Index1]);
      FVertexInstanceID InstanceID2 = MeshBuilder.AppendInstance(VertexIDs[(int32)Index2]);

      // Set attributes with validation
      if (FMath::IsFinite(V0.Normal.X) && FMath::IsFinite(V0.Normal.Y) && FMath::IsFinite(V0.Normal.Z))
      {
          MeshBuilder.SetInstanceNormal(InstanceID0, V0.Normal);
      }
      if (FMath::IsFinite(V1.Normal.X) && FMath::IsFinite(V1.Normal.Y) && FMath::IsFinite(V1.Normal.Z))
      {
          MeshBuilder.SetInstanceNormal(InstanceID1, V1.Normal);
      }
      if (FMath::IsFinite(V2.Normal.X) && FMath::IsFinite(V2.Normal.Y) && FMath::IsFinite(V2.Normal.Z))
      {
          MeshBuilder.SetInstanceNormal(InstanceID2, V2.Normal);
      }

      // Set UVs with validation
      if (FMath::IsFinite(V0.UV0.X) && FMath::IsFinite(V0.UV0.Y))
      {
          MeshBuilder.SetInstanceUV(InstanceID0, V0.UV0, 0);
      }
      if (FMath::IsFinite(V1.UV0.X) && FMath::IsFinite(V1.UV0.Y))
      {
          MeshBuilder.SetInstanceUV(InstanceID1, V1.UV0, 0);
      }
      if (FMath::IsFinite(V2.UV0.X) && FMath::IsFinite(V2.UV0.Y))
      {
          MeshBuilder.SetInstanceUV(InstanceID2, V2.UV0, 0);
      }

      // Set colors with validation
      MeshBuilder.SetInstanceColor(InstanceID0, FVector4(V0.Color.R / 255.0f, V0.Color.G / 255.0f, V0.Color.B / 255.0f, V0.Color.A / 255.0f));
      MeshBuilder.SetInstanceColor(InstanceID1, FVector4(V1.Color.R / 255.0f, V1.Color.G / 255.0f, V1.Color.B / 255.0f, V1.Color.A / 255.0f));
      MeshBuilder.SetInstanceColor(InstanceID2, FVector4(V2.Color.R / 255.0f, V2.Color.G / 255.0f, V2.Color.B / 255.0f, V2.Color.A / 255.0f));

      // Create the polygon
      TArray<FVertexID> TriangleVertexIDs;
      TriangleVertexIDs.Add(MeshDescription.GetVertexInstanceVertex(InstanceID0));
      TriangleVertexIDs.Add(MeshDescription.GetVertexInstanceVertex(InstanceID1));
      TriangleVertexIDs.Add(MeshDescription.GetVertexInstanceVertex(InstanceID2));
      
      try
      {
          MeshBuilder.AppendPolygon(TriangleVertexIDs, PolygonGroupID);
          TotalTriangles++;
      }
      catch (...)
      {
          UE_LOG(LogTemp, Warning, TEXT("添加三角形 %d 失败，跳过"), TriIdx);
          continue;
      }
    }
    
    TotalVertices += NumVertices;
  }

  // If we have no valid geometry, abort.
  if (MeshDescription.Vertices().Num() == 0) {
    UE_LOG(LogTemp, Error, TEXT("没有有效的几何体数据"));
    return nullptr;
  }

  UE_LOG(LogTemp, Log, TEXT("成功创建网格描述: %d 顶点, %d 三角形"), TotalVertices, TotalTriangles);

  // Commit the MeshDescription to the StaticMesh's SourceModel
  UStaticMesh::FBuildMeshDescriptionsParams BuildParams;

  TArray<const FMeshDescription*> MeshDescriptions;
  MeshDescriptions.Add(&MeshDescription);

  try
  {
      StaticMesh->BuildFromMeshDescriptions(MeshDescriptions, BuildParams);
      UE_LOG(LogTemp, Log, TEXT("静态网格构建成功"));
  }
  catch (...)
  {
      UE_LOG(LogTemp, Error, TEXT("静态网格构建失败"));
      return nullptr;
  }

  // Assign materials
  for (int32 i = 0; i < MeshMaterials.Num(); i++) {
    if (MeshMaterials[i]) {
      StaticMesh->AddMaterial(MeshMaterials[i]);
    } else {
      StaticMesh->AddMaterial(nullptr);
    }
  }

  // Configure the StaticMesh
  StaticMesh->CreateBodySetup();

  // IMPORTANT: Finalize the mesh
  StaticMesh->PostEditChange();

  // Notify the asset registry that we've created a new asset
  FAssetRegistryModule::AssetCreated(StaticMesh);

  // Mark the package as dirty so it will be saved if the user chooses to.
  Package->MarkPackageDirty();

  UE_LOG(LogTemp, Log, TEXT("静态网格创建完成: %s"), *AssetName);
  return StaticMesh;
}

void AProceduralMeshActor::ConvertToStaticMeshComponent()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        return;
    }

    // 使用现有的转换方法创建静态网格
    UStaticMesh* NewStaticMesh = ConvertProceduralMeshToStaticMesh(ProceduralMeshComponent);
    
    if (NewStaticMesh)
    {
        // 设置静态网格组件
        StaticMeshComponent->SetStaticMesh(NewStaticMesh);
        
        // 复制材质
        if (Material)
        {
            StaticMeshComponent->SetMaterial(0, Material);
        }
        
        // 设置显示模式为静态网格
        SetDisplayMode(EDisplayMode::StaticOnly);
    }
}

void AProceduralMeshActor::ToggleDisplayMode()
{
    if (bUsingStaticMesh)
    {
        SetDisplayMode(EDisplayMode::ProceduralOnly);
    }
    else
    {
        SetDisplayMode(EDisplayMode::StaticOnly);
    }
}

bool AProceduralMeshActor::IsUsingStaticMesh() const
{
    return bUsingStaticMesh;
}

UStaticMesh* AProceduralMeshActor::ConvertAndGetStaticMesh()
{
    if (!ProceduralMeshComponent)
    {
        return nullptr;
    }
    
    // 直接返回转换后的静态网格，不改变显示状态
    return ConvertProceduralMeshToStaticMesh(ProceduralMeshComponent);
}

void AProceduralMeshActor::SetDisplayMode(EDisplayMode Mode)
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        return;
    }
    
    switch (Mode)
    {
        case EDisplayMode::ProceduralOnly:
            ProceduralMeshComponent->SetVisibility(true);
            StaticMeshComponent->SetVisibility(false);
            ProceduralMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            bUsingStaticMesh = false;
            break;
            
        case EDisplayMode::StaticOnly:
            if (!StaticMeshComponent->GetStaticMesh())
            {
                GenerateStaticMesh();
            }
            ProceduralMeshComponent->SetVisibility(false);
            StaticMeshComponent->SetVisibility(true);
            ProceduralMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            StaticMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            bUsingStaticMesh = true;
            break;
            
        case EDisplayMode::Both:
            if (!StaticMeshComponent->GetStaticMesh())
            {
                GenerateStaticMesh();
            }
            ProceduralMeshComponent->SetVisibility(true);
            StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
            ProceduralMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            bUsingStaticMesh = false;
            break;
    }
}

void AProceduralMeshActor::GenerateStaticMesh()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent || ProceduralMeshComponent->GetNumSections() == 0)
    {
        return;
    }
    
    UStaticMesh* NewStaticMesh = ConvertProceduralMeshToStaticMesh(ProceduralMeshComponent);
    if (NewStaticMesh)
    {
        StaticMeshComponent->SetStaticMesh(NewStaticMesh);
        
        if (Material)
        {
            StaticMeshComponent->SetMaterial(0, Material);
        }
        else if (ProceduralMeshComponent->GetNumSections() > 0)
        {
            UMaterialInterface* ProcMaterial = ProceduralMeshComponent->GetMaterial(0);
            if (ProcMaterial)
            {
                StaticMeshComponent->SetMaterial(0, ProcMaterial);
            }
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

// 强制刷新静态网格（即使没有现有内容）
void AProceduralMeshActor::ForceRefreshStaticMesh()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent || ProceduralMeshComponent->GetNumSections() == 0)
    {
        return;
    }
    
    GenerateStaticMesh();
}

// 测试方法：在编辑器中测试静态网格生成
void AProceduralMeshActor::TestStaticMeshGeneration()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        return;
    }
    
    if (ProceduralMeshComponent->GetNumSections() > 0)
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
