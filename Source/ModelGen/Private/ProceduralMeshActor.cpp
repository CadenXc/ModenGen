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
    
    // 将静态网格组件附加到根组件
    StaticMeshComponent->SetupAttachment(RootComponent);
    
    // 根据设置决定是否显示静态网格组件
    StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
    StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    // 设置静态网格组件的初始变换，可以稍微偏移以避免完全重叠
    StaticMeshComponent->SetRelativeLocation(FVector(0, 0, 0));
    StaticMeshComponent->SetRelativeRotation(FRotator(0, 0, 0));
    StaticMeshComponent->SetRelativeScale3D(FVector(1, 1, 1));

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
        
        // 处理静态网格相关的属性
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bShowStaticMeshInEditor))
        {
            // 更新静态网格组件的可见性
            if (StaticMeshComponent)
            {
                StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
            }
            return;
        }
        
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bAutoGenerateStaticMesh))
        {
            // 如果启用了自动生成，则重新生成静态网格
            if (bAutoGenerateStaticMesh && StaticMeshComponent)
            {
                RefreshStaticMesh();
            }
            return;
        }
        
        // 处理其他需要重新生成网格的属性
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, Material) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bGenerateCollision) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bUseAsyncCooking))
        {
            RegenerateMesh();
            
            // 如果启用了自动生成静态网格，则同时更新静态网格
            if (bAutoGenerateStaticMesh && StaticMeshComponent)
            {
                RefreshStaticMesh();
            }
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
    
    // 调用子类实现的网格生成方法
    GenerateMesh();
    
    // 应用材质和碰撞设置
    ApplyMaterial();
    SetupCollision();
    
    // 如果启用了自动生成静态网格，则生成静态网格
    if (bAutoGenerateStaticMesh && StaticMeshComponent && ProceduralMeshComponent->GetNumSections() > 0)
    {
        RefreshStaticMesh();
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
    // 此函数现在会同时显示两个组件：程序化网格和静态网格
    // 程序化网格负责碰撞，静态网格负责显示
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
        
        // 设置碰撞
        StaticMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        
        // 保持程序化网格组件可见，但禁用碰撞避免重复
        ProceduralMeshComponent->SetVisibility(true);
        ProceduralMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        
        // 显示静态网格组件
        StaticMeshComponent->SetVisibility(true);
        
        // 更新状态标志 - 现在表示同时显示两个组件
        bUsingStaticMesh = true;
    }
}

void AProceduralMeshActor::ToggleDisplayMode()
{
    if (bUsingStaticMesh)
    {
        // 当前使用静态网格，切换到程序化网格
        if (ProceduralMeshComponent && StaticMeshComponent)
        {
            // 隐藏静态网格组件
            StaticMeshComponent->SetVisibility(false);
            StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            
            // 显示程序化网格组件
            ProceduralMeshComponent->SetVisibility(true);
            ProceduralMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        }
        bUsingStaticMesh = false;
    }
    else
    {
        // 当前使用程序化网格，切换到静态网格
        ConvertToStaticMeshComponent();
        bUsingStaticMesh = true;
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

void AProceduralMeshActor::ShowBothComponents()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        return;
    }
    
    // 确保静态网格组件有内容
    if (!StaticMeshComponent->GetStaticMesh())
    {
        GenerateStaticMesh();
    }
    
    // 显示两个组件
    ProceduralMeshComponent->SetVisibility(true);
    StaticMeshComponent->SetVisibility(true);
    
    // 程序化网格组件负责碰撞
    ProceduralMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    bUsingStaticMesh = false; // 标记为同时显示模式
}

void AProceduralMeshActor::ShowProceduralMeshOnly()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        return;
    }
    
    // 只显示程序化网格组件
    ProceduralMeshComponent->SetVisibility(true);
    StaticMeshComponent->SetVisibility(false);
    
    // 程序化网格组件负责碰撞
    ProceduralMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    bUsingStaticMesh = false;
}

void AProceduralMeshActor::ShowStaticMeshOnly()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        return;
    }
    
    // 确保静态网格组件有内容
    if (!StaticMeshComponent->GetStaticMesh())
    {
        GenerateStaticMesh();
    }
    
    // 只显示静态网格组件
    ProceduralMeshComponent->SetVisibility(false);
    StaticMeshComponent->SetVisibility(true);
    
    // 静态网格组件负责碰撞
    ProceduralMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StaticMeshComponent->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    
    bUsingStaticMesh = true;
}

void AProceduralMeshActor::GenerateStaticMesh()
{
    if (!ProceduralMeshComponent || !StaticMeshComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateStaticMesh: 组件无效"));
        return;
    }
    
    if (ProceduralMeshComponent->GetNumSections() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateStaticMesh: 程序化网格没有数据"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("开始手动生成静态网格..."));
    
    // 生成静态网格
    UStaticMesh* NewStaticMesh = ConvertProceduralMeshToStaticMesh(ProceduralMeshComponent);
    if (NewStaticMesh)
    {
        StaticMeshComponent->SetStaticMesh(NewStaticMesh);
        if (Material)
        {
            StaticMeshComponent->SetMaterial(0, Material);
        }
        
        // 设置静态网格组件的碰撞
        StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        
        // 根据设置决定是否显示静态网格
        if (bShowStaticMeshInEditor)
        {
            StaticMeshComponent->SetVisibility(true);
        }
        
        UE_LOG(LogTemp, Log, TEXT("静态网格生成成功: %s"), *NewStaticMesh->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("静态网格生成失败"));
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
