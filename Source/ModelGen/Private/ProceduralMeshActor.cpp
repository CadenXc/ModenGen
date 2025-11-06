#include "ProceduralMeshActor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
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
    
    if (!ProceduralMeshComponent || ProceduralMeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
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
    if (!ProceduralMeshComponent || ProceduralMeshComponent->GetNumSections() == 0)
    {
        return nullptr;
    }

    const int32 NumSections = ProceduralMeshComponent->GetNumSections();

    // 创建 StaticMesh
    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
        GetTransientPackage(), NAME_None, RF_Public | RF_Standalone);
    if (!StaticMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("无法创建 StaticMesh 对象"));
        return nullptr;
    }

    // 基础属性
    StaticMesh->NeverStream = true;
    StaticMesh->bAllowCPUAccess = true;
    // StaticMesh->bIsBuiltAtRuntime = true;  // 运行时生成标记（可选，UE版本可能不支持）
    StaticMesh->SetLightMapResolution(64);
    StaticMesh->SetLightMapCoordinateIndex(1);
    StaticMesh->bGenerateMeshDistanceField = false;
    StaticMesh->bHasNavigationData = false;

    // 提前创建 BodySetup（关键！）
    StaticMesh->CreateBodySetup();
    UBodySetup* NewBodySetup = StaticMesh->GetBodySetup();
    if (!NewBodySetup)
    {
        return nullptr;
    }

    // 构建 MeshDescription
    FMeshDescription MeshDescription;
    FStaticMeshAttributes Attributes(MeshDescription);
    Attributes.Register();

    FMeshDescriptionBuilder MeshDescBuilder;
    MeshDescBuilder.SetMeshDescription(&MeshDescription);
    MeshDescBuilder.EnablePolyGroups();
    MeshDescBuilder.SetNumUVLayers(1);

    TMap<FVector, FVertexID> VertexMap;
    TArray<FName> MaterialSlotNames;

    // 预处理材质槽
    for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
    {
        FProcMeshSection* Section = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
        if (!Section || Section->ProcVertexBuffer.Num() < 3 || Section->ProcIndexBuffer.Num() < 3)
            continue;

        FName SlotName = FName(*FString::Printf(TEXT("MaterialSlot_%d"), SectionIdx));
        MaterialSlotNames.Add(SlotName);

        FStaticMaterial StaticMat;
        StaticMat.MaterialSlotName = SlotName;
        StaticMat.UVChannelData.bInitialized = true;

        // 计算 UV 密度
        float UVDensity = 100.0f;
        if (Section->ProcVertexBuffer.Num() > 0)
        {
            FVector2D MinUV(FLT_MAX), MaxUV(-FLT_MAX);
            for (const FProcMeshVertex& V : Section->ProcVertexBuffer)
            {
                MinUV.X = FMath::Min(MinUV.X, V.UV0.X);
                MinUV.Y = FMath::Min(MinUV.Y, V.UV0.Y);
                MaxUV.X = FMath::Max(MaxUV.X, V.UV0.X);
                MaxUV.Y = FMath::Max(MaxUV.Y, V.UV0.Y);
            }
            FVector2D Range = MaxUV - MinUV;
            if (Range.Size() > SMALL_NUMBER)
            {
                UVDensity = FMath::Max(Range.X, Range.Y);
            }
        }
        for (int32 i = 0; i < MAX_TEXCOORDS; ++i)
        {
            StaticMat.UVChannelData.LocalUVDensities[i] = UVDensity;
        }

        StaticMesh->GetStaticMaterials().Add(StaticMat);
    }

    // 填充顶点和三角形
    for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
    {
        FProcMeshSection* Section = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
        if (!Section || Section->ProcVertexBuffer.Num() < 3 || Section->ProcIndexBuffer.Num() < 3)
            continue;

        FPolygonGroupID PolyGroupID = MeshDescBuilder.AppendPolygonGroup();
        Attributes.GetPolygonGroupMaterialSlotNames().Set(PolyGroupID, MaterialSlotNames[SectionIdx]);

        TArray<FVertexInstanceID> InstanceIDs;
        InstanceIDs.Reserve(Section->ProcVertexBuffer.Num());

        for (const FProcMeshVertex& ProcVert : Section->ProcVertexBuffer)
        {
            FVertexID VertID;
            if (FVertexID* Found = VertexMap.Find(ProcVert.Position))
            {
                VertID = *Found;
            }
            else
            {
                VertID = MeshDescBuilder.AppendVertex(ProcVert.Position);
                VertexMap.Add(ProcVert.Position, VertID);
            }

            FVertexInstanceID InstanceID = MeshDescBuilder.AppendInstance(VertID);
            MeshDescBuilder.SetInstanceNormal(InstanceID, ProcVert.Normal);
            // 注意：FMeshDescriptionBuilder 没有 SetInstanceTangent，切线会在 BuildFromMeshDescriptions 时自动计算
            MeshDescBuilder.SetInstanceUV(InstanceID, ProcVert.UV0, 0);
            MeshDescBuilder.SetInstanceColor(InstanceID, FLinearColor(ProcVert.Color));

            InstanceIDs.Add(InstanceID);
        }

        for (int32 i = 0; i < Section->ProcIndexBuffer.Num(); i += 3)
        {
            if (i + 2 >= Section->ProcIndexBuffer.Num()) break;

            int32 I0 = (int32)Section->ProcIndexBuffer[i];
            int32 I1 = (int32)Section->ProcIndexBuffer[i + 1];
            int32 I2 = (int32)Section->ProcIndexBuffer[i + 2];

            if (I0 >= InstanceIDs.Num() || I1 >= InstanceIDs.Num() || I2 >= InstanceIDs.Num())
                continue;

            MeshDescBuilder.AppendTriangle(
                InstanceIDs[I0],
                InstanceIDs[I1],
                InstanceIDs[I2],
                PolyGroupID
            );
        }
    }

    if (MeshDescription.Vertices().Num() == 0)
    {
        return nullptr;
    }

    // === 关键：配置 BuildParams 和 BodySetup ===
    UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
    // 设置基本参数
    BuildParams.bMarkPackageDirty = false;  // 运行时生成不需要标记包为脏
    BuildParams.bUseHashAsGuid = false;
    BuildParams.bCommitMeshDescription = true;
    BuildParams.bFastBuild = false;
    BuildParams.bAllowCpuAccess = true;  // 允许CPU访问（用于运行时生成）

    if (bUseComplexCollision)
    {
        // 使用网格本身作为简单碰撞
        NewBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
        BuildParams.bBuildSimpleCollision = false;  // 关闭自动包围盒
    }
    else
    {
        // 使用简单碰撞（引擎生成或手动）
        NewBodySetup->CollisionTraceFlag = CTF_UseDefault;
        BuildParams.bBuildSimpleCollision = true;
    }

    // === 构建 StaticMesh ===
    TArray<const FMeshDescription*> MeshDescriptions = { &MeshDescription };
    StaticMesh->BuildFromMeshDescriptions(MeshDescriptions, BuildParams);

    // === 设置材质 ===
    for (int32 SectionIdx = 0; SectionIdx < NumSections && SectionIdx < StaticMesh->GetStaticMaterials().Num(); ++SectionIdx)
    {
        UMaterialInterface* Mat = ProceduralMeshComponent->GetMaterial(SectionIdx);
        if (!Mat && ProceduralDefaultMaterial)
            Mat = ProceduralDefaultMaterial;
        StaticMesh->GetStaticMaterials()[SectionIdx].MaterialInterface = Mat;
    }

    // === 简单碰撞手动 fallback（无论复杂/简单模式，都强制生成 ConvexElem 作为备用）===
    bool bGeneratedSimpleCollision = false;

    // 尝试从 ProceduralMesh 复制（优先）
    if (ProceduralMeshComponent->ProcMeshBodySetup &&
        ProceduralMeshComponent->ProcMeshBodySetup->AggGeom.GetElementCount() > 0)
    {
        NewBodySetup->AggGeom = ProceduralMeshComponent->ProcMeshBodySetup->AggGeom;
        bGeneratedSimpleCollision = true;
        UE_LOG(LogTemp, Log, TEXT("从 ProceduralMesh 复制了 AggGeom（元素数: %d）"),
            NewBodySetup->AggGeom.GetElementCount());
    }

    // 如果未复制，生成 ConvexElem（用唯一顶点）
    if (!bGeneratedSimpleCollision)
    {
        TArray<FVector> UniqueVertices;
        TMap<FString, bool> VertexKeyMap;
        const float Tolerance = 0.01f;

        for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
        {
            FProcMeshSection* SectionData = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
            if (SectionData && SectionData->ProcVertexBuffer.Num() > 0)
            {
                for (const FProcMeshVertex& Vertex : SectionData->ProcVertexBuffer)
                {
                    int32 X = FMath::RoundToInt(Vertex.Position.X / Tolerance);
                    int32 Y = FMath::RoundToInt(Vertex.Position.Y / Tolerance);
                    int32 Z = FMath::RoundToInt(Vertex.Position.Z / Tolerance);
                    FString VertexKey = FString::Printf(TEXT("%d_%d_%d"), X, Y, Z);

                    if (!VertexKeyMap.Contains(VertexKey))
                    {
                        VertexKeyMap.Add(VertexKey, true);
                        UniqueVertices.Add(Vertex.Position);
                    }
                }
            }
        }

        if (UniqueVertices.Num() >= 4)
        {
            FKConvexElem ConvexElem;
            ConvexElem.VertexData = UniqueVertices;
            ConvexElem.UpdateElemBox();  // 更新包围盒（无参数）
            NewBodySetup->AggGeom.ConvexElems.Add(ConvexElem);
            bGeneratedSimpleCollision = true;
            UE_LOG(LogTemp, Log, TEXT("生成了 ConvexElem（顶点数: %d）"), UniqueVertices.Num());
        }
        else if (UniqueVertices.Num() > 0)
        {
            // Fallback 到 BoxElem
            const FBoxSphereBounds& Bounds = StaticMesh->GetBounds();
            if (Bounds.BoxExtent.Size() > 0)
            {
                FKBoxElem BoxElem;
                BoxElem.Center = Bounds.Origin;
                BoxElem.X = Bounds.BoxExtent.X * 2.0f;
                BoxElem.Y = Bounds.BoxExtent.Y * 2.0f;
                BoxElem.Z = Bounds.BoxExtent.Z * 2.0f;
                // BoxElem 无 UpdateElemBox()，包围盒自动基于 Center/X/Y/Z 更新
                NewBodySetup->AggGeom.BoxElems.Add(BoxElem);
                bGeneratedSimpleCollision = true;
                UE_LOG(LogTemp, Log, TEXT("Fallback 到 BoxElem（基于 Bounds）"));
            }
        }
    }

    // 如果复杂模式，AggGeom 作为简单备用
    if (bUseComplexCollision && bGeneratedSimpleCollision)
    {
        UE_LOG(LogTemp, Log, TEXT("复杂模式下生成了简单碰撞备用（AggGeom 元素: %d）"),
            NewBodySetup->AggGeom.GetElementCount());
        // 保持 CollisionTraceFlag=UseComplexAsSimple（优先复杂）
    }

    // === BodySetup 通用设置 ===
    NewBodySetup->bDoubleSidedGeometry = true;
    NewBodySetup->bGenerateMirroredCollision = false;
    NewBodySetup->bGenerateNonMirroredCollision = true;
    NewBodySetup->bMeshCollideAll = true;
    NewBodySetup->BuildScale3D = FVector(1.0f);

    if (ProceduralMeshComponent->ProcMeshBodySetup && ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial)
    {
        NewBodySetup->PhysMaterial = ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial;
    }

    if (bGenerateCollision)
    {
        NewBodySetup->DefaultInstance.SetCollisionProfileName(TEXT("BlockAll"));
    }
    else
    {
        NewBodySetup->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 刷新物理
    NewBodySetup->InvalidatePhysicsData();
    NewBodySetup->CreatePhysicsMeshes();

    // === 打印 UStaticMesh 数据（重点关注 ConvexElem）===
    UE_LOG(LogTemp, Log, TEXT("========================================"));
    UE_LOG(LogTemp, Log, TEXT("=== UStaticMesh 数据输出 ==="));
    UE_LOG(LogTemp, Log, TEXT("========================================"));

    // StaticMesh 基本信息
    UE_LOG(LogTemp, Log, TEXT("StaticMesh 名称: %s"), *StaticMesh->GetName());
    UE_LOG(LogTemp, Log, TEXT("StaticMesh 材质槽数量: %d"), StaticMesh->GetStaticMaterials().Num());
    UE_LOG(LogTemp, Log, TEXT("StaticMesh 渲染数据 LOD 数量: %d"), StaticMesh->GetNumLODs());

    // Bounds 信息
    const FBoxSphereBounds& Bounds = StaticMesh->GetBounds();
    UE_LOG(LogTemp, Log, TEXT("StaticMesh Bounds - Origin: %s, Extent: %s"),
        *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString());

    // BodySetup 信息
    if (NewBodySetup)
    {
        UE_LOG(LogTemp, Log, TEXT("--- BodySetup 信息 ---"));
        UE_LOG(LogTemp, Log, TEXT("  CollisionTraceFlag: %d (%s)"),
            (int32)NewBodySetup->CollisionTraceFlag,
            NewBodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple ? TEXT("UseComplexAsSimple") :
            NewBodySetup->CollisionTraceFlag == CTF_UseSimpleAsComplex ? TEXT("UseSimpleAsComplex") :
            NewBodySetup->CollisionTraceFlag == CTF_UseDefault ? TEXT("UseDefault") : TEXT("Unknown"));
        UE_LOG(LogTemp, Log, TEXT("  bDoubleSidedGeometry: %s"), NewBodySetup->bDoubleSidedGeometry ? TEXT("true") : TEXT("false"));
        UE_LOG(LogTemp, Log, TEXT("  bGenerateMirroredCollision: %s"), NewBodySetup->bGenerateMirroredCollision ? TEXT("true") : TEXT("false"));
        UE_LOG(LogTemp, Log, TEXT("  bGenerateNonMirroredCollision: %s"), NewBodySetup->bGenerateNonMirroredCollision ? TEXT("true") : TEXT("false"));
        UE_LOG(LogTemp, Log, TEXT("  bMeshCollideAll: %s"), NewBodySetup->bMeshCollideAll ? TEXT("true") : TEXT("false"));

        // DefaultInstance 信息
        UE_LOG(LogTemp, Log, TEXT("  DefaultInstance.CollisionEnabled: %s"),
            NewBodySetup->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics ? TEXT("QueryAndPhysics") :
            NewBodySetup->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::QueryOnly ? TEXT("QueryOnly") :
            NewBodySetup->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly ? TEXT("PhysicsOnly") :
            TEXT("NoCollision"));
        UE_LOG(LogTemp, Log, TEXT("  DefaultInstance.CollisionProfileName: %s"),
            *NewBodySetup->DefaultInstance.GetCollisionProfileName().ToString());

        // AggGeom 信息
        const FKAggregateGeom& AggGeom = NewBodySetup->AggGeom;
        const int32 TotalElementCount = AggGeom.GetElementCount();
        UE_LOG(LogTemp, Log, TEXT("--- AggGeom 信息 ---"));
        UE_LOG(LogTemp, Log, TEXT("  总元素数量: %d"), TotalElementCount);
        UE_LOG(LogTemp, Log, TEXT("  ConvexElems: %d"), AggGeom.ConvexElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  BoxElems: %d"), AggGeom.BoxElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  SphereElems: %d"), AggGeom.SphereElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  SphylElems: %d"), AggGeom.SphylElems.Num());
        UE_LOG(LogTemp, Log, TEXT("  TaperedCapsuleElems: %d"), AggGeom.TaperedCapsuleElems.Num());

        // === 详细输出 ConvexElems ===
        if (AggGeom.ConvexElems.Num() > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("--- ConvexElems 详细信息 ---"));
            for (int32 ConvexIdx = 0; ConvexIdx < AggGeom.ConvexElems.Num(); ++ConvexIdx)
            {
                const FKConvexElem& ConvexElem = AggGeom.ConvexElems[ConvexIdx];
                UE_LOG(LogTemp, Log, TEXT("  ConvexElem[%d]:"), ConvexIdx);
                UE_LOG(LogTemp, Log, TEXT("    顶点数量 (VertexData): %d"), ConvexElem.VertexData.Num());

                // 输出所有顶点
                for (int32 VertIdx = 0; VertIdx < ConvexElem.VertexData.Num(); ++VertIdx)
                {
                    UE_LOG(LogTemp, Log, TEXT("      Vertex[%d]: %s"), VertIdx, *ConvexElem.VertexData[VertIdx].ToString());
                }

                // 输出 ConvexElem 的其他属性
                UE_LOG(LogTemp, Log, TEXT("    ElemBox: Min=%s, Max=%s"),
                    *ConvexElem.ElemBox.Min.ToString(), *ConvexElem.ElemBox.Max.ToString());
                UE_LOG(LogTemp, Log, TEXT("    Transform: %s"), *ConvexElem.GetTransform().ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("--- ConvexElems 为空！---"));
        }

        // === 输出其他碰撞几何元素（如果有）===
        if (AggGeom.BoxElems.Num() > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("--- BoxElems 详细信息 ---"));
            for (int32 BoxIdx = 0; BoxIdx < AggGeom.BoxElems.Num(); ++BoxIdx)
            {
                const FKBoxElem& BoxElem = AggGeom.BoxElems[BoxIdx];
                UE_LOG(LogTemp, Log, TEXT("  BoxElem[%d]: Center=%s, X=%.2f, Y=%.2f, Z=%.2f"),
                    BoxIdx, *BoxElem.Center.ToString(), BoxElem.X, BoxElem.Y, BoxElem.Z);
            }
        }

        if (AggGeom.SphereElems.Num() > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("--- SphereElems 详细信息 ---"));
            for (int32 SphereIdx = 0; SphereIdx < AggGeom.SphereElems.Num(); ++SphereIdx)
            {
                const FKSphereElem& SphereElem = AggGeom.SphereElems[SphereIdx];
                UE_LOG(LogTemp, Log, TEXT("  SphereElem[%d]: Center=%s, Radius=%.2f"),
                    SphereIdx, *SphereElem.Center.ToString(), SphereElem.Radius);
            }
        }

        if (AggGeom.SphylElems.Num() > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("--- SphylElems 详细信息 ---"));
            for (int32 SphylIdx = 0; SphylIdx < AggGeom.SphylElems.Num(); ++SphylIdx)
            {
                const FKSphylElem& SphylElem = AggGeom.SphylElems[SphylIdx];
                UE_LOG(LogTemp, Log, TEXT("  SphylElem[%d]: Center=%s, Rotation=%s, Radius=%.2f, Length=%.2f"),
                    SphylIdx, *SphylElem.Center.ToString(), *SphylElem.Rotation.ToString(),
                    SphylElem.Radius, SphylElem.Length);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("BodySetup 为空！"));
    }

    UE_LOG(LogTemp, Log, TEXT("========================================"));
    UE_LOG(LogTemp, Log, TEXT("=== UStaticMesh 数据输出结束 ==="));
    UE_LOG(LogTemp, Log, TEXT("========================================"));

    StaticMesh->MarkPackageDirty();

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