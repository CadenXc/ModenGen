#include "ProceduralMeshActor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "NavCollision.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"
#include "ModelGenConvexDecomp.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "HAL/PlatformProperties.h"
#include "Interface_CollisionDataProviderCore.h"
#include "IPhysXCookingModule.h"
#include "IPhysXCooking.h"
#include "PhysicsPublicCore.h"
#include "Modules/ModuleManager.h"

AProceduralMeshActor::AProceduralMeshActor()
{
    UE_LOG(LogTemp, Log, TEXT("=== ProceduralMeshActor 构造函数被调用 ==="));

    PrimaryActorTick.bCanEverTick = false;

    ProceduralMeshComponent =
        CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMeshComponent;

    // 创建StaticMeshComponent
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
    if (StaticMeshComponent)
    {
        // 将 StaticMeshComponent 附加到 RootComponent（ProceduralMeshComponent）
        // 这样它才能正确显示
        StaticMeshComponent->SetupAttachment(ProceduralMeshComponent);

        // 设置可见性
        StaticMeshComponent->SetVisibility(bShowStaticMeshComponent);

        // 注意：不在这里设置碰撞，让组件使用 StaticMesh 资源的 DefaultInstance 设置
        // 这样 StaticMesh 资源本身的碰撞配置会被组件自动继承
        UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent 创建成功（已附加到RootComponent，碰撞将使用StaticMesh资源的默认设置）"));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(
        TEXT("Material'/Engine/BasicShapes/"
            "BasicShapeMaterial.BasicShapeMaterial'"));
    if (DefaultMatFinder.Succeeded())
    {
        ProceduralDefaultMaterial = DefaultMatFinder.Object;
        UE_LOG(LogTemp, Log, TEXT("默认材质预加载成功: %s"), *ProceduralDefaultMaterial->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("所有默认材质预加载失败"));
    }

    if (ProceduralMeshComponent)
    {
        UE_LOG(LogTemp, Log, TEXT("ProceduralMeshComponent 创建成功"));
        ProceduralMeshComponent->SetVisibility(bShowProceduralComponent);
        ProceduralMeshComponent->bUseAsyncCooking = bUseAsyncCooking;

        // 设置碰撞 - 默认使用 QueryAndPhysics
        ProceduralMeshComponent->SetCollisionEnabled(
            bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        UE_LOG(LogTemp, Log, TEXT("碰撞设置完成: QueryAndPhysics"));

        // 在构造时为PMC设置一个默认材质槽（后续生成段后会应用到各段）
        if (ProceduralDefaultMaterial)
        {
            ProceduralMeshComponent->SetMaterial(0, ProceduralDefaultMaterial);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent 创建失败"));
    }

    // 构造阶段已尝试预加载并设置默认材质
    UE_LOG(LogTemp, Log, TEXT("=== ProceduralMeshActor 构造函数完成 ==="));
}

void AProceduralMeshActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);  // 添加这行

    // 生成ProceduralMesh - 子类需要自己实现清理和生成逻辑
    if (ProceduralMeshComponent && IsValid())
    {
        ProceduralMeshComponent->ClearAllMeshSections();

        // 确保碰撞设置正确应用 - 默认使用 QueryAndPhysics
        ProceduralMeshComponent->SetCollisionEnabled(
            bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);

        GenerateMesh();
        ProceduralMeshComponent->SetVisibility(true);
        // 生成完段后再为每个Section设置默认材质

        // 注意：StaticMeshComponent不会自动生成
        // 需要手动点击"转换到 StaticMesh"按钮来生成
    }
}

void AProceduralMeshActor::SetPMCCollisionEnabled(bool bEnable)
{
    bGenerateCollision = bEnable;
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetCollisionEnabled(
            bGenerateCollision ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision);
    }
}

void AProceduralMeshActor::GeneratePMCollisionData()
{
    // ===== 为PMC自动生成碰撞数据（如果还没有的话） =====
    // 这样在转换时就可以使用方式1（从PMC复制碰撞数据），获得更好的性能
    
    UE_LOG(LogTemp, Log, TEXT("=== GeneratePMCollisionData() 被调用 ==="));
    
    if (!ProceduralMeshComponent || ProceduralMeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        UE_LOG(LogTemp, Log, TEXT("GeneratePMCollisionData: PMC无效或碰撞已禁用，退出"));
        return;
    }

    // 确保PMC有BodySetup（如果不存在，手动创建）
    if (!ProceduralMeshComponent->ProcMeshBodySetup)
    {
        // 手动创建BodySetup对象
        ProceduralMeshComponent->ProcMeshBodySetup = NewObject<UBodySetup>(
            ProceduralMeshComponent, 
            UBodySetup::StaticClass(), 
            NAME_None, 
            RF_Transactional);
        
        if (ProceduralMeshComponent->ProcMeshBodySetup)
        {
            UE_LOG(LogTemp, Log, TEXT("为PMC手动创建了BodySetup"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("无法为PMC创建BodySetup"));
            return;
        }
    }
    
    if (!ProceduralMeshComponent->ProcMeshBodySetup)
    {
        return;
    }

    UBodySetup* PMCBodySetup = ProceduralMeshComponent->ProcMeshBodySetup;
    int32 CurrentElementCount = PMCBodySetup->AggGeom.GetElementCount();
    
    UE_LOG(LogTemp, Log, TEXT("GeneratePMCollisionData: PMC当前碰撞元素数: %d"), CurrentElementCount);
    if (CurrentElementCount > 0)
    {
        const FKAggregateGeom& PMCAggGeom = PMCBodySetup->AggGeom;
        UE_LOG(LogTemp, Log, TEXT("GeneratePMCollisionData: PMC已有碰撞数据详情:"));
        UE_LOG(LogTemp, Log, TEXT("  - ConvexElems: %d"), PMCAggGeom.ConvexElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - BoxElems: %d"), PMCAggGeom.BoxElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - SphereElems: %d"), PMCAggGeom.SphereElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - SphylElems: %d"), PMCAggGeom.SphylElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - TaperedCapsuleElems: %d"), PMCAggGeom.TaperedCapsuleElems.Num());
        
        // 输出每个BoxElem的详细信息
        for (int32 i = 0; i < PMCAggGeom.BoxElems.Num(); ++i)
        {
            const FKBoxElem& Box = PMCAggGeom.BoxElems[i];
            UE_LOG(LogTemp, Log, TEXT("  PMC BoxElem[%d]: 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
                i, Box.Center.X, Box.Center.Y, Box.Center.Z, Box.X, Box.Y, Box.Z);
        }
    }
    
    // 如果PMC还没有碰撞数据，基于实际顶点计算并添加
    if (CurrentElementCount == 0)
    {
        const int32 NumSections = ProceduralMeshComponent->GetNumSections();
        if (NumSections == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("PMC无网格段，无法生成碰撞数据"));
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("PMC无碰撞数据，自动生成基于实际顶点的包围盒..."));
        
        // 收集所有Section的顶点来计算包围盒
        FBox BoundingBox(ForceInitToZero);
        bool bHasVertices = false;
        
        for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
        {
            FProcMeshSection* SectionData = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
            if (SectionData && SectionData->ProcVertexBuffer.Num() > 0)
            {
                for (const FProcMeshVertex& Vertex : SectionData->ProcVertexBuffer)
                {
                    BoundingBox += Vertex.Position;
                    bHasVertices = true;
                }
            }
        }
        
        if (bHasVertices && BoundingBox.IsValid)
        {
            // 计算包围盒的中心和大小
            FVector Center = BoundingBox.GetCenter();
            FVector Extent = BoundingBox.GetExtent();
            
            // 验证包围盒是否有效（大小不能为0或负数）
            if (Extent.X > 0.0f && Extent.Y > 0.0f && Extent.Z > 0.0f)
            {
                // 创建包围盒碰撞元素并添加到PMC
                FKBoxElem BoxElem;
                BoxElem.Center = Center;
                BoxElem.X = Extent.X * 2.0f;
                BoxElem.Y = Extent.Y * 2.0f;
                BoxElem.Z = Extent.Z * 2.0f;
                
                PMCBodySetup->AggGeom.BoxElems.Add(BoxElem);
                
                // 生成物理网格
                PMCBodySetup->CreatePhysicsMeshes();
                
                UE_LOG(LogTemp, Log, TEXT("✓ 已为PMC自动生成碰撞数据: 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
                    Center.X, Center.Y, Center.Z,
                    BoxElem.X, BoxElem.Y, BoxElem.Z);
                UE_LOG(LogTemp, Log, TEXT("✓ 现在可以使用方式1（从PMC复制碰撞数据）了！"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("无法为PMC生成碰撞数据：包围盒尺寸无效"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("无法为PMC生成碰撞数据：无有效顶点"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("PMC已有碰撞数据（%d个元素），无需自动生成"), CurrentElementCount);
    }
}

void AProceduralMeshActor::SetProceduralMeshVisibility(bool bVisible)
{
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetVisibility(bVisible);
    }
}

void AProceduralMeshActor::UpdateStaticMeshComponent()
{
    if (!StaticMeshComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateStaticMeshComponent: StaticMeshComponent 为空"));
        return;
    }

    if (!ProceduralMeshComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateStaticMeshComponent: ProceduralMeshComponent 为空，无法转换"));
        return;
    }

    const int32 NumSections = ProceduralMeshComponent->GetNumSections();
    if (NumSections == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateStaticMeshComponent: ProceduralMeshComponent 没有网格段，请先生成网格"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("=== 开始转换 ProceduralMesh 到 StaticMesh ==="));
    UE_LOG(LogTemp, Log, TEXT("网格段数量: %d"), NumSections);

    // 调用 ConvertProceduralMeshToStaticMesh 转换网格
    UStaticMesh* ConvertedMesh = ConvertProceduralMeshToStaticMesh();
    if (ConvertedMesh)
    {
        // 设置StaticMesh到组件
        StaticMeshComponent->SetStaticMesh(ConvertedMesh);

        // ===== 验证组件继承的碰撞 =====
        if (ConvertedMesh->BodySetup)
        {
            // 组件应该自动从资源的 DefaultInstance 继承碰撞设置
            // 但为了确保正确，我们显式设置 Profile（这会自动应用所有相关设置）
            // 关键：只设置 Profile，不要单独设置 CollisionEnabled 和 ObjectType
            // 因为 Profile 会自动设置这些，单独设置会破坏 Profile 状态导致显示为 Custom
            FName ProfileName = ConvertedMesh->BodySetup->DefaultInstance.GetCollisionProfileName();
            if (!ProfileName.IsNone())
            {
                StaticMeshComponent->SetCollisionProfileName(ProfileName);
            }
            else
            {
                // 如果没有 Profile，则回退到手动设置
            StaticMeshComponent->SetCollisionEnabled(ConvertedMesh->BodySetup->DefaultInstance.GetCollisionEnabled());
            StaticMeshComponent->SetCollisionObjectType(ConvertedMesh->BodySetup->DefaultInstance.GetObjectType());
            }

            // 运行时检查
            FBodyInstance* CompBodyInst = StaticMeshComponent->GetBodyInstance();
            if (CompBodyInst)
            {
                UE_LOG(LogTemp, Log, TEXT("组件继承验证: 碰撞启用=%s, Profile=%s"),
                    *UEnum::GetValueAsString(CompBodyInst->GetCollisionEnabled()),
                    *CompBodyInst->GetCollisionProfileName().ToString());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("组件BodyInstance无效！碰撞未初始化"));
            }

            const FBodyInstance& DefaultInstance = ConvertedMesh->BodySetup->DefaultInstance;
            ECollisionEnabled::Type DefaultCollisionEnabled = DefaultInstance.GetCollisionEnabled();

            UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent 使用 StaticMesh 资源的默认碰撞设置"));
            UE_LOG(LogTemp, Log, TEXT("  - StaticMesh资源碰撞状态: %s"),
                DefaultCollisionEnabled == ECollisionEnabled::QueryAndPhysics ? TEXT("QueryAndPhysics") :
                DefaultCollisionEnabled == ECollisionEnabled::QueryOnly ? TEXT("QueryOnly") :
                DefaultCollisionEnabled == ECollisionEnabled::PhysicsOnly ? TEXT("PhysicsOnly") :
                TEXT("NoCollision"));
            UE_LOG(LogTemp, Log, TEXT("  - StaticMesh资源碰撞配置: %s"),
                *DefaultInstance.GetCollisionProfileName().ToString());
            UE_LOG(LogTemp, Log, TEXT("  - StaticMeshComponent会自动继承这些设置"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("StaticMesh 没有 BodySetup，无法继承碰撞设置"));
        }

        // 设置材质（如果指定了）
        if (StaticMeshMaterial)
        {
            StaticMeshComponent->SetMaterial(0, StaticMeshMaterial);
        }
        else if (ProceduralDefaultMaterial)
        {
            // 使用默认材质
            StaticMeshComponent->SetMaterial(0, ProceduralDefaultMaterial);
        }
        else
        {
            // 使用从ProceduralMeshComponent中获取的材质
            for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
            {
                UMaterialInterface* SectionMaterial = ProceduralMeshComponent->GetMaterial(SectionIdx);
                if (SectionMaterial)
                {
                    StaticMeshComponent->SetMaterial(SectionIdx, SectionMaterial);
                }
            }
        }

        // 设置可见性
        StaticMeshComponent->SetVisibility(bShowStaticMeshComponent, true);  // 第二个参数表示递归更新子组件

        // 确保组件已正确附加（运行时可能需要重新附加）
        if (!StaticMeshComponent->GetAttachParent())
        {
            StaticMeshComponent->AttachToComponent(ProceduralMeshComponent, FAttachmentTransformRules::KeepWorldTransform);
            UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent 已重新附加到 RootComponent"));
        }

        UE_LOG(LogTemp, Log, TEXT("=== 转换完成 ==="));
        UE_LOG(LogTemp, Log, TEXT("StaticMesh 材质槽数量: %d"), ConvertedMesh->StaticMaterials.Num());
        UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent 可见性: %s"), bShowStaticMeshComponent ? TEXT("可见") : TEXT("隐藏"));
        UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent 是否附加: %s"), StaticMeshComponent->GetAttachParent() ? TEXT("是") : TEXT("否"));

        // 检查StaticMesh的碰撞设置
        if (ConvertedMesh->BodySetup)
        {
            UE_LOG(LogTemp, Log, TEXT("StaticMesh BodySetup 存在，碰撞追踪标志: %d"),
                (int32)ConvertedMesh->BodySetup->CollisionTraceFlag);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("StaticMesh BodySetup 不存在！"));
        }

        // 标记组件需要更新（包括碰撞数据）
        StaticMeshComponent->MarkRenderStateDirty();
        StaticMeshComponent->RecreatePhysicsState();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateStaticMeshComponent: ConvertProceduralMeshToStaticMesh 返回 nullptr，转换失败"));
    }
}

UStaticMesh* AProceduralMeshActor::ConvertProceduralMeshToStaticMesh() 
{
  // 1. 验证输入参数
  if (!ProceduralMeshComponent) {
    return nullptr;
  }

  const int32 NumSections = ProceduralMeshComponent->GetNumSections();
  if (NumSections == 0) {
    return nullptr;
  }

  UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
      GetTransientPackage(), NAME_None, RF_Public | RF_Standalone);

  if (!StaticMesh) {
    return nullptr;
  }

  StaticMesh->SetFlags(RF_Public | RF_Standalone);
  StaticMesh->NeverStream = false;
  StaticMesh->LightMapResolution = 64;
  StaticMesh->LightMapCoordinateIndex = 1;
  StaticMesh->LightmapUVDensity = 0.0f;
  StaticMesh->LODForCollision = 0;
  StaticMesh->bAllowCPUAccess = true;
  StaticMesh->bIsBuiltAtRuntime = true;
  StaticMesh->bGenerateMeshDistanceField = false;
  StaticMesh->bHasNavigationData = false;
  StaticMesh->bSupportPhysicalMaterialMasks = false;
  StaticMesh->bSupportUniformlyDistributedSampling = false;
  StaticMesh->LpvBiasMultiplier = 1.0f;

  for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
  {
    UMaterialInterface* SectionMaterial = ProceduralMeshComponent->GetMaterial(SectionIdx);
    FName MaterialSlotName = FName(*FString::Printf(TEXT("MaterialSlot_%d"), SectionIdx));
    FStaticMaterial NewStaticMaterial(SectionMaterial, MaterialSlotName);
    NewStaticMaterial.UVChannelData = FMeshUVChannelInfo(1.f);
    StaticMesh->StaticMaterials.Add(NewStaticMaterial);
  }

  FMeshDescription MeshDescription;
  FStaticMeshAttributes Attributes(MeshDescription);
  Attributes.Register();
  
  TPolygonGroupAttributesRef<FName> PolygonGroupMaterialSlotNames =
      Attributes.GetPolygonGroupMaterialSlotNames();

  FMeshDescriptionBuilder MeshDescBuilder;
  MeshDescBuilder.SetMeshDescription(&MeshDescription);
  MeshDescBuilder.EnablePolyGroups();
  MeshDescBuilder.SetNumUVLayers(2);

  TMap<FVector, FVertexID> VertexMap;

  for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx) {
    FProcMeshSection* SectionData =
        ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
    if (!SectionData || SectionData->ProcVertexBuffer.Num() < 3 ||
        SectionData->ProcIndexBuffer.Num() < 3) {
      continue;
    }

    FName MaterialSlotName = StaticMesh->StaticMaterials[SectionIdx].MaterialSlotName;
    const FPolygonGroupID PolygonGroup = MeshDescBuilder.AppendPolygonGroup();
    PolygonGroupMaterialSlotNames.Set(PolygonGroup, MaterialSlotName);

    TArray<FVertexInstanceID> VertexInstanceIDs;

    for (const FProcMeshVertex& ProcVertex : SectionData->ProcVertexBuffer) {
      FVertexID VertexID;

      if (FVertexID* FoundID = VertexMap.Find(ProcVertex.Position)) {
        VertexID = *FoundID;
      } else {
        VertexID = MeshDescBuilder.AppendVertex(ProcVertex.Position);
        VertexMap.Add(ProcVertex.Position, VertexID);
      }
      
      FVertexInstanceID InstanceID = MeshDescBuilder.AppendInstance(VertexID);
      MeshDescBuilder.SetInstanceNormal(InstanceID, ProcVertex.Normal);
      MeshDescBuilder.SetInstanceUV(InstanceID, ProcVertex.UV0, 0);
      MeshDescBuilder.SetInstanceUV(InstanceID, ProcVertex.UV0, 1);
      MeshDescBuilder.SetInstanceColor(InstanceID, FVector4(ProcVertex.Color));
      VertexInstanceIDs.Add(InstanceID);
    }

    for (int32 i = 0; i < SectionData->ProcIndexBuffer.Num(); i += 3) {
      if (i + 2 >= SectionData->ProcIndexBuffer.Num()) {
        break;
      }

      int32 Index1 = SectionData->ProcIndexBuffer[i + 0];
      int32 Index2 = SectionData->ProcIndexBuffer[i + 1];
      int32 Index3 = SectionData->ProcIndexBuffer[i + 2];
      
      if (Index1 >= VertexInstanceIDs.Num() ||
          Index2 >= VertexInstanceIDs.Num() ||
          Index3 >= VertexInstanceIDs.Num()) {
        continue;
      }
      
      FVertexInstanceID V1 = VertexInstanceIDs[Index1];
      FVertexInstanceID V2 = VertexInstanceIDs[Index2];
      FVertexInstanceID V3 = VertexInstanceIDs[Index3];

      MeshDescBuilder.AppendTriangle(V1, V2, V3, PolygonGroup);
    }
  }

  if (MeshDescription.Vertices().Num() == 0) {
    return nullptr;
  }

  {
    UE_LOG(LogTemp, Log, TEXT("========== ProceduralMeshComponent 详细数据 =========="));
    
    if (!ProceduralMeshComponent)
    {
      UE_LOG(LogTemp, Warning, TEXT("ProceduralMeshComponent 为 nullptr"));
    }
    else
    {
      int32 TotalSections = ProceduralMeshComponent->GetNumSections();
      UE_LOG(LogTemp, Log, TEXT("网格段数量: %d"), TotalSections);
      UE_LOG(LogTemp, Log, TEXT("可见性: %s"), ProceduralMeshComponent->IsVisible() ? TEXT("可见") : TEXT("隐藏"));
      UE_LOG(LogTemp, Log, TEXT("碰撞启用: %s"), 
        ProceduralMeshComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision ? TEXT("是") : TEXT("否"));
      
      int32 TotalVertices = 0;
      int32 TotalTriangles = 0;
      
      for (int32 SectionIdx = 0; SectionIdx < TotalSections; ++SectionIdx)
      {
        FProcMeshSection* SectionData = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
        if (SectionData)
        {
          int32 SectionVertices = SectionData->ProcVertexBuffer.Num();
          int32 SectionTriangles = SectionData->ProcIndexBuffer.Num() / 3;
          TotalVertices += SectionVertices;
          TotalTriangles += SectionTriangles;
          
          UE_LOG(LogTemp, Log, TEXT("  Section[%d]: 顶点数=%d, 三角形数=%d, 材质索引=%d"),
            SectionIdx, SectionVertices, SectionTriangles, SectionIdx);
          
          if (SectionVertices > 0)
          {
            FBox SectionBounds(ForceInitToZero);
            for (const FProcMeshVertex& Vertex : SectionData->ProcVertexBuffer)
            {
              SectionBounds += Vertex.Position;
            }
            if (SectionBounds.IsValid)
            {
              FVector Center = SectionBounds.GetCenter();
              FVector Extent = SectionBounds.GetExtent();
              UE_LOG(LogTemp, Log, TEXT("    包围盒: 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
                Center.X, Center.Y, Center.Z,
                Extent.X * 2.0f, Extent.Y * 2.0f, Extent.Z * 2.0f);
            }
          }
        }
        else
        {
          UE_LOG(LogTemp, Warning, TEXT("  Section[%d]: 数据为 nullptr"), SectionIdx);
        }
      }
      
      UE_LOG(LogTemp, Log, TEXT("总计: 顶点数=%d, 三角形数=%d"), TotalVertices, TotalTriangles);
      
      if (ProceduralMeshComponent->ProcMeshBodySetup)
      {
        UBodySetup* PMCBodySetup = ProceduralMeshComponent->ProcMeshBodySetup;
        const FKAggregateGeom& PMCAggGeom = PMCBodySetup->AggGeom;
        
        UE_LOG(LogTemp, Log, TEXT("碰撞数据:"));
        UE_LOG(LogTemp, Log, TEXT("  ConvexElems: %d"), PMCAggGeom.ConvexElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  BoxElems: %d"), PMCAggGeom.BoxElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  SphereElems: %d"), PMCAggGeom.SphereElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  SphylElems: %d"), PMCAggGeom.SphylElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  TaperedCapsuleElems: %d"), PMCAggGeom.TaperedCapsuleElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  总元素数: %d"), PMCAggGeom.GetElementCount());
        
        for (int32 i = 0; i < PMCAggGeom.BoxElems.Num(); ++i)
        {
          const FKBoxElem& Box = PMCAggGeom.BoxElems[i];
          UE_LOG(LogTemp, Log, TEXT("  Box[%d]: 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
            i, Box.Center.X, Box.Center.Y, Box.Center.Z, Box.X, Box.Y, Box.Z);
        }
        
        for (int32 i = 0; i < PMCAggGeom.SphereElems.Num(); ++i)
        {
          const FKSphereElem& Sphere = PMCAggGeom.SphereElems[i];
          UE_LOG(LogTemp, Log, TEXT("  Sphere[%d]: 中心=(%.2f, %.2f, %.2f), 半径=%.2f"),
            i, Sphere.Center.X, Sphere.Center.Y, Sphere.Center.Z, Sphere.Radius);
        }
      }
      else
      {
        UE_LOG(LogTemp, Log, TEXT("碰撞数据: ProcMeshBodySetup 为 nullptr（无碰撞数据）"));
      }
      
      // 材质信息
      int32 NumMaterials = ProceduralMeshComponent->GetNumMaterials();
      UE_LOG(LogTemp, Log, TEXT("材质数量: %d"), NumMaterials);
      for (int32 MatIdx = 0; MatIdx < NumMaterials; ++MatIdx)
      {
        UMaterialInterface* Material = ProceduralMeshComponent->GetMaterial(MatIdx);
        UE_LOG(LogTemp, Log, TEXT("  材质[%d]: %s"), MatIdx, Material ? *Material->GetName() : TEXT("None"));
      }
    }
    
    UE_LOG(LogTemp, Log, TEXT("================================================"));
  }

  TArray<const FMeshDescription*> MeshDescPtrs;
  MeshDescPtrs.Emplace(&MeshDescription);
  UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
  
  BuildParams.bUseHashAsGuid = true;
  BuildParams.bMarkPackageDirty = true;
  BuildParams.bBuildSimpleCollision = false;  // 设置为 false，避免生成不必要的 Box 碰撞，直接手动生成更精确的碰撞
  BuildParams.bCommitMeshDescription = true;
  
  bool bManuallySetCollision = true;
  UE_LOG(LogTemp, Log, TEXT("使用简单碰撞（手动生成精确碰撞）"));
  
  StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);

  if (StaticMesh->RenderData && StaticMesh->RenderData->LODResources.Num() >= 1)
  {
    FStaticMeshLODResources& LODResources = StaticMesh->RenderData->LODResources[0];
    StaticMesh->RenderData->LODResources[0].bHasColorVertexData = true;
    StaticMesh->InitResources();
    
    // 计算扩展边界（参考 InitStaticMeshInfo）
    StaticMesh->CalculateExtendedBounds();
    
    // 设置 ScreenSize（参考 InitStaticMeshInfo）
    StaticMesh->RenderData->ScreenSize[0] = 1.0f;
    StaticMesh->RenderData->ScreenSize[1] = 0.2f;
    StaticMesh->RenderData->ScreenSize[2] = 0.1f;
    
    UE_LOG(LogTemp, Log, TEXT("RenderData初始化完成 - LOD数量: %d, 有颜色数据: 是"),
      StaticMesh->RenderData->LODResources.Num());
  }
  else
  {
    UE_LOG(LogTemp, Warning, TEXT("BuildFromMeshDescriptions后RenderData无效或没有LOD资源"));
  }

  {
    StaticMesh->CreateBodySetup();
    UBodySetup* NewBodySetup = StaticMesh->BodySetup;
    if (NewBodySetup) {
      UE_LOG(LogTemp, Log, TEXT("=== 使用简单碰撞 ==="));

      // 方式1：使用QuickHull凸包分解算法生成多个高精度凸包
      UE_LOG(LogTemp, Log, TEXT("使用QuickHull凸包分解算法生成碰撞..."));
      
      int32 HullCount = 8;
      int32 MaxHullVerts = 16;
      uint32 HullPrecision = 100000;
      
      bool bSuccess = FModelGenConvexDecomp::GenerateConvexHulls(
        ProceduralMeshComponent,
        NewBodySetup,
        HullCount,
        MaxHullVerts,
        HullPrecision);
      
      if (bSuccess && NewBodySetup->AggGeom.ConvexElems.Num() > 0)
      {
        int32 ConvexCount = NewBodySetup->AggGeom.ConvexElems.Num();
        UE_LOG(LogTemp, Log, TEXT("✓ QuickHull凸包分解成功！生成了 %d 个凸包元素"), ConvexCount);
      }
      else
      {
        // 方式2：如果QuickHull失败，使用RenderData.Bounds生成简单的包围盒作为兜底方案
        UE_LOG(LogTemp, Warning, TEXT("QuickHull凸包分解失败，使用RenderData.Bounds生成包围盒作为兜底方案"));
        
        if (StaticMesh->RenderData.IsValid())
        {
          const FBoxSphereBounds& Bounds = StaticMesh->RenderData->Bounds;
          if (Bounds.BoxExtent.X > 0.0f && Bounds.BoxExtent.Y > 0.0f && Bounds.BoxExtent.Z > 0.0f)
          {
            FKBoxElem BoxElem;
            BoxElem.Center = Bounds.Origin;
            BoxElem.X = Bounds.BoxExtent.X * 2.0f;
            BoxElem.Y = Bounds.BoxExtent.Y * 2.0f;
            BoxElem.Z = Bounds.BoxExtent.Z * 2.0f;
            NewBodySetup->AggGeom.BoxElems.Add(BoxElem);
            
            UE_LOG(LogTemp, Log, TEXT("✓ 基于RenderData.Bounds创建包围盒: 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
              BoxElem.Center.X, BoxElem.Center.Y, BoxElem.Center.Z,
              BoxElem.X, BoxElem.Y, BoxElem.Z);
          }
          else
          {
            UE_LOG(LogTemp, Error, TEXT("✗ RenderData.Bounds无效（BoxExtent为0或负数），无法生成碰撞"));
          }
        }
        else
        {
          UE_LOG(LogTemp, Error, TEXT("✗ RenderData无效，无法生成碰撞数据"));
        }
      }
      
      int32 FinalElementCount = NewBodySetup->AggGeom.GetElementCount();
      if (FinalElementCount > 0)
      {
        UE_LOG(LogTemp, Log, TEXT("碰撞生成完成，共 %d 个元素"), FinalElementCount);
      }
      else
      {
        UE_LOG(LogTemp, Error, TEXT("✗ 所有碰撞生成方式均失败，无法生成碰撞数据"));
      }

      NewBodySetup->CollisionTraceFlag = CTF_UseDefault;
      
      NewBodySetup->bGenerateMirroredCollision = false;
      NewBodySetup->bGenerateNonMirroredCollision = true;
      NewBodySetup->bDoubleSidedGeometry = true;
      NewBodySetup->bSupportUVsAndFaceRemap = false;
      NewBodySetup->bConsiderForBounds = true;
      NewBodySetup->bMeshCollideAll = true;
      NewBodySetup->PhysicsType = EPhysicsType::PhysType_Default;
      
      if (ProceduralMeshComponent->ProcMeshBodySetup && ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial)
      {
        NewBodySetup->PhysMaterial = ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial;
        UE_LOG(LogTemp, Log, TEXT("已复制物理材质: %s"), *NewBodySetup->PhysMaterial->GetName());
      }
      
      NewBodySetup->BuildScale3D = FVector(1.0f, 1.0f, 1.0f);

      NewBodySetup->DefaultInstance.SetCollisionProfileName(TEXT("BlockAll"));
      NewBodySetup->DefaultInstance.SetEnableGravity(false);
      NewBodySetup->DefaultInstance.bUseCCD = false;
      
      // 在所有 BodySetup 属性设置完成后，生成物理网格
      if (NewBodySetup->AggGeom.GetElementCount() > 0)
      {
        // 确保所有 ConvexElem 都调用了 UpdateElemBox() 并设置了有效的 Transform
        for (FKConvexElem& ConvexElem : NewBodySetup->AggGeom.ConvexElems)
        {
          if (ConvexElem.VertexData.Num() >= 4)
          {
            ConvexElem.UpdateElemBox();
            // 确保 Transform 有效（GetCookInfo 需要有效的 Transform）
            if (!ConvexElem.GetTransform().IsValid())
            {
              ConvexElem.SetTransform(FTransform::Identity);
            }
          }
        }
        
        UE_LOG(LogTemp, Log, TEXT("准备生成物理网格 - ConvexElems数量: %d"), NewBodySetup->AggGeom.ConvexElems.Num());
        
        // 确保 bNeverNeedsCookedCollisionData = false（允许运行时烹饪）
        NewBodySetup->bNeverNeedsCookedCollisionData = false;
        
        // 清除旧的 cooked data，强制重新生成
        NewBodySetup->InvalidatePhysicsData();
        
        // 检查运行时烹饪是否启用（检查 RuntimePhysXCooking 模块）
        bool bRuntimeCookingEnabled = false;
        // IsRuntimeCookingEnabled() 检查的是 RuntimePhysXCooking 模块
        // 这个模块在编辑器中可能未加载，但在运行时应该可用
        IPhysXCookingModule* PhysXCookingModule = GetPhysXCookingModule(false);
        if (PhysXCookingModule && PhysXCookingModule->GetPhysXCooking())
        {
          // 检查 RuntimePhysXCooking 模块是否已加载
          FModuleManager& ModuleManager = FModuleManager::Get();
          if (ModuleManager.IsModuleLoaded("RuntimePhysXCooking"))
          {
            bRuntimeCookingEnabled = true;
            UE_LOG(LogTemp, Log, TEXT("✓ 运行时烹饪已启用 - RuntimePhysXCooking 模块已加载"));
          }
          else
          {
            UE_LOG(LogTemp, Warning, TEXT("⚠ PhysXCookingModule 可用，但 RuntimePhysXCooking 模块未加载"));
            UE_LOG(LogTemp, Warning, TEXT("  这可能导致 ConvexMesh 无法在运行时创建"));
            UE_LOG(LogTemp, Warning, TEXT("  请确保在项目设置中启用了 RuntimePhysXCooking 插件"));
          }
        }
        else
        {
          UE_LOG(LogTemp, Warning, TEXT("⚠ PhysXCookingModule 不可用"));
          UE_LOG(LogTemp, Warning, TEXT("  这可能导致 ConvexMesh 无法在运行时创建"));
        }
        
        NewBodySetup->CreatePhysicsMeshes();
        NewBodySetup->bCreatedPhysicsMeshes = true;  // 参考 InitStaticMeshInfo
        
        // 检查是否有创建失败标志
        if (NewBodySetup->bFailedToCreatePhysicsMeshes)
        {
          UE_LOG(LogTemp, Error, TEXT("✗ CreatePhysicsMeshes 失败 - bFailedToCreatePhysicsMeshes = true"));
          UE_LOG(LogTemp, Error, TEXT("可能的原因：运行时烹饪未启用或 PhysXCookingModule 不可用"));
        }
        
        // 验证 ConvexMesh 是否成功创建
        int32 ValidConvexMeshCount = 0;
        for (const FKConvexElem& ConvexElem : NewBodySetup->AggGeom.ConvexElems)
        {
          if (ConvexElem.GetConvexMesh() != nullptr)
          {
            ValidConvexMeshCount++;
          }
        }
        
        if (ValidConvexMeshCount == NewBodySetup->AggGeom.ConvexElems.Num())
        {
          UE_LOG(LogTemp, Log, TEXT("✓ 成功生成物理网格 - 所有 %d 个 ConvexMesh 已创建"), ValidConvexMeshCount);
        }
        else
        {
          UE_LOG(LogTemp, Warning, TEXT("⚠ 物理网格生成不完整 - 成功: %d/%d"), 
            ValidConvexMeshCount, NewBodySetup->AggGeom.ConvexElems.Num());
        }
      }
      
      UE_LOG(LogTemp, Log, TEXT("StaticMesh资源层面：完整碰撞配置已设置"));
      UE_LOG(LogTemp, Log, TEXT("  - 碰撞启用: %s"), 
          *UEnum::GetValueAsString(NewBodySetup->DefaultInstance.GetCollisionEnabled()));
      UE_LOG(LogTemp, Log, TEXT("  - 碰撞配置: %s"), 
          *NewBodySetup->DefaultInstance.GetCollisionProfileName().ToString());
      UE_LOG(LogTemp, Log, TEXT("  - 对象类型: %s"), 
          *UEnum::GetValueAsString(NewBodySetup->DefaultInstance.GetObjectType()));
      UE_LOG(LogTemp, Log, TEXT("  - 重力: 禁用"));
      UE_LOG(LogTemp, Log, TEXT("  - CCD: 禁用"));

      // 生成复杂碰撞（TriMeshes）- 参考 InitStaticMeshInfo
      if (NewBodySetup->TriMeshes.Num() == 0 && StaticMesh->RenderData && StaticMesh->RenderData->LODResources.Num() > 0)
      {
        UE_LOG(LogTemp, Log, TEXT("=== 开始生成复杂碰撞（TriMeshes）==="));
        
        TArray<FVector> NewVertices;
        TArray<FTriIndices> NewIndices;
        int32 NumTris = 0;

        // 从 RenderData 提取顶点和索引数据
        const FStaticMeshLODResources& LODResource = StaticMesh->RenderData->LODResources[0];
        const FPositionVertexBuffer& PositionVertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
        const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;

        // 提取顶点
        const int32 NumVertices = PositionVertexBuffer.GetNumVertices();
        NewVertices.Reserve(NumVertices);
        for (int32 VertIdx = 0; VertIdx < NumVertices; ++VertIdx)
        {
          NewVertices.Add(PositionVertexBuffer.VertexPosition(VertIdx));
        }

        // 提取索引（参考 InitStaticMeshInfo）- 转换为 FTriIndices 格式
        const int32 NumIndices = IndexBuffer.GetNumIndices();
        NumTris = NumIndices / 3;
        NewIndices.Reserve(NumTris);
        for (int32 Idx = 0; Idx < NumIndices; Idx += 3)
        {
          FTriIndices Tri;
          Tri.v0 = IndexBuffer.GetIndex(Idx);
          Tri.v1 = IndexBuffer.GetIndex(Idx + 1);
          Tri.v2 = IndexBuffer.GetIndex(Idx + 2);
          NewIndices.Add(Tri);
        }

        UE_LOG(LogTemp, Log, TEXT("从 RenderData 提取数据: 顶点数=%d, 索引数=%d, 三角形数=%d"), 
          NumVertices, NumIndices, NumTris);

        if (NewVertices.Num() > 0 && NewIndices.Num() > 0)
        {
          // 获取 PhysXCookingModule
          IPhysXCookingModule* PhysXCookingModule = GetPhysXCookingModule();
          if (PhysXCookingModule && PhysXCookingModule->GetPhysXCooking())
          {
            // 创建 TriMeshes
            NewBodySetup->TriMeshes.AddZeroed();

            // 设置 RuntimeCookFlags
            EPhysXMeshCookFlags RuntimeCookFlags = EPhysXMeshCookFlags::Default;
            if (UPhysicsSettings::Get()->bSuppressFaceRemapTable)
            {
              RuntimeCookFlags |= EPhysXMeshCookFlags::SuppressFaceRemapTable;
              UE_LOG(LogTemp, Log, TEXT("CookFlags: 启用 SuppressFaceRemapTable"));
            }

            // 创建 MaterialIndices
            TArray<uint16> MaterialIndices;
            MaterialIndices.AddZeroed(NumTris);

            UE_LOG(LogTemp, Log, TEXT("调用 CreateTriMesh 生成复杂碰撞..."));

            // 调用 CreateTriMesh（参考 InitStaticMeshInfo）
            const bool bError = !PhysXCookingModule->GetPhysXCooking()->CreateTriMesh(
              FName(FPlatformProperties::GetPhysicsFormat()),
              RuntimeCookFlags,
              NewVertices,
              NewIndices,
              MaterialIndices,
              false,
              NewBodySetup->TriMeshes[0]);

            if (!bError)
            {
              NewBodySetup->bCreatedPhysicsMeshes = true;
              UE_LOG(LogTemp, Log, TEXT("✓ 成功创建 TriMeshes（复杂碰撞）"));
              UE_LOG(LogTemp, Log, TEXT("  - 顶点数: %d"), NewVertices.Num());
              UE_LOG(LogTemp, Log, TEXT("  - 三角形数: %d"), NumTris);
              UE_LOG(LogTemp, Log, TEXT("  - TriMeshes 数量: %d"), NewBodySetup->TriMeshes.Num());
            }
            else
            {
              UE_LOG(LogTemp, Warning, TEXT("✗ CreateTriMesh 失败，无法生成复杂碰撞"));
              NewBodySetup->TriMeshes.Empty();
            }
          }
          else
          {
            UE_LOG(LogTemp, Warning, TEXT("✗ 无法获取 PhysXCookingModule，跳过 TriMeshes 生成"));
          }
        }
        else
        {
          UE_LOG(LogTemp, Warning, TEXT("✗ 无法提取顶点或索引数据，跳过 TriMeshes 生成"));
          UE_LOG(LogTemp, Warning, TEXT("  - 顶点数: %d, 索引数: %d"), NewVertices.Num(), NewIndices.Num());
        }
        
        UE_LOG(LogTemp, Log, TEXT("=== 复杂碰撞生成完成 ==="));
      }
      else
      {
        if (NewBodySetup->TriMeshes.Num() > 0)
        {
          UE_LOG(LogTemp, Log, TEXT("TriMeshes 已存在（数量: %d），跳过生成"), NewBodySetup->TriMeshes.Num());
        }
        else if (!StaticMesh->RenderData)
        {
          UE_LOG(LogTemp, Warning, TEXT("StaticMesh->RenderData 无效，无法生成 TriMeshes"));
        }
        else if (StaticMesh->RenderData->LODResources.Num() == 0)
        {
          UE_LOG(LogTemp, Warning, TEXT("RenderData 没有 LOD 资源，无法生成 TriMeshes"));
        }
      }
      
      // 碰撞数据统计
      const int32 AggGeomElementCount = NewBodySetup->AggGeom.GetElementCount();
      const int32 TriMeshesCount = NewBodySetup->TriMeshes.Num();
      
      UE_LOG(LogTemp, Log, TEXT("=== 碰撞数据统计 ==="));
      UE_LOG(LogTemp, Log, TEXT("简单碰撞（AggGeom）元素数量: %d"), AggGeomElementCount);
      UE_LOG(LogTemp, Log, TEXT("复杂碰撞（TriMeshes）数量: %d"), TriMeshesCount);
      UE_LOG(LogTemp, Log, TEXT("碰撞追踪标志: %s"),
          NewBodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple ? TEXT("UseComplexAsSimple") :
          NewBodySetup->CollisionTraceFlag == CTF_UseSimpleAsComplex ? TEXT("UseSimpleAsComplex") :
          TEXT("UseDefault"));

      if (AggGeomElementCount > 0)
      {
        const FKAggregateGeom& FinalAggGeom = NewBodySetup->AggGeom;
        UE_LOG(LogTemp, Log, TEXT("简单碰撞详细信息:"));
        UE_LOG(LogTemp, Log, TEXT("  - ConvexElems: %d"), FinalAggGeom.ConvexElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - BoxElems: %d"), FinalAggGeom.BoxElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - SphereElems: %d"), FinalAggGeom.SphereElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - SphylElems: %d"), FinalAggGeom.SphylElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - TaperedCapsuleElems: %d"), FinalAggGeom.TaperedCapsuleElems.Num());
      }
      else
      {
        UE_LOG(LogTemp, Warning, TEXT("简单碰撞数据为空！"));
      }

      if (TriMeshesCount > 0)
      {
        UE_LOG(LogTemp, Log, TEXT("复杂碰撞已生成，可用于精确碰撞检测"));
      }
      else
      {
        UE_LOG(LogTemp, Log, TEXT("复杂碰撞未生成，将仅使用简单碰撞"));
      }

      UE_LOG(LogTemp, Log, TEXT("=== StaticMesh BodySetup 配置总结 ==="));
      UE_LOG(LogTemp, Log, TEXT("碰撞类型: %s"), 
          (AggGeomElementCount > 0 && TriMeshesCount > 0) ? TEXT("简单碰撞 + 复杂碰撞") :
          (TriMeshesCount > 0) ? TEXT("复杂碰撞") :
          (AggGeomElementCount > 0) ? TEXT("简单碰撞") : TEXT("无碰撞"));
      UE_LOG(LogTemp, Log, TEXT("碰撞追踪标志: %s"),
          NewBodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple ? TEXT("UseComplexAsSimple") :
          NewBodySetup->CollisionTraceFlag == CTF_UseSimpleAsComplex ? TEXT("UseSimpleAsComplex") :
          TEXT("UseDefault"));
      UE_LOG(LogTemp, Log, TEXT("双面几何: %s"), NewBodySetup->bDoubleSidedGeometry ? TEXT("是") : TEXT("否"));
      UE_LOG(LogTemp, Log, TEXT("镜像碰撞: %s"), NewBodySetup->bGenerateMirroredCollision ? TEXT("生成") : TEXT("不生成"));
      UE_LOG(LogTemp, Log, TEXT("非镜像碰撞: %s"), NewBodySetup->bGenerateNonMirroredCollision ? TEXT("生成") : TEXT("不生成"));
      UE_LOG(LogTemp, Log, TEXT("考虑边界: %s"), NewBodySetup->bConsiderForBounds ? TEXT("是") : TEXT("否"));
      UE_LOG(LogTemp, Log, TEXT("物理材质: %s"), NewBodySetup->PhysMaterial ? *NewBodySetup->PhysMaterial->GetName() : TEXT("默认"));
      UE_LOG(LogTemp, Log, TEXT("默认碰撞响应: %s"),
          NewBodySetup->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics ? TEXT("QueryAndPhysics") :
          NewBodySetup->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::QueryOnly ? TEXT("QueryOnly") :
          TEXT("NoCollision"));
      UE_LOG(LogTemp, Log, TEXT("=== StaticMesh 资源属性总结 ==="));
      UE_LOG(LogTemp, Log, TEXT("光照贴图分辨率: %d"), StaticMesh->LightMapResolution);
      UE_LOG(LogTemp, Log, TEXT("光照贴图UV通道: %d"), StaticMesh->LightMapCoordinateIndex);
      UE_LOG(LogTemp, Log, TEXT("碰撞LOD: %d"), StaticMesh->LODForCollision);
      UE_LOG(LogTemp, Log, TEXT("允许CPU访问: %s"), StaticMesh->bAllowCPUAccess ? TEXT("是") : TEXT("否"));
      UE_LOG(LogTemp, Log, TEXT("运行时构建: %s"), StaticMesh->bIsBuiltAtRuntime ? TEXT("是") : TEXT("否"));
      UE_LOG(LogTemp, Log, TEXT("StaticMesh BodySetup 配置完成！"));
    }
    else
    {
      UE_LOG(LogTemp, Error, TEXT("无法获取StaticMesh的BodySetup"));
    }
  }

  StaticMesh->AddToRoot();

  StaticMesh->CreateNavCollision(true);

  return StaticMesh;
}
