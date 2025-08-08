// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrism.cpp
 * @brief 可配置的空心棱柱生成器的实现
 */

#include "HollowPrism.h"

// Engine includes
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

// 日志分类定义
DEFINE_LOG_CATEGORY_STATIC(LogHollowPrism, Log, All);

// ============================================================================
// FMeshSection Implementation
// 网格数据结构的实现
// ============================================================================

/**
 * 清除所有网格数据
 * 在重新生成几何体或清理资源时调用
 */
void FMeshSection::Clear()
{
    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UVs.Reset();
    VertexColors.Reset();
    Tangents.Reset();
}

/**
 * 预分配内存以提高性能
 * 
 * @param VertexCount - 预估的顶点数量
 * @param TriangleCount - 预估的三角形数量
 */
void FMeshSection::Reserve(int32 VertexCount, int32 TriangleCount)
{
    Vertices.Reserve(VertexCount);
    Triangles.Reserve(TriangleCount);
    Normals.Reserve(VertexCount);
    UVs.Reserve(VertexCount);
    VertexColors.Reserve(VertexCount);
    Tangents.Reserve(VertexCount);
}

/**
 * 验证网格数据的完整性
 * 
 * @return bool - 如果所有必要的网格数据都存在且数量匹配则返回true
 */
bool FMeshSection::IsValid() const
{
    // 检查是否有基本的顶点和三角形数据
    const bool bHasBasicGeometry = Vertices.Num() > 0 && Triangles.Num() > 0;
    
    // 检查三角形索引是否为3的倍数（每个三角形3个顶点）
    const bool bValidTriangleCount = Triangles.Num() % 3 == 0;
    
    // 检查所有顶点属性数组的大小是否匹配
    const bool bMatchingArraySizes = Normals.Num() == Vertices.Num() &&
                                   UVs.Num() == Vertices.Num() &&
                                   Tangents.Num() == Vertices.Num();
    
    return bHasBasicGeometry && bValidTriangleCount && bMatchingArraySizes;
}

// ============================================================================
// FPrismParameters Implementation
// 生成参数结构体的实现
// ============================================================================

/**
 * 验证生成参数的有效性
 * 
 * @return bool - 如果所有参数都在有效范围内则返回true
 */
bool FPrismParameters::IsValid() const
{
    // 检查基础尺寸是否有效
    const bool bValidInnerRadius = InnerRadius > 0.0f;
    const bool bValidOuterRadius = OuterRadius > 0.0f;
    const bool bValidHeight = Height > 0.0f;
    
    // 检查边数是否在有效范围内
    const bool bValidSides = Sides >= 3 && Sides <= 100;
    
    // 检查弧角是否在有效范围内
    const bool bValidArcAngle = ArcAngle >= 0.0f && ArcAngle <= 360.0f;
    
    // 检查倒角参数是否有效
    const bool bValidChamferRadius = BevelRadius >= 0.0f;
    const bool bValidChamferSections = BevelSections >= 1 && BevelSections <= 20;
    
    // 检查外半径是否大于内半径
    const bool bValidRadiusRelationship = OuterRadius > InnerRadius;
    
    return bValidInnerRadius && bValidOuterRadius && bValidHeight && 
           bValidSides && bValidArcAngle && bValidChamferRadius && 
           bValidChamferSections && bValidRadiusRelationship;
}

/**
 * 获取壁厚
 * 
 * @return float - 空心棱柱的壁厚
 */
float FPrismParameters::GetWallThickness() const
{
    return OuterRadius - InnerRadius;
}

/**
 * 检查是否为完整圆环
 * 
 * @return bool - 如果弧角接近360度则返回true
 */
bool FPrismParameters::IsFullCircle() const
{
    return ArcAngle >= 360.0f - KINDA_SMALL_NUMBER;
}

// ============================================================================
// AHollowPrism Implementation
// 空心棱柱Actor的实现
// ============================================================================

/**
 * 构造函数
 * 初始化组件和默认设置
 */
AHollowPrism::AHollowPrism()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建程序化网格组件
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PrismMesh"));
    RootComponent = MeshComponent;

    // 配置网格组件
    MeshComponent->bUseAsyncCooking = true;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetSimulatePhysics(false);

    // 初始生成几何体
    GenerateGeometry();
}

/**
 * 游戏开始时调用
 * 确保几何体已正确生成
 */
void AHollowPrism::BeginPlay()
{
    Super::BeginPlay();
    GenerateGeometry();
}

#if WITH_EDITOR
/**
 * 编辑器属性变更时调用
 * 实时更新几何体以反映参数变化
 * 
 * @param PropertyChangedEvent - 属性变更事件
 */
void AHollowPrism::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    
    // 定义需要触发重新生成的属性
    static const TArray<FName> RelevantProperties = {
        "InnerRadius", "OuterRadius", "Height",
        "Sides", "ArcAngle", "bUseTriangleMethod",
        "BevelRadius", "BevelSections"
    };

    if (RelevantProperties.Contains(PropertyName))
    {
        UE_LOG(LogHollowPrism, Verbose, TEXT("属性变更：%s，重新生成几何体"), *PropertyName.ToString());
        GenerateGeometry();
    }
}
#endif

/**
 * 重新生成空心棱柱几何体
 * 蓝图可调用的公共接口
 */
void AHollowPrism::Regenerate()
{
    UE_LOG(LogHollowPrism, Log, TEXT("手动重新生成空心棱柱几何体"));
    GenerateGeometry();
}

/**
 * 生成空心棱柱的几何体
 * 主要的几何体生成函数，协调各个部分的生成
 */
void AHollowPrism::GenerateGeometry()
{
    // 验证网格组件
    if (!ensureMsgf(MeshComponent, TEXT("网格组件缺失！")))
    {
        UE_LOG(LogHollowPrism, Error, TEXT("网格组件缺失，无法生成几何体"));
        return;
    }

    // 清除之前的网格数据
    MeshComponent->ClearAllMeshSections();

    // 验证和修正参数
    if (!ValidateAndClampParameters())
    {
        UE_LOG(LogHollowPrism, Error, TEXT("参数验证失败，使用默认值"));
    }

    // 创建网格数据容器
    FMeshSection MeshData;

    // 估算顶点和三角形数量
    const int32 VertexCountEstimate = CalculateVertexCountEstimate();
    const int32 TriangleCountEstimate = CalculateTriangleCountEstimate();

    // 预分配内存以提高性能
    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate * 3);

    UE_LOG(LogHollowPrism, Verbose, TEXT("开始生成空心棱柱几何体：顶点预估=%d，三角形预估=%d"), 
           VertexCountEstimate, TriangleCountEstimate);

    // 生成各个几何部分
    GenerateSideWalls(MeshData);
    GenerateTopCapWithQuads(MeshData);
    GenerateBottomCapWithQuads(MeshData);

    // 生成倒角几何（如果启用）
    if (Parameters.BevelRadius > 0.0f)
    {
        const float HalfHeight = Parameters.Height / 2.0f;
        
        UE_LOG(LogHollowPrism, Verbose, TEXT("生成倒角几何：半径=%.2f，分段数=%d"), 
               Parameters.BevelRadius, Parameters.BevelSections);
        
        // 分别生成内外倒角
        GenerateTopInnerBevel(MeshData, HalfHeight);
        GenerateTopOuterBevel(MeshData, HalfHeight);
        GenerateBottomInnerBevel(MeshData, HalfHeight);
        GenerateBottomOuterBevel(MeshData, HalfHeight);
    }

    // 生成端盖（如果不是完整圆环）
    if (!Parameters.IsFullCircle())
    {
        GenerateEndCaps(MeshData);
        UE_LOG(LogHollowPrism, Verbose, TEXT("生成端盖：弧角=%.2f"), Parameters.ArcAngle);
    }

    // 验证生成的几何体
    if (!MeshData.IsValid())
    {
        UE_LOG(LogHollowPrism, Error, TEXT("生成的几何体无效"));
        return;
    }

    // 更新网格组件
    UpdateMeshComponent(MeshData);

    UE_LOG(LogHollowPrism, Log, TEXT("空心棱柱生成完成：顶点=%d，三角形=%d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

/**
 * 验证和修正参数
 * 
 * @return bool - 如果参数有效则返回true
 */
bool AHollowPrism::ValidateAndClampParameters()
{
    // 修正基础参数
    Parameters.InnerRadius = FMath::Max(0.01f, Parameters.InnerRadius);
    Parameters.OuterRadius = FMath::Max(0.01f, Parameters.OuterRadius);
    Parameters.Height = FMath::Max(0.01f, Parameters.Height);
    Parameters.Sides = FMath::Clamp(Parameters.Sides, 3, 100);
    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);
    Parameters.BevelRadius = FMath::Max(0.0f, Parameters.BevelRadius);
    Parameters.BevelSections = FMath::Clamp(Parameters.BevelSections, 1, 20);

    // 确保外半径大于内半径
    if (Parameters.OuterRadius <= Parameters.InnerRadius)
    {
        Parameters.OuterRadius = Parameters.InnerRadius + 1.0f;
        UE_LOG(LogHollowPrism, Warning, TEXT("外半径已自动调整为：%.2f"), Parameters.OuterRadius);
    }

    return Parameters.IsValid();
}

/**
 * 计算预估的顶点数量
 * 
 * @return int32 - 预估的顶点数量
 */
int32 AHollowPrism::CalculateVertexCountEstimate() const
{
    int32 VertexCount = 0;
    
    // 侧面顶点（内外各4个顶点）
    VertexCount += Parameters.Sides * 8;
    
    // 顶面/底面顶点（内外各2个顶点）
    VertexCount += Parameters.Sides * 4;
    
    // 倒角顶点（如果启用）
    if (Parameters.BevelRadius > 0.0f)
    {
        VertexCount += Parameters.Sides * Parameters.BevelSections * 8; // 内外各4个顶点
    }
    
    // 端盖顶点（如果不是完整圆环）
    if (!Parameters.IsFullCircle())
    {
        VertexCount += 8; // 端盖的顶点数量
    }
    
    return VertexCount;
}

/**
 * 计算预估的三角形数量
 * 
 * @return int32 - 预估的三角形数量
 */
int32 AHollowPrism::CalculateTriangleCountEstimate() const
{
    int32 TriangleCount = 0;
    
    // 侧面三角形
    TriangleCount += Parameters.Sides * 8;
    
    // 顶面/底面三角形
    TriangleCount += Parameters.Sides * 4;
    
    // 倒角三角形（如果启用）
    if (Parameters.BevelRadius > 0.0f)
    {
        TriangleCount += Parameters.Sides * Parameters.BevelSections * 8;
    }
    
    // 端盖三角形（如果不是完整圆环）
    if (!Parameters.IsFullCircle())
    {
        TriangleCount += 8;
    }
    
    return TriangleCount;
}

/**
 * 更新网格组件
 * 
 * @param MeshData - 要应用的网格数据
 */
void AHollowPrism::UpdateMeshComponent(const FMeshSection& MeshData)
{
    if (!ensureMsgf(MeshComponent, TEXT("网格组件缺失")))
    {
        return;
    }

    // 创建网格段
    MeshComponent->CreateMeshSection_LinearColor(0, MeshData.Vertices, MeshData.Triangles, 
        MeshData.Normals, MeshData.UVs, MeshData.VertexColors, 
        MeshData.Tangents, true);

    // 应用默认材质
    static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (DefaultMaterial.Succeeded())
    {
        MeshComponent->SetMaterial(0, DefaultMaterial.Object);
    }
    else
    {
        UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        if (FallbackMaterial)
        {
            MeshComponent->SetMaterial(0, FallbackMaterial);
        }
    }

    UE_LOG(LogHollowPrism, Verbose, TEXT("网格组件更新完成：顶点=%d，三角形=%d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}





/**
 * 生成侧面墙壁
 * 创建空心棱柱的侧面几何体，包括内外环之间的墙壁
 * 
 * @param Section - 目标网格段
 */
void AHollowPrism::GenerateSideWalls(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点（顶部和底部）
    TArray<FVector> InnerTopVertices, InnerBottomVertices;
    TArray<FVector2D> InnerTopUVs, InnerBottomUVs;
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, HalfHeight - Parameters.BevelRadius, ArcAngle, InnerTopVertices, InnerTopUVs);
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, -HalfHeight + Parameters.BevelRadius, ArcAngle, InnerBottomVertices, InnerBottomUVs);

    // 生成外环顶点（顶部和底部）
    TArray<FVector> OuterTopVertices, OuterBottomVertices;
    TArray<FVector2D> OuterTopUVs, OuterBottomUVs;
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, HalfHeight - Parameters.BevelRadius, ArcAngle, OuterTopVertices, OuterTopUVs, 0.5f);
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, -HalfHeight + Parameters.BevelRadius, ArcAngle, OuterBottomVertices, OuterBottomUVs, 0.5f);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerTopIndices, InnerBottomIndices;
    TArray<int32> OuterTopIndices, OuterBottomIndices;

    // 内环顶点
    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        const FVector RadialNormal = (InnerTopVertices[i] - FVector(0, 0, InnerTopVertices[i].Z)).GetSafeNormal();
        InnerTopIndices.Add(AddVertex(Section, InnerTopVertices[i], -RadialNormal, InnerTopUVs[i]));
        InnerBottomIndices.Add(AddVertex(Section, InnerBottomVertices[i], -RadialNormal, InnerBottomUVs[i]));
    }

    // 外环顶点
    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        const FVector RadialNormal = (OuterTopVertices[i] - FVector(0, 0, OuterTopVertices[i].Z)).GetSafeNormal();
        OuterTopIndices.Add(AddVertex(Section, OuterTopVertices[i], RadialNormal, OuterTopUVs[i]));
        OuterBottomIndices.Add(AddVertex(Section, OuterBottomVertices[i], RadialNormal, OuterBottomUVs[i]));
    }

    // 生成内侧面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            InnerTopIndices[i],
            InnerBottomIndices[i],
            InnerBottomIndices[i + 1],
            InnerTopIndices[i + 1]
        );
    }

    // 生成外侧面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            OuterTopIndices[i],
            OuterTopIndices[i + 1],
            OuterBottomIndices[i + 1],
            OuterBottomIndices[i]
        );
    }
}



// 生成顶面
void AHollowPrism::GenerateTopCapWithQuads(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(Parameters.InnerRadius + Parameters.BevelRadius, Parameters.Sides, HalfHeight, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(Parameters.OuterRadius - Parameters.BevelRadius, Parameters.Sides, HalfHeight, ArcAngle, OuterVertices, OuterUVs);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerIndices, OuterIndices;
    const FVector TopNormal(0, 0, 1);

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (InnerVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (InnerVertices[i].Y / Parameters.OuterRadius)
        );
        InnerIndices.Add(AddVertex(Section, InnerVertices[i], TopNormal, UV));
    }

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (OuterVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (OuterVertices[i].Y / Parameters.OuterRadius)
        );
        OuterIndices.Add(AddVertex(Section, OuterVertices[i], TopNormal, UV));
    }

    // 连接内外环形成顶面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            InnerIndices[i],
            InnerIndices[i + 1],
            OuterIndices[i + 1],
            OuterIndices[i]
        );
    }
}

// 生成底面
void AHollowPrism::GenerateBottomCapWithQuads(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(Parameters.InnerRadius + Parameters.BevelRadius, Parameters.Sides, -HalfHeight, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(Parameters.OuterRadius - Parameters.BevelRadius, Parameters.Sides, -HalfHeight, ArcAngle, OuterVertices, OuterUVs);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerIndices, OuterIndices;
    const FVector BottomNormal(0, 0, -1);

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (InnerVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (InnerVertices[i].Y / Parameters.OuterRadius)
        );
        InnerIndices.Add(AddVertex(Section, InnerVertices[i], BottomNormal, UV));
    }

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (OuterVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (OuterVertices[i].Y / Parameters.OuterRadius)
        );
        OuterIndices.Add(AddVertex(Section, OuterVertices[i], BottomNormal, UV));
    }

    // 连接内外环形成底面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            OuterIndices[i],
            OuterIndices[i + 1],
            InnerIndices[i + 1],
            InnerIndices[i]
        );
    }
}



// 生成顶部内环倒角
void AHollowPrism::GenerateTopInnerBevel(FMeshSection& Section, float HalfHeight)
{
    const float ChamferRadius = Parameters.BevelRadius;
    const int32 ChamferSections = Parameters.BevelSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 内环倒角：从内半径开始，向外扩展
        const float StartRadius = Parameters.InnerRadius;
        const float EndRadius = Parameters.InnerRadius + ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(HalfHeight - ChamferRadius, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            // 使用与CalculateRingVertices相同的角度计算方式
            const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Parameters.Sides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 内环法线：指向内部（负径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector TopNormal = FVector(0, 0, 1.0f);
            FVector Normal = FMath::Lerp(-RadialDirection, TopNormal, alpha).GetSafeNormal();

            // 确保法线指向内部
            if (FVector::DotProduct(Normal, RadialDirection) > 0)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (CurrentZ + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(Section, V00, V01, V11, V10);
            }
        }

        PrevRing = CurrentRing;
    }
}

// 生成顶部外环倒角
void AHollowPrism::GenerateTopOuterBevel(FMeshSection& Section, float HalfHeight)
{
    const float ChamferRadius = Parameters.BevelRadius;
    const int32 ChamferSections = Parameters.BevelSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 外环倒角：从外半径开始，向内收缩
        const float StartRadius = Parameters.OuterRadius;
        const float EndRadius = Parameters.OuterRadius - ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(HalfHeight - ChamferRadius, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            // 使用与CalculateRingVertices相同的角度计算方式
            const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Parameters.Sides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 外环法线：指向外部（正径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector TopNormal = FVector(0, 0, 1.0f);
            FVector Normal = FMath::Lerp(RadialDirection, TopNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            if (FVector::DotProduct(Normal, RadialDirection) < 0)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (CurrentZ + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(Section, V00, V10, V11, V01);
            }
        }

        PrevRing = CurrentRing;
    }
}

// 生成底部内环倒角
void AHollowPrism::GenerateBottomInnerBevel(FMeshSection& Section, float HalfHeight)
{
    const float ChamferRadius = Parameters.BevelRadius;
    const int32 ChamferSections = Parameters.BevelSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 内环倒角：从内半径开始，向外扩展
        const float StartRadius = Parameters.InnerRadius;
        const float EndRadius = Parameters.InnerRadius + ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(-HalfHeight + ChamferRadius, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            // 使用与CalculateRingVertices相同的角度计算方式
            const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Parameters.Sides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 内环法线：指向内部（负径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector BottomNormal = FVector(0, 0, -1.0f);
            FVector Normal = FMath::Lerp(-RadialDirection, BottomNormal, alpha).GetSafeNormal();

            // 确保法线指向内部
            if (FVector::DotProduct(Normal, RadialDirection) > 0)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (CurrentZ + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(Section, V00, V10, V11, V01);
            }
        }

        PrevRing = CurrentRing;
    }
}

// 生成底部外环倒角
void AHollowPrism::GenerateBottomOuterBevel(FMeshSection& Section, float HalfHeight)
{
    const float ChamferRadius = Parameters.BevelRadius;
    const int32 ChamferSections = Parameters.BevelSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 外环倒角：从外半径开始，向内收缩
        const float StartRadius = Parameters.OuterRadius;
        const float EndRadius = Parameters.OuterRadius - ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(-HalfHeight + ChamferRadius, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            // 使用与CalculateRingVertices相同的角度计算方式
            const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Parameters.Sides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 外环法线：指向外部（正径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector BottomNormal = FVector(0, 0, -1.0f);
            FVector Normal = FMath::Lerp(RadialDirection, BottomNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            if (FVector::DotProduct(Normal, RadialDirection) < 0)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (CurrentZ + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(Section, V00, V01, V11, V10);
            }
        }

        PrevRing = CurrentRing;
    }
}

// 生成端盖
void AHollowPrism::GenerateEndCaps(FMeshSection& Section)
{
    if (Parameters.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
    {
        return; // 完整环形，不需要端盖
    }

    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float EndAngle = ArcAngleRadians / 2.0f;

    // 生成起始端盖和结束端盖
    GenerateEndCap(Section, StartAngle, FVector(-1, 0, 0), true);   // 起始端盖
    GenerateEndCap(Section, EndAngle, FVector(1, 0, 0), false);      // 结束端盖
}

void AHollowPrism::GenerateEndCap(FMeshSection& Section, float Angle, const FVector& Normal, bool IsStart)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.BevelRadius;

    // 计算倒角高度
    const float TopChamferHeight = FMath::Min(ChamferRadius, Parameters.OuterRadius - Parameters.InnerRadius);
    const float BottomChamferHeight = FMath::Min(ChamferRadius, Parameters.OuterRadius - Parameters.InnerRadius);

    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomChamferHeight;
    const float EndZ = HalfHeight - TopChamferHeight;

    // 按照指定顺序存储顶点：上底中心 -> 上倒角圆弧 -> 侧边 -> 下倒角圆弧 -> 下底中心
    TArray<int32> OrderedVertices;
    
    // 1. 上底中心顶点
    const int32 TopCenterVertex = AddVertex(Section,
        FVector(0, 0, HalfHeight),
        Normal,
        FVector2D(0.5f, 1.0f)
    );
    OrderedVertices.Add(TopCenterVertex);

    // 2. 上倒角弧线顶点（从顶面到侧边）
    if (Parameters.BevelRadius > 0.0f)
    {
        const int32 TopChamferSections = Parameters.BevelSections;
        for (int32 i = 0; i < TopChamferSections; ++i) // 从顶面到侧边
        {
            const float Alpha = static_cast<float>(i) / TopChamferSections;
            
            // 从顶面开始（HalfHeight位置）到侧边（EndZ位置）
            const float CurrentZ = FMath::Lerp(HalfHeight, EndZ, Alpha);
            
            // 从顶面半径到侧边半径（减去倒角半径，与主体几何体保持一致）
            const float TopInnerRadius = Parameters.InnerRadius + Parameters.BevelRadius;
            const float TopOuterRadius = Parameters.OuterRadius - Parameters.BevelRadius;
            const float SideInnerRadius = Parameters.InnerRadius;
            const float SideOuterRadius = Parameters.OuterRadius;
            
            const float CurrentInnerRadius = FMath::Lerp(TopInnerRadius, SideInnerRadius, Alpha);
            const float CurrentOuterRadius = FMath::Lerp(TopOuterRadius, SideOuterRadius, Alpha);
            
            // 内环顶点
            const FVector InnerChamferPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
            const int32 InnerChamferVertex = AddVertex(Section, InnerChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height));
            OrderedVertices.Add(InnerChamferVertex);
            
            // 外环顶点
            const FVector OuterChamferPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
            const int32 OuterChamferVertex = AddVertex(Section, OuterChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height));
            OrderedVertices.Add(OuterChamferVertex);
        }
    }

    // 3. 侧边顶点（从上到下）
    for (int32 h = 0; h <= 4; ++h) // 使用4个分段
    {
        const float Z = FMath::Lerp(EndZ, StartZ, static_cast<float>(h) / 4.0f);

        // 内环顶点
        const FVector InnerEdgePos = FVector(Parameters.InnerRadius * FMath::Cos(Angle), Parameters.InnerRadius * FMath::Sin(Angle), Z);
        const int32 InnerEdgeVertex = AddVertex(Section, InnerEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (Z + HalfHeight) / Parameters.Height));
        OrderedVertices.Add(InnerEdgeVertex);
        
        // 外环顶点
        const FVector OuterEdgePos = FVector(Parameters.OuterRadius * FMath::Cos(Angle), Parameters.OuterRadius * FMath::Sin(Angle), Z);
        const int32 OuterEdgeVertex = AddVertex(Section, OuterEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (Z + HalfHeight) / Parameters.Height));
        OrderedVertices.Add(OuterEdgeVertex);
    }

    // 4. 下倒角弧线顶点（从侧边到底面）
    if (Parameters.BevelRadius > 0.0f)
    {
        const int32 BottomChamferSections = Parameters.BevelSections;
        for (int32 i = 0; i < BottomChamferSections; ++i) // 从侧边到底面
        {
            const float Alpha = static_cast<float>(i) / BottomChamferSections;
            
            // 从侧边开始（StartZ位置）到底面（-HalfHeight位置）
            const float CurrentZ = FMath::Lerp(StartZ, -HalfHeight, Alpha);
            
            // 从侧边半径到底面半径（减去倒角半径，与主体几何体保持一致）
            const float SideInnerRadius = Parameters.InnerRadius;
            const float SideOuterRadius = Parameters.OuterRadius;
            const float BottomInnerRadius = Parameters.InnerRadius + Parameters.BevelRadius;
            const float BottomOuterRadius = Parameters.OuterRadius - Parameters.BevelRadius;
            
            const float CurrentInnerRadius = FMath::Lerp(SideInnerRadius, BottomInnerRadius, Alpha);
            const float CurrentOuterRadius = FMath::Lerp(SideOuterRadius, BottomOuterRadius, Alpha);
            
            // 内环顶点
            const FVector InnerChamferPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
            const int32 InnerChamferVertex = AddVertex(Section, InnerChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height));
            OrderedVertices.Add(InnerChamferVertex);
            
            // 外环顶点
            const FVector OuterChamferPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
            const int32 OuterChamferVertex = AddVertex(Section, OuterChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height));
            OrderedVertices.Add(OuterChamferVertex);
        }
    }

    // 5. 下底中心顶点
    const int32 BottomCenterVertex = AddVertex(Section,
        FVector(0, 0, -HalfHeight),
        Normal,
        FVector2D(0.5f, 0.0f)
    );
    OrderedVertices.Add(BottomCenterVertex);

    // 生成端盖三角形
    for (int32 i = 0; i < OrderedVertices.Num() - 2; i += 2)
    {
        // 连接相邻的顶点对，形成三角形
        if (IsStart)
        {
			AddTriangle(Section, OrderedVertices[i], OrderedVertices[i + 2], OrderedVertices[i + 1]);
			AddTriangle(Section, OrderedVertices[i + 1], OrderedVertices[i + 2], OrderedVertices[i + 3]);
        }
        else
        {
			AddTriangle(Section, OrderedVertices[i], OrderedVertices[i + 1], OrderedVertices[i + 2]);
			AddTriangle(Section, OrderedVertices[i + 1], OrderedVertices[i + 3], OrderedVertices[i + 2]);
        }
    }
}

/**
 * 计算环形顶点
 * 在指定半径和高度上生成环形顶点，用于构建空心棱柱的各个部分
 * 
 * @param Radius - 环形半径
 * @param Sides - 边数
 * @param Z - Z轴高度
 * @param ArcAngle - 弧角（度）
 * @param OutVertices - 输出的顶点数组
 * @param OutUVs - 输出的UV坐标数组
 * @param UVScale - UV缩放因子
 */
void AHollowPrism::CalculateRingVertices(float Radius, int32 Sides, float Z, float ArcAngle,
                                        TArray<FVector>& OutVertices, TArray<FVector2D>& OutUVs, float UVScale)
{
    OutVertices.Empty();
    OutUVs.Empty();

    const float ArcAngleRadians = FMath::DegreesToRadians(ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Sides;

    for (int32 i = 0; i <= Sides; i++)
    {
        const float angle = StartAngle + i * AngleStep;
        const FVector Vertex = FVector(
            Radius * FMath::Cos(angle),
            Radius * FMath::Sin(angle),
            Z
        );
        OutVertices.Add(Vertex);

        // UV映射
        const float U = (static_cast<float>(i) / Sides) * UVScale;
        const float V = (Z + Parameters.Height / 2.0f) / Parameters.Height;
        OutUVs.Add(FVector2D(U, V));
    }
}

/**
 * 添加顶点到网格
 * 
 * @param Section - 目标网格段
 * @param Position - 顶点位置
 * @param Normal - 顶点法线
 * @param UV - 顶点UV坐标
 * @return int32 - 新顶点的索引
 */
int32 AHollowPrism::AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV)
{
    // 验证输入参数
    if (!ensureMsgf(!Position.ContainsNaN(), TEXT("顶点位置包含NaN值")))
    {
        return INDEX_NONE;
    }

    if (!ensureMsgf(!Normal.ContainsNaN(), TEXT("顶点法线包含NaN值")))
    {
        return INDEX_NONE;
    }

    if (!ensureMsgf(!UV.ContainsNaN(), TEXT("顶点UV包含NaN值")))
    {
        return INDEX_NONE;
    }

    const int32 VertexIndex = Section.Vertices.Num();
    
    // 添加顶点数据
    Section.Vertices.Add(Position);
    Section.Normals.Add(Normal);
    Section.UVs.Add(UV);
    Section.VertexColors.Add(FLinearColor::White);
    Section.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
    
    return VertexIndex;
}

/**
 * 添加四边形到网格
 * 将四边形分解为两个三角形
 * 
 * @param Section - 目标网格段
 * @param V1,V2,V3,V4 - 四边形的四个顶点索引（按逆时针顺序）
 */
void AHollowPrism::AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4)
{
    // 验证顶点索引
    if (!ensureMsgf(V1 >= 0 && V2 >= 0 && V3 >= 0 && V4 >= 0, TEXT("四边形顶点索引无效")))
    {
        return;
    }

    if (!ensureMsgf(V1 < Section.Vertices.Num() && V2 < Section.Vertices.Num() && 
                    V3 < Section.Vertices.Num() && V4 < Section.Vertices.Num(),
                    TEXT("四边形顶点索引超出范围")))
    {
        return;
    }

    // 第一个三角形
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V2);
    Section.Triangles.Add(V3);

    // 第二个三角形
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V3);
    Section.Triangles.Add(V4);
}

/**
 * 添加三角形到网格
 * 
 * @param Section - 目标网格段
 * @param V1,V2,V3 - 三角形的三个顶点索引（按逆时针顺序）
 */
void AHollowPrism::AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3)
{
    // 验证顶点索引
    if (!ensureMsgf(V1 >= 0 && V2 >= 0 && V3 >= 0, TEXT("三角形顶点索引无效")))
    {
        return;
    }

    if (!ensureMsgf(V1 < Section.Vertices.Num() && V2 < Section.Vertices.Num() && V3 < Section.Vertices.Num(),
                    TEXT("三角形顶点索引超出范围")))
    {
        return;
    }

    // 添加三角形索引
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V2);
    Section.Triangles.Add(V3);
}

/**
 * 计算顶点法线
 * 基于顶点位置计算径向法线
 * 
 * @param Vertex - 顶点位置
 * @return FVector - 计算出的法线向量
 */
FVector AHollowPrism::CalculateVertexNormal(const FVector& Vertex)
{
    // 简单的径向法线计算
    const FVector2D XY(Vertex.X, Vertex.Y);
    const float Length = XY.Size();
    
    if (Length > KINDA_SMALL_NUMBER)
    {
        return FVector(XY.X / Length, XY.Y / Length, 0.0f);
    }
    
    return FVector::UpVector;
}