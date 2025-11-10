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

  // 2. 创建一个临时的、只存在于内存中的UStaticMesh对象
  UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
      GetTransientPackage(), NAME_None, RF_Public | RF_Standalone);

  if (!StaticMesh) {
    return nullptr;
  }

  // 设置StaticMesh的基本属性，确保资源完整
  StaticMesh->SetFlags(RF_Public | RF_Standalone);
  StaticMesh->NeverStream = false;  // 允许流式加载（如果需要）

  // ===== StaticMesh 基础属性设置 =====
  // 光照贴图设置
  StaticMesh->LightMapResolution = 64;  // 默认光照贴图分辨率
  StaticMesh->LightMapCoordinateIndex = 1;  // 默认使用UV通道1作为光照贴图
  StaticMesh->LightmapUVDensity = 0.0f;  // 初始化为0，会在BuildFromMeshDescriptions时计算
  
  // LOD和碰撞设置
  StaticMesh->LODForCollision = 0;  // 使用LOD0进行碰撞
  
  // 运行时创建的特殊设置
  StaticMesh->bAllowCPUAccess = true;  // 允许CPU访问几何数据（运行时创建需要）
  StaticMesh->bIsBuiltAtRuntime = true;  // 标记为运行时构建
  
  // 高级功能设置（默认禁用以节省内存）
  StaticMesh->bGenerateMeshDistanceField = false;  // 不生成距离场
  StaticMesh->bHasNavigationData = false;  // 默认不生成导航数据
  StaticMesh->bSupportPhysicalMaterialMasks = false;  // 不使用物理材质遮罩
  StaticMesh->bSupportUniformlyDistributedSampling = false;  // 不使用均匀采样
  
  // LPV（Light Propagation Volume）设置
  StaticMesh->LpvBiasMultiplier = 1.0f;  // 默认LPV偏移乘数

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

  for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx) {
    FProcMeshSection* SectionData =
        ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
    if (!SectionData || SectionData->ProcVertexBuffer.Num() < 3 ||
        SectionData->ProcIndexBuffer.Num() < 3) {
      continue;
    }

    // --- 开始材质设置修正 ---
    // a. 为每个材质段创建一个唯一的槽名称
    const FName MaterialSlotName =
        FName(*FString::Printf(TEXT("MaterialSlot_%d"), SectionIdx));
    
       // b. 将材质和槽名称添加到StaticMesh的材质列表中
       // 这会在最终的UStaticMesh上创建对应的材质槽
		FStaticMaterial NewStaticMaterial(nullptr, MaterialSlotName);

       // 初始化UV通道数据，避免ensure失败
       NewStaticMaterial.UVChannelData.bInitialized = true;
       NewStaticMaterial.UVChannelData.bOverrideDensities = false;
       
       // 计算UV密度（基于实际的UV数据）
       // 分析当前section的UV数据来计算合适的密度值
       float UVDensity = 1.0f; // 默认密度
       if (SectionData->ProcVertexBuffer.Num() > 0)
       {
           // 计算UV坐标的范围
           FVector2D MinUV(FLT_MAX, FLT_MAX);
           FVector2D MaxUV(-FLT_MAX, -FLT_MAX);
           
           for (const FProcMeshVertex& Vertex : SectionData->ProcVertexBuffer)
           {
               MinUV.X = FMath::Min(MinUV.X, Vertex.UV0.X);
               MinUV.Y = FMath::Min(MinUV.Y, Vertex.UV0.Y);
               MaxUV.X = FMath::Max(MaxUV.X, Vertex.UV0.X);
               MaxUV.Y = FMath::Max(MaxUV.Y, Vertex.UV0.Y);
           }
           
           // 计算UV范围
           FVector2D UVRange = MaxUV - MinUV;
           if (UVRange.X > 0.0f && UVRange.Y > 0.0f)
           {
               // 基于UV范围计算密度，范围越大密度越小
               UVDensity = FMath::Clamp(1.0f / FMath::Max(UVRange.X, UVRange.Y), 0.1f, 10.0f);
           }
       }
       
       // 设置UV通道密度
       for (int32 UVIndex = 0; UVIndex < 4; ++UVIndex)
       {
           NewStaticMaterial.UVChannelData.LocalUVDensities[UVIndex] = UVDensity;
       }
       
       StaticMesh->StaticMaterials.Add(NewStaticMaterial);
    
    // c. 在MeshDescription中为这个材质段创建一个新的多边形组
    const FPolygonGroupID PolygonGroup = MeshDescBuilder.AppendPolygonGroup();

    // d. 将多边形组与我们刚刚创建的材质槽名称关联起来
    // 这是最关键的一步！
    PolygonGroupMaterialSlotNames.Set(PolygonGroup, MaterialSlotName);
    // --- 结束材质设置修正 ---

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

      // 将三角形添加到指定了材质信息的PolygonGroup中
      MeshDescBuilder.AppendTriangle(V1, V2, V3, PolygonGroup);
    }
  }

  if (MeshDescription.Vertices().Num() == 0) {
    return nullptr;
  }

  // ===== 输出ProceduralMeshComponent的详细数据 =====
  {
    UE_LOG(LogTemp, Log, TEXT("========== ProceduralMeshComponent 详细数据 =========="));
    
    if (!ProceduralMeshComponent)
    {
      UE_LOG(LogTemp, Warning, TEXT("ProceduralMeshComponent 为 nullptr"));
    }
    else
    {
      // 基本信息
      int32 TotalSections = ProceduralMeshComponent->GetNumSections();
      UE_LOG(LogTemp, Log, TEXT("网格段数量: %d"), TotalSections);
      UE_LOG(LogTemp, Log, TEXT("可见性: %s"), ProceduralMeshComponent->IsVisible() ? TEXT("可见") : TEXT("隐藏"));
      UE_LOG(LogTemp, Log, TEXT("碰撞启用: %s"), 
        ProceduralMeshComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision ? TEXT("是") : TEXT("否"));
      
      // 每个段的详细信息
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
          
          // 计算该段的包围盒
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
      
      // 碰撞数据
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
        
        // 输出每个碰撞元素的详细信息
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

  // 7. 使用 FMeshDescription 构建 Static Mesh
  TArray<const FMeshDescription*> MeshDescPtrs;
  MeshDescPtrs.Emplace(&MeshDescription);
  UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
  
  // 根据设置决定碰撞类型
  // 策略：先让BuildFromMeshDescriptions自动生成碰撞作为后备，然后尝试手动设置更精确的碰撞
  // 如果手动设置成功，就使用手动设置的；如果失败，就使用自动生成的（回退机制）
  bool bManuallySetCollision = false;
  
  // 使用简单碰撞：先启用自动生成作为后备
  // 然后尝试手动设置更精确的碰撞，如果失败就使用自动生成的
  BuildParams.bBuildSimpleCollision = true;
  bManuallySetCollision = true;  // 标记我们将尝试手动设置
  UE_LOG(LogTemp, Log, TEXT("使用简单碰撞（先自动生成作为后备，然后尝试手动设置更精确的碰撞）"));
  
  StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);

  // 在BuildFromMeshDescriptions之后，从ProceduralMeshComponent复制材质到StaticMesh
  // 材质槽已经在BuildFromMeshDescriptions之前添加，现在只需要更新材质引用
  {
    TArray<FStaticMaterial>& StaticMaterials = StaticMesh->StaticMaterials;
    
    // 更新每个材质槽的材质引用
    for (int32 SectionIdx = 0; SectionIdx < NumSections && SectionIdx < StaticMaterials.Num(); ++SectionIdx)
    {
      // 从ProceduralMeshComponent获取材质
      UMaterialInterface* SectionMaterial = ProceduralMeshComponent->GetMaterial(SectionIdx);
      if (SectionMaterial)
      {
        StaticMaterials[SectionIdx].MaterialInterface = SectionMaterial;
      }
    }
    
    // 标记StaticMesh已修改，确保材质更新生效
    StaticMesh->MarkPackageDirty();
  }

  // 处理碰撞 - StaticMesh默认开启碰撞
  {
    StaticMesh->CreateBodySetup();
    UBodySetup* NewBodySetup = StaticMesh->BodySetup;
    if (NewBodySetup) {
      // ========== 核心参数：CollisionTraceFlag（碰撞追踪标志） ==========
      // 这是决定使用哪种碰撞几何体的最重要参数
      // 我们使用简单碰撞（CTF_UseSimpleAsComplex）
      
      // 【核心参数】CTF_UseSimpleAsComplex（使用简单碰撞）
      // 类型：ECollisionTraceFlag
      // 值：2
      // 作用：使用简化的碰撞几何体（凸包、包围盒等）
      // 详细说明：
      //   - 使用简化的碰撞几何体（Box、Sphere、Convex、Capsule等）
      //   - 通过 BuildFromMeshDescriptions 自动生成简化碰撞
      //   - AggGeom 包含简化几何体元素
      // 优点：
      //   ✅ 性能开销小（内存和CPU）
      //   ✅ 适合大量物体和动态物体
      //   ✅ 内存占用小
      // 缺点：
      //   ❌ 碰撞精度较低
      //   ❌ 可能与渲染网格不完全一致
      // 适用场景：
      //   - 动态物体
      //   - 性能要求高的场景
      //   - 简单形状的网格
      //   - 大量物体的场景
      UE_LOG(LogTemp, Log, TEXT("=== 使用简单碰撞 ==="));

      // ===== 优先从ProceduralMeshComponent获取碰撞数据 =====
      // 策略：先保存自动生成的碰撞作为后备，然后尝试手动设置更精确的碰撞
      // 如果手动设置成功，就替换；如果失败，就恢复自动生成的
      
      bool bCopiedCollisionData = false;
      FKAggregateGeom BackupAggGeom;  // 保存自动生成的碰撞作为后备
      int32 CollisionSourceType = 0; // 0=未设置, 1=从PMC复制, 2=基于顶点计算, 3=自动生成, 4=基于RenderData.Bounds
      
      if (bManuallySetCollision)
      {
        // 保存BuildFromMeshDescriptions自动生成的碰撞数据作为后备
        BackupAggGeom = NewBodySetup->AggGeom;
        int32 AutoGenCount = BackupAggGeom.GetElementCount();
        
        if (AutoGenCount > 0)
        {
          UE_LOG(LogTemp, Log, TEXT("已保存自动生成的碰撞数据作为后备（%d个元素），准备尝试手动设置更精确的碰撞"), 
            AutoGenCount);
        }
        
        // 清空AggGeom，准备手动设置
        NewBodySetup->AggGeom.EmptyElements();
      }
      
      // ===== 方式1：优先从ProceduralMeshComponent复制现有碰撞数据（最优） =====
      // 优点：支持多种几何体类型、通常是预设优化的、速度快、精度高
      // 适用：PMC已有碰撞数据时
      if (ProceduralMeshComponent->ProcMeshBodySetup)
      {
        UBodySetup* SourceBodySetup = ProceduralMeshComponent->ProcMeshBodySetup;
        const FKAggregateGeom& SourceAggGeom = SourceBodySetup->AggGeom;
        
        int32 SourceElementCount = SourceAggGeom.GetElementCount();

        // 输出源碰撞几何的详细信息
        UE_LOG(LogTemp, Log, TEXT("【方式1】ProceduralMeshComponent碰撞信息 - ConvexElems: %d, BoxElems: %d, SphereElems: %d, SphylElems: %d, TaperedCapsuleElems: %d"),
          SourceAggGeom.ConvexElems.Num(),
          SourceAggGeom.BoxElems.Num(),
          SourceAggGeom.SphereElems.Num(),
          SourceAggGeom.SphylElems.Num(),
          SourceAggGeom.TaperedCapsuleElems.Num());
        
        // 只有当PMC确实有碰撞数据时才尝试复制
        if (SourceElementCount > 0)
        {
          UE_LOG(LogTemp, Log, TEXT("【方式1】检测到PMC有碰撞数据（%d个元素），开始复制..."), SourceElementCount);

          // 复制所有简单碰撞几何体（比自动生成更准确）
          if (SourceAggGeom.ConvexElems.Num() > 0)
          {
            UE_LOG(LogTemp, Log, TEXT("复制凸包元素到StaticMesh..."));
            NewBodySetup->AggGeom.ConvexElems = SourceAggGeom.ConvexElems;
            bCopiedCollisionData = true;
            CollisionSourceType = 1; // 从PMC复制
            UE_LOG(LogTemp, Log, TEXT("已复制 %d 个凸包元素"), SourceAggGeom.ConvexElems.Num());
          }
          
          if (SourceAggGeom.BoxElems.Num() > 0)
          {
            UE_LOG(LogTemp, Log, TEXT("复制包围盒元素到StaticMesh..."));
            NewBodySetup->AggGeom.BoxElems = SourceAggGeom.BoxElems;
            bCopiedCollisionData = true;
            CollisionSourceType = 1; // 从PMC复制
            UE_LOG(LogTemp, Log, TEXT("已复制 %d 个包围盒元素"), SourceAggGeom.BoxElems.Num());
          }
          
          if (SourceAggGeom.SphereElems.Num() > 0)
          {
            UE_LOG(LogTemp, Log, TEXT("复制球体元素到StaticMesh..."));
            NewBodySetup->AggGeom.SphereElems = SourceAggGeom.SphereElems;
            bCopiedCollisionData = true;
            CollisionSourceType = 1; // 从PMC复制
            UE_LOG(LogTemp, Log, TEXT("已复制 %d 个球体元素"), SourceAggGeom.SphereElems.Num());
          }
          
          if (SourceAggGeom.SphylElems.Num() > 0)
          {
            UE_LOG(LogTemp, Log, TEXT("复制胶囊体元素到StaticMesh..."));
            NewBodySetup->AggGeom.SphylElems = SourceAggGeom.SphylElems;
            bCopiedCollisionData = true;
            CollisionSourceType = 1; // 从PMC复制
            UE_LOG(LogTemp, Log, TEXT("已复制 %d 个胶囊体元素"), SourceAggGeom.SphylElems.Num());
          }
          
          if (SourceAggGeom.TaperedCapsuleElems.Num() > 0)
          {
            UE_LOG(LogTemp, Log, TEXT("复制锥形胶囊体元素到StaticMesh..."));
            NewBodySetup->AggGeom.TaperedCapsuleElems = SourceAggGeom.TaperedCapsuleElems;
            bCopiedCollisionData = true;
            CollisionSourceType = 1; // 从PMC复制
            UE_LOG(LogTemp, Log, TEXT("已复制 %d 个锥形胶囊体元素"), SourceAggGeom.TaperedCapsuleElems.Num());
          }
          
          if (bCopiedCollisionData)
          {
            UE_LOG(LogTemp, Log, TEXT("【方式1】成功！已从PMC复制碰撞数据（总计 %d 个元素）"), 
              NewBodySetup->AggGeom.GetElementCount());
          }
        }
        else
        {
          UE_LOG(LogTemp, Log, TEXT("【方式1】跳过：PMC虽然有BodySetup，但AggGeom为空（元素数为0）"));
          UE_LOG(LogTemp, Log, TEXT("提示：如需使用方式1，请先在PMC中手动设置碰撞几何体（如包围盒、凸包等）"));
        }
      }
      
      // ===== 方式2：基于实际顶点生成碰撞（次优） =====
      // 优点：基于实际几何体更准确、自动适应几何变化
      // 策略：顶点数>=4时生成凸包，否则生成包围盒
      // 适用：PMC无碰撞数据时
      if (!bCopiedCollisionData)
      {
        UE_LOG(LogTemp, Log, TEXT("【方式2】PMC无碰撞数据，基于实际顶点生成碰撞..."));
        
        // 收集所有唯一顶点（用于生成凸包）
        TArray<FVector> AllUniqueVertices;
        TMap<FIntVector, FVector> QuantizedVertexMap; // 使用量化后的位置作为键来去重
        FBox BoundingBox(ForceInitToZero);
        bool bHasVertices = false;
        const float QuantizeScale = 100.0f; // 量化精度：1cm

        for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
        {
          FProcMeshSection* SectionData = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
          if (SectionData && SectionData->ProcVertexBuffer.Num() > 0)
          {
            for (const FProcMeshVertex& Vertex : SectionData->ProcVertexBuffer)
            {
              FVector Position = Vertex.Position;
              BoundingBox += Position;
              bHasVertices = true;
              
              // 量化位置用于去重（1cm精度）
              FIntVector QuantizedPos(
                FMath::RoundToInt(Position.X * QuantizeScale),
                FMath::RoundToInt(Position.Y * QuantizeScale),
                FMath::RoundToInt(Position.Z * QuantizeScale)
              );
              
              if (!QuantizedVertexMap.Contains(QuantizedPos))
              {
                QuantizedVertexMap.Add(QuantizedPos, Position);
                AllUniqueVertices.Add(Position);
              }
            }
          }
        }

        if (bHasVertices && BoundingBox.IsValid)
        {
          // 如果唯一顶点数 >= 4，生成凸包；否则生成包围盒
          if (AllUniqueVertices.Num() >= 4)
          {
            FKConvexElem ConvexElem;
            ConvexElem.VertexData = AllUniqueVertices;
            ConvexElem.UpdateElemBox();
            NewBodySetup->AggGeom.ConvexElems.Add(ConvexElem);
            
            UE_LOG(LogTemp, Log, TEXT("【方式2】基于实际顶点创建ConvexElem: 顶点数=%d, 中心=(%.2f, %.2f, %.2f), 包围盒大小=(%.2f, %.2f, %.2f)"),
              AllUniqueVertices.Num(),
              ConvexElem.ElemBox.GetCenter().X, ConvexElem.ElemBox.GetCenter().Y, ConvexElem.ElemBox.GetCenter().Z,
              ConvexElem.ElemBox.GetSize().X, ConvexElem.ElemBox.GetSize().Y, ConvexElem.ElemBox.GetSize().Z);
            bCopiedCollisionData = true;
            CollisionSourceType = 2; // 基于实际顶点计算
          }
          else
          {
            FVector Center = BoundingBox.GetCenter();
            FVector Extent = BoundingBox.GetExtent();

            if (Extent.X > 0.0f && Extent.Y > 0.0f && Extent.Z > 0.0f)
            {
              FKBoxElem BoxElem;
              BoxElem.Center = Center;
              BoxElem.X = Extent.X * 2.0f;
              BoxElem.Y = Extent.Y * 2.0f;
              BoxElem.Z = Extent.Z * 2.0f;
              UE_LOG(LogTemp, Log, TEXT("【方式2】顶点数不足4个，创建BoxElem: 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
                Center.X, Center.Y, Center.Z, BoxElem.X, BoxElem.Y, BoxElem.Z);
              NewBodySetup->AggGeom.BoxElems.Add(BoxElem);
              bCopiedCollisionData = true;
              CollisionSourceType = 2; // 基于实际顶点计算
            }
          }
        }
        else
        {
          UE_LOG(LogTemp, Warning, TEXT("无法生成碰撞（无有效顶点），将回退到自动生成"));
        }
      }

      // ===== 方式3：使用BuildFromMeshDescriptions自动生成的碰撞（后备） =====
      // 优点：引擎标准方法、可靠、基于RenderData->Bounds
      // 缺点：可能不够精确、只有包围盒
      // 适用：前两种方式都失败时作为后备
      if (!bCopiedCollisionData)
      {
        // 手动设置失败，恢复之前保存的自动生成的碰撞数据
        int32 BackupElementCount = BackupAggGeom.GetElementCount();
        if (BackupElementCount > 0)
        {
          NewBodySetup->AggGeom = BackupAggGeom;
          CollisionSourceType = 3; // BuildFromMeshDescriptions自动生成
          UE_LOG(LogTemp, Log, TEXT("【方式3】手动创建碰撞失败，已恢复自动生成的碰撞（%d个元素）"), 
            BackupElementCount);
          UE_LOG(LogTemp, Log, TEXT("自动生成的碰撞详情 - ConvexElems: %d, BoxElems: %d, SphereElems: %d, SphylElems: %d"),
            BackupAggGeom.ConvexElems.Num(),
            BackupAggGeom.BoxElems.Num(),
            BackupAggGeom.SphereElems.Num(),
            BackupAggGeom.SphylElems.Num());
        }
        else
        {
          // ===== 方式4：基于RenderData.Bounds生成（最后的后备） =====
          // 参考StaticMesh.cpp:5473-5482，这是引擎内部使用的标准方法
          if (StaticMesh->RenderData.IsValid())
          {
            const FBoxSphereBounds& Bounds = StaticMesh->RenderData->Bounds;
            // 检查Bounds是否有效：BoxExtent应该大于0
            if (Bounds.BoxExtent.X > 0.0f && Bounds.BoxExtent.Y > 0.0f && Bounds.BoxExtent.Z > 0.0f)
            {
              UE_LOG(LogTemp, Warning, TEXT("【方式4】手动创建碰撞失败，且自动生成也未产生碰撞数据，尝试基于RenderData.Bounds生成包围盒..."));

              FKBoxElem BoxElem;
              BoxElem.Center = Bounds.Origin;
              BoxElem.X = Bounds.BoxExtent.X * 2.0f;
              BoxElem.Y = Bounds.BoxExtent.Y * 2.0f;
              BoxElem.Z = Bounds.BoxExtent.Z * 2.0f;
              NewBodySetup->AggGeom.BoxElems.Add(BoxElem);
              CollisionSourceType = 4; // 基于RenderData.Bounds生成
              
              UE_LOG(LogTemp, Log, TEXT("基于RenderData.Bounds创建包围盒: 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
                BoxElem.Center.X, BoxElem.Center.Y, BoxElem.Center.Z,
                BoxElem.X, BoxElem.Y, BoxElem.Z);
            }
            else
            {
              UE_LOG(LogTemp, Warning, TEXT("RenderData.Bounds无效（BoxExtent为0或负数），无法生成碰撞"));
            }
          }
          else
          {
            UE_LOG(LogTemp, Warning, TEXT("手动创建碰撞失败，且无法从RenderData.Bounds生成碰撞！"));
            UE_LOG(LogTemp, Warning, TEXT("建议检查网格数据是否有效，或手动添加碰撞几何体"));
          }
        }
      }
      else
      {
        // 手动设置成功，输出信息
        int32 ManualElementCount = NewBodySetup->AggGeom.GetElementCount();
        UE_LOG(LogTemp, Log, TEXT("手动设置碰撞成功（%d个元素），已替换自动生成的碰撞"), ManualElementCount);
      }
      
      // 重要：无论手动设置成功还是失败，都需要调用CreatePhysicsMeshes()来生成物理网格
      // 参考StaticMesh.cpp:5481，这是生成碰撞数据的标准方法，不需要重新构建整个mesh
      if (NewBodySetup->AggGeom.GetElementCount() > 0)
      {
        NewBodySetup->CreatePhysicsMeshes();
        UE_LOG(LogTemp, Log, TEXT("已调用BodySetup->CreatePhysicsMeshes()生成物理网格"));
      }

      NewBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
      
      // ===== BodySetup 完整配置（根据 BodySetup.h 源码） =====
      // ========== 几何和碰撞生成设置 ==========
      
      // 【参数1】bGenerateMirroredCollision（镜像碰撞生成）
      // 类型：bool
      // 默认值：false
      // 作用：是否生成支持镜像版本网格的碰撞数据
      // 详细说明：
      //   - false：不生成镜像碰撞数据（节省内存，推荐）
      //   - true：生成镜像碰撞数据，碰撞数据大小会减半，但会禁用镜像实例的碰撞
      // 适用场景：只有在需要镜像网格（负缩放）且需要碰撞时才启用
      // 内存影响：启用后会减少碰撞数据大小，但可能影响镜像实例的碰撞功能
      NewBodySetup->bGenerateMirroredCollision = false;
      
      // 【参数2】bGenerateNonMirroredCollision（非镜像碰撞生成）
      // 类型：bool
      // 默认值：true
      // 作用：生成非镜像版本的碰撞数据（正常情况）
      // 说明：这是正常网格的碰撞数据，应该始终启用
      NewBodySetup->bGenerateNonMirroredCollision = true;
      
      // 【参数3】bDoubleSidedGeometry（双面几何体）
      // 类型：bool
      // 默认值：true
      // 作用：物理三角形网格在场景查询时是否使用双面面片
      // 详细说明：
      //   - true：启用双面碰撞，射线可以从两侧检测到碰撞
      //   - false：单面碰撞，只有正面才能检测到碰撞
      // 适用场景：
      //   - 平面和单面网格：如果需要在两侧都能进行射线检测，应该启用
      //   - 封闭网格：通常不需要，但启用也没有问题
      // 性能影响：几乎无影响
      NewBodySetup->bDoubleSidedGeometry = true;
      
      // 【参数4】bSupportUVsAndFaceRemap（UV和面重映射支持）
      // 类型：bool
      // 默认值：false
      // 作用：是否支持UV和面重映射，用于物理材质遮罩功能
      // 详细说明：
      //   - false：不生成UV和面重映射数据（节省内存，推荐）
      //   - true：生成UV和面重映射数据，可以支持物理材质遮罩
      // 适用场景：
      //   - 需要使用物理材质遮罩（PhysicalMaterialMask）时才启用
      //   - 需要从碰撞结果获取UV坐标时才启用
      // 内存影响：启用后会增加内存占用
      NewBodySetup->bSupportUVsAndFaceRemap = false;
      
      // 【参数5】bConsiderForBounds（考虑边界计算）
      // 类型：bool
      // 默认值：true
      // 作用：是否在计算PhysicsAsset的边界时考虑此BodySetup
      // 详细说明：
      //   - true：参与边界计算，影响物理资产的边界框
      //   - false：不参与边界计算，可以提升边界计算性能
      // 适用场景：
      //   - 大多数情况应该启用，确保边界计算准确
      //   - 如果有很多BodySetup且性能敏感，可以禁用不重要的
      // 性能影响：启用后会参与边界计算，轻微性能开销
      NewBodySetup->bConsiderForBounds = true;
      
      // 【参数6】bMeshCollideAll（全网格碰撞）
      // 类型：bool
      // 默认值：true
      // 作用：是否强制使用整个渲染网格进行碰撞
      // 详细说明：
      //   - true：强制使用整个渲染网格进行碰撞（包括未启用碰撞的部分）
      //   - false：只使用启用碰撞的网格元素
      // 适用场景：
      //   - true：需要整个网格都参与碰撞，提供最精确的碰撞
      //   - false：只使用启用碰撞的部分，性能更好
      // 性能影响：启用后会增加内存和性能开销
      // 注意：这个参数只影响静态网格，且需要配合CollisionTraceFlag使用
      NewBodySetup->bMeshCollideAll = true;
      
      // ========== 物理模拟设置 ==========
      
      // 【参数7】PhysicsType（物理类型）
      // 类型：EPhysicsType
      // 默认值：PhysType_Default
      // 作用：物理模拟类型
      // 可选值：
      //   - PhysType_Default：默认物理类型
      //   - PhysType_Kinematic：运动学物理（不响应力，但可以移动）
      //   - PhysType_Simulated：完全模拟物理（响应力和碰撞）
      // 说明：静态网格通常使用默认类型
      NewBodySetup->PhysicsType = EPhysicsType::PhysType_Default;
      
      // 【参数8】PhysMaterial（物理材质）
      // 类型：UPhysicalMaterial*
      // 默认值：nullptr（使用默认物理材质）
      // 作用：定义物理对象的响应属性（密度、摩擦、弹性等）
      // 详细说明：
      //   - nullptr：使用默认物理材质（密度1.0，摩擦0.7，弹性0.0）
      //   - 自定义材质：可以设置密度、摩擦系数、弹性等
      // 适用场景：
      //   - 需要特殊物理属性的物体（如冰面、橡胶、金属等）
      //   - 简单场景可以保持默认
      // 注意：这是简单碰撞使用的物理材质（用于简单几何体）
      if (ProceduralMeshComponent->ProcMeshBodySetup && ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial)
      {
        NewBodySetup->PhysMaterial = ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial;
        UE_LOG(LogTemp, Log, TEXT("已复制物理材质: %s"), *NewBodySetup->PhysMaterial->GetName());
      }
      
      // 【参数9】BuildScale3D（构建缩放）
      // 类型：FVector
      // 默认值：FVector(1.0f, 1.0f, 1.0f)（无缩放）
      // 作用：构建碰撞时的缩放因子
      // 详细说明：
      //   - (1,1,1)：无缩放，使用原始大小
      //   - 其他值：按比例缩放碰撞几何体
      // 适用场景：
      //   - 静态网格设置中定义的构建缩放
      //   - 通常保持默认值
      // 注意：这个值会影响碰撞几何体的实际大小
      NewBodySetup->BuildScale3D = FVector(1.0f, 1.0f, 1.0f);

      // ===== StaticMesh 资源层面的完整碰撞设置 =====
      // BodySetup->DefaultInstance 设置的是 StaticMesh 资源本身的默认碰撞属性
      // 根据源码，这些设置会被所有使用该 StaticMesh 的组件继承（除非组件层面明确覆盖）
      // 默认始终生成碰撞
      
      // ===== 正确的碰撞配置设置顺序 =====
      // 关键：必须先设置 Profile，它会自动应用所有默认设置（包括 CollisionEnabled 和 ObjectType）
      // 然后再设置需要自定义的属性（如重力、CCD等）
      
      // ========== BodyInstance 碰撞设置（资源层面的默认设置） ==========
      // 这些设置会被所有使用该 StaticMesh 的组件自动继承
      
      // 【参数11】CollisionProfileName（碰撞配置名称）
      // 类型：FName
      // 当前值："BlockAll"
      // 作用：预设的碰撞配置，包含所有碰撞相关设置
      // 详细说明：
      //   - BlockAll：阻挡所有通道，适合静态障碍物
      //     * CollisionEnabled: QueryAndPhysics
      //     * ObjectType: ECC_WorldStatic
      //     * 所有通道响应: ECR_Block
      //   - OverlapAll：重叠所有通道，适合触发器
      //   - Custom：自定义配置（手动设置时）
      // 重要：必须先设置 Profile，它会自动应用所有默认设置
      // 如果手动设置 CollisionEnabled 或 ObjectType，Profile 会变为 Custom
      NewBodySetup->DefaultInstance.SetCollisionProfileName(TEXT("BlockAll"));
      
      // 注意：不需要再手动设置 CollisionEnabled 和 ObjectType，因为 Profile 已经设置了
      // 如果手动设置这些，可能会破坏 Profile 的内部状态，导致显示为 Custom

      // 【参数12】SetEnableGravity（重力设置）
      // 类型：bool
      // 当前值：false（禁用重力）
      // 作用：是否受重力影响
      // 详细说明：
      //   - false：不受重力影响（静态网格的默认设置）
      //   - true：受重力影响（用于动态物体）
      // 适用场景：
      //   - 静态网格：应该禁用重力
      //   - 动态物体：需要启用重力
      // 性能影响：禁用重力可以提升性能
      NewBodySetup->DefaultInstance.SetEnableGravity(false);
      
      // 【参数13】bUseCCD（连续碰撞检测）
      // 类型：bool
      // 当前值：false（禁用CCD）
      // 作用：是否使用连续碰撞检测
      // 详细说明：
      //   - false：不使用CCD（静态网格的默认设置）
      //   - true：使用CCD，可以检测高速物体的碰撞
      // 适用场景：
      //   - 静态网格：不需要CCD
      //   - 高速移动的物体：需要CCD避免穿透
      // 性能影响：启用CCD会增加性能开销
      NewBodySetup->DefaultInstance.bUseCCD = false;
      
      UE_LOG(LogTemp, Log, TEXT("StaticMesh资源层面：完整碰撞配置已设置"));
      UE_LOG(LogTemp, Log, TEXT("  - 碰撞启用: %s"), 
          *UEnum::GetValueAsString(NewBodySetup->DefaultInstance.GetCollisionEnabled()));
      UE_LOG(LogTemp, Log, TEXT("  - 碰撞配置: %s"), 
          *NewBodySetup->DefaultInstance.GetCollisionProfileName().ToString());
      UE_LOG(LogTemp, Log, TEXT("  - 对象类型: %s"), 
          *UEnum::GetValueAsString(NewBodySetup->DefaultInstance.GetObjectType()));
      UE_LOG(LogTemp, Log, TEXT("  - 重力: 禁用"));
      UE_LOG(LogTemp, Log, TEXT("  - CCD: 禁用"));

      // 标记 BodySetup 需要重新构建
      NewBodySetup->InvalidatePhysicsData();
      
      // ===== 创建物理网格后检查是否有复杂碰撞（三角网格） =====
      // 创建物理网格后检查是否有复杂碰撞（三角网格）
      bool bHasComplexCollision = NewBodySetup->TriMeshes.Num() > 0 && 
                                  NewBodySetup->TriMeshes[0] != nullptr;
      
      // 如果没有复杂碰撞，从 RenderData 生成 TriMeshes（类似 FBX 导入的处理）
      // 这确保了即使没有简单碰撞，也能生成复杂碰撞用于物理交互
      if (!bHasComplexCollision && StaticMesh->RenderData && StaticMesh->RenderData->LODResources.Num() > 0)
      {
#if PLATFORM_WINDOWS || PLATFORM_MAC
        // 注意：CreatePhysicsMeshesForFbxLoad() 在 UE 4.26 中可能不存在
        // 如果需要复杂碰撞（TriMeshes），可以使用其他方法
        // 当前使用简单碰撞（ConvexElem/BoxElem）已足够
        // NewBodySetup->CreatePhysicsMeshesForFbxLoad();
        
        // 重新检查是否成功生成了复杂碰撞
        bHasComplexCollision = NewBodySetup->TriMeshes.Num() > 0 && 
                              NewBodySetup->TriMeshes[0] != nullptr;
#endif
      }
      
      // ===== 验证物理网格创建结果 =====
      const int32 AggGeomElementCount = NewBodySetup->AggGeom.GetElementCount();
      UE_LOG(LogTemp, Log, TEXT("物理网格验证 - AggGeom元素数量: %d, 追踪标志: %d"),
          AggGeomElementCount, (int32)NewBodySetup->CollisionTraceFlag);

      if (AggGeomElementCount > 0)
      {
        // 输出详细的碰撞几何信息
        const FKAggregateGeom& FinalAggGeom = NewBodySetup->AggGeom;
        UE_LOG(LogTemp, Log, TEXT("物理网格详细信息:"));
        UE_LOG(LogTemp, Log, TEXT("  - ConvexElems: %d"), FinalAggGeom.ConvexElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - BoxElems: %d"), FinalAggGeom.BoxElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - SphereElems: %d"), FinalAggGeom.SphereElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - SphylElems: %d"), FinalAggGeom.SphylElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  - TaperedCapsuleElems: %d"), FinalAggGeom.TaperedCapsuleElems.Num());
        UE_LOG(LogTemp, Log, TEXT("物理网格创建成功！"));
      }
      else
      {
        UE_LOG(LogTemp, Warning, TEXT("物理网格数据为空！可能的问题："));
        UE_LOG(LogTemp, Warning, TEXT("  - 网格几何体可能无效"));
        UE_LOG(LogTemp, Warning, TEXT("  - BuildFromMeshDescriptions 可能没有正确生成碰撞"));
        UE_LOG(LogTemp, Warning, TEXT("  - 请检查网格数据和碰撞设置"));
        UE_LOG(LogTemp, Warning, TEXT("  - 建议在编辑器中手动添加UCX碰撞以改善简单碰撞"));
      }

      // ===== 最终验证 =====
      UE_LOG(LogTemp, Log, TEXT("=== StaticMesh BodySetup 配置总结 ==="));
      UE_LOG(LogTemp, Log, TEXT("碰撞类型: 简单碰撞"));
      UE_LOG(LogTemp, Log, TEXT("碰撞追踪标志: %d (%s)"),
          (int32)NewBodySetup->CollisionTraceFlag,
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

  // 确保 StaticMesh 不被垃圾回收
  StaticMesh->AddToRoot();

  // 设置 LOD ScreenSize（与 MWAssetStaticMeshFromLocal 保持一致）
  if (StaticMesh->RenderData)
  {
    StaticMesh->RenderData->ScreenSize[0] = 1.0f;
    StaticMesh->RenderData->ScreenSize[1] = 0.2f; // Lod1ScreenSize
    StaticMesh->RenderData->ScreenSize[2] = 0.1f; // Lod2ScreenSize
  }

  // 设置导航碰撞
  StaticMesh->CreateNavCollision(true);

  // 注意：bAllowCPUAccess 已经在上面设置为 true

  return StaticMesh;
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