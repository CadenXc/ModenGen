// Frustum.cpp

#include "Frustum.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

AFrustum::AFrustum()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建网格组件
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FrustumMesh"));
    RootComponent = MeshComponent;

    // 配置网格属性
    MeshComponent->bUseAsyncCooking = true;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetSimulatePhysics(false);

    // 初始生成
    GenerateGeometry();
}

void AFrustum::BeginPlay()
{
    Super::BeginPlay();
    GenerateGeometry();
}

void AFrustum::PostLoad()
{
    Super::PostLoad();
    GenerateGeometry();
}

#if WITH_EDITOR
void AFrustum::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    static const TArray<FName> RelevantProperties = {
        "TopRadius", "BottomRadius", "Height",
        "Sides", "HeightSegments",
        "ChamferRadius", "ChamferSections",
        "BendAmount", "MinBendRadius", "ArcAngle",
        "CapThickness"
    };

    if (RelevantProperties.Contains(PropertyName))
    {
        bGeometryDirty = true;
        GenerateGeometry();
    }
}
#endif

void AFrustum::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bGeometryDirty)
    {
        GenerateGeometry();
        bGeometryDirty = false;
    }
}

void AFrustum::Regenerate()
{
    GenerateGeometry();
}

void AFrustum::GenerateGeometry()
{
    if (!MeshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("Mesh component is missing!"));
        return;
    }

    // 清除现有网格
    MeshComponent->ClearAllMeshSections();

    // 验证参数
    Parameters.TopRadius = FMath::Max(0.01f, Parameters.TopRadius);
    Parameters.BottomRadius = FMath::Max(0.01f, Parameters.BottomRadius);
    Parameters.Height = FMath::Max(0.01f, Parameters.Height);
    Parameters.Sides = FMath::Max(3, Parameters.Sides);
    Parameters.HeightSegments = FMath::Max(1, Parameters.HeightSegments);
    Parameters.ChamferRadius = FMath::Max(0.0f, Parameters.ChamferRadius);
    Parameters.ChamferSections = FMath::Max(1, Parameters.ChamferSections);
    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);
    Parameters.MinBendRadius = FMath::Max(1.0f, Parameters.MinBendRadius);
    Parameters.CapThickness = FMath::Max(0.0f, Parameters.CapThickness);

    // 清除现有网格数据
    MeshData.Clear();

    // 预估内存需求
    const int32 VertexCountEstimate =
        (Parameters.HeightSegments + 1) * (Parameters.Sides + 1) * 4 +
        (Parameters.ArcAngle < 360.0f ? Parameters.HeightSegments * 4 : 0);

    const int32 TriangleCountEstimate =
        Parameters.HeightSegments * Parameters.Sides * 6 +
        (Parameters.ArcAngle < 360.0f ? Parameters.HeightSegments * 6 : 0);

    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate);

    const float HalfHeight = Parameters.Height / 2.0f;
    const float TopChamferHeight = FMath::Min(Parameters.ChamferRadius, Parameters.TopRadius);
    const float BottomChamferHeight = FMath::Min(Parameters.ChamferRadius, Parameters.BottomRadius);

    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomChamferHeight;
    const float EndZ = HalfHeight - TopChamferHeight;

    // 生成几何部件 - 确保不重叠
    if (EndZ > StartZ) // 只有当主体高度大于0时才生成主体
    {
        CreateSideGeometry(StartZ, EndZ);
    }
    
    // 生成倒角几何体
    if (Parameters.ChamferRadius > 0.0f)
    {
        CreateTopChamferGeometry(EndZ);
        CreateBottomChamferGeometry(StartZ);
    }
    
    // 生成顶底面几何体
    CreateTopGeometry(HalfHeight);
    CreateBottomGeometry(-HalfHeight);

    if (Parameters.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CreateEndCaps();
    }

    // 验证数据完整性
    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Generated mesh data is invalid!"));
        return;
    }

    // 更新ProceduralMesh组件
    UpdateProceduralMeshComponent();
    
    // 运行法线验证测试
    TestNormalValidation();
}

int32 AFrustum::AddVertex(const FVector& Position, const FVector& Normal, 
                         const FVector2D& UV, const FLinearColor& Color)
{
    return MeshData.AddVertex(Position, Normal, UV, Color);
}

void AFrustum::AddQuad(int32 V1, int32 V2, int32 V3, int32 V4, int32 MaterialIndex)
{
    MeshData.AddQuad(V1, V2, V3, V4, MaterialIndex);
}

void AFrustum::AddTriangle(int32 V1, int32 V2, int32 V3, int32 MaterialIndex)
{
    MeshData.AddTriangle(V1, V2, V3, MaterialIndex);
}

void AFrustum::UpdateProceduralMeshComponent()
{
    if (!MeshComponent || MeshData.Vertices.Num() == 0)
    {
        return;
    }

    // 验证法线数据
    ValidateAndFixNormals();

    // 提交网格
    if (MeshData.Vertices.Num() > 0)
    {
        MeshComponent->CreateMeshSection_LinearColor(
            0,
            MeshData.Vertices,
            MeshData.Triangles,
            MeshData.Normals,
            MeshData.UVs,
            MeshData.VertexColors,
            MeshData.Tangents,
            true // 启用碰撞
        );

        // 应用材质
        ApplyMaterial();
    }
}

// 新增：法线验证和修复函数
void AFrustum::ValidateAndFixNormals()
{
    if (MeshData.Vertices.Num() == 0 || MeshData.Normals.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ValidateAndFixNormals: No mesh data to validate"));
        return;
    }

    // 确保法线数组大小与顶点数组一致
    if (MeshData.Normals.Num() != MeshData.Vertices.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("ValidateAndFixNormals: Normal count (%d) doesn't match vertex count (%d)"), 
               MeshData.Normals.Num(), MeshData.Vertices.Num());
        return;
    }

    int32 InvalidNormals = 0;
    int32 ZeroNormals = 0;
    int32 FlippedNormals = 0;
    FVector AverageNormal = FVector::ZeroVector;

    // 检查每个法线
    for (int32 i = 0; i < MeshData.Normals.Num(); ++i)
    {
        FVector& Normal = MeshData.Normals[i];
        const FVector& Vertex = MeshData.Vertices[i];

        // 检查法线是否为零向量
        if (Normal.IsNearlyZero())
        {
            ZeroNormals++;
            // 尝试从顶点位置计算法线
            Normal = Vertex.GetSafeNormal();
            if (Normal.IsNearlyZero())
            {
                Normal = FVector(0, 0, 1); // 默认向上
            }
        }

        // 检查法线是否已标准化
        const float NormalLength = Normal.Size();
        if (!FMath::IsNearlyEqual(NormalLength, 1.0f, 0.01f))
        {
            InvalidNormals++;
            Normal = Normal.GetSafeNormal();
        }

        // 检查法线方向（对于Frustum，法线应该指向外部）
        if (!Normal.IsNearlyZero())
        {
            // 对于侧面，法线应该指向远离中心的方向
            if (FMath::Abs(Vertex.Z) < Parameters.Height / 2.0f - 0.1f) // 侧面顶点
            {
                FVector ToCenter = FVector(Vertex.X, Vertex.Y, 0).GetSafeNormal();
                if (FVector::DotProduct(Normal, ToCenter) < 0)
                {
                    FlippedNormals++;
                    Normal = -Normal;
                }
            }
            // 对于顶面，法线应该向上
            else if (Vertex.Z > 0)
            {
                if (Normal.Z < 0)
                {
                    FlippedNormals++;
                    Normal = -Normal;
                }
            }
            // 对于底面，法线应该向下
            else if (Vertex.Z < 0)
            {
                if (Normal.Z > 0)
                {
                    FlippedNormals++;
                    Normal = -Normal;
                }
            }
        }

        AverageNormal += Normal;
    }

    // 输出统计信息
    if (InvalidNormals > 0 || ZeroNormals > 0 || FlippedNormals > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Frustum Normal Validation: Invalid=%d, Zero=%d, Flipped=%d"), 
               InvalidNormals, ZeroNormals, FlippedNormals);
    }

    // 输出平均法线方向
    if (MeshData.Normals.Num() > 0)
    {
        AverageNormal = AverageNormal.GetSafeNormal();
        UE_LOG(LogTemp, Log, TEXT("Frustum Average Normal: %s"), *AverageNormal.ToString());
    }

    // 验证三角形法线一致性
    ValidateTriangleNormals();
}

// 新增：简单的法线测试函数
void AFrustum::TestNormalValidation()
{
    UE_LOG(LogTemp, Log, TEXT("=== Frustum Normal Validation Test ==="));
    
    // 测试基本的法线计算
    FVector TestNormal1 = FVector(1, 0, 0);
    FVector TestNormal2 = FVector(0, 1, 0);
    FVector TestNormal3 = FVector(0, 0, 1);
    
    UE_LOG(LogTemp, Log, TEXT("Test Normal 1: %s"), *TestNormal1.ToString());
    UE_LOG(LogTemp, Log, TEXT("Test Normal 2: %s"), *TestNormal2.ToString());
    UE_LOG(LogTemp, Log, TEXT("Test Normal 3: %s"), *TestNormal3.ToString());
    
    // 测试点积计算
    float Dot1 = FVector::DotProduct(TestNormal1, TestNormal2);
    float Dot2 = FVector::DotProduct(TestNormal1, TestNormal3);
    float Dot3 = FVector::DotProduct(TestNormal2, TestNormal3);
    
    UE_LOG(LogTemp, Log, TEXT("Dot Product Tests: %f, %f, %f"), Dot1, Dot2, Dot3);
    
    // 测试法线标准化
    FVector Unnormalized = FVector(2, 3, 4);
    FVector Normalized = Unnormalized.GetSafeNormal();
    UE_LOG(LogTemp, Log, TEXT("Unnormalized: %s"), *Unnormalized.ToString());
    UE_LOG(LogTemp, Log, TEXT("Normalized: %s"), *Normalized.ToString());
    UE_LOG(LogTemp, Log, TEXT("Normalized Length: %f"), Normalized.Size());
    
    UE_LOG(LogTemp, Log, TEXT("=== Test Complete ==="));
}

// 新增：验证三角形法线一致性
void AFrustum::ValidateTriangleNormals()
{
    if (MeshData.Triangles.Num() == 0)
    {
        return;
    }

    int32 InconsistentTriangles = 0;
    int32 DegenerateTriangles = 0;

    for (int32 i = 0; i < MeshData.Triangles.Num(); i += 3)
    {
        if (i + 2 >= MeshData.Triangles.Num())
        {
            break;
        }

        const int32 V0 = MeshData.Triangles[i];
        const int32 V1 = MeshData.Triangles[i + 1];
        const int32 V2 = MeshData.Triangles[i + 2];

        if (V0 < 0 || V1 < 0 || V2 < 0 || 
            V0 >= MeshData.Vertices.Num() || 
            V1 >= MeshData.Vertices.Num() || 
            V2 >= MeshData.Vertices.Num())
        {
            UE_LOG(LogTemp, Error, TEXT("Invalid triangle indices: %d, %d, %d"), V0, V1, V2);
            continue;
        }

        // 计算面法线
        const FVector Edge1 = MeshData.Vertices[V1] - MeshData.Vertices[V0];
        const FVector Edge2 = MeshData.Vertices[V2] - MeshData.Vertices[V0];
        const FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        if (FaceNormal.IsNearlyZero())
        {
            DegenerateTriangles++;
            continue;
        }

        // 检查顶点法线是否与面法线一致
        const FVector& Normal0 = MeshData.Normals[V0];
        const FVector& Normal1 = MeshData.Normals[V1];
        const FVector& Normal2 = MeshData.Normals[V2];

        const float Dot0 = FVector::DotProduct(FaceNormal, Normal0);
        const float Dot1 = FVector::DotProduct(FaceNormal, Normal1);
        const float Dot2 = FVector::DotProduct(FaceNormal, Normal2);

        // 如果法线方向差异太大，标记为不一致
        if (Dot0 < 0.5f || Dot1 < 0.5f || Dot2 < 0.5f)
        {
            InconsistentTriangles++;
        }
    }

    if (InconsistentTriangles > 0 || DegenerateTriangles > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Triangle Normal Issues: Inconsistent=%d, Degenerate=%d"), 
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
        for (int32 i = 0; i <= TopChamferSections; ++i) // 从顶面到侧边
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
    for (int32 h = 1; h < Parameters.HeightSegments; ++h)
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
        for (int32 i = 0; i <= BottomChamferSections; ++i) // 从侧边到底面
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