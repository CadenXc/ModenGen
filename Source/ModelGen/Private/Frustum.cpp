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

    // 创建新网格数据
    FMeshSection MeshData;

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

    const float StartZ = -HalfHeight + BottomChamferHeight;
    const float EndZ = HalfHeight - TopChamferHeight;

    // 生成几何部件
    CreateSideGeometry(MeshData, StartZ, EndZ);
    CreateTopChamferGeometry(MeshData, EndZ);
    CreateBottomChamferGeometry(MeshData, -HalfHeight + BottomChamferHeight);
    CreateTopGeometry(MeshData, HalfHeight);
    CreateBottomGeometry(MeshData, -HalfHeight);


    if (Parameters.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CreateEndCaps(MeshData);
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

int32 AFrustum::AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV)
{
    const int32 Index = Section.Vertices.Add(Position);
    Section.Normals.Add(Normal);
    Section.UVs.Add(UV);
    Section.VertexColors.Add(FLinearColor::White);

    // 切线计算
    FVector Tangent = FVector::CrossProduct(Normal, FVector::UpVector);
    if (Tangent.IsNearlyZero())
    {
        Tangent = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    Tangent.Normalize();

    Section.Tangents.Add(FProcMeshTangent(Tangent, false));

    return Index;
}

void AFrustum::AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4)
{
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V2);
    Section.Triangles.Add(V3);

    Section.Triangles.Add(V1);
    Section.Triangles.Add(V3);
    Section.Triangles.Add(V4);
}

void AFrustum::AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3)
{
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V2);
    Section.Triangles.Add(V3);
}

void AFrustum::CreateSideGeometry(FMeshSection& Section, float StartZ, float EndZ)
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

            Ring[s] = AddVertex(Section, FVector(X, Y, Z), Normal, FVector2D(U, V));
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

            AddQuad(Section, V00, V10, V11, V01);
        }
    }
}


void AFrustum::CreateTopGeometry(FMeshSection& Section, float Z)
{
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;
    const float Radius = FMath::Max(0.0f, Parameters.TopRadius - Parameters.ChamferRadius);

    // 中心顶点
    const int32 CenterVertex = AddVertex(
        Section,
        FVector(0, 0, Z),
        FVector(0, 0, 1),
        FVector2D(0.5f, 0.5f)
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
            Section,
            FVector(X, Y, Z),
            FVector(0, 0, 1),
            FVector2D(U, V)
        );
    }

    // 创建顶部三角扇
    for (int32 s = 0; s < Parameters.Sides; ++s)
    {
        AddTriangle(
            Section,
            CenterVertex,
            TopRing[s + 1],
            TopRing[s]
        );
    }
}

void AFrustum::CreateBottomGeometry(FMeshSection& Section, float Z)
{
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;
    const float Radius = FMath::Max(0.0f, Parameters.BottomRadius - Parameters.ChamferRadius);

    // 中心顶点
    const int32 CenterVertex = AddVertex(
        Section,
        FVector(0, 0, Z),
        FVector(0, 0, -1),
        FVector2D(0.5f, 0.5f)
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
            Section,
            FVector(X, Y, Z),
            FVector(0, 0, -1),
            FVector2D(U, V)
        );
    }

    // 创建底部三角扇
    for (int32 s = 0; s < Parameters.Sides; ++s)
    {
        AddTriangle(
            Section,
            CenterVertex,
            BottomRing[s],
            BottomRing[s + 1]
        );
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

    // 控制点：位于原始的尖角处，由起点半径和终点高度确定
    Points.ControlPoint = FVector(SideVertex.X, 0, TopBottomVertex.Z);

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

void AFrustum::CreateTopChamferGeometry(FMeshSection& Section, float StartZ)
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
        const FVector ChamferStartPoint = FVector(BentRadius, 0, StartZ);

        // 倒角终点：在顶面上，半径减去倒角半径
        const float TopRadius = FMath::Max(0.0f, Parameters.TopRadius - Parameters.ChamferRadius);
        const FVector ChamferEndPoint = FVector(TopRadius, 0, HalfHeight);

        // 调用修正后的函数，传入起点和终点
        FChamferArcControlPoints ControlPoints = CalculateChamferControlPoints(ChamferStartPoint, ChamferEndPoint);

        TArray<int32> CurrentRing;

        // ... (以下部分保持不变，使用新的ControlPoints) ...

        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            const float angle = s * AngleStep; // 圆周旋转角度

            // 使用贝塞尔曲线计算倒角弧线上的点
            const FVector LocalChamferPos = CalculateChamferArcPoint(ControlPoints, alpha);
            const FVector Position = FVector(
                LocalChamferPos.X * FMath::Cos(angle),
                LocalChamferPos.X * FMath::Sin(angle),
                LocalChamferPos.Z
            );

            // 计算法线
            const FVector Tangent = CalculateChamferArcTangent(ControlPoints, alpha);
            const FVector Normal = FVector(
                Tangent.X * FMath::Cos(angle),
                Tangent.X * FMath::Sin(angle),
                Tangent.Z
            ).GetSafeNormal();

            // 计算 UV
            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (Position.Z + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        if (i > 0)
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

void AFrustum::CreateBottomChamferGeometry(FMeshSection& Section, float StartZ)
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
        const FVector ChamferStartPoint = FVector(BentRadius, 0, StartZ);

        // 倒角终点：在底面上，半径减去倒角半径
        const float BottomRadius = FMath::Max(0.0f, Parameters.BottomRadius - Parameters.ChamferRadius);
        const FVector ChamferEndPoint = FVector(BottomRadius, 0, -HalfHeight);

        // 调用修正后的函数，传入起点和终点
        FChamferArcControlPoints ControlPoints = CalculateChamferControlPoints(ChamferStartPoint, ChamferEndPoint);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            const float angle = s * AngleStep;

            const FVector LocalChamferPos = CalculateChamferArcPoint(ControlPoints, alpha);
            const FVector Position = FVector(
                LocalChamferPos.X * FMath::Cos(angle),
                LocalChamferPos.X * FMath::Sin(angle),
                LocalChamferPos.Z
            );

            const FVector Tangent = CalculateChamferArcTangent(ControlPoints, alpha);
            const FVector Normal = FVector(
                Tangent.X * FMath::Cos(angle),
                Tangent.X * FMath::Sin(angle),
                Tangent.Z
            ).GetSafeNormal();

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (Position.Z + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        if (i > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];

                // 注意：这里为了保持法线朝外，调整了顶点顺序
                AddQuad(Section, V00, V01, V11, V10);
            }
        }
        PrevRing = CurrentRing;
    }
}

// 修正后的 CreateEndCaps 函数
void AFrustum::CreateEndCaps(FMeshSection& Section)
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

        const int32 V1 = AddVertex(Section, StartCapVerticesOuter[h], StartNormal, UVCorners[0]);
        const int32 V2 = AddVertex(Section, StartCapVerticesInner[h], StartNormal, UVCorners[1]);
        const int32 V3 = AddVertex(Section, StartCapVerticesOuter[h + 1], StartNormal, UVCorners[2]);
        const int32 V4 = AddVertex(Section, StartCapVerticesInner[h + 1], StartNormal, UVCorners[3]);

        // 正确的四边形连接顺序
        AddQuad(Section, V1, V2, V4, V3);
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

        const int32 V1 = AddVertex(Section, EndCapVerticesOuter[h], EndNormal, UVCorners[0]);
        const int32 V2 = AddVertex(Section, EndCapVerticesInner[h], EndNormal, UVCorners[1]);
        const int32 V3 = AddVertex(Section, EndCapVerticesOuter[h + 1], EndNormal, UVCorners[2]);
        const int32 V4 = AddVertex(Section, EndCapVerticesInner[h + 1], EndNormal, UVCorners[3]);

        // 正确的四边形连接顺序
        AddQuad(Section, V1, V3, V4, V2);
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