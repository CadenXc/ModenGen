// Copyright (c) 2024. All rights reserved.

/**
 * @file Pyramid.cpp
 * @brief 可配置的程序化金字塔生成器的实现
 */

#include "Pyramid.h"

// Engine includes
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

// 日志分类定义
DEFINE_LOG_CATEGORY_STATIC(LogPyramid, Log, All);

// ============================================================================
// FPyramidGeometry Implementation
// 几何数据结构的实现
// ============================================================================

/**
 * 清除所有几何数据
 * 在重新生成几何体或清理资源时调用
 */
void FPyramidGeometry::Clear()
{
    Vertices.Empty();
    Triangles.Empty();
    Normals.Empty();
    UV0.Empty();
    VertexColors.Empty();
    Tangents.Empty();
}

/**
 * 验证几何数据的完整性
 * 
 * @return bool - 如果所有必要的几何数据都存在且数量匹配则返回true
 */
bool FPyramidGeometry::IsValid() const
{
    // 检查是否有基本的顶点和三角形数据
    const bool bHasBasicGeometry = Vertices.Num() > 0 && Triangles.Num() > 0;
    
    // 检查三角形索引是否为3的倍数（每个三角形3个顶点）
    const bool bValidTriangleCount = Triangles.Num() % 3 == 0;
    
    // 检查所有顶点属性数组的大小是否匹配
    const bool bMatchingArraySizes = Normals.Num() == Vertices.Num() &&
                                   UV0.Num() == Vertices.Num() &&
                                   Tangents.Num() == Vertices.Num();
    
    return bHasBasicGeometry && bValidTriangleCount && bMatchingArraySizes;
}

// ============================================================================
// FPyramidBuildParameters Implementation
// 生成参数结构体的实现
// ============================================================================

/**
 * 验证生成参数的有效性
 * 
 * @return bool - 如果所有参数都在有效范围内则返回true
 */
bool FPyramidBuildParameters::IsValid() const
{
    // 检查基础尺寸是否有效
    const bool bValidBaseRadius = BaseRadius > 0.0f;
    const bool bValidHeight = Height > 0.0f;
    
    // 检查边数是否在有效范围内
    const bool bValidSides = Sides >= 3 && Sides <= 100;
    
    // 检查倒角半径是否有效
    const bool bValidBevelRadius = BevelRadius >= 0.0f && BevelRadius < Height;
    
    return bValidBaseRadius && bValidHeight && bValidSides && bValidBevelRadius;
}

/**
 * 获取倒角顶部半径
 * 
 * @return float - 倒角顶部的半径
 */
float FPyramidBuildParameters::GetBevelTopRadius() const
{
    if (BevelRadius <= 0.0f) 
    {
        return BaseRadius;
    }
    
    // 计算倒角顶部的半径（线性缩放）
    return FMath::Max(0.0f, BaseRadius - BaseRadius * BevelRadius / Height);
}

/**
 * 获取总高度
 * 
 * @return float - 金字塔的总高度
 */
float FPyramidBuildParameters::GetTotalHeight() const
{
    return Height;
}

/**
 * 获取金字塔底面半径
 * 
 * @return float - 金字塔部分的底面半径
 */
float FPyramidBuildParameters::GetPyramidBaseRadius() const
{
    return (BevelRadius > 0) ? GetBevelTopRadius() : BaseRadius;
}

/**
 * 获取金字塔底面高度
 * 
 * @return float - 金字塔底面距离地面的高度
 */
float FPyramidBuildParameters::GetPyramidBaseHeight() const
{
    return (BevelRadius > 0) ? BevelRadius : 0.0f;
}

// ============================================================================
// FPyramidBuilder Implementation
// 几何体生成器的实现
// ============================================================================

/**
 * 构造函数
 * @param InParams - 初始化生成器的参数
 */
FPyramidBuilder::FPyramidBuilder(const FPyramidBuildParameters& InParams)
    : Params(InParams)
{
}

/**
 * 生成金字塔的几何体
 * 
 * @param OutGeometry - 输出参数，用于存储生成的几何数据
 * @return bool - 如果生成成功则返回true
 */
bool FPyramidBuilder::Generate(FPyramidGeometry& OutGeometry)
{
    // 验证生成参数
    if (!Params.IsValid())
    {
        UE_LOG(LogPyramid, Error, TEXT("无效的金字塔生成参数"));
        return false;
    }

    // 清除之前的几何数据
    OutGeometry.Clear();

    // 生成几何体的不同部分
    if (Params.BevelRadius > 0.0f)
    {
        // 如果有倒角，先生成底部棱柱部分
        GeneratePrismSection(OutGeometry);
        UE_LOG(LogPyramid, Verbose, TEXT("生成倒角部分：半径=%.2f"), Params.BevelRadius);
    }
    else
    {
        // 没有倒角，直接生成底面
        GenerateBottomFace(OutGeometry);
    }
    
    // 生成金字塔锥形部分
    GeneratePyramidSection(OutGeometry);

    // 验证生成的几何体
    const bool bIsValid = OutGeometry.IsValid();
    if (bIsValid)
    {
        UE_LOG(LogPyramid, Log, TEXT("金字塔生成完成：顶点=%d，三角形=%d"), 
               OutGeometry.GetVertexCount(), OutGeometry.GetTriangleCount());
    }
    else
    {
        UE_LOG(LogPyramid, Error, TEXT("生成的金字塔几何体无效"));
    }

    return bIsValid;
}

/**
 * 生成底部棱柱部分（用于倒角）
 * 
 * @param Geometry - 目标几何体
 */
void FPyramidBuilder::GeneratePrismSection(FPyramidGeometry& Geometry)
{
    // 生成棱柱的底部和顶部顶点
    const TArray<FVector> BottomVerts = GenerateCircleVertices(Params.GetBevelTopRadius(), 0.0f, Params.Sides);
    const TArray<FVector> TopVerts = GenerateCircleVertices(Params.GetBevelTopRadius(), Params.BevelRadius, Params.Sides);

    // 生成棱柱侧面
    GeneratePrismSides(Geometry, BottomVerts, TopVerts, false, 0.0f, 0.5f);

    // 生成棱柱底面
    GeneratePolygonFace(Geometry, BottomVerts, FVector(0, 0, -1), false);
}

/**
 * 生成金字塔锥形部分
 * 
 * @param Geometry - 目标几何体
 */
void FPyramidBuilder::GeneratePyramidSection(FPyramidGeometry& Geometry)
{
    // 计算金字塔部分的参数
    const float PyramidBaseRadius = Params.GetPyramidBaseRadius();
    const float PyramidBaseHeight = Params.GetPyramidBaseHeight();
    const TArray<FVector> BaseVertices = GenerateCircleVertices(PyramidBaseRadius, PyramidBaseHeight, Params.Sides);
    const FVector Apex(0.0f, 0.0f, Params.GetTotalHeight());

    // 为每个侧面生成三角形
    for (int32 i = 0; i < Params.Sides; i++)
    {
        const int32 NextIndex = (i + 1) % Params.Sides;

        // 计算三角形法线（确保指向外部）
        const FVector Edge1 = BaseVertices[i] - Apex;
        const FVector Edge2 = BaseVertices[NextIndex] - Apex;
        const FVector Normal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        // 计算UV坐标
        const float U_Current = static_cast<float>(i) / Params.Sides;
        const float U_Next = static_cast<float>(i + 1) / Params.Sides;
        const float V_Base = (Params.BevelRadius > 0) ? 0.5f : 0.0f;

        // 添加三个顶点
        const int32 ApexIndex = GetOrAddVertex(Geometry, Apex, Normal, FVector2D(0.5f, 1.0f));
        const int32 CurrentBaseIndex = GetOrAddVertex(Geometry, BaseVertices[i], Normal, FVector2D(U_Current, V_Base));
        const int32 NextBaseIndex = GetOrAddVertex(Geometry, BaseVertices[NextIndex], Normal, FVector2D(U_Next, V_Base));

        // 添加三角形（确保逆时针顺序）
        AddTriangle(Geometry, ApexIndex, NextBaseIndex, CurrentBaseIndex);
    }
}

/**
 * 生成底面
 * 
 * @param Geometry - 目标几何体
 */
void FPyramidBuilder::GenerateBottomFace(FPyramidGeometry& Geometry)
{
    const TArray<FVector> BaseVerts = GenerateCircleVertices(Params.BaseRadius, 0.0f, Params.Sides);
    GeneratePolygonFace(Geometry, BaseVerts, FVector(0, 0, -1), false);
}

/**
 * 添加顶点到几何体
 * 每次都创建新顶点以确保法线正确性
 * 
 * @param Geometry - 目标几何体
 * @param Pos - 顶点位置
 * @param Normal - 顶点法线
 * @param UV - 顶点UV坐标
 * @return int32 - 新顶点的索引
 */
int32 FPyramidBuilder::GetOrAddVertex(FPyramidGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    // 为了确保法线正确，我们总是创建新顶点
    // 这样可以避免不同面之间的法线冲突
    const int32 Index = Geometry.Vertices.Add(Pos);
    Geometry.Normals.Add(Normal);
    Geometry.UV0.Add(UV);

    // 计算切线向量（垂直于法线）
    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    TangentDirection.Normalize();
    Geometry.Tangents.Add(FProcMeshTangent(TangentDirection, false));

    return Index;
}

/**
 * 添加三角形到几何体
 * 
 * @param Geometry - 目标几何体
 * @param V1,V2,V3 - 三角形的三个顶点索引（按逆时针顺序）
 */
void FPyramidBuilder::AddTriangle(FPyramidGeometry& Geometry, int32 V1, int32 V2, int32 V3)
{
    // 验证顶点索引
    if (!ensureMsgf(V1 >= 0 && V2 >= 0 && V3 >= 0, TEXT("三角形顶点索引无效")))
    {
        return;
    }

    if (!ensureMsgf(V1 < Geometry.Vertices.Num() && V2 < Geometry.Vertices.Num() && V3 < Geometry.Vertices.Num(),
                    TEXT("三角形顶点索引超出范围")))
    {
        return;
    }

    // 按逆时针顺序添加顶点索引
    Geometry.Triangles.Add(V1);
    Geometry.Triangles.Add(V2);
    Geometry.Triangles.Add(V3);
}

/**
 * 生成圆形顶点
 * 
 * @param Radius - 圆的半径
 * @param Z - 圆所在的Z高度
 * @param NumSides - 圆的边数
 * @return TArray<FVector> - 生成的顶点数组
 */
TArray<FVector> FPyramidBuilder::GenerateCircleVertices(float Radius, float Z, int32 NumSides)
{
    // 验证参数
    if (!ensureMsgf(Radius >= 0.0f, TEXT("半径不能为负数")))
    {
        Radius = 0.0f;
    }

    if (!ensureMsgf(NumSides >= 3, TEXT("边数至少为3")))
    {
        NumSides = 3;
    }

    TArray<FVector> Vertices;
    Vertices.Reserve(NumSides);

    // 生成圆周上的顶点
    for (int32 i = 0; i < NumSides; i++)
    {
        const float Angle = 2.0f * PI * i / NumSides;
        Vertices.Add(FVector(
            Radius * FMath::Cos(Angle),
            Radius * FMath::Sin(Angle),
            Z
        ));
    }
    
    return Vertices;
}

void FPyramidBuilder::GeneratePrismSides(FPyramidGeometry& Geometry, const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal, float UVOffsetY, float UVScaleY)
{
    const int32 NumSides = BottomVerts.Num();
    for (int32 i = 0; i < NumSides; i++)
    {
        int32 NextIndex = (i + 1) % NumSides;

        // 计算法线
        FVector Edge1 = BottomVerts[NextIndex] - BottomVerts[i];
        FVector Edge2 = TopVerts[i] - BottomVerts[i];
        FVector Normal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
        if (bReverseNormal) Normal = -Normal;

        // 添加四边形（两个三角形）
        int32 V0 = GetOrAddVertex(Geometry, BottomVerts[i], Normal,
            FVector2D(static_cast<float>(i) / NumSides, UVOffsetY));
        int32 V1 = GetOrAddVertex(Geometry, BottomVerts[NextIndex], Normal,
            FVector2D(static_cast<float>(i + 1) / NumSides, UVOffsetY));
        int32 V2 = GetOrAddVertex(Geometry, TopVerts[NextIndex], Normal,
            FVector2D(static_cast<float>(i + 1) / NumSides, UVOffsetY + UVScaleY));
        int32 V3 = GetOrAddVertex(Geometry, TopVerts[i], Normal,
            FVector2D(static_cast<float>(i) / NumSides, UVOffsetY + UVScaleY));

        if (bReverseNormal)
        {
            AddTriangle(Geometry, V0, V2, V3);
            AddTriangle(Geometry, V0, V1, V2);
        }
        else
        {
            AddTriangle(Geometry, V0, V2, V1);
            AddTriangle(Geometry, V0, V3, V2);
        }
    }
}

void FPyramidBuilder::GeneratePolygonFace(FPyramidGeometry& Geometry, const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder, float UVOffsetZ)
{
    const int32 NumSides = PolygonVerts.Num();
    const FVector Center = FVector(0, 0, PolygonVerts[0].Z);
    int32 CenterIndex = GetOrAddVertex(Geometry, Center, Normal, FVector2D(0.5f, 0.5f));

    for (int32 i = 0; i < NumSides; i++)
    {
        int32 NextIndex = (i + 1) % NumSides;
        int32 V0 = GetOrAddVertex(Geometry, PolygonVerts[i], Normal, FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * i / NumSides),
                0.5f + 0.5f * FMath::Sin(2 * PI * i / NumSides)));
        int32 V1 = GetOrAddVertex(Geometry, PolygonVerts[NextIndex], Normal, FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * NextIndex / NumSides),
                0.5f + 0.5f * FMath::Sin(2 * PI * NextIndex / NumSides)));

        if (bReverseOrder)
        {
            AddTriangle(Geometry, CenterIndex, V1, V0);
        }
        else
        {
            AddTriangle(Geometry, CenterIndex, V0, V1);
        }
    }
}

// ============================================================================
// APyramid 实现
// ============================================================================

APyramid::APyramid()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建ProceduralMesh组件
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;

    // 设置默认材质
    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (MaterialFinder.Succeeded())
    {
        Material = MaterialFinder.Object;
    }

    // 初始生成
    GenerateMeshInternal();
}

void APyramid::BeginPlay()
{
    Super::BeginPlay();
    GenerateMeshInternal();
}

void APyramid::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerateMeshInternal();
}

bool APyramid::GenerateMeshInternal()
{
    if (!ProceduralMesh)
    {
		return false;
    }

    // 创建构建参数
    FPyramidBuildParameters BuildParams;
    BuildParams.BaseRadius = BaseRadius;
    BuildParams.Height = Height;
    BuildParams.Sides = Sides;
    BuildParams.BevelRadius = BevelRadius;

    // 创建构建器并生成几何
    Builder = MakeUnique<FPyramidBuilder>(BuildParams);
    if (!Builder->Generate(CurrentGeometry))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate Pyramid geometry"));
        return false;
    }

    // 清除之前的网格
    ProceduralMesh->ClearAllMeshSections();

    // 创建网格
    ProceduralMesh->CreateMeshSection(0, CurrentGeometry.Vertices, CurrentGeometry.Triangles, 
        CurrentGeometry.Normals, CurrentGeometry.UV0, CurrentGeometry.VertexColors, 
        CurrentGeometry.Tangents, bGenerateCollision);

    // 设置材质和碰撞
    SetupMaterial();
    SetupCollision();

    return true;
}

void APyramid::SetupMaterial()
{
    if (ProceduralMesh && Material)
    {
        ProceduralMesh->SetMaterial(0, Material);
    }
}

void APyramid::SetupCollision()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->bUseAsyncCooking = true;
        ProceduralMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void APyramid::RegenerateMesh()
{
    GenerateMeshInternal();
}

void APyramid::GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides)
{
    BaseRadius = InBaseRadius;
    Height = InHeight;
    Sides = InSides;
    
    GenerateMeshInternal();
}
