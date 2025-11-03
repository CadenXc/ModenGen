#include "ProceduralMeshActor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "PhysicsEngine/BodySetup.h"
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

UStaticMesh* AProceduralMeshActor::ConvertProceduralMeshToStaticMesh()
{
    // 1. 验证输入参数
    if (!ProceduralMeshComponent)
    {
        return nullptr;
    }

    const int32 NumSections = ProceduralMeshComponent->GetNumSections();
    if (NumSections == 0)
    {
        return nullptr;
    }

    // 2. 创建一个完整的UStaticMesh对象
    // RF_Public | RF_Standalone 确保资源可以独立存在
    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
        GetTransientPackage(), NAME_None, RF_Public | RF_Standalone);

    if (!StaticMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ConvertProceduralMeshToStaticMesh: 无法创建StaticMesh对象"));
        return nullptr;
    }

    // 设置StaticMesh的基本属性，确保资源完整
    StaticMesh->SetFlags(RF_Public | RF_Standalone);

    // 确保StaticMesh可以被正确使用
    StaticMesh->NeverStream = false;  // 允许流式加载（如果需要）

    UE_LOG(LogTemp, Log, TEXT("StaticMesh 对象创建成功"));

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

        // 注意：材质会在BuildFromMeshDescriptions时从MeshDescription自动同步到StaticMaterials
        // 这里我们暂时保存材质信息，在BuildFromMeshDescriptions后再添加

        // c. 在MeshDescription中为这个材质段创建一个新的多边形组
        const FPolygonGroupID PolygonGroup = MeshDescBuilder.AppendPolygonGroup();

        // d. 将多边形组与我们刚刚创建的材质槽名称关联起来
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
            // 将FVector4转换为FVector4f
            FVector4f Color4f(ProcVertex.Color.R, ProcVertex.Color.G, ProcVertex.Color.B, ProcVertex.Color.A);
            MeshDescBuilder.SetInstanceColor(InstanceID, Color4f);
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

    // 7. 使用 FMeshDescription 构建 Static Mesh
    TArray<const FMeshDescription*> MeshDescPtrs;
    MeshDescPtrs.Emplace(&MeshDescription);
    UStaticMesh::FBuildMeshDescriptionsParams BuildParams;

    // 根据设置决定碰撞类型
    if (bUseComplexCollision)
    {
        // 使用复杂碰撞：从MeshDescription生成精确的碰撞网格
        BuildParams.bBuildSimpleCollision = false;
        UE_LOG(LogTemp, Log, TEXT("使用复杂碰撞（从网格几何体生成精确碰撞）"));
    }
    else
    {
        // 使用简单碰撞：自动生成的简化碰撞（性能更好）
        BuildParams.bBuildSimpleCollision = true;
        UE_LOG(LogTemp, Log, TEXT("使用简单碰撞（自动生成的简化碰撞）"));
    }

    StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);

    // 在BuildFromMeshDescriptions之后，添加材质到StaticMaterials
    // 需要根据MeshDescription中的材质槽名称来匹配
    {
        TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
        // 清空自动生成的材质槽，添加我们自己的
        StaticMaterials.Empty();

        for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
        {
            const FName MaterialSlotName = FName(*FString::Printf(TEXT("MaterialSlot_%d"), SectionIdx));
            UMaterialInterface* SectionMaterial = ProceduralMeshComponent->GetMaterial(SectionIdx);
            FStaticMaterial NewStaticMaterial(SectionMaterial, MaterialSlotName);
            NewStaticMaterial.UVChannelData.bInitialized = true;
            NewStaticMaterial.UVChannelData.bOverrideDensities = false;

            // 设置默认UV密度
            float UVDensity = 1.0f;
            for (int32 UVIndex = 0; UVIndex < 4; ++UVIndex)
            {
                NewStaticMaterial.UVChannelData.LocalUVDensities[UVIndex] = UVDensity;
            }

            StaticMaterials.Add(NewStaticMaterial);
        }
    }

    // 处理碰撞 - StaticMesh默认开启碰撞
    {
        StaticMesh->CreateBodySetup();
        UBodySetup* NewBodySetup = StaticMesh->GetBodySetup();
        if (NewBodySetup)
        {
            if (bUseComplexCollision)
            {
                // 使用复杂碰撞：使用完整网格几何体作为碰撞（最精确）
                UE_LOG(LogTemp, Log, TEXT("=== 生成复杂碰撞网格 ==="));

                // 设置碰撞复杂度为使用复杂碰撞
                // CTF_UseComplexAsSimple: 使用复杂网格作为简单和复杂碰撞
                // 这将使用完整的渲染网格几何体作为碰撞检测
                NewBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

                // 复杂碰撞使用渲染网格本身，所以不需要额外构建
                // StaticMesh的复杂碰撞网格会在BuildFromMeshDescriptions时自动生成
                // 我们只需要确保CollisionTraceFlag设置正确

                UE_LOG(LogTemp, Log, TEXT("已配置使用复杂碰撞（使用完整网格几何体）"));
                UE_LOG(LogTemp, Log, TEXT("注意：复杂碰撞使用渲染网格，确保网格几何体正确"));
            }
            else
            {
                // 使用简单碰撞
                UE_LOG(LogTemp, Log, TEXT("=== 使用简单碰撞 ==="));

                // 如果PMC有BodySetup，尝试复制其碰撞数据
                if (ProceduralMeshComponent->ProcMeshBodySetup)
                {
                    UBodySetup* SourceBodySetup = ProceduralMeshComponent->ProcMeshBodySetup;
                    const FKAggregateGeom& SourceAggGeom = SourceBodySetup->AggGeom;

                    // 输出源碰撞几何的详细信息
                    UE_LOG(LogTemp, Log, TEXT("ProceduralMeshComponent碰撞信息 - ConvexElems: %d, BoxElems: %d, SphereElems: %d, SphylElems: %d"),
                        SourceAggGeom.ConvexElems.Num(),
                        SourceAggGeom.BoxElems.Num(),
                        SourceAggGeom.SphereElems.Num(),
                        SourceAggGeom.SphylElems.Num());

                    // 如果有复杂碰撞数据（ConvexElems），复制它们
                    if (SourceAggGeom.ConvexElems.Num() > 0)
                    {
                        UE_LOG(LogTemp, Log, TEXT("复制复杂碰撞数据到StaticMesh..."));
                        NewBodySetup->AggGeom.ConvexElems = SourceAggGeom.ConvexElems;
                        UE_LOG(LogTemp, Log, TEXT("已复制 %d 个凸包元素"), SourceAggGeom.ConvexElems.Num());
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("PMC无凸包，依赖BuildFromMeshDescriptions的自动简单碰撞生成"));
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("PMC无BodySetup，依赖自动简单碰撞"));
                }

                // 注意：移除手动凸包生成，因为UE5中FConvexDecompUtil已过时/不可用
                // BuildParams.bBuildSimpleCollision = true 已启用自动简化碰撞（如包围盒或基本凸包）

                NewBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
            }

            // ===== BodySetup 完整配置（根据 UE5 源码） =====
            // 几何和碰撞生成设置
            NewBodySetup->bGenerateMirroredCollision = false;      // 不生成镜像碰撞（节省内存）
            NewBodySetup->bDoubleSidedGeometry = true;            // 双面几何体（支持双面碰撞）
            NewBodySetup->bSupportUVsAndFaceRemap = false;         // 如果不使用物理材质遮罩，可以不启用

            // 物理模拟设置
            NewBodySetup->PhysicsType = EPhysicsType::PhysType_Default;

            // ===== StaticMesh 资源层面的完整碰撞设置 =====
            // BodySetup->DefaultInstance 设置的是 StaticMesh 资源本身的默认碰撞属性
            // 根据源码，这些设置会被所有使用该 StaticMesh 的组件继承（除非组件层面明确覆盖）

            if (bGenerateCollision)
            {
                // 1. 设置碰撞启用状态（根据 BodyInstance.h 第1198行）
                NewBodySetup->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

                // 2. 设置碰撞配置名称（根据 BodyInstance.h 第1233行）
                // BlockAll 碰撞配置包含了所有必要的响应设置
                NewBodySetup->DefaultInstance.SetCollisionProfileName(TEXT("BlockAll"));

                // 3. 设置对象类型（根据 BodyInstance.h 第1192行）
                NewBodySetup->DefaultInstance.SetObjectType(ECollisionChannel::ECC_WorldStatic);

                // 4. 设置所有通道的响应（根据 BodyInstance.h 第1173行）
                // 虽然 BlockAll 配置已经设置了，但这里明确设置确保正确
                // NewBodySetup->DefaultInstance.SetResponseToAllChannels(ECR_Block);  // 注释掉以避免Profile显示为Custom

                // 5. 物理模拟设置（静态网格默认不进行物理模拟）
                NewBodySetup->DefaultInstance.SetEnableGravity(false);    // 静态网格不受重力影响（BodyInstance.h 第1016行）
                NewBodySetup->DefaultInstance.bUseCCD = false;            // 连续碰撞检测（静态网格通常不需要，BodyInstance.h 第1029行）

                // 6. 物理材质（使用默认材质，如果需要可以在组件层面覆盖）
                // NewBodySetup->DefaultInstance.SetPhysMaterialOverride(nullptr);  // nullptr 表示使用默认（BodyInstance.h 第1161行）

                UE_LOG(LogTemp, Log, TEXT("StaticMesh资源层面：完整碰撞配置已设置"));
                UE_LOG(LogTemp, Log, TEXT("  - 碰撞启用: QueryAndPhysics"));
                UE_LOG(LogTemp, Log, TEXT("  - 碰撞配置: BlockAll"));
                UE_LOG(LogTemp, Log, TEXT("  - 对象类型: WorldStatic"));
                UE_LOG(LogTemp, Log, TEXT("  - 响应通道: 阻挡所有 (ECR_Block)"));
                UE_LOG(LogTemp, Log, TEXT("  - 重力: 禁用"));
                UE_LOG(LogTemp, Log, TEXT("  - CCD: 禁用"));
            }
            else
            {
                // 在 StaticMesh 资源层面禁用碰撞
                NewBodySetup->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
                NewBodySetup->DefaultInstance.SetResponseToAllChannels(ECR_Ignore);
                UE_LOG(LogTemp, Log, TEXT("StaticMesh资源层面：碰撞已禁用"));
            }

            // 标记 BodySetup 需要重新构建
            NewBodySetup->InvalidatePhysicsData();

            // ===== 创建物理网格 =====
            // 这是关键步骤，确保碰撞数据被正确处理和烘焙
            UE_LOG(LogTemp, Log, TEXT("正在创建物理网格..."));
            NewBodySetup->CreatePhysicsMeshes();

            // ===== 验证物理网格创建结果 =====
            const int32 AggGeomElementCount = NewBodySetup->AggGeom.GetElementCount();
            UE_LOG(LogTemp, Log, TEXT("物理网格验证 - AggGeom元素数量: %d, 追踪标志: %d"),
                AggGeomElementCount, (int32)NewBodySetup->CollisionTraceFlag);

            if (AggGeomElementCount > 0)
            {
                // 输出详细的碰撞几何信息
                const FKAggregateGeom& AggGeom = NewBodySetup->AggGeom;
                UE_LOG(LogTemp, Log, TEXT("物理网格详细信息:"));
                UE_LOG(LogTemp, Log, TEXT("  - ConvexElems: %d"), AggGeom.ConvexElems.Num());
                UE_LOG(LogTemp, Log, TEXT("  - BoxElems: %d"), AggGeom.BoxElems.Num());
                UE_LOG(LogTemp, Log, TEXT("  - SphereElems: %d"), AggGeom.SphereElems.Num());
                UE_LOG(LogTemp, Log, TEXT("  - SphylElems: %d"), AggGeom.SphylElems.Num());
                UE_LOG(LogTemp, Log, TEXT("  - TaperedCapsuleElems: %d"), AggGeom.TaperedCapsuleElems.Num());
                UE_LOG(LogTemp, Log, TEXT("物理网格创建成功！"));
            }
            else
            {
                // 如果是复杂碰撞，可能使用渲染网格，所以AggGeom可能为空
                if (bUseComplexCollision && NewBodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple)
                {
                    UE_LOG(LogTemp, Log, TEXT("使用复杂碰撞模式 - 将使用渲染网格作为碰撞（AggGeom为空是正常的）"));
                    UE_LOG(LogTemp, Log, TEXT("这是预期的行为，复杂碰撞直接使用渲染网格几何体"));
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("物理网格数据为空！可能的问题："));
                    UE_LOG(LogTemp, Warning, TEXT("  - 网格几何体可能无效"));
                    UE_LOG(LogTemp, Warning, TEXT("  - BuildFromMeshDescriptions 可能没有正确生成碰撞"));
                    UE_LOG(LogTemp, Warning, TEXT("  - 请检查网格数据和碰撞设置"));
                    UE_LOG(LogTemp, Warning, TEXT("  - 建议在编辑器中手动添加UCX碰撞以改善简单碰撞"));
                }
            }

            // ===== 最终验证 =====
            UE_LOG(LogTemp, Log, TEXT("=== StaticMesh BodySetup 配置总结 ==="));
            UE_LOG(LogTemp, Log, TEXT("碰撞类型: %s"), bUseComplexCollision ? TEXT("复杂碰撞") : TEXT("简单碰撞"));
            UE_LOG(LogTemp, Log, TEXT("碰撞追踪标志: %d (%s)"),
                (int32)NewBodySetup->CollisionTraceFlag,
                NewBodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple ? TEXT("UseComplexAsSimple") :
                NewBodySetup->CollisionTraceFlag == CTF_UseSimpleAsComplex ? TEXT("UseSimpleAsComplex") :
                TEXT("UseDefault"));
            UE_LOG(LogTemp, Log, TEXT("双面几何: %s"), NewBodySetup->bDoubleSidedGeometry ? TEXT("是") : TEXT("否"));
            UE_LOG(LogTemp, Log, TEXT("镜像碰撞: %s"), NewBodySetup->bGenerateMirroredCollision ? TEXT("生成") : TEXT("不生成"));
            UE_LOG(LogTemp, Log, TEXT("默认碰撞响应: %s"),
                NewBodySetup->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics ? TEXT("QueryAndPhysics") :
                NewBodySetup->DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::QueryOnly ? TEXT("QueryOnly") :
                TEXT("NoCollision"));
            UE_LOG(LogTemp, Log, TEXT("StaticMesh BodySetup 配置完成！"));

            // ===== 新增：PostEditChange 以触发编辑器更新 =====
            StaticMesh->PostEditChange();  // 确保资产更新，包括LOD/碰撞缓存
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取StaticMesh的BodySetup"));
        }
    }

    // ===== 新增：输出StaticMesh数据总结（在返回前） =====
    if (StaticMesh && StaticMesh->IsValidLowLevel())
    {
        int32 NumVertices = 0;
        int32 NumTriangles = 0;
        const int32 MaterialsNum = StaticMesh->GetStaticMaterials().Num();
        int32 AggGeomCount = 0;
        int32 TraceFlag = 0;
        if (UBodySetup* BodySetup = StaticMesh->GetBodySetup())
        {
            AggGeomCount = BodySetup->AggGeom.GetElementCount();
            TraceFlag = (int32)BodySetup->CollisionTraceFlag;
        }

        if (StaticMesh->GetRenderData() && StaticMesh->GetRenderData()->LODResources.Num() > 0)
        {
            const FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
            NumVertices = StaticMesh->GetNumVertices(0);
            NumTriangles = StaticMesh->GetNumTriangles(0);
        }

        UE_LOG(LogTemp, Log, TEXT("=== StaticMesh 数据总结（返回前） ==="));
        UE_LOG(LogTemp, Log, TEXT("  - 顶点数: %d"), NumVertices);
        UE_LOG(LogTemp, Log, TEXT("  - 三角形数: %d"), NumTriangles);
        UE_LOG(LogTemp, Log, TEXT("  - 材质槽数: %d"), MaterialsNum);
        UE_LOG(LogTemp, Log, TEXT("  - AggGeom元素数: %d"), AggGeomCount);
        UE_LOG(LogTemp, Log, TEXT("  - 碰撞追踪标志: %d (%s)"),
            TraceFlag,
            TraceFlag == CTF_UseComplexAsSimple ? TEXT("UseComplexAsSimple") :
            TraceFlag == CTF_UseSimpleAsComplex ? TEXT("UseSimpleAsComplex") :
            TEXT("UseDefault"));
        UE_LOG(LogTemp, Log, TEXT("StaticMesh 数据输出完成！"));
    }

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
        if (ConvertedMesh->GetBodySetup())
        {
            // 强制组件从资源继承（默认行为，但显式调用确保）
            StaticMeshComponent->SetCollisionEnabled(ConvertedMesh->GetBodySetup()->DefaultInstance.GetCollisionEnabled());
            StaticMeshComponent->SetCollisionObjectType(ConvertedMesh->GetBodySetup()->DefaultInstance.GetObjectType());
            StaticMeshComponent->SetCollisionProfileName(ConvertedMesh->GetBodySetup()->DefaultInstance.GetCollisionProfileName());

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

            const FBodyInstance& DefaultInstance = ConvertedMesh->GetBodySetup()->DefaultInstance;
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
        UE_LOG(LogTemp, Log, TEXT("StaticMesh 材质槽数量: %d"), ConvertedMesh->GetStaticMaterials().Num());
        UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent 可见性: %s"), bShowStaticMeshComponent ? TEXT("可见") : TEXT("隐藏"));
        UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent 是否附加: %s"), StaticMeshComponent->GetAttachParent() ? TEXT("是") : TEXT("否"));

        // 检查StaticMesh的碰撞设置
        if (ConvertedMesh->GetBodySetup())
        {
            UE_LOG(LogTemp, Log, TEXT("StaticMesh BodySetup 存在，碰撞追踪标志: %d"),
                (int32)ConvertedMesh->GetBodySetup()->CollisionTraceFlag);
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