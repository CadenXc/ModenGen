// Copyright (c) 2024. All rights reserved.

/**
 * @file Frustum.cpp
 * @brief 可配置的程序化截锥体生成器的实现
 */

#include "Frustum.h"

// Engine includes
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

// 日志分类定义
DEFINE_LOG_CATEGORY_STATIC(LogFrustum, Log, All);

/**
 * 构造函数
 * 初始化组件和默认属性
 */
AFrustum::AFrustum()
{
    // 启用Tick以支持实时更新
    PrimaryActorTick.bCanEverTick = true;

    // 创建并配置程序化网格组件
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FrustumMesh"));
    RootComponent = MeshComponent;

    // 配置网格属性
    if (MeshComponent)
    {
        // 启用异步烘焙以提高性能
        MeshComponent->bUseAsyncCooking = true;
        
        // 配置碰撞设置
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        MeshComponent->SetSimulatePhysics(false);
    }

    // 初始生成几何体
    GenerateGeometry();
}

/**
 * 游戏开始时调用
 * 确保几何体已正确生成
 */
void AFrustum::BeginPlay()
{
    Super::BeginPlay();
    GenerateGeometry();
}

/**
 * 加载完成时调用
 * 确保几何体已正确生成
 */
void AFrustum::PostLoad()
{
    Super::PostLoad();
    GenerateGeometry();
}

#if WITH_EDITOR
/**
 * 编辑器中属性变更时调用
 * 
 * @param PropertyChangedEvent - 属性变更事件信息
 */
void AFrustum::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // 检查是否是相关属性发生变化
    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    static const TArray<FName> RelevantProperties = {
        // 基础几何参数
        "TopRadius", "BottomRadius", "Height",
        // 细分参数
        "Sides", "HeightSegments",
        // 倒角参数
        "ChamferRadius", "ChamferSections",
        // 变形参数
        "BendAmount", "MinBendRadius",
        // 形状控制参数
        "ArcAngle", "CapThickness"
    };

    // 如果是相关属性变化，标记几何体需要重新生成
    if (RelevantProperties.Contains(PropertyName))
    {
        bGeometryDirty = true;
        GenerateGeometry();
    }
}
#endif

/**
 * 每帧更新
 * 
 * @param DeltaTime - 帧间隔时间
 */
void AFrustum::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 如果几何体需要更新，重新生成
    if (bGeometryDirty)
    {
        GenerateGeometry();
        bGeometryDirty = false;
    }
}

/**
 * 重新生成几何体
 * 可从蓝图中调用以手动触发重新生成
 */
void AFrustum::Regenerate()
{
    GenerateGeometry();
}

/**
 * 生成截锥体的几何体
 * 包括主体、倒角、顶底面和端面（如果需要）
 */
void AFrustum::GenerateGeometry()
{
    // 验证组件
    if (!ensureMsgf(MeshComponent, TEXT("Frustum: Mesh component is missing!")))
    {
        UE_LOG(LogFrustum, Error, TEXT("无法生成几何体：网格组件未初始化"));
        return;
    }

    // 清除现有网格
    MeshComponent->ClearAllMeshSections();

    // 验证并修正参数
    ValidateAndClampParameters();

    // 清除并预分配网格数据
    PrepareGeometryData();

    // 计算关键尺寸
    const float HalfHeight = Parameters.Height * 0.5f;
    const float TopChamferHeight = FMath::Min(Parameters.ChamferRadius, Parameters.TopRadius);
    const float BottomChamferHeight = FMath::Min(Parameters.ChamferRadius, Parameters.BottomRadius);

    // 计算主体几何体的范围
    const float StartZ = -HalfHeight + BottomChamferHeight;
    const float EndZ = HalfHeight - TopChamferHeight;

    // 生成几何体的各个部分
    GenerateGeometryParts(StartZ, EndZ);

    // 验证生成的数据
    if (!ValidateGeneratedData())
    {
        return;
    }

    // 更新网格组件
    UpdateMeshComponent();
}

/**
 * 验证并修正生成参数
 */
void AFrustum::ValidateAndClampParameters()
{
    // 基础几何参数
    Parameters.TopRadius = FMath::Max(0.01f, Parameters.TopRadius);
    Parameters.BottomRadius = FMath::Max(0.01f, Parameters.BottomRadius);
    Parameters.Height = FMath::Max(0.01f, Parameters.Height);

    // 细分参数
    Parameters.Sides = FMath::Max(3, Parameters.Sides);
    Parameters.HeightSegments = FMath::Max(1, Parameters.HeightSegments);

    // 倒角参数
    Parameters.ChamferRadius = FMath::Max(0.0f, Parameters.ChamferRadius);
    Parameters.ChamferSections = FMath::Max(1, Parameters.ChamferSections);

    // 变形参数
    Parameters.BendAmount = FMath::Clamp(Parameters.BendAmount, -1.0f, 1.0f);
    Parameters.MinBendRadius = FMath::Max(1.0f, Parameters.MinBendRadius);

    // 形状控制参数
    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);
    Parameters.CapThickness = FMath::Max(0.0f, Parameters.CapThickness);

    // 记录参数验证结果
    UE_LOG(LogFrustum, Verbose, TEXT("参数验证完成：TopRadius=%.2f, BottomRadius=%.2f, Height=%.2f, Sides=%d"),
           Parameters.TopRadius, Parameters.BottomRadius, Parameters.Height, Parameters.Sides);
}

/**
 * 准备几何数据，包括清理和预分配内存
 */
void AFrustum::PrepareGeometryData()
{
    // 清除现有数据
    MeshData.Clear();

    // 计算预估的顶点和三角形数量
    const int32 VertexCountEstimate = CalculateVertexCountEstimate();
    const int32 TriangleCountEstimate = CalculateTriangleCountEstimate();

    // 预分配内存
    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate);

    UE_LOG(LogFrustum, Verbose, TEXT("预分配内存：顶点=%d，三角形=%d"),
           VertexCountEstimate, TriangleCountEstimate);
}

/**
 * 计算预估的顶点数量
 */
int32 AFrustum::CalculateVertexCountEstimate() const
{
    // 基础顶点数（主体 + 顶底面）
    int32 EstimatedCount = (Parameters.HeightSegments + 1) * (Parameters.Sides + 1) * 4;

    // 如果不是完整圆形，需要额外的端面顶点
    if (Parameters.ArcAngle < 360.0f)
    {
        EstimatedCount += Parameters.HeightSegments * 4;
    }

    // 倒角需要的额外顶点
    if (Parameters.ChamferRadius > 0.0f)
    {
        EstimatedCount += Parameters.Sides * Parameters.ChamferSections * 2;
    }

    return EstimatedCount;
}

/**
 * 计算预估的三角形数量
 */
int32 AFrustum::CalculateTriangleCountEstimate() const
{
    // 基础三角形数（主体 + 顶底面）
    int32 EstimatedCount = Parameters.HeightSegments * Parameters.Sides * 6;

    // 如果不是完整圆形，需要额外的端面三角形
    if (Parameters.ArcAngle < 360.0f)
    {
        EstimatedCount += Parameters.HeightSegments * 6;
    }

    // 倒角需要的额外三角形
    if (Parameters.ChamferRadius > 0.0f)
    {
        EstimatedCount += Parameters.Sides * Parameters.ChamferSections * 4;
    }

    return EstimatedCount;
}

/**
 * 生成几何体的各个部分
 */
void AFrustum::GenerateGeometryParts(float StartZ, float EndZ)
{
    // 生成主体几何体（如果有足够的高度）
    if (EndZ > StartZ)
    {
        CreateSideGeometry(StartZ, EndZ);
        UE_LOG(LogFrustum, Verbose, TEXT("生成主体几何体：StartZ=%.2f, EndZ=%.2f"), StartZ, EndZ);
    }
    
    // 生成倒角几何体（如果启用）
    if (Parameters.ChamferRadius > 0.0f)
    {
        CreateTopChamferGeometry(EndZ);
        CreateBottomChamferGeometry(StartZ);
        UE_LOG(LogFrustum, Verbose, TEXT("生成倒角几何体：Radius=%.2f, Sections=%d"), 
               Parameters.ChamferRadius, Parameters.ChamferSections);
    }
    
    // 生成顶底面几何体
    CreateTopGeometry(Parameters.Height * 0.5f);
    CreateBottomGeometry(-Parameters.Height * 0.5f);

    // 生成端面（如果不是完整圆形）
    if (Parameters.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CreateEndCaps();
        UE_LOG(LogFrustum, Verbose, TEXT("生成端面：ArcAngle=%.2f"), Parameters.ArcAngle);
    }
}

/**
 * 验证生成的几何数据
 */
bool AFrustum::ValidateGeneratedData()
{
    if (!MeshData.IsValid())
    {
        UE_LOG(LogFrustum, Error, TEXT("生成的网格数据无效：顶点=%d，三角形=%d"), 
               MeshData.GetVertexCount(), MeshData.GetTriangleCount());
        return false;
    }

    UE_LOG(LogFrustum, Log, TEXT("几何体生成完成：顶点=%d，三角形=%d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
    return true;
}

/**
 * 更新网格组件
 */
void AFrustum::UpdateMeshComponent()
{
    // 更新网格数据
    UpdateProceduralMeshComponent();
    
    // 运行调试验证
    if (WITH_EDITOR || UE_BUILD_DEBUG)
    {
        TestNormalValidation();
    }
}

/**
 * 添加顶点到网格
 * 
 * @param Position - 顶点位置
 * @param Normal - 顶点法线
 * @param UV - 顶点UV坐标
 * @param Color - 顶点颜色
 * @return int32 - 新顶点的索引
 */
int32 AFrustum::AddVertex(const FVector& Position, const FVector& Normal, 
                         const FVector2D& UV, const FLinearColor& Color)
{
    // 验证输入参数
    if (!ensureMsgf(!Position.ContainsNaN(), TEXT("顶点位置包含无效值")))
    {
        return INDEX_NONE;
    }

    if (!ensureMsgf(!Normal.ContainsNaN() && !Normal.IsZero(), TEXT("顶点法线无效")))
    {
        return INDEX_NONE;
    }

    if (!ensureMsgf(!UV.ContainsNaN(), TEXT("UV坐标包含无效值")))
    {
        return INDEX_NONE;
    }

    // 添加顶点并返回索引
    const int32 NewIndex = MeshData.AddVertex(Position, Normal, UV, Color);

    // 记录顶点添加
    UE_LOG(LogFrustum, VeryVerbose, TEXT("添加顶点：Index=%d, Position=%s, Normal=%s"), 
           NewIndex, *Position.ToString(), *Normal.ToString());

    return NewIndex;
}

/**
 * 添加四边形到网格
 * 
 * @param V1,V2,V3,V4 - 四边形的四个顶点索引（按逆时针顺序）
 * @param MaterialIndex - 材质索引
 */
void AFrustum::AddQuad(int32 V1, int32 V2, int32 V3, int32 V4, int32 MaterialIndex)
{
    // 验证顶点索引
    if (!ensureMsgf(V1 >= 0 && V2 >= 0 && V3 >= 0 && V4 >= 0, TEXT("四边形顶点索引无效")))
    {
        return;
    }

    if (!ensureMsgf(V1 < MeshData.GetVertexCount() && V2 < MeshData.GetVertexCount() &&
                    V3 < MeshData.GetVertexCount() && V4 < MeshData.GetVertexCount(),
                    TEXT("四边形顶点索引超出范围")))
    {
        return;
    }

    // 添加四边形
    MeshData.AddQuad(V1, V2, V3, V4, MaterialIndex);

    // 记录四边形添加
    UE_LOG(LogFrustum, VeryVerbose, TEXT("添加四边形：V1=%d, V2=%d, V3=%d, V4=%d, Material=%d"), 
           V1, V2, V3, V4, MaterialIndex);
}

/**
 * 添加三角形到网格
 * 
 * @param V1,V2,V3 - 三角形的三个顶点索引（按逆时针顺序）
 * @param MaterialIndex - 材质索引
 */
void AFrustum::AddTriangle(int32 V1, int32 V2, int32 V3, int32 MaterialIndex)
{
    // 验证顶点索引
    if (!ensureMsgf(V1 >= 0 && V2 >= 0 && V3 >= 0, TEXT("三角形顶点索引无效")))
    {
        return;
    }

    if (!ensureMsgf(V1 < MeshData.GetVertexCount() && V2 < MeshData.GetVertexCount() &&
                    V3 < MeshData.GetVertexCount(),
                    TEXT("三角形顶点索引超出范围")))
    {
        return;
    }

    // 添加三角形
    MeshData.AddTriangle(V1, V2, V3, MaterialIndex);

    // 记录三角形添加
    UE_LOG(LogFrustum, VeryVerbose, TEXT("添加三角形：V1=%d, V2=%d, V3=%d, Material=%d"), 
           V1, V2, V3, MaterialIndex);
}

/**
 * 更新程序化网格组件
 * 将生成的几何数据应用到网格组件
 */
void AFrustum::UpdateProceduralMeshComponent()
{
    // 验证组件和数据
    if (!ensureMsgf(MeshComponent, TEXT("网格组件未初始化")))
    {
        return;
    }

    if (MeshData.Vertices.Num() == 0)
    {
        UE_LOG(LogFrustum, Warning, TEXT("没有顶点数据可更新"));
        return;
    }

    // 验证法线数据
    ValidateAndFixNormals();

    // 提交网格数据
    MeshComponent->CreateMeshSection_LinearColor(
        0,                          // 段索引
        MeshData.Vertices,         // 顶点位置
        MeshData.Triangles,        // 三角形索引
        MeshData.Normals,          // 顶点法线
        MeshData.UVs,              // UV坐标
        MeshData.VertexColors,     // 顶点颜色
        MeshData.Tangents,         // 切线
        true                       // 启用碰撞
    );

    // 应用材质
    ApplyMaterial();

    // 记录更新结果
    UE_LOG(LogFrustum, Log, TEXT("网格更新完成：顶点=%d，三角形=%d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

/**
 * 验证并修复网格的法线数据
 * 包括检查法线长度、方向和一致性
 */
void AFrustum::ValidateAndFixNormals()
{
    // 验证数据有效性
    if (!ValidateNormalDataIntegrity())
    {
        return;
    }

    // 统计信息
    FNormalValidationStats Stats;

    // 检查并修复每个顶点的法线
    ValidateVertexNormals(Stats);

    // 输出统计信息
    LogNormalValidationStats(Stats);

    // 验证三角形法线一致性
    ValidateTriangleNormals();
}

/**
 * 验证法线数据的完整性
 * @return bool - 如果数据完整则返回true
 */
bool AFrustum::ValidateNormalDataIntegrity() const
{
    // 检查是否有网格数据
    if (MeshData.Vertices.Num() == 0 || MeshData.Normals.Num() == 0)
    {
        UE_LOG(LogFrustum, Warning, TEXT("没有网格数据可验证"));
        return false;
    }

    // 检查法线数组大小是否匹配
    if (MeshData.Normals.Num() != MeshData.Vertices.Num())
    {
        UE_LOG(LogFrustum, Error, TEXT("法线数量(%d)与顶点数量(%d)不匹配"), 
               MeshData.Normals.Num(), MeshData.Vertices.Num());
        return false;
    }

    return true;
}

/**
 * 验证并修复顶点法线
 * @param Stats - 输出参数，用于收集统计信息
 */
void AFrustum::ValidateVertexNormals(FNormalValidationStats& Stats)
{
    const float HalfHeight = Parameters.Height * 0.5f;
    Stats.AverageNormal = FVector::ZeroVector;

    // 检查每个顶点的法线
    for (int32 i = 0; i < MeshData.Normals.Num(); ++i)
    {
        FVector& Normal = MeshData.Normals[i];
        const FVector& Vertex = MeshData.Vertices[i];

        // 修复零向量法线
        if (FixZeroNormal(Normal, Vertex, Stats))
        {
            continue;
        }

        // 标准化法线
        if (NormalizeNormal(Normal, Stats))
        {
            continue;
        }

        // 修正法线方向
        FixNormalDirection(Normal, Vertex, HalfHeight, Stats);

        // 累加法线以计算平均值
        Stats.AverageNormal += Normal;
    }
}

/**
 * 修复零向量法线
 * @return bool - 如果法线是零向量并已修复则返回true
 */
bool AFrustum::FixZeroNormal(FVector& Normal, const FVector& Vertex, FNormalValidationStats& Stats)
{
    if (!Normal.IsNearlyZero())
    {
        return false;
    }

    Stats.ZeroNormals++;

    // 尝试从顶点位置计算法线
    Normal = Vertex.GetSafeNormal();
    if (Normal.IsNearlyZero())
    {
        Normal = FVector(0, 0, 1); // 默认向上
    }

    return true;
}

/**
 * 标准化法线
 * @return bool - 如果法线需要标准化并已修复则返回true
 */
bool AFrustum::NormalizeNormal(FVector& Normal, FNormalValidationStats& Stats)
{
    const float NormalLength = Normal.Size();
    if (FMath::IsNearlyEqual(NormalLength, 1.0f, 0.01f))
    {
        return false;
    }

    Stats.InvalidNormals++;
    Normal = Normal.GetSafeNormal();
    return true;
}

/**
 * 修正法线方向
 */
void AFrustum::FixNormalDirection(FVector& Normal, const FVector& Vertex, float HalfHeight, FNormalValidationStats& Stats)
{
    if (Normal.IsNearlyZero())
    {
        return;
    }

    // 根据顶点位置确定期望的法线方向
    if (FMath::Abs(Vertex.Z) < HalfHeight - 0.1f)
    {
        // 侧面顶点：法线应指向远离中心
        FixSideNormal(Normal, Vertex, Stats);
    }
    else if (Vertex.Z > 0)
    {
        // 顶面顶点：法线应向上
        FixTopNormal(Normal, Stats);
    }
    else
    {
        // 底面顶点：法线应向下
        FixBottomNormal(Normal, Stats);
    }
}

/**
 * 修正侧面法线方向
 */
void AFrustum::FixSideNormal(FVector& Normal, const FVector& Vertex, FNormalValidationStats& Stats)
{
    const FVector ToCenter = FVector(Vertex.X, Vertex.Y, 0).GetSafeNormal();
    if (FVector::DotProduct(Normal, ToCenter) < 0)
    {
        Stats.FlippedNormals++;
        Normal = -Normal;
    }
}

/**
 * 修正顶面法线方向
 */
void AFrustum::FixTopNormal(FVector& Normal, FNormalValidationStats& Stats)
{
    if (Normal.Z < 0)
    {
        Stats.FlippedNormals++;
        Normal = -Normal;
    }
}

/**
 * 修正底面法线方向
 */
void AFrustum::FixBottomNormal(FVector& Normal, FNormalValidationStats& Stats)
{
    if (Normal.Z > 0)
    {
        Stats.FlippedNormals++;
        Normal = -Normal;
    }
}

/**
 * 输出法线验证统计信息
 */
void AFrustum::LogNormalValidationStats(FNormalValidationStats& Stats)
{
    // 输出问题统计
    if (Stats.InvalidNormals > 0 || Stats.ZeroNormals > 0 || Stats.FlippedNormals > 0)
    {
        UE_LOG(LogFrustum, Warning, TEXT("法线验证结果：无效=%d，零向量=%d，方向错误=%d"), 
               Stats.InvalidNormals, Stats.ZeroNormals, Stats.FlippedNormals);
    }

    // 输出平均法线方向
    if (MeshData.Normals.Num() > 0)
    {
        Stats.AverageNormal = Stats.AverageNormal.GetSafeNormal();
        UE_LOG(LogFrustum, Log, TEXT("平均法线方向：%s"), *Stats.AverageNormal.ToString());
    }
}

/**
 * 运行法线验证测试
 * 仅在编辑器或调试模式下使用
 */
void AFrustum::TestNormalValidation()
{
    UE_LOG(LogFrustum, Log, TEXT("=== 法线验证测试开始 ==="));
    
    // 测试基本法线
    const FVector TestNormals[] = {
        FVector(1, 0, 0),  // X轴
        FVector(0, 1, 0),  // Y轴
        FVector(0, 0, 1)   // Z轴
    };
    
    // 测试法线属性
    for (int32 i = 0; i < 3; ++i)
    {
        const FVector& Normal = TestNormals[i];
        UE_LOG(LogFrustum, Log, TEXT("测试法线 %d: %s (长度=%.3f)"), 
               i + 1, *Normal.ToString(), Normal.Size());
    }
    
    // 测试法线点积
    TestNormalDotProducts(TestNormals);
    
    // 测试法线标准化
    TestNormalNormalization();
    
    UE_LOG(LogFrustum, Log, TEXT("=== 法线验证测试完成 ==="));
}

/**
 * 测试法线点积计算
 */
void AFrustum::TestNormalDotProducts(const FVector TestNormals[3])
{
    for (int32 i = 0; i < 3; ++i)
    {
        for (int32 j = i + 1; j < 3; ++j)
        {
            const float Dot = FVector::DotProduct(TestNormals[i], TestNormals[j]);
            UE_LOG(LogFrustum, Log, TEXT("法线 %d 和 %d 的点积: %.3f"), 
                   i + 1, j + 1, Dot);
        }
    }
}

/**
 * 测试法线标准化
 */
void AFrustum::TestNormalNormalization()
{
    const FVector Unnormalized(2, 3, 4);
    const FVector Normalized = Unnormalized.GetSafeNormal();
    
    UE_LOG(LogFrustum, Log, TEXT("未标准化: %s (长度=%.3f)"), 
           *Unnormalized.ToString(), Unnormalized.Size());
    UE_LOG(LogFrustum, Log, TEXT("已标准化: %s (长度=%.3f)"), 
           *Normalized.ToString(), Normalized.Size());
}

/**
 * 验证三角形法线的一致性
 */
void AFrustum::ValidateTriangleNormals()
{
    // 验证数据有效性
    if (MeshData.Triangles.Num() == 0)
    {
        return;
    }

    // 统计信息
    int32 InconsistentTriangles = 0;
    int32 DegenerateTriangles = 0;

    // 检查每个三角形
    for (int32 i = 0; i < MeshData.Triangles.Num(); i += 3)
    {
        // 验证索引范围
        if (!ValidateTriangleIndices(i))
        {
            continue;
        }

        // 获取三角形顶点索引
        const int32 V0 = MeshData.Triangles[i];
        const int32 V1 = MeshData.Triangles[i + 1];
        const int32 V2 = MeshData.Triangles[i + 2];

        // 计算三角形法线
        const FVector Edge1 = MeshData.Vertices[V1] - MeshData.Vertices[V0];
        const FVector Edge2 = MeshData.Vertices[V2] - MeshData.Vertices[V0];
        const FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        // 检查退化三角形
        if (FaceNormal.IsNearlyZero())
        {
            DegenerateTriangles++;
            continue;
        }

        // 检查顶点法线一致性
        if (!CheckVertexNormalConsistency(V0, V1, V2, FaceNormal))
        {
            InconsistentTriangles++;
        }
    }

    // 输出统计信息
    LogTriangleValidationResults(InconsistentTriangles, DegenerateTriangles);
}

/**
 * 验证三角形索引的有效性
 */
bool AFrustum::ValidateTriangleIndices(int32 BaseIndex) const
{
    if (BaseIndex + 2 >= MeshData.Triangles.Num())
    {
        return false;
    }

    const int32 Indices[] = {
        MeshData.Triangles[BaseIndex],
        MeshData.Triangles[BaseIndex + 1],
        MeshData.Triangles[BaseIndex + 2]
    };

    for (int32 Index : Indices)
    {
        if (Index < 0 || Index >= MeshData.Vertices.Num())
        {
            UE_LOG(LogFrustum, Error, TEXT("无效的三角形索引：%d"), Index);
            return false;
        }
    }

    return true;
}

/**
 * 检查顶点法线与面法线的一致性
 */
bool AFrustum::CheckVertexNormalConsistency(int32 V0, int32 V1, int32 V2, const FVector& FaceNormal) const
{
    const FVector& Normal0 = MeshData.Normals[V0];
    const FVector& Normal1 = MeshData.Normals[V1];
    const FVector& Normal2 = MeshData.Normals[V2];

    const float Dot0 = FVector::DotProduct(FaceNormal, Normal0);
    const float Dot1 = FVector::DotProduct(FaceNormal, Normal1);
    const float Dot2 = FVector::DotProduct(FaceNormal, Normal2);

    return Dot0 >= 0.5f && Dot1 >= 0.5f && Dot2 >= 0.5f;
}

/**
 * 输出三角形验证结果
 */
void AFrustum::LogTriangleValidationResults(int32 InconsistentTriangles, int32 DegenerateTriangles) const
{
    if (InconsistentTriangles > 0 || DegenerateTriangles > 0)
    {
        UE_LOG(LogFrustum, Warning, TEXT("三角形问题：法线不一致=%d，退化=%d"), 
               InconsistentTriangles, DegenerateTriangles);
    }
}

void AFrustum::CreateSideGeometry(float StartZ, float EndZ)
{
    const float SideHeight = EndZ - StartZ;
    if (SideHeight <= 0) return;

    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;
    const float HeightStep = SideHeight / Parameters.HeightSegments;
    const float HalfHeight = Parameters.Height / 2.0f;

    // 顶点环缓存 [高度层][边]
    TArray<TArray<int32>> VertexRings;
    VertexRings.SetNum(Parameters.HeightSegments + 1);

    // 生成顶点环
    for (int32 h = 0; h <= Parameters.HeightSegments; ++h)
    {
        const float Z = StartZ + h * HeightStep;
        const float Alpha = (Z + HalfHeight) / Parameters.Height; // Corrected alpha calculation for lerp
        const float Radius = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha);

        // 弯曲效果
        const float BendFactor = FMath::Sin(Alpha * PI);
        const float BentRadius = FMath::Max(
            Radius + Parameters.BendAmount * BendFactor * Radius,
            Parameters.MinBendRadius
        );

        TArray<int32>& Ring = VertexRings[h];
        Ring.SetNum(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            const float Angle = s * AngleStep;
            const float X = BentRadius * FMath::Cos(Angle);
            const float Y = BentRadius * FMath::Sin(Angle);

            // 法线计算（考虑弯曲）
            FVector Normal = FVector(X, Y, 0).GetSafeNormal();
            if (!FMath::IsNearlyZero(Parameters.BendAmount))
            {
                const float NormalZ = -Parameters.BendAmount * FMath::Cos(Alpha * PI);
                Normal = (Normal + FVector(0, 0, NormalZ)).GetSafeNormal();
            }

            // 确保法线指向外部（远离中心）
            FVector ToCenter = FVector(X, Y, 0).GetSafeNormal();
            if (FVector::DotProduct(Normal, ToCenter) < 0)
            {
                Normal = -Normal;
            }

            // UV映射
            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = Alpha;

            Ring[s] = AddVertex(FVector(X, Y, Z), Normal, FVector2D(U, V), FLinearColor::White);
        }
    }

    // 生成侧面四边形
    for (int32 h = 0; h < Parameters.HeightSegments; ++h)
    {
        for (int32 s = 0; s < Parameters.Sides; ++s)
        {
            const int32 V00 = VertexRings[h][s];
            const int32 V10 = VertexRings[h + 1][s];
            const int32 V01 = VertexRings[h][s + 1];
            const int32 V11 = VertexRings[h + 1][s + 1];

            AddQuad(V00, V10, V11, V01, 0); // MaterialIndex 0 for side
        }
    }
}


void AFrustum::CreateTopGeometry(float Z)
{
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;
    const float Radius = FMath::Max(0.0f, Parameters.TopRadius - Parameters.ChamferRadius);

    // 中心顶点
    const int32 CenterVertex = AddVertex(
        FVector(0, 0, Z),
        FVector(0, 0, 1),
        FVector2D(0.5f, 0.5f),
        FLinearColor::White
    );

    // 顶部顶点环
    TArray<int32> TopRing;
    TopRing.SetNum(Parameters.Sides + 1);

    for (int32 s = 0; s <= Parameters.Sides; ++s)
    {
        const float Angle = s * AngleStep;
        const float X = Radius * FMath::Cos(Angle);
        const float Y = Radius * FMath::Sin(Angle);

        // UV映射到圆形
        const float U = 0.5f + 0.5f * FMath::Cos(Angle);
        const float V = 0.5f + 0.5f * FMath::Sin(Angle);

        TopRing[s] = AddVertex(
            FVector(X, Y, Z),
            FVector(0, 0, 1),
            FVector2D(U, V),
            FLinearColor::White
        );
    }

    // 创建顶部三角扇
    for (int32 s = 0; s < Parameters.Sides; ++s)
    {
        AddTriangle(CenterVertex, TopRing[s + 1], TopRing[s], 0); // MaterialIndex 0 for top
    }
}

void AFrustum::CreateBottomGeometry(float Z)
{
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;
    const float Radius = FMath::Max(0.0f, Parameters.BottomRadius - Parameters.ChamferRadius);

    // 中心顶点
    const int32 CenterVertex = AddVertex(
        FVector(0, 0, Z),
        FVector(0, 0, -1),
        FVector2D(0.5f, 0.5f),
        FLinearColor::White
    );

    // 底部顶点环
    TArray<int32> BottomRing;
    BottomRing.SetNum(Parameters.Sides + 1);

    for (int32 s = 0; s <= Parameters.Sides; ++s)
    {
        const float Angle = s * AngleStep;
        const float X = Radius * FMath::Cos(Angle);
        const float Y = Radius * FMath::Sin(Angle);

        // UV映射到圆形
        const float U = 0.5f + 0.5f * FMath::Cos(Angle);
        const float V = 0.5f + 0.5f * FMath::Sin(Angle);

        BottomRing[s] = AddVertex(
            FVector(X, Y, Z),
            FVector(0, 0, -1),
            FVector2D(U, V),
            FLinearColor::White
        );
    }

    // 创建底部三角扇
    for (int32 s = 0; s < Parameters.Sides; ++s)
    {
        AddTriangle(CenterVertex, BottomRing[s], BottomRing[s + 1], 0); // MaterialIndex 0 for bottom
    }
}


// 修正后的辅助函数
AFrustum::FChamferArcControlPoints AFrustum::CalculateChamferControlPoints(const FVector& SideVertex, const FVector& TopBottomVertex)
{
    FChamferArcControlPoints Points;

    // 起点：使用传入的侧面顶点
    Points.StartPoint = SideVertex;

    // 终点：使用传入的顶/底面顶点
    Points.EndPoint = TopBottomVertex;

    // 改进的控制点计算：考虑倒角半径和几何约束
    const float ChamferRadius = Parameters.ChamferRadius;
    const float StartRadius = SideVertex.X;
    const float EndRadius = TopBottomVertex.X;
    
    // 控制点应该位于倒角弧线的中点附近，考虑半径变化
    const float MidRadius = (StartRadius + EndRadius) * 0.5f;
    const float MidZ = (SideVertex.Z + TopBottomVertex.Z) * 0.5f;
    
    // 添加一些偏移以确保平滑的倒角曲线
    const float RadiusOffset = ChamferRadius * 0.3f;
    Points.ControlPoint = FVector(MidRadius + RadiusOffset, 0, MidZ);

    return Points;
}


FVector AFrustum::CalculateChamferArcPoint(const FChamferArcControlPoints& ControlPoints, float t)
{
    // 二次贝塞尔曲线：B(t) = (1-t)²P₀ + 2(1-t)tP₁ + t²P₂
    float t2 = t * t;
    float mt = 1.0f - t;
    float mt2 = mt * mt;

    return ControlPoints.StartPoint * mt2 +
        ControlPoints.ControlPoint * (2.0f * mt * t) +
        ControlPoints.EndPoint * t2;
}

FVector AFrustum::CalculateChamferArcTangent(const FChamferArcControlPoints& ControlPoints, float t)
{
    // 二次贝塞尔曲线切线：B'(t) = 2(1-t)(P₁-P₀) + 2t(P₂-P₁)
    float mt = 1.0f - t;
    return (ControlPoints.ControlPoint - ControlPoints.StartPoint) * (2.0f * mt) +
        (ControlPoints.EndPoint - ControlPoints.ControlPoint) * (2.0f * t);
}

void AFrustum::CreateTopChamferGeometry(float StartZ)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections; // 沿倒角弧线的进度

        // 倒角起点：主侧面几何体的最顶层边缘顶点
        const float Alpha_Height = (StartZ + HalfHeight) / Parameters.Height;
        const float RadiusAtZ = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha_Height);
        const float BendFactor = FMath::Sin(Alpha_Height * PI);
        const float BentRadius = FMath::Max(
            RadiusAtZ + Parameters.BendAmount * BendFactor * RadiusAtZ,
            Parameters.MinBendRadius
        );

        // 倒角终点：在顶面上，半径减去倒角半径
        const float TopRadius = FMath::Max(0.0f, Parameters.TopRadius - Parameters.ChamferRadius);

        // 使用线性插值而不是贝塞尔曲线
        const float CurrentRadius = FMath::Lerp(BentRadius, TopRadius, alpha);
        const float CurrentZ = FMath::Lerp(StartZ, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            const float angle = s * AngleStep; // 圆周旋转角度

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 改进的法线计算：使用连续线性插值，确保法线平滑过渡
            const FVector SideNormal = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector TopNormal = FVector(0, 0, 1.0f);
            FVector Normal = FMath::Lerp(SideNormal, TopNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            FVector ToCenter = FVector(Position.X, Position.Y, 0).GetSafeNormal();
            if (FVector::DotProduct(Normal, ToCenter) < 0)
            {
                Normal = -Normal;
            }

            // 确保法线方向正确
            if (Normal.Z < 0.0f)
            {
                Normal = -Normal;
            }

            // 计算 UV - 使用与主体一致的UV映射
            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (Position.Z + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Position, Normal, FVector2D(U, V), FLinearColor::White));
        }

        // 只在有前一个环时才生成四边形，避免重复
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                // 顶部倒角：确保法线朝外，弧线方向正确
                // 使用一致的顶点顺序，避免重叠
                AddQuad(V00, V10, V11, V01, 0); // MaterialIndex 0 for top chamfer
            }
        }
        PrevRing = CurrentRing;
    }
}

void AFrustum::CreateBottomChamferGeometry(float StartZ)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 倒角起点：主侧面几何体的最底层边缘顶点
        const float Alpha_Height = (StartZ + HalfHeight) / Parameters.Height;
        const float RadiusAtZ = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha_Height);
        const float BendFactor = FMath::Sin(Alpha_Height * PI);
        const float BentRadius = FMath::Max(
            RadiusAtZ + Parameters.BendAmount * BendFactor * RadiusAtZ,
            Parameters.MinBendRadius
        );

        // 倒角终点：在底面上，半径减去倒角半径
        const float BottomRadius = FMath::Max(0.0f, Parameters.BottomRadius - Parameters.ChamferRadius);

        // 使用线性插值而不是贝塞尔曲线
        const float CurrentRadius = FMath::Lerp(BentRadius, BottomRadius, alpha);
        const float CurrentZ = FMath::Lerp(StartZ, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            const float angle = s * AngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 改进的法线计算：使用连续线性插值，确保法线平滑过渡
            const FVector SideNormal = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector BottomNormal = FVector(0, 0, -1.0f);
            FVector Normal = FMath::Lerp(SideNormal, BottomNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            FVector ToCenter = FVector(Position.X, Position.Y, 0).GetSafeNormal();
            if (FVector::DotProduct(Normal, ToCenter) < 0)
            {
                Normal = -Normal;
            }

            // 确保法线方向正确
            if (Normal.Z > 0.0f)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (Position.Z + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Position, Normal, FVector2D(U, V), FLinearColor::White));
        }

        // 只在有前一个环时才生成四边形，避免重复
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];

                // 底部倒角需要不同的顶点顺序，因为法线朝下
                // 使用一致的顶点顺序，避免重叠
                AddQuad(V00, V01, V11, V10, 0); // MaterialIndex 0 for bottom chamfer
            }
        }
        PrevRing = CurrentRing;
    }
}

// 修正后的 CreateEndCaps 函数 - 使用三角形连接到中心点
void AFrustum::CreateEndCaps()
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float StartAngle = 0.0f;
    const float EndAngle = FMath::DegreesToRadians(Parameters.ArcAngle);
    const float ChamferRadius = Parameters.ChamferRadius;

    // 起始端面法线：指向圆弧外部（远离中心）
    const FVector StartNormal = FVector(FMath::Sin(StartAngle), -FMath::Cos(StartAngle), 0.0f);

    // 结束端面法线：指向圆弧外部（远离中心）
    const FVector EndNormal = FVector(FMath::Sin(EndAngle), -FMath::Cos(EndAngle), 0.0f);

    // 计算倒角高度
    const float TopChamferHeight = FMath::Min(ChamferRadius, Parameters.TopRadius);
    const float BottomChamferHeight = FMath::Min(ChamferRadius, Parameters.BottomRadius);

    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomChamferHeight;
    const float EndZ = HalfHeight - TopChamferHeight;

    // 创建端面几何体 - 使用三角形连接到中心点
    CreateEndCapTriangles(StartAngle, StartNormal, true);  // 起始端面
    CreateEndCapTriangles(EndAngle, EndNormal, false);     // 结束端面
}

void AFrustum::CreateEndCapTriangles(float Angle, const FVector& Normal, bool IsStart)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.ChamferRadius;
    
    // 计算倒角高度
    const float TopChamferHeight = FMath::Min(ChamferRadius, Parameters.TopRadius);
    const float BottomChamferHeight = FMath::Min(ChamferRadius, Parameters.BottomRadius);
    
    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomChamferHeight;
    const float EndZ = HalfHeight - TopChamferHeight;

    // 中心点（0,0,0）
    const int32 CenterVertex = AddVertex(
        FVector(0, 0, 0),
        Normal,
        FVector2D(0.5f, 0.5f),
        FLinearColor::White
    );

    // 按照指定顺序存储顶点：上底中心 -> 上倒角圆弧 -> 侧边 -> 下倒角圆弧 -> 下底中心
    TArray<int32> OrderedVertices;
    
    // 1. 上底中心顶点
    const int32 TopCenterVertex = AddVertex(
        FVector(0, 0, HalfHeight),
        Normal,
        FVector2D(0.5f, 1.0f),
        FLinearColor::White
    );
    OrderedVertices.Add(TopCenterVertex);

    // 2. 上倒角弧线顶点（从顶面到侧边）
    if (Parameters.ChamferRadius > 0.0f)
    {
        const int32 TopChamferSections = Parameters.ChamferSections;
        for (int32 i = 0; i < TopChamferSections; ++i) // 从顶面到侧边
        {
            const float Alpha = static_cast<float>(i) / TopChamferSections;
            
            // 从顶面开始（HalfHeight位置）到侧边（EndZ位置）
            const float CurrentZ = FMath::Lerp(HalfHeight, EndZ, Alpha);
            
            // 计算侧边结束半径（在EndZ位置）
            const float Alpha_EndZ = (EndZ + HalfHeight) / Parameters.Height;
            const float Radius_EndZ = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha_EndZ);
            const float BendFactor_EndZ = FMath::Sin(Alpha_EndZ * PI);
            const float BentRadius_EndZ = FMath::Max(
                Radius_EndZ + Parameters.BendAmount * BendFactor_EndZ * Radius_EndZ,
                Parameters.MinBendRadius
            );
            
            // 从顶面半径到侧边半径（减去倒角半径，与主体几何体保持一致）
            const float TopRadius = FMath::Max(0.0f, Parameters.TopRadius - Parameters.ChamferRadius);
            const float CurrentRadius = FMath::Lerp(TopRadius, BentRadius_EndZ, Alpha);
            
            const FVector ChamferPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
            const int32 ChamferVertex = AddVertex(ChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height), FLinearColor::White);
            OrderedVertices.Add(ChamferVertex);
        }
    }

    // 3. 侧边顶点（从上到下）
    for (int32 h = 0; h <= Parameters.HeightSegments; ++h)
    {
        const float Z = FMath::Lerp(EndZ, StartZ, static_cast<float>(h) / Parameters.HeightSegments);
        const float Alpha = (Z + HalfHeight) / Parameters.Height;

        // 计算当前高度对应的半径
        const float Radius = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha);
        const float BendFactor = FMath::Sin(Alpha * PI);
        const float BentRadius = FMath::Max(
            Radius + Parameters.BendAmount * BendFactor * Radius,
            Parameters.MinBendRadius
        );

        const FVector EdgePos = FVector(BentRadius * FMath::Cos(Angle), BentRadius * FMath::Sin(Angle), Z);
        const int32 EdgeVertex = AddVertex(EdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, Alpha), FLinearColor::White);
        OrderedVertices.Add(EdgeVertex);
    }

    // 4. 下倒角弧线顶点（从侧边到底面）
    if (Parameters.ChamferRadius > 0.0f)
    {
        const int32 BottomChamferSections = Parameters.ChamferSections;
        for (int32 i = 1; i <= BottomChamferSections; ++i) // 从侧边到底面
        {
            const float Alpha = static_cast<float>(i) / BottomChamferSections;
            
            // 从侧边开始（StartZ位置）到底面（-HalfHeight位置）
            const float CurrentZ = FMath::Lerp(StartZ, -HalfHeight, Alpha);
            
            // 计算侧边起始半径（在StartZ位置）
            const float Alpha_StartZ = (StartZ + HalfHeight) / Parameters.Height;
            const float Radius_StartZ = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha_StartZ);
            const float BendFactor_StartZ = FMath::Sin(Alpha_StartZ * PI);
            const float BentRadius_StartZ = FMath::Max(
                Radius_StartZ + Parameters.BendAmount * BendFactor_StartZ * Radius_StartZ,
                Parameters.MinBendRadius
            );
            
            // 从侧边半径到底面半径（减去倒角半径，与主体几何体保持一致）
            const float BottomRadius = FMath::Max(0.0f, Parameters.BottomRadius - Parameters.ChamferRadius);
            const float CurrentRadius = FMath::Lerp(BentRadius_StartZ, BottomRadius, Alpha);
            
            const FVector ChamferPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
            const int32 ChamferVertex = AddVertex(ChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height), FLinearColor::White);
            OrderedVertices.Add(ChamferVertex);
        }
    }

    // 5. 下底中心顶点
    const int32 BottomCenterVertex = AddVertex(
        FVector(0, 0, -HalfHeight),
        Normal,
        FVector2D(0.5f, 0.0f),
        FLinearColor::White
    );
    OrderedVertices.Add(BottomCenterVertex);

    // 生成三角形：每两个相邻顶点与中心点形成三角形
    for (int32 i = 0; i < OrderedVertices.Num() - 1; ++i)
    {
        const int32 V1 = OrderedVertices[i];
        const int32 V2 = OrderedVertices[i + 1];
        
        // 创建三角形：V1 -> V2 -> 中心点
        if (IsStart)
        {
			AddTriangle(V1, V2, CenterVertex, 0);
        }
        else
        {
			AddTriangle(V2, V1, CenterVertex, 0);
        }
    }
}

void AFrustum::CreateChamferArcTriangles(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    
    // 中心点（0,0,0）
    const int32 CenterVertex = AddVertex(
        FVector(0, 0, 0),
        Normal,
        FVector2D(0.5f, 0.5f),
        FLinearColor::White
    );

    // 计算倒角弧线的起点和终点半径
    float StartRadius, EndRadius;
    if (IsTop)
    {
        // 顶部倒角：从侧面边缘到顶面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Parameters.Height;
        const float Radius_Start = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha_Start);
        const float BendFactor_Start = FMath::Sin(Alpha_Start * PI);
        StartRadius = FMath::Max(
            Radius_Start + Parameters.BendAmount * BendFactor_Start * Radius_Start,
            Parameters.MinBendRadius
        );
        EndRadius = FMath::Max(0.0f, Parameters.TopRadius - Parameters.ChamferRadius);
    }
    else
    {
        // 底部倒角：从侧面边缘到底面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Parameters.Height;
        const float Radius_Start = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha_Start);
        const float BendFactor_Start = FMath::Sin(Alpha_Start * PI);
        StartRadius = FMath::Max(
            Radius_Start + Parameters.BendAmount * BendFactor_Start * Radius_Start,
            Parameters.MinBendRadius
        );
        EndRadius = FMath::Max(0.0f, Parameters.BottomRadius - Parameters.ChamferRadius);
    }

    // 生成倒角弧线的三角形
    for (int32 i = 0; i < ChamferSections; ++i)
    {
        const float Alpha = static_cast<float>(i) / ChamferSections;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, Alpha);
        const float CurrentZ = FMath::Lerp(Z1, Z2, Alpha);

        // 当前倒角弧线点
        const FVector ArcPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
        const int32 ArcVertex = AddVertex(ArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height), FLinearColor::White);

        // 下一个倒角弧线点
        const float NextAlpha = static_cast<float>(i + 1) / ChamferSections;
        const float NextRadius = FMath::Lerp(StartRadius, EndRadius, NextAlpha);
        const float NextZ = FMath::Lerp(Z1, Z2, NextAlpha);

        const FVector NextArcPos = FVector(NextRadius * FMath::Cos(Angle), NextRadius * FMath::Sin(Angle), NextZ);
        const int32 NextArcVertex = AddVertex(NextArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (NextZ + HalfHeight) / Parameters.Height), FLinearColor::White);

        // 创建三角形：当前弧线点 -> 下一个弧线点 -> 中心点
        AddTriangle(ArcVertex, NextArcVertex, CenterVertex, 0);
    }
}

void AFrustum::CreateChamferArcTrianglesWithCaps(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop, int32 CenterVertex, int32 CapCenterVertex)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    
    // 计算倒角弧线的起点和终点半径
    float StartRadius, EndRadius;
    if (IsTop)
    {
        // 顶部倒角：从侧面边缘到顶面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Parameters.Height;
        const float Radius_Start = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha_Start);
        const float BendFactor_Start = FMath::Sin(Alpha_Start * PI);
        StartRadius = FMath::Max(
            Radius_Start + Parameters.BendAmount * BendFactor_Start * Radius_Start,
            Parameters.MinBendRadius
        );
        EndRadius = FMath::Max(0.0f, Parameters.TopRadius - Parameters.ChamferRadius);
    }
    else
    {
        // 底部倒角：从侧面边缘到底面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Parameters.Height;
        const float Radius_Start = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha_Start);
        const float BendFactor_Start = FMath::Sin(Alpha_Start * PI);
        StartRadius = FMath::Max(
            Radius_Start + Parameters.BendAmount * BendFactor_Start * Radius_Start,
            Parameters.MinBendRadius
        );
        EndRadius = FMath::Max(0.0f, Parameters.BottomRadius - Parameters.ChamferRadius);
    }

    // 生成倒角弧线的三角形
    for (int32 i = 0; i < ChamferSections; ++i)
    {
        const float Alpha = static_cast<float>(i) / ChamferSections;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, Alpha);
        const float CurrentZ = FMath::Lerp(Z1, Z2, Alpha);

        // 当前倒角弧线点
        const FVector ArcPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
        const int32 ArcVertex = AddVertex(ArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height), FLinearColor::White);

        // 下一个倒角弧线点
        const float NextAlpha = static_cast<float>(i + 1) / ChamferSections;
        const float NextRadius = FMath::Lerp(StartRadius, EndRadius, NextAlpha);
        const float NextZ = FMath::Lerp(Z1, Z2, NextAlpha);

        const FVector NextArcPos = FVector(NextRadius * FMath::Cos(Angle), NextRadius * FMath::Sin(Angle), NextZ);
        const int32 NextArcVertex = AddVertex(NextArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (NextZ + HalfHeight) / Parameters.Height), FLinearColor::White);

        // 创建三角形：当前弧线点 -> 下一个弧线点 -> 中心点（0,0,0）
        AddTriangle(ArcVertex, NextArcVertex, CenterVertex, 0);
    }

    // 连接最边缘的点到上/下底中心点
    // 最边缘的点是倒角弧线的起点（i=0）
    const float StartAlpha = 0.0f;
    const float StartRadius_Edge = FMath::Lerp(StartRadius, EndRadius, StartAlpha);
    const float StartZ_Edge = FMath::Lerp(Z1, Z2, StartAlpha);
    const FVector StartEdgePos = FVector(StartRadius_Edge * FMath::Cos(Angle), StartRadius_Edge * FMath::Sin(Angle), StartZ_Edge);
    const int32 StartEdgeVertex = AddVertex(StartEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (StartZ_Edge + HalfHeight) / Parameters.Height), FLinearColor::White);

    // 最边缘的点是倒角弧线的终点（i=ChamferSections）
    const float EndAlpha = 1.0f;
    const float EndRadius_Edge = FMath::Lerp(StartRadius, EndRadius, EndAlpha);
    const float EndZ_Edge = FMath::Lerp(Z1, Z2, EndAlpha);
    const FVector EndEdgePos = FVector(EndRadius_Edge * FMath::Cos(Angle), EndRadius_Edge * FMath::Sin(Angle), EndZ_Edge);
    const int32 EndEdgeVertex = AddVertex(EndEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (EndZ_Edge + HalfHeight) / Parameters.Height), FLinearColor::White);

    // 创建三角形：最边缘点 -> 上/下底中心点 -> 中心点（0,0,0）    // 注意顶点顺序，确保法线方向正确
    if (IsTop)
    {
        // 顶部：StartEdgeVertex -> CapCenterVertex -> CenterVertex
        AddTriangle(StartEdgeVertex, CapCenterVertex, CenterVertex, 0);
        // 顶部：EndEdgeVertex -> CapCenterVertex -> CenterVertex
        AddTriangle(EndEdgeVertex, CapCenterVertex, CenterVertex, 0);
    }
    else
    {
        // 底部：StartEdgeVertex -> CenterVertex -> CapCenterVertex（修正顶点顺序）
        AddTriangle(StartEdgeVertex, CenterVertex, CapCenterVertex, 0);
        // 底部：EndEdgeVertex -> CenterVertex -> CapCenterVertex（修正顶点顺序）
        AddTriangle(EndEdgeVertex, CenterVertex, CapCenterVertex, 0);
    }
}


void AFrustum::ApplyMaterial()
{
    if (!MeshComponent)
    {
        return;
    }

    // 创建调试材质来可视化法线
    static UMaterial* DebugNormalMaterial = nullptr;
    
    if (!DebugNormalMaterial)
    {
        // 尝试加载调试材质
        static ConstructorHelpers::FObjectFinder<UMaterial> DebugMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
        if (DebugMaterialFinder.Succeeded())
        {
            DebugNormalMaterial = DebugMaterialFinder.Object;
        }
    }

    // 应用调试材质
    if (DebugNormalMaterial)
    {
        MeshComponent->SetMaterial(0, DebugNormalMaterial);
    }
    else
    {
        // 如果没有找到调试材质，使用默认材质
        static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
        if (DefaultMaterialFinder.Succeeded())
        {
            MeshComponent->SetMaterial(0, DefaultMaterialFinder.Object);
        }
    }

    // 输出法线统计信息
    if (MeshData.Normals.Num() > 0)
    {
        FVector MinNormal = MeshData.Normals[0];
        FVector MaxNormal = MeshData.Normals[0];
        FVector SumNormal = FVector::ZeroVector;

        for (const FVector& Normal : MeshData.Normals)
        {
            MinNormal = MinNormal.ComponentMin(Normal);
            MaxNormal = MaxNormal.ComponentMax(Normal);
            SumNormal += Normal;
        }

        FVector AvgNormal = SumNormal / MeshData.Normals.Num();
        UE_LOG(LogTemp, Log, TEXT("Frustum Normal Stats: Count=%d, Min=%s, Max=%s, Avg=%s"), 
               MeshData.Normals.Num(), *MinNormal.ToString(), *MaxNormal.ToString(), *AvgNormal.ToString());
    }
}