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

        ApplyMaterial();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Generated frustum mesh has no vertices"));
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

// 修正后的 CreateEndCaps 函数
void AFrustum::CreateEndCaps()
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float StartAngle = 0.0f;
    const float EndAngle = FMath::DegreesToRadians(Parameters.ArcAngle);

    // 起始端面法线：指向圆弧内部
    const FVector StartNormal = FVector(FMath::Sin(StartAngle), -FMath::Cos(StartAngle), 0.0f);

    // 结束端面法线：指向圆弧内部
    const FVector EndNormal = FVector(FMath::Sin(EndAngle), -FMath::Cos(EndAngle), 0.0f);

    // 缓存每层高度的顶点，因为我们需要内外两圈
    TArray<FVector> StartCapVerticesOuter;
    TArray<FVector> StartCapVerticesInner;
    TArray<FVector> EndCapVerticesOuter;
    TArray<FVector> EndCapVerticesInner;

    // 生成端面顶点环
    for (int32 h = 0; h <= Parameters.HeightSegments; ++h)
    {
        const float Z = -HalfHeight + h * (Parameters.Height / Parameters.HeightSegments);
        const float Alpha = (Z + HalfHeight) / Parameters.Height;

        // 与侧面几何体保持一致的半径计算
        const float Radius = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha);
        const float BendFactor = FMath::Sin(Alpha * PI);
        const float BentRadius = FMath::Max(
            Radius + Parameters.BendAmount * BendFactor * Radius,
            Parameters.MinBendRadius
        );

        // 外圈顶点（侧面边缘）
        const FVector StartEdgePos = FVector(BentRadius * FMath::Cos(StartAngle), BentRadius * FMath::Sin(StartAngle), Z);
        StartCapVerticesOuter.Add(StartEdgePos);
        const FVector EndEdgePos = FVector(BentRadius * FMath::Cos(EndAngle), BentRadius * FMath::Sin(EndAngle), Z);
        EndCapVerticesOuter.Add(EndEdgePos);

        // 内圈顶点（中心轴上的点）
        const FVector StartInnerPos = FVector(0, 0, Z);
        StartCapVerticesInner.Add(StartInnerPos);
        const FVector EndInnerPos = FVector(0, 0, Z);
        EndCapVerticesInner.Add(EndInnerPos);
    }

    // 构建起始端面
    for (int32 h = 0; h < Parameters.HeightSegments; ++h)
    {
        const FVector2D UVCorners[4] = {
            FVector2D(0, (float)h / Parameters.HeightSegments),
            FVector2D(1, (float)h / Parameters.HeightSegments),
            FVector2D(0, (float)(h + 1) / Parameters.HeightSegments),
            FVector2D(1, (float)(h + 1) / Parameters.HeightSegments)
        };

        const int32 V1 = AddVertex(StartCapVerticesOuter[h], StartNormal, UVCorners[0], FLinearColor::White);
        const int32 V2 = AddVertex(StartCapVerticesInner[h], StartNormal, UVCorners[1], FLinearColor::White);
        const int32 V3 = AddVertex(StartCapVerticesOuter[h + 1], StartNormal, UVCorners[2], FLinearColor::White);
        const int32 V4 = AddVertex(StartCapVerticesInner[h + 1], StartNormal, UVCorners[3], FLinearColor::White);

        // 正确的四边形连接顺序
        AddQuad(V1, V2, V4, V3, 0); // MaterialIndex 0 for end caps
    }

    // 构建结束端面
    for (int32 h = 0; h < Parameters.HeightSegments; ++h)
    {
        const FVector2D UVCorners[4] = {
            FVector2D(0, (float)h / Parameters.HeightSegments),
            FVector2D(1, (float)h / Parameters.HeightSegments),
            FVector2D(0, (float)(h + 1) / Parameters.HeightSegments),
            FVector2D(1, (float)(h + 1) / Parameters.HeightSegments)
        };

        const int32 V1 = AddVertex(EndCapVerticesOuter[h], EndNormal, UVCorners[0], FLinearColor::White);
        const int32 V2 = AddVertex(EndCapVerticesInner[h], EndNormal, UVCorners[1], FLinearColor::White);
        const int32 V3 = AddVertex(EndCapVerticesOuter[h + 1], EndNormal, UVCorners[2], FLinearColor::White);
        const int32 V4 = AddVertex(EndCapVerticesInner[h + 1], EndNormal, UVCorners[3], FLinearColor::White);

        // 正确的四边形连接顺序
        AddQuad(V1, V3, V4, V2, 0); // MaterialIndex 0 for end caps
    }
}


void AFrustum::ApplyMaterial()
{
    static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (DefaultMaterial.Succeeded())
    {
        MeshComponent->SetMaterial(0, DefaultMaterial.Object);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to find default material. Using fallback."));

        // 创建简单回退材质
        UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        if (FallbackMaterial)
        {
            MeshComponent->SetMaterial(0, FallbackMaterial);
        }
    }
}