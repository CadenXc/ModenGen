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

        // 【纹理流送修复 - 方案一】设置流送距离乘数。默认是 1.0，设为 10.0 或更高会让引擎加载更高清的贴图。
        // UE 4.26 API: 使用 StreamingDistanceMultiplier 属性
        StaticMeshComponent->StreamingDistanceMultiplier = 10.0f;
        UE_LOG(LogTemp, Log, TEXT("已设置流送距离乘数: 10.0 (强制加载高清贴图)"));

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

  // 2. 创建并初始化 StaticMesh 对象
  UStaticMesh* StaticMesh = CreateStaticMeshObject();
  if (!StaticMesh) {
    return nullptr;
  }

  // 3. 从 ProceduralMeshComponent 构建 StaticMesh 几何体
  if (!BuildStaticMeshGeometryFromProceduralMesh(StaticMesh)) {
    return nullptr;
  }

  // 4. 初始化 StaticMesh 的 RenderData
  InitializeStaticMeshRenderData(StaticMesh);

  // 5. 创建 BodySetup 并生成碰撞
  SetupBodySetupAndCollision(StaticMesh);

  StaticMesh->CreateNavCollision(true);

  return StaticMesh;
}

// ============================================================================
// 辅助函数实现
// ============================================================================

// 辅助函数：创建并初始化 StaticMesh 对象
UStaticMesh* AProceduralMeshActor::CreateStaticMeshObject() const
{
  // 使用 RF_Public 和 RF_Transient，让 StaticMesh 可以被垃圾回收器管理
  // StaticMeshComponent 会通过 SetStaticMesh() 持有引用，所以不需要 AddToRoot()
  UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
      GetTransientPackage(), NAME_None, RF_Public | RF_Transient);

  if (!StaticMesh) {
    return nullptr;
  }

  StaticMesh->SetFlags(RF_Public | RF_Transient);
  StaticMesh->NeverStream = true;
  StaticMesh->LightMapResolution = 64;
  StaticMesh->LightMapCoordinateIndex = 0;  // 与参考模型一致，使用 UV0（纹理UV）来做光照烘焙
  StaticMesh->LightmapUVDensity = 512.0f;  // 给一个合理的非零值，0.0f 可能干扰流送系统的启发式计算
  StaticMesh->LODForCollision = 0;
  StaticMesh->bAllowCPUAccess = true;
  StaticMesh->bIsBuiltAtRuntime = true;
  StaticMesh->bGenerateMeshDistanceField = false;
  StaticMesh->bHasNavigationData = false;
  StaticMesh->bSupportPhysicalMaterialMasks = false;
  StaticMesh->bSupportUniformlyDistributedSampling = false;
  StaticMesh->LpvBiasMultiplier = 1.0f;

  const int32 NumSections = ProceduralMeshComponent ? ProceduralMeshComponent->GetNumSections() : 0;
  for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
  {
    UMaterialInterface* SectionMaterial = ProceduralMeshComponent->GetMaterial(SectionIdx);
    FName MaterialSlotName = FName(*FString::Printf(TEXT("MaterialSlot_%d"), SectionIdx));
    FStaticMaterial NewStaticMaterial(SectionMaterial, MaterialSlotName);
    // 【修改点 1】将默认密度从 1.f 改为 1024.f (或更高)
    // 告诉引擎：每1个世界单位对应大量的纹理像素，迫使它加载高Mips
    // 这能确保在 UpdateUVChannelData 计算失败或未完全覆盖时，有一个足够大的保底值
    NewStaticMaterial.UVChannelData = FMeshUVChannelInfo(1024.f);
    StaticMesh->StaticMaterials.Add(NewStaticMaterial);
  }

  return StaticMesh;
}

// 辅助函数：从 ProceduralMeshComponent 构建 MeshDescription
bool AProceduralMeshActor::BuildMeshDescriptionFromPMC(FMeshDescription& OutMeshDescription, UStaticMesh* StaticMesh) const
{
  if (!ProceduralMeshComponent) {
    return false;
  }

  const int32 NumSections = ProceduralMeshComponent->GetNumSections();
  if (NumSections == 0) {
    return false;
  }

  FStaticMeshAttributes Attributes(OutMeshDescription);
  Attributes.Register();
  
  TPolygonGroupAttributesRef<FName> PolygonGroupMaterialSlotNames =
      Attributes.GetPolygonGroupMaterialSlotNames();

  FMeshDescriptionBuilder MeshDescBuilder;
  MeshDescBuilder.SetMeshDescription(&OutMeshDescription);
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
      
      // 只设置法线，让引擎根据 UV 自动计算切线和副法线
      // 引擎会使用 MikkTSpace 算法基于已有的法线和 UV 正确计算切线
      // 这样可以保持硬边法线，同时确保切线与 UV 展开方向匹配
      
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

  return OutMeshDescription.Vertices().Num() > 0;
}

// 辅助函数：手动计算切线（用于解决移动端引擎计算失败的问题）
void AProceduralMeshActor::GenerateTangentsManually(FMeshDescription& MeshDescription) const
{
    FStaticMeshAttributes Attributes(MeshDescription);
    
    // 获取各种数据访问器
    TVertexAttributesRef<FVector> VertexPositions = Attributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
    TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
    
    // 遍历所有多边形（Polygon）
    for (const FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs())
    {
        // 【修正点】UE4.26 API: 先获取多边形包含的三角形ID列表
        const TArray<FTriangleID>& TriangleIDs = MeshDescription.GetPolygonTriangleIDs(PolygonID);
        
        for (const FTriangleID TriangleID : TriangleIDs)
        {
            // 【修正点】UE4.26 API: 通过三角形ID获取3个顶点实例ID
            TArrayView<const FVertexInstanceID> VertexInstances = MeshDescription.GetTriangleVertexInstances(TriangleID);
            
            // 确保是一个有效的三角形（必须有3个顶点）
            if (VertexInstances.Num() != 3)
            {
                continue;
            }

            const FVertexInstanceID Instance0 = VertexInstances[0];
            const FVertexInstanceID Instance1 = VertexInstances[1];
            const FVertexInstanceID Instance2 = VertexInstances[2];
            
            // 获取顶点位置
            const FVector P0 = VertexPositions[MeshDescription.GetVertexInstanceVertex(Instance0)];
            const FVector P1 = VertexPositions[MeshDescription.GetVertexInstanceVertex(Instance1)];
            const FVector P2 = VertexPositions[MeshDescription.GetVertexInstanceVertex(Instance2)];
            
            // 获取 UV (使用第0层 UV)
            const FVector2D UV0 = VertexInstanceUVs.Get(Instance0, 0);
            const FVector2D UV1 = VertexInstanceUVs.Get(Instance1, 0);
            const FVector2D UV2 = VertexInstanceUVs.Get(Instance2, 0);
            
            // 计算边向量 (Edge Vectors)
            const FVector Edge1 = P1 - P0;
            const FVector Edge2 = P2 - P0;
            
            // 计算 UV 差值 (Delta UV)
            const FVector2D DeltaUV1 = UV1 - UV0;
            const FVector2D DeltaUV2 = UV2 - UV0;
            
            // 标准切线计算公式
            // 解决方程：Edge = Tangent * DeltaU + Bitangent * DeltaV
            float Det = (DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X);
            
            // 防止除以0（UV重叠或退化三角形）
            if (FMath::IsNearlyZero(Det))
            {
                Det = 1.0f; 
            }
            
            const float InvDet = 1.0f / Det;
            
            FVector FaceTangent;
            FaceTangent.X = InvDet * (DeltaUV2.Y * Edge1.X - DeltaUV1.Y * Edge2.X);
            FaceTangent.Y = InvDet * (DeltaUV2.Y * Edge1.Y - DeltaUV1.Y * Edge2.Y);
            FaceTangent.Z = InvDet * (DeltaUV2.Y * Edge1.Z - DeltaUV1.Y * Edge2.Z);
            FaceTangent.Normalize();
            
            // 将面切线应用到三个顶点上 (Gram-Schmidt 正交化)
            const FVertexInstanceID CurrentInstances[3] = { Instance0, Instance1, Instance2 };
            
            for (int i = 0; i < 3; i++)
            {
                FVertexInstanceID CurrentID = CurrentInstances[i];
                const FVector Normal = VertexInstanceNormals[CurrentID];
                
                // Gram-Schmidt 正交化：Tangent = FaceTangent - Normal * (Normal · FaceTangent)
                FVector OrthoTangent = (FaceTangent - Normal * FVector::DotProduct(Normal, FaceTangent));
                OrthoTangent.Normalize();
                
                // 如果结果是0（比如法线和切线完全平行），给个默认值防止渲染错误
                if (OrthoTangent.IsZero())
                {
                    FVector TangentX = FVector::CrossProduct(Normal, FVector::UpVector);
                    if (TangentX.IsZero()) TangentX = FVector::CrossProduct(Normal, FVector::RightVector);
                    OrthoTangent = TangentX.GetSafeNormal();
                }
                
                // 写入数据
                VertexInstanceTangents[CurrentID] = OrthoTangent;
                
                // 计算副法线符号
                FVector Bitangent = FVector::CrossProduct(Normal, OrthoTangent);
                float Sign = (FVector::DotProduct(FVector::CrossProduct(Normal, FaceTangent), Bitangent) < 0.0f) ? -1.0f : 1.0f;
                VertexInstanceBinormalSigns[CurrentID] = Sign;
            }
        }
    }
}

// 辅助函数：从 ProceduralMeshComponent 构建 StaticMesh 几何体
bool AProceduralMeshActor::BuildStaticMeshGeometryFromProceduralMesh(UStaticMesh* StaticMesh) const
{
  if (!StaticMesh) {
    return false;
  }

  // 1. 准备 MeshDescription（先创建空的）
  FMeshDescription MeshDescription;
  
  // 2. 【关键】先注册属性！确保容器准备好了
  FStaticMeshAttributes Attributes(MeshDescription);
  Attributes.Register(); // 注册所有标准属性（包括法线、切线、UV、颜色）

  // 3. 填充数据（顶点、三角面、法线、UV）
  if (!BuildMeshDescriptionFromPMC(MeshDescription, StaticMesh)) {
    return false;
  }

  // 4. 【关键修改】使用手动计算代替引擎内置计算
  // 删除原来的 FStaticMeshOperations::ComputeTangentsAndNormals
  // 使用我们刚写的函数：
  GenerateTangentsManually(MeshDescription);


  // 5. (可选) 调试日志，确保不是 0
  if (MeshDescription.VertexInstances().Num() > 0) {
      FVector T = Attributes.GetVertexInstanceTangents()[FVertexInstanceID(0)];
      UE_LOG(LogTemp, Log, TEXT("手动切线计算结果 Sample: %s"), *T.ToString());
  }

  // 6. 提交构建 (BuildFromMeshDescriptions)
  TArray<const FMeshDescription*> MeshDescPtrs;
  MeshDescPtrs.Emplace(&MeshDescription);
  
  UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
  BuildParams.bUseHashAsGuid = true;
  BuildParams.bMarkPackageDirty = true;
  BuildParams.bBuildSimpleCollision = false;  // 设置为 false，避免生成不必要的 Box 碰撞，直接手动生成更精确的碰撞
  BuildParams.bCommitMeshDescription = true;
  
  StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);

  return true;
}

// 辅助函数：初始化 StaticMesh 的 RenderData
bool AProceduralMeshActor::InitializeStaticMeshRenderData(UStaticMesh* StaticMesh) const
{
  if (!StaticMesh || !StaticMesh->RenderData || StaticMesh->RenderData->LODResources.Num() < 1)
  {
    return false;
  }

  // 1. 基础设置
  StaticMesh->NeverStream = true; 
  StaticMesh->bIgnoreStreamingMipBias = true;
  StaticMesh->LightMapCoordinateIndex = 0;

  // 2. 预先修正 ScreenSize (Runtime 下 InitResources 会读取它)
  StaticMesh->RenderData->ScreenSize[0].Default = 0.0f;
  StaticMesh->RenderData->ScreenSize[1].Default = 0.0f;

  // 3. 预先修正包围盒 (Runtime 下 InitResources 会读取它)
  StaticMesh->CalculateExtendedBounds();
  if (StaticMesh->ExtendedBounds.SphereRadius < 10.0f)
  {
    StaticMesh->ExtendedBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(500.0f), 1000.0f);
    if (StaticMesh->RenderData) StaticMesh->RenderData->Bounds = StaticMesh->ExtendedBounds;
  }

  // 4. 【核心修复】在 InitResources 之前注入 UV 密度
  // 这一步至关重要，必须在 LinkStreaming 发生前完成
  const float ForcedUVDensity = 1024.0f;
  if (StaticMesh->StaticMaterials.Num() > 0)
  {
    for (FStaticMaterial& Mat : StaticMesh->StaticMaterials)
    {
      // 覆盖所有通道，确保万无一失
      Mat.UVChannelData.LocalUVDensities[0] = ForcedUVDensity;
      Mat.UVChannelData.LocalUVDensities[1] = ForcedUVDensity;
      Mat.UVChannelData.LocalUVDensities[2] = ForcedUVDensity;
      Mat.UVChannelData.LocalUVDensities[3] = ForcedUVDensity;
    }
  }

  // 标记颜色数据
  FStaticMeshLODResources& LODResources = StaticMesh->RenderData->LODResources[0];
  LODResources.bHasColorVertexData = true;

  // 5. 启动资源初始化 (此时它会读取我们在上面准备好的正确数据)
  StaticMesh->InitResources();

  // 6. 双重保险
  StaticMesh->bForceMiplevelsToBeResident = true;
  StaticMesh->SetForceMipLevelsToBeResident(30.0f, 0);

  return true;
}

// 辅助函数：设置 BodySetup 的基本属性
void AProceduralMeshActor::SetupBodySetupProperties(UBodySetup* BodySetup) const
{
  if (!BodySetup) {
    return;
  }
  
  BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
  
  // 【关键修复】关闭双面碰撞，防止角色在落地瞬间被"夹"在网格内部导致卡死
  BodySetup->bDoubleSidedGeometry = false;
  
  // bMeshCollideAll = true 与默认值false不同，需要保留
  BodySetup->bMeshCollideAll = true;
  
  // 以下属性与默认值相同，已删除：
  // BodySetup->bSupportUVsAndFaceRemap = false;  // 默认值
  // BodySetup->bConsiderForBounds = true;  // 默认值
  // BodySetup->BuildScale3D = FVector(1.0f, 1.0f, 1.0f);  // 默认值
  
  if (ProceduralMeshComponent && ProceduralMeshComponent->ProcMeshBodySetup && ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial)
  {
    BodySetup->PhysMaterial = ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial;
    UE_LOG(LogTemp, Log, TEXT("已复制物理材质: %s"), *BodySetup->PhysMaterial->GetName());
  }
  
  BodySetup->DefaultInstance.SetCollisionProfileName(TEXT("BlockAll"));
  BodySetup->DefaultInstance.SetEnableGravity(false);
  BodySetup->DefaultInstance.bUseCCD = false;
}

// 辅助函数：生成简单碰撞（ConvexElems 或 BoxElems）
bool AProceduralMeshActor::GenerateSimpleCollision(UBodySetup* BodySetup, UStaticMesh* StaticMesh) const
{
  if (!BodySetup || !ProceduralMeshComponent) {
    return false;
  }

  // 方式1：使用QuickHull凸包分解算法生成多个高精度凸包
  int32 HullCount = 8;
  int32 MaxHullVerts = 16;
  uint32 HullPrecision = 100000;
  
  bool bSuccess = FModelGenConvexDecomp::GenerateConvexHulls(
    ProceduralMeshComponent,
    BodySetup,
    HullCount,
    MaxHullVerts,
    HullPrecision);
  
  if (bSuccess && BodySetup->AggGeom.ConvexElems.Num() > 0)
  {
    int32 ConvexCount = BodySetup->AggGeom.ConvexElems.Num();
    UE_LOG(LogTemp, Log, TEXT("✓ QuickHull凸包分解成功！生成了 %d 个凸包元素"), ConvexCount);
    return true;
  }
  else
  {
    // 方式2：如果QuickHull失败，使用RenderData.Bounds生成简单的包围盒作为兜底方案
    UE_LOG(LogTemp, Warning, TEXT("QuickHull凸包分解失败，使用RenderData.Bounds生成包围盒作为兜底方案"));
    
    if (StaticMesh && StaticMesh->RenderData.IsValid())
    {
      const FBoxSphereBounds& Bounds = StaticMesh->RenderData->Bounds;
      if (Bounds.BoxExtent.X > 0.0f && Bounds.BoxExtent.Y > 0.0f && Bounds.BoxExtent.Z > 0.0f)
      {
        FKBoxElem BoxElem;
        BoxElem.Center = Bounds.Origin;
        BoxElem.X = Bounds.BoxExtent.X * 2.0f;
        BoxElem.Y = Bounds.BoxExtent.Y * 2.0f;
        BoxElem.Z = Bounds.BoxExtent.Z * 2.0f;
        BodySetup->AggGeom.BoxElems.Add(BoxElem);
        UE_LOG(LogTemp, Log, TEXT("✓ 使用包围盒作为简单碰撞"));
        return true;
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
  
  return false;
}

// 辅助函数：手动创建所有 ConvexMesh
int32 AProceduralMeshActor::CreateConvexMeshesManually(UBodySetup* BodySetup, IPhysXCookingModule* PhysXCookingModule) const
{
  if (!BodySetup || !PhysXCookingModule || !PhysXCookingModule->GetPhysXCooking()) {
    return 0;
  }

  if (BodySetup->AggGeom.GetElementCount() == 0) {
    return 0;
  }

  // 确保所有 ConvexElem 都调用了 UpdateElemBox() 并设置了有效的 Transform
  int32 ValidConvexElemCount = 0;
  for (FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
  {
    if (ConvexElem.VertexData.Num() < 4)
    {
      UE_LOG(LogTemp, Error, TEXT("ConvexElem 顶点数不足: %d < 4，跳过"), ConvexElem.VertexData.Num());
      continue;
    }
    
    // 验证顶点数据有效性（检查 NaN 和无穷大）
    bool bHasValidVertices = true;
    for (const FVector& Vert : ConvexElem.VertexData)
    {
      // UE 4.26 兼容：使用 FMath::IsFinite 检查每个分量
      if (Vert.ContainsNaN() || 
          !FMath::IsFinite(Vert.X) || 
          !FMath::IsFinite(Vert.Y) || 
          !FMath::IsFinite(Vert.Z))
      {
        UE_LOG(LogTemp, Error, TEXT("ConvexElem 包含无效顶点: %s（NaN 或无穷大）"), *Vert.ToString());
        bHasValidVertices = false;
        break;
      }
    }
    
    if (!bHasValidVertices)
    {
      UE_LOG(LogTemp, Error, TEXT("ConvexElem 顶点数据无效，跳过"));
      continue;
    }
    
    ConvexElem.UpdateElemBox();
    // 确保 Transform 有效（GetCookInfo 需要有效的 Transform）
    if (!ConvexElem.GetTransform().IsValid())
    {
      ConvexElem.SetTransform(FTransform::Identity);
    }
    // 关键：将 Transform 应用到顶点数据中（参考 MWGenerateActorGroup.cpp:444）
    // 这必须在 CreatePhysicsMeshes 之前调用，否则 PhysX 无法正确创建 ConvexMesh
    ConvexElem.BakeTransformToVerts();
    ValidConvexElemCount++;
  }
  
  // 确保 bNeverNeedsCookedCollisionData = false（允许运行时烹饪）
  BodySetup->bNeverNeedsCookedCollisionData = false;
  
  // 清除旧的 cooked data，强制重新生成
  BodySetup->InvalidatePhysicsData();
  
  // 检查 RuntimePhysXCooking 模块是否已加载
  FModuleManager& ModuleManager = FModuleManager::Get();
  if (!ModuleManager.IsModuleLoaded("RuntimePhysXCooking"))
  {
    UE_LOG(LogTemp, Warning, TEXT("RuntimePhysXCooking 模块未加载，可能导致 ConvexMesh 无法在运行时创建"));
  }
  
  int32 ValidConvexMeshCount = 0;
  
  // 遍历所有 ConvexElems，手动创建 ConvexMesh
  for (int32 ElemIdx = 0; ElemIdx < BodySetup->AggGeom.ConvexElems.Num(); ++ElemIdx)
  {
    FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems[ElemIdx];
    
    // 确保顶点数据有效
    if (ConvexElem.VertexData.Num() < 4)
    {
      UE_LOG(LogTemp, Error, TEXT("ConvexElem[%d] 顶点数不足 (%d < 4)，跳过"), ElemIdx, ConvexElem.VertexData.Num());
      continue;
    }
    
    // 验证顶点数据有效性
    bool bHasValidVertices = true;
    for (const FVector& Vert : ConvexElem.VertexData)
    {
      if (Vert.ContainsNaN() || 
          !FMath::IsFinite(Vert.X) || 
          !FMath::IsFinite(Vert.Y) || 
          !FMath::IsFinite(Vert.Z))
      {
        UE_LOG(LogTemp, Error, TEXT("ConvexElem[%d] 包含无效顶点: %s（NaN 或无穷大）"), ElemIdx, *Vert.ToString());
        bHasValidVertices = false;
        break;
      }
    }
    
    if (!bHasValidVertices)
    {
      UE_LOG(LogTemp, Error, TEXT("ConvexElem[%d] 顶点数据无效，跳过"), ElemIdx);
      continue;
    }
    
    // 确保 Transform 和 ElemBox 正确
    ConvexElem.UpdateElemBox();
    if (!ConvexElem.GetTransform().IsValid())
    {
      ConvexElem.SetTransform(FTransform::Identity);
    }
    
    // 如果 Transform 不是 Identity，需要 BakeTransformToVerts
    if (!ConvexElem.GetTransform().Equals(FTransform::Identity))
    {
      ConvexElem.BakeTransformToVerts();
    }
    
    // 手动调用 CreateConvex
    physx::PxConvexMesh* NewConvexMesh = nullptr;
    EPhysXMeshCookFlags CookFlags = EPhysXMeshCookFlags::Default;
    
    EPhysXCookingResult Result = PhysXCookingModule->GetPhysXCooking()->CreateConvex(
      FName(FPlatformProperties::GetPhysicsFormat()),
      CookFlags,
      ConvexElem.VertexData,
      NewConvexMesh
    );
    
    switch (Result)
    {
    case EPhysXCookingResult::Succeeded:
      ConvexElem.SetConvexMesh(NewConvexMesh);
      ValidConvexMeshCount++;
      break;
    case EPhysXCookingResult::SucceededWithInflation:
      ConvexElem.SetConvexMesh(NewConvexMesh);
      ValidConvexMeshCount++;
      break;
    case EPhysXCookingResult::Failed:
      UE_LOG(LogTemp, Error, TEXT("✗ ConvexElem[%d] 手动创建 ConvexMesh 失败: Result=Failed"), ElemIdx);
      UE_LOG(LogTemp, Error, TEXT("  顶点数: %d"), ConvexElem.VertexData.Num());
      if (ConvexElem.VertexData.Num() > 0)
      {
        UE_LOG(LogTemp, Error, TEXT("  顶点范围: 最小=(%.2f, %.2f, %.2f), 最大=(%.2f, %.2f, %.2f)"),
          ConvexElem.ElemBox.Min.X, ConvexElem.ElemBox.Min.Y, ConvexElem.ElemBox.Min.Z,
          ConvexElem.ElemBox.Max.X, ConvexElem.ElemBox.Max.Y, ConvexElem.ElemBox.Max.Z);
      }
      break;
    default:
      UE_LOG(LogTemp, Error, TEXT("✗ ConvexElem[%d] 手动创建 ConvexMesh 失败: Result=%d"), ElemIdx, (int32)Result);
      break;
    }
  }
  
  // 设置标志
  BodySetup->bCreatedPhysicsMeshes = true;
  
  // 最终统计
  if (ValidConvexMeshCount == BodySetup->AggGeom.ConvexElems.Num())
  {
    UE_LOG(LogTemp, Log, TEXT("✓ 成功生成物理网格 - 所有 %d 个 ConvexMesh 已创建"), ValidConvexMeshCount);
          }
          else
          {
    UE_LOG(LogTemp, Warning, TEXT("⚠ 物理网格生成不完整 - 成功: %d/%d"), 
      ValidConvexMeshCount, BodySetup->AggGeom.ConvexElems.Num());
    if (ValidConvexMeshCount == 0)
    {
      UE_LOG(LogTemp, Error, TEXT("✗ 所有 ConvexMesh 创建失败！请检查："));
      UE_LOG(LogTemp, Error, TEXT("  1. RuntimePhysXCooking 插件是否已启用"));
      UE_LOG(LogTemp, Error, TEXT("  2. 顶点数据是否有效（无 NaN/Inf）"));
      UE_LOG(LogTemp, Error, TEXT("  3. 顶点数是否 >= 4"));
      UE_LOG(LogTemp, Error, TEXT("  4. PhysX SDK 是否正确初始化"));
    }
  }
  
  UE_LOG(LogTemp, Log, TEXT("=== 手动创建完成 ==="));
  
  return ValidConvexMeshCount;
}

// 辅助函数：从 ProceduralMeshComponent 提取 TriMesh 数据
bool AProceduralMeshActor::ExtractTriMeshDataFromPMC(TArray<FVector>& OutVertices, TArray<FTriIndices>& OutIndices) const
{
  if (!ProceduralMeshComponent) {
    return false;
  }

  OutVertices.Empty();
  OutIndices.Empty();
  
  int32 TotalVertices = 0;
  int32 TotalSkippedTriangles = 0;
  
  for (int32 SectionIdx = 0; SectionIdx < ProceduralMeshComponent->GetNumSections(); ++SectionIdx)
  {
    FProcMeshSection* SectionData = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
    if (!SectionData) continue;
    
    const int32 SectionVertexOffset = TotalVertices;
    const int32 SectionVertexCount = SectionData->ProcVertexBuffer.Num();
    
    // 提取顶点
    for (const FProcMeshVertex& ProcVertex : SectionData->ProcVertexBuffer)
    {
      OutVertices.Add(ProcVertex.Position);
    }
    
    // 提取索引（需要调整偏移）
    const int32 NumSectionIndices = SectionData->ProcIndexBuffer.Num();
    if (NumSectionIndices % 3 == 0 && SectionVertexCount > 0)
    {
      for (int32 Idx = 0; Idx < NumSectionIndices - 2; Idx += 3)
      {
        // 边界检查：确保索引访问安全
        if (Idx < 0 || Idx + 2 >= NumSectionIndices)
        {
          UE_LOG(LogTemp, Warning, TEXT("Section[%d] 索引边界检查失败: Idx=%d, NumSectionIndices=%d"), 
            SectionIdx, Idx, NumSectionIndices);
          break;
        }
        
        // 安全获取索引值
        uint32 LocalV0 = SectionData->ProcIndexBuffer[Idx];
        uint32 LocalV1 = SectionData->ProcIndexBuffer[Idx + 1];
        uint32 LocalV2 = SectionData->ProcIndexBuffer[Idx + 2];
        
        // 验证原始索引是否在当前Section的有效范围内
        if (LocalV0 >= static_cast<uint32>(SectionVertexCount) || 
            LocalV1 >= static_cast<uint32>(SectionVertexCount) || 
            LocalV2 >= static_cast<uint32>(SectionVertexCount))
        {
          UE_LOG(LogTemp, Warning, TEXT("Section[%d] 跳过无效三角形索引: [%d, %d, %d] (Section顶点数=%d)"), 
            SectionIdx, LocalV0, LocalV1, LocalV2, SectionVertexCount);
          TotalSkippedTriangles++;
          continue;
        }
        
        // 计算全局索引（加上偏移）
        const uint32 V0 = SectionVertexOffset + LocalV0;
        const uint32 V1 = SectionVertexOffset + LocalV1;
        const uint32 V2 = SectionVertexOffset + LocalV2;
        
        // 验证全局索引
        if (V0 < static_cast<uint32>(OutVertices.Num()) && 
            V1 < static_cast<uint32>(OutVertices.Num()) && 
            V2 < static_cast<uint32>(OutVertices.Num()))
        {
          FTriIndices Tri;
          Tri.v0 = V0;
          Tri.v1 = V1;
          Tri.v2 = V2;
          OutIndices.Add(Tri);
          }
          else
          {
          UE_LOG(LogTemp, Warning, TEXT("Section[%d] 全局索引验证失败: [%d, %d, %d] (总顶点数=%d)"), 
            SectionIdx, V0, V1, V2, OutVertices.Num());
          TotalSkippedTriangles++;
        }
      }
    }
    else if (NumSectionIndices > 0)
    {
      UE_LOG(LogTemp, Warning, TEXT("Section[%d] 索引数=%d（不是3的倍数），跳过"), SectionIdx, NumSectionIndices);
    }
    
    TotalVertices = OutVertices.Num();
  }
  
  if (TotalSkippedTriangles > 0)
  {
    UE_LOG(LogTemp, Warning, TEXT("从 ProceduralMeshComponent 提取数据时跳过了 %d 个无效三角形"), TotalSkippedTriangles);
  }
  
  if (OutVertices.Num() > 0 && OutIndices.Num() > 0)
  {
    return true;
  }
  
  return false;
}

// 辅助函数：从 RenderData 提取 TriMesh 数据
bool AProceduralMeshActor::ExtractTriMeshDataFromRenderData(UStaticMesh* StaticMesh, TArray<FVector>& OutVertices, TArray<FTriIndices>& OutIndices) const
{
  if (!StaticMesh || !StaticMesh->RenderData || StaticMesh->RenderData->LODResources.Num() == 0) {
    return false;
  }

  OutVertices.Empty();
  OutIndices.Empty();
  
  const FStaticMeshLODResources& LODResource = StaticMesh->RenderData->LODResources[0];
  const FPositionVertexBuffer& PositionVertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
  const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;

  // 提取顶点
  const int32 NumVertices = PositionVertexBuffer.GetNumVertices();
  if (NumVertices > 0)
  {
    OutVertices.Reserve(NumVertices);
    for (int32 VertIdx = 0; VertIdx < NumVertices; ++VertIdx)
    {
      // 边界检查：确保顶点访问安全
      if (VertIdx >= 0 && VertIdx < NumVertices)
      {
        OutVertices.Add(PositionVertexBuffer.VertexPosition(VertIdx));
      }
      else
      {
        UE_LOG(LogTemp, Warning, TEXT("顶点索引越界: VertIdx=%d, NumVertices=%d"), VertIdx, NumVertices);
        break;
      }
    }

    // 提取索引并转换为 FTriIndices 格式
    const int32 NumIndices = IndexBuffer.GetNumIndices();
    
    if (NumIndices > 0 && NumIndices % 3 == 0)
    {
      OutIndices.Reserve(NumIndices / 3);
      
      int32 SkippedTriangles = 0;
      for (int32 Idx = 0; Idx < NumIndices - 2; Idx += 3)
      {
        // 边界检查：确保索引访问安全
        if (Idx < 0 || Idx + 2 >= NumIndices)
        {
          UE_LOG(LogTemp, Warning, TEXT("索引边界检查失败: Idx=%d, NumIndices=%d"), Idx, NumIndices);
          break;
        }
        
        // 安全获取索引值
        uint32 V0 = IndexBuffer.GetIndex(Idx);
        uint32 V1 = IndexBuffer.GetIndex(Idx + 1);
        uint32 V2 = IndexBuffer.GetIndex(Idx + 2);
        
        // 验证索引值是否在有效范围内
        if (V0 >= static_cast<uint32>(NumVertices) || 
            V1 >= static_cast<uint32>(NumVertices) || 
            V2 >= static_cast<uint32>(NumVertices))
        {
          UE_LOG(LogTemp, Warning, TEXT("跳过无效三角形索引: [%d, %d, %d] (顶点数=%d)"), 
            V0, V1, V2, NumVertices);
          SkippedTriangles++;
          continue;
        }
        
        FTriIndices Tri;
        Tri.v0 = V0;
        Tri.v1 = V1;
        Tri.v2 = V2;
        OutIndices.Add(Tri);
      }
      
      if (SkippedTriangles > 0)
      {
        UE_LOG(LogTemp, Warning, TEXT("跳过了 %d 个无效三角形索引"), SkippedTriangles);
      }
      
      if (OutIndices.Num() > 0)
      {
        return true;
      }
    }
  }
  
  return false;
}

// 辅助函数：生成复杂碰撞（TriMeshes）
bool AProceduralMeshActor::GenerateComplexCollision(UBodySetup* BodySetup, IPhysXCookingModule* PhysXCookingModule) const
{
  if (!BodySetup) {
    return false;
  }

  // 如果已有自动生成的 TriMeshes，清空后重新生成
  if (BodySetup->TriMeshes.Num() > 0)
  {
    BodySetup->TriMeshes.Empty();
  }
  
  TArray<FVector> NewVertices;
  TArray<FTriIndices> NewIndices;
  bool bDataFromPMC = false;
  
  // 优先从 ProceduralMeshComponent 提取数据（方案2-优先）
  if (ExtractTriMeshDataFromPMC(NewVertices, NewIndices))
  {
    bDataFromPMC = true;
  }
  
  // 如果从 ProceduralMeshComponent 提取失败，从 RenderData 提取数据（方案2-备用）
  // 注意：需要 StaticMesh 来访问 RenderData，这里需要从外部传入
  // 但为了保持函数签名简洁，我们暂时跳过这个备用方案
  
  // 确保有数据，然后生成 TriMeshes
  if (NewVertices.Num() > 0 && NewIndices.Num() > 0)
  {
    // 使用之前获取的 PhysXCookingModule（如果未获取则重新获取）
    if (!PhysXCookingModule)
    {
      PhysXCookingModule = GetPhysXCookingModule();
    }
    
    if (PhysXCookingModule && PhysXCookingModule->GetPhysXCooking())
    {
      // 创建 TriMeshes
      BodySetup->TriMeshes.AddZeroed();

      // 设置 RuntimeCookFlags
      EPhysXMeshCookFlags RuntimeCookFlags = EPhysXMeshCookFlags::Default;
      if (UPhysicsSettings::Get()->bSuppressFaceRemapTable)
      {
        RuntimeCookFlags |= EPhysXMeshCookFlags::SuppressFaceRemapTable;
      }

      // 创建 MaterialIndices
      TArray<uint16> MaterialIndices;
      MaterialIndices.AddZeroed(NewIndices.Num());

      // 调用 CreateTriMesh（参考 InitStaticMeshInfo）
      // 【关键修复】：如果法线指向内部，需要反转法线让碰撞在外部
      // FlipNormals = true 会反转法线方向，使碰撞在外部
      const bool bError = !PhysXCookingModule->GetPhysXCooking()->CreateTriMesh(
        FName(FPlatformProperties::GetPhysicsFormat()),
        RuntimeCookFlags,
        NewVertices,
        NewIndices,
        MaterialIndices,
        true,  // FlipNormals = true：反转法线，让碰撞在外部
        BodySetup->TriMeshes[0]);

      if (!bError)
      {
        BodySetup->bCreatedPhysicsMeshes = true;
        return true;
      }
      else
      {
        UE_LOG(LogTemp, Warning, TEXT("CreateTriMesh 失败，无法生成复杂碰撞"));
        BodySetup->TriMeshes.Empty();
      }
    }
    else
    {
      UE_LOG(LogTemp, Error, TEXT("无法获取 PhysXCookingModule，无法生成 TriMeshes"));
    }
  }
  else
  {
    UE_LOG(LogTemp, Error, TEXT("无法提取有效的顶点或索引数据，无法生成 TriMeshes"));
  }
  
  return false;
}

// 辅助函数：记录碰撞统计信息
void AProceduralMeshActor::LogCollisionStatistics(UBodySetup* BodySetup, UStaticMesh* StaticMesh) const
{
  if (!BodySetup) {
        return;
    }

  const int32 AggGeomElementCount = BodySetup->AggGeom.GetElementCount();
  const int32 TriMeshesCount = BodySetup->TriMeshes.Num();
  
  if (AggGeomElementCount > 0 || TriMeshesCount > 0)
  {
    UE_LOG(LogTemp, Log, TEXT("碰撞生成完成 - 简单碰撞: %d 个元素, 复杂碰撞: %d 个 TriMesh"), 
      AggGeomElementCount, TriMeshesCount);
            }
            else
            {
    UE_LOG(LogTemp, Warning, TEXT("碰撞生成失败 - 所有方案均未成功"));
  }
}

// 辅助函数：创建 BodySetup 并生成碰撞（简单碰撞和复杂碰撞）
void AProceduralMeshActor::SetupBodySetupAndCollision(UStaticMesh* StaticMesh) const
{
  if (!StaticMesh) {
    return;
  }

  StaticMesh->CreateBodySetup();
  UBodySetup* NewBodySetup = StaticMesh->BodySetup;
  if (!NewBodySetup)
  {
    UE_LOG(LogTemp, Error, TEXT("无法获取StaticMesh的BodySetup"));
    return;
  }

  // 7.1 生成简单碰撞（ConvexElems 或 BoxElems）
  GenerateSimpleCollision(NewBodySetup, StaticMesh);
  
  int32 FinalElementCount = NewBodySetup->AggGeom.GetElementCount();
  if (FinalElementCount == 0)
  {
    UE_LOG(LogTemp, Error, TEXT("所有碰撞生成方式均失败，无法生成碰撞数据"));
  }

  // 7.2 设置 BodySetup 的基本属性
  SetupBodySetupProperties(NewBodySetup);

  // 7.3 手动创建所有 ConvexMesh
  IPhysXCookingModule* PhysXCookingModule = GetPhysXCookingModule();
  if (NewBodySetup->AggGeom.GetElementCount() > 0)
  {
    CreateConvexMeshesManually(NewBodySetup, PhysXCookingModule);
  }
  
  // 7.4 生成复杂碰撞（TriMeshes）
  // 如果从 PMC 提取失败，尝试从 RenderData 提取
  TArray<FVector> NewVertices;
  TArray<FTriIndices> NewIndices;
  bool bDataFromPMC = ExtractTriMeshDataFromPMC(NewVertices, NewIndices);
  if (!bDataFromPMC)
  {
    ExtractTriMeshDataFromRenderData(StaticMesh, NewVertices, NewIndices);
  }
  
  // 如果从 RenderData 提取成功，使用这些数据生成 TriMeshes
  if (NewVertices.Num() > 0 && NewIndices.Num() > 0)
  {
    // 使用之前获取的 PhysXCookingModule（如果未获取则重新获取）
    if (!PhysXCookingModule)
    {
      PhysXCookingModule = GetPhysXCookingModule();
    }
    
    if (PhysXCookingModule && PhysXCookingModule->GetPhysXCooking())
    {
      // 创建 TriMeshes
      NewBodySetup->TriMeshes.AddZeroed();

      // 设置 RuntimeCookFlags
      EPhysXMeshCookFlags RuntimeCookFlags = EPhysXMeshCookFlags::Default;
      if (UPhysicsSettings::Get()->bSuppressFaceRemapTable)
      {
        RuntimeCookFlags |= EPhysXMeshCookFlags::SuppressFaceRemapTable;
      }

      // 创建 MaterialIndices
      TArray<uint16> MaterialIndices;
      MaterialIndices.AddZeroed(NewIndices.Num());

      // 调用 CreateTriMesh（参考 InitStaticMeshInfo）
      // 【关键修复】：如果法线指向内部，需要反转法线让碰撞在外部
      // FlipNormals = true 会反转法线方向，使碰撞在外部
      const bool bError = !PhysXCookingModule->GetPhysXCooking()->CreateTriMesh(
        FName(FPlatformProperties::GetPhysicsFormat()),
        RuntimeCookFlags,
        NewVertices,
        NewIndices,
        MaterialIndices,
        true,  // FlipNormals = true：反转法线，让碰撞在外部
        NewBodySetup->TriMeshes[0]);

      if (!bError)
      {
        NewBodySetup->bCreatedPhysicsMeshes = true;
        }
        else
        {
        UE_LOG(LogTemp, Warning, TEXT("CreateTriMesh 失败，无法生成复杂碰撞"));
        NewBodySetup->TriMeshes.Empty();
        }
    }
    else
    {
      UE_LOG(LogTemp, Error, TEXT("无法获取 PhysXCookingModule，无法生成 TriMeshes"));
    }
}
  
  // 7.5 记录碰撞统计信息
  LogCollisionStatistics(NewBodySetup, StaticMesh);
}
