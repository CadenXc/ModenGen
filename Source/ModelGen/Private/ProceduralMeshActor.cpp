

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

#include "PhysicsEngine/BodySetup.h"

#include "StaticMeshResources.h"

#include "Rendering/StaticMeshVertexBuffer.h"

#include "Rendering/PositionVertexBuffer.h"

#include "Rendering/ColorVertexBuffer.h"

#include "StaticMeshOperations.h"
// 将UProceduralMeshComponent转换为内存中的UStaticMesh

UStaticMesh* AProceduralMeshActor::ConvertProceduralMeshToStaticMesh()

{
  // 1. 验证输入参数

  if (!ProceduralMeshComponent)

  {
    UE_LOG(LogTemp, Warning,
           TEXT("ConvertProceduralMeshToStaticMesh: PMC为空。"));

    return nullptr;
  }

  const int32 NumSections = ProceduralMeshComponent->GetNumSections();

  if (NumSections == 0)

  {
    UE_LOG(LogTemp, Warning,
           TEXT("ConvertProceduralMeshToStaticMesh: PMC没有网格段。"));

    return nullptr;
  }

  // 2. 创建一个临时的、只存在于内存中的UStaticMesh对象

  UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
      GetTransientPackage(), NAME_None, RF_Public | RF_Standalone);

  if (!StaticMesh)

  {
    UE_LOG(LogTemp, Error, TEXT("无法创建临时的静态网格对象。"));

    return nullptr;
  }
  // 3. 创建并设置 FMeshDescription

  FMeshDescription MeshDescription;

  FStaticMeshAttributes Attributes(MeshDescription);

  Attributes.Register();  // 这会注册包括材质槽名称在内的所有必要属性
  // 获取对多边形组材质槽名称属性的引用，以便后续设置

  // 这是将几何体与材质关联的关键

  TPolygonGroupAttributesRef<FName> PolygonGroupMaterialSlotNames =

      Attributes.GetPolygonGroupMaterialSlotNames();

  FMeshDescriptionBuilder MeshDescBuilder;

  MeshDescBuilder.SetMeshDescription(&MeshDescription);

  MeshDescBuilder.EnablePolyGroups();

  MeshDescBuilder.SetNumUVLayers(1);
  // 4. 从PMC数据填充 FMeshDescription

  TMap<FVector, FVertexID> VertexMap;

  for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)

  {
    FProcMeshSection* SectionData =
        ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
    if (!SectionData || SectionData->ProcVertexBuffer.Num() < 3 ||
        SectionData->ProcIndexBuffer.Num() < 3)

    {
      UE_LOG(LogTemp, Warning,
             TEXT("ConvertProceduralMeshToStaticMesh - 跳过段 %d: 数据不完整"),
             SectionIdx);

      continue;
    }
    // --- 开始材质设置修正 ---
    // a. 获取本段的材质

    UMaterialInterface* SectionMaterial = nullptr;

    if (SectionIdx < StaticSectionMaterials.Num() &&
        StaticSectionMaterials[SectionIdx] &&
        ::IsValid(StaticSectionMaterials[SectionIdx]))

    {
      SectionMaterial = StaticSectionMaterials[SectionIdx];

    }

    else if (StaticMeshMaterial && ::IsValid(StaticMeshMaterial))

    {
      SectionMaterial = StaticMeshMaterial;

    }

    else

    {
      SectionMaterial = ProceduralMeshComponent->GetMaterial(SectionIdx);

      if (!SectionMaterial || !::IsValid(SectionMaterial))

      {
        SectionMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
      }
    }

    // b. 为每个材质段创建一个唯一的槽名称

    const FName MaterialSlotName =
        FName(*FString::Printf(TEXT("MaterialSlot_%d"), SectionIdx));
    // c. 将材质和槽名称添加到StaticMesh的材质列表中

    // 这会在最终的UStaticMesh上创建对应的材质槽

         StaticMesh->StaticMaterials.Add(
         FStaticMaterial(SectionMaterial, MaterialSlotName));
    // d. 在MeshDescription中为这个材质段创建一个新的多边形组

    const FPolygonGroupID PolygonGroup = MeshDescBuilder.AppendPolygonGroup();

    // e. 将多边形组与我们刚刚创建的材质槽名称关联起来

    // 这是最关键的一步！

    PolygonGroupMaterialSlotNames.Set(PolygonGroup, MaterialSlotName);

    // --- 结束材质设置修正 ---
    TArray<FVertexInstanceID> VertexInstanceIDs;

    for (const FProcMeshVertex& ProcVertex : SectionData->ProcVertexBuffer)

    {
      FVertexID VertexID;

      if (FVertexID* FoundID = VertexMap.Find(ProcVertex.Position))

      {
        VertexID = *FoundID;

      }

      else

      {
        VertexID = MeshDescBuilder.AppendVertex(ProcVertex.Position);

        VertexMap.Add(ProcVertex.Position, VertexID);
      }
      FVertexInstanceID InstanceID = MeshDescBuilder.AppendInstance(VertexID);

      MeshDescBuilder.SetInstanceNormal(InstanceID, ProcVertex.Normal);

      MeshDescBuilder.SetInstanceUV(InstanceID, ProcVertex.UV0, 0);

      MeshDescBuilder.SetInstanceColor(InstanceID, FVector4(ProcVertex.Color));
      VertexInstanceIDs.Add(InstanceID);
    }
    for (int32 i = 0; i < SectionData->ProcIndexBuffer.Num(); i += 3)

    {
      if (i + 2 >= SectionData->ProcIndexBuffer.Num())

      {
        break;
      }

      int32 Index1 = SectionData->ProcIndexBuffer[i + 0];

      int32 Index2 = SectionData->ProcIndexBuffer[i + 1];

      int32 Index3 = SectionData->ProcIndexBuffer[i + 2];
      if (Index1 >= VertexInstanceIDs.Num() ||
          Index2 >= VertexInstanceIDs.Num() ||
          Index3 >= VertexInstanceIDs.Num())

      {
        UE_LOG(
            LogTemp, Error,
            TEXT("ConvertProceduralMeshToStaticMesh - 段 %d: 三角形索引越界"),
            SectionIdx);

        continue;
      }
      FVertexInstanceID V1 = VertexInstanceIDs[Index1];

      FVertexInstanceID V2 = VertexInstanceIDs[Index2];

      FVertexInstanceID V3 = VertexInstanceIDs[Index3];

      // 将三角形添加到指定了材质信息的PolygonGroup中

      MeshDescBuilder.AppendTriangle(V1, V2, V3, PolygonGroup);
    }
  }
  if (MeshDescription.Vertices().Num() == 0)

  {
    UE_LOG(LogTemp, Warning,
           TEXT("无法从PMC创建静态网格，因为没有有效的顶点数据。"));

    return nullptr;
  }

  // 7. 使用 FMeshDescription 构建 Static Mesh

  TArray<const FMeshDescription*> MeshDescPtrs;

  MeshDescPtrs.Emplace(&MeshDescription);
  UStaticMesh::FBuildMeshDescriptionsParams BuildParams;

  BuildParams.bBuildSimpleCollision = bGenerateCollision;

  StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);
  // ... (后续的碰撞体设置和PostEditChange保持不变)

  if (bGenerateCollision && ProceduralMeshComponent->ProcMeshBodySetup)

  {
    StaticMesh->CreateBodySetup();

         UBodySetup* NewBodySetup = StaticMesh->BodySetup;

    if (NewBodySetup)

    {
      NewBodySetup->AggGeom.ConvexElems =

          ProceduralMeshComponent->ProcMeshBodySetup->AggGeom.ConvexElems;

      NewBodySetup->bGenerateMirroredCollision = false;

      NewBodySetup->bDoubleSidedGeometry = true;

      NewBodySetup->CollisionTraceFlag = CTF_UseDefault;

      NewBodySetup->CreatePhysicsMeshes();
    }
  }

  // StaticMesh->PostEditChange();
  return StaticMesh;
}

void AProceduralMeshActor::GenerateStaticMesh()

{
  if (!ProceduralMeshComponent || !StaticMeshComponent ||
      ProceduralMeshComponent->GetNumSections() == 0)

  {
    return;
  }
  // 1. 清理并生成新的静态网格

  CleanupCurrentStaticMesh();

  UStaticMesh* NewStaticMesh = ConvertProceduralMeshToStaticMesh();

  if (!NewStaticMesh)

  {
    return;
  }

  // 2. 将新网格设置到组件上

  StaticMeshComponent->SetStaticMesh(NewStaticMesh);
  // 3. 确定要使用的材质，并应用到组件上

  for (int32 i = 0; i < NewStaticMesh->StaticMaterials.Num(); i++)

  {
    UMaterialInterface* MaterialToUse =
        NewStaticMesh->StaticMaterials[i].MaterialInterface;

    if (MaterialToUse)

    {
      StaticMeshComponent->SetMaterial(i, MaterialToUse);
    }
  }
  StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

  StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);
  // 4. 强制更新渲染状态以解决同步问题

  StaticMeshComponent->MarkRenderStateDirty();

  StaticMeshComponent->RecreateRenderState_Concurrent();
}
void AProceduralMeshActor::RefreshStaticMesh()

{
  if (!ProceduralMeshComponent || !StaticMeshComponent)

  {
    return;
  }

  GenerateStaticMesh();  // 统一调用这个函数来处理所有逻辑
}
void AProceduralMeshActor::ApplyMaterialToStaticMesh()

{
  // 这个函数将不再需要，因为材质设置逻辑已经整合到GenerateStaticMesh中
}
void AProceduralMeshActor::ConvertToStaticMeshComponent()

{
  // 这个函数也用GenerateStaticMesh替换

  GenerateStaticMesh();
}
// 保留其他原始函数

AProceduralMeshActor::AProceduralMeshActor()

{
  PrimaryActorTick.bCanEverTick = false;

  ProceduralMeshComponent =
      CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));

  StaticMeshComponent =
      CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));

  if (ProceduralMeshComponent && StaticMeshComponent)

  {
    RootComponent = ProceduralMeshComponent;

    StaticMeshComponent->SetupAttachment(RootComponent);

    StaticMeshComponent->SetRelativeTransform(FTransform::Identity);

    StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);

    StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProceduralMeshComponent->SetVisibility(bShowProceduralMeshInEditor);
  }

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

void AProceduralMeshActor::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent)

{
  Super::PostEditChangeProperty(PropertyChangedEvent);

  if (PropertyChangedEvent.Property)

  {
    const FName PropertyName = PropertyChangedEvent.Property->GetFName();

    if (PropertyName ==
        GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bShowStaticMeshInEditor))

    {
      if (StaticMeshComponent)

      {
        StaticMeshComponent->SetVisibility(bShowStaticMeshInEditor);

        if (bShowStaticMeshInEditor && !StaticMeshComponent->GetStaticMesh() &&
            bAutoGenerateStaticMesh)

        {
          if (ProceduralMeshComponent &&
              ProceduralMeshComponent->GetNumSections() > 0)

          {
            GenerateStaticMesh();
          }
        }
      }

      return;
    }

    if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor,
                                                bShowProceduralMeshInEditor))

    {
      if (ProceduralMeshComponent)

      {
        ProceduralMeshComponent->SetVisibility(bShowProceduralMeshInEditor);
      }

      return;
    }

    if (PropertyName ==
        GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bAutoGenerateStaticMesh))

    {
      if (bAutoGenerateStaticMesh && StaticMeshComponent &&
          ProceduralMeshComponent &&
          ProceduralMeshComponent->GetNumSections() > 0)

      {
        GenerateStaticMesh();
      }

      return;
    }

    if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMeshActor,
                                                ProceduralMeshMaterial) ||

        PropertyName ==
            GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, StaticMeshMaterial) ||

        PropertyName ==
            GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bGenerateCollision) ||

        PropertyName ==
            GET_MEMBER_NAME_CHECKED(AProceduralMeshActor, bUseAsyncCooking))

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
    ProceduralMeshComponent->SetCollisionEnabled(
        ECollisionEnabled::QueryAndPhysics);

    ProceduralMeshComponent->SetCollisionObjectType(
        ECollisionChannel::ECC_WorldStatic);

    ProceduralMeshComponent->bUseAsyncCooking = bUseAsyncCooking;

    if (ProceduralMeshMaterial)

    {
      ProceduralMeshComponent->SetMaterial(0, ProceduralMeshMaterial);
    }
  }

  if (bAutoGenerateStaticMesh && StaticMeshComponent && ProceduralMeshComponent)

  {
    GetWorld()->GetTimerManager().SetTimerForNextTick(
        [this]()

        {
          if (ProceduralMeshComponent &&
              ProceduralMeshComponent->GetNumSections() > 0)

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
    ProceduralMeshComponent->SetCollisionEnabled(
        bGenerateCollision ? ECollisionEnabled::QueryAndPhysics
                           : ECollisionEnabled::NoCollision);
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

  if (bAutoGenerateStaticMesh && StaticMeshComponent &&
      ProceduralMeshComponent->GetNumSections() > 0)

  {
    GenerateStaticMesh();  // 统一调用
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

void AProceduralMeshActor::SetProceduralMeshMaterial(
    UMaterialInterface* NewMaterial)

{
  ProceduralMeshMaterial = NewMaterial;

  ApplyMaterial();
}

void AProceduralMeshActor::SetStaticMeshMaterial(
    UMaterialInterface* NewMaterial)

{
  StaticMeshMaterial = NewMaterial;

  if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())

  {
    GenerateStaticMesh();  // 重新生成来应用材质
  }
}

void AProceduralMeshActor::SetProceduralSectionMaterial(
    int32 SectionIndex,
    UMaterialInterface* NewMaterial)

{
  if (SectionIndex < 0)

  {
    return;
  }

  if (SectionIndex >= ProceduralSectionMaterials.Num())

  {
    ProceduralSectionMaterials.SetNum(SectionIndex + 1);
  }

  ProceduralSectionMaterials[SectionIndex] = NewMaterial;

  if (ProceduralMeshComponent &&
      SectionIndex < ProceduralMeshComponent->GetNumSections())

  {
    ProceduralMeshComponent->SetMaterial(SectionIndex, NewMaterial);
  }
}

void AProceduralMeshActor::SetStaticSectionMaterial(
    int32 SectionIndex,
    UMaterialInterface* NewMaterial)

{
  if (SectionIndex < 0)

  {
    return;
  }

  if (SectionIndex >= StaticSectionMaterials.Num())

  {
    StaticSectionMaterials.SetNum(SectionIndex + 1);
  }

  StaticSectionMaterials[SectionIndex] = NewMaterial;

  if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())

  {
    GenerateStaticMesh();  // 重新生成来应用材质
  }
}

void AProceduralMeshActor::SetCollisionEnabled(bool bEnable)

{
  bGenerateCollision = bEnable;

  SetupCollision();
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

    StaticMeshComponent->SetStaticMesh(nullptr);
  }
}

void AProceduralMeshActor::EndPlay(const EEndPlayReason::Type EndPlayReason)

{
  Super::EndPlay(EndPlayReason);

  CleanupCurrentStaticMesh();
}