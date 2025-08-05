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
        "TopSides", "BottomSides", "HeightSegments",
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
    Parameters.TopSides = FMath::Max(3, Parameters.TopSides);
    Parameters.BottomSides = FMath::Max(3, Parameters.BottomSides);
    Parameters.HeightSegments = FMath::Max(1, Parameters.HeightSegments);
    Parameters.ChamferSections = FMath::Max(1, Parameters.ChamferSections);
    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);
    Parameters.MinBendRadius = FMath::Max(1.0f, Parameters.MinBendRadius);
    Parameters.CapThickness = FMath::Max(0.0f, Parameters.CapThickness);

    // 确保顶部边数不超过底部
    Parameters.TopSides = FMath::Min(Parameters.TopSides, Parameters.BottomSides);

    // 创建新网格数据
    FMeshSection MeshData;

    // 预估内存需求
    const int32 TotalSides = FMath::Max(Parameters.TopSides, Parameters.BottomSides);
    const int32 VertexCountEstimate =
        (Parameters.HeightSegments + 1) * (TotalSides + 1) * 4 +
        Parameters.ChamferSections * TotalSides * 8 +
        (Parameters.ArcAngle < 360.0f ? Parameters.HeightSegments * 4 : 0);

    const int32 TriangleCountEstimate =
        Parameters.HeightSegments * TotalSides * 6 +
        Parameters.ChamferSections * TotalSides * 6 * 2 +
        (Parameters.ArcAngle < 360.0f ? Parameters.HeightSegments * 6 : 0);

    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate);

    // 生成几何部件
    CreateSideGeometry(MeshData);
    CreateTopGeometry(MeshData);
    CreateBottomGeometry(MeshData);

    if (Parameters.ChamferRadius > KINDA_SMALL_NUMBER)
    {
        CreateChamfers(MeshData);
    }

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

void AFrustum::CreateSideGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.BottomSides;
    const float HeightStep = Parameters.Height / Parameters.HeightSegments;

    // 顶点环缓存 [高度层][边]
    TArray<TArray<int32>> VertexRings;
    VertexRings.SetNum(Parameters.HeightSegments + 1);

    // 生成顶点环
    for (int32 h = 0; h <= Parameters.HeightSegments; ++h)
    {
        const float Z = -HalfHeight + h * HeightStep;
        const float Alpha = static_cast<float>(h) / Parameters.HeightSegments;
        const float Radius = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha);

        // 弯曲效果
        const float BendFactor = FMath::Sin(Alpha * PI);
        const float BentRadius = FMath::Max(
            Radius + Parameters.BendAmount * BendFactor * Radius,
            Parameters.MinBendRadius
        );

        TArray<int32>& Ring = VertexRings[h];
        Ring.SetNum(Parameters.BottomSides + 1);

        for (int32 s = 0; s <= Parameters.BottomSides; ++s)
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
            const float U = static_cast<float>(s) / Parameters.BottomSides;
            const float V = Alpha;

            Ring[s] = AddVertex(Section, FVector(X, Y, Z), Normal, FVector2D(U, V));
        }
    }

    // 生成侧面四边形
    for (int32 h = 0; h < Parameters.HeightSegments; ++h)
    {
        for (int32 s = 0; s < Parameters.BottomSides; ++s)
        {
            const int32 V00 = VertexRings[h][s];
            const int32 V10 = VertexRings[h + 1][s];
            const int32 V01 = VertexRings[h][s + 1];
            const int32 V11 = VertexRings[h + 1][s + 1];

            AddQuad(Section, V00, V10, V11, V01);
        }
    }
}

void AFrustum::CreateTopGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.TopSides;

    // 中心顶点
    const int32 CenterVertex = AddVertex(
        Section,
        FVector(0, 0, HalfHeight),
        FVector(0, 0, 1),
        FVector2D(0.5f, 0.5f)
    );

    // 顶部顶点环
    TArray<int32> TopRing;
    TopRing.SetNum(Parameters.TopSides + 1);

    for (int32 s = 0; s <= Parameters.TopSides; ++s)
    {
        const float Angle = s * AngleStep;
        const float X = Parameters.TopRadius * FMath::Cos(Angle);
        const float Y = Parameters.TopRadius * FMath::Sin(Angle);

        // UV映射到圆形
        const float U = 0.5f + 0.5f * FMath::Cos(Angle);
        const float V = 0.5f + 0.5f * FMath::Sin(Angle);

        TopRing[s] = AddVertex(
            Section,
            FVector(X, Y, HalfHeight),
            FVector(0, 0, 1),
            FVector2D(U, V)
        );
    }

    // 创建顶部三角扇
    for (int32 s = 0; s < Parameters.TopSides; ++s)
    {
        AddTriangle(
            Section,
            CenterVertex,
            TopRing[s + 1],
            TopRing[s]
        );
    }
}

void AFrustum::CreateBottomGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.BottomSides;

    // 中心顶点
    const int32 CenterVertex = AddVertex(
        Section,
        FVector(0, 0, -HalfHeight),
        FVector(0, 0, -1),
        FVector2D(0.5f, 0.5f)
    );

    // 底部顶点环
    TArray<int32> BottomRing;
    BottomRing.SetNum(Parameters.BottomSides + 1);

    for (int32 s = 0; s <= Parameters.BottomSides; ++s)
    {
        const float Angle = s * AngleStep;
        const float X = Parameters.BottomRadius * FMath::Cos(Angle);
        const float Y = Parameters.BottomRadius * FMath::Sin(Angle);

        // UV映射到圆形
        const float U = 0.5f + 0.5f * FMath::Cos(Angle);
        const float V = 0.5f + 0.5f * FMath::Sin(Angle);

        BottomRing[s] = AddVertex(
            Section,
            FVector(X, Y, -HalfHeight),
            FVector(0, 0, -1),
            FVector2D(U, V)
        );
    }

    // 创建底部三角扇
    for (int32 s = 0; s < Parameters.BottomSides; ++s)
    {
        AddTriangle(
            Section,
            CenterVertex,
            BottomRing[s],
            BottomRing[s + 1]
        );
    }
}

void AFrustum::CreateChamfers(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.BottomSides;

    // 顶部倒角
    for (int32 s = 0; s < Parameters.BottomSides; ++s)
    {
        const float Angle = s * AngleStep;
        const FVector RadialDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0);

        // 顶部边缘位置
        const FVector TopEdgePos = RadialDir * Parameters.TopRadius + FVector(0, 0, HalfHeight);

        // 生成倒角环
        TArray<int32> ChamferRing;
        ChamferRing.SetNum(Parameters.ChamferSections + 1);

        for (int32 c = 0; c <= Parameters.ChamferSections; ++c)
        {
            const float Alpha = static_cast<float>(c) / Parameters.ChamferSections;
            const float Radius = Parameters.ChamferRadius * (1.0f - Alpha);

            // 混合法线
            FVector Normal = FMath::Lerp(
                FVector(0, 0, 1),
                RadialDir,
                Alpha
            ).GetSafeNormal();

            // 计算位置
            const FVector Position = TopEdgePos - Normal * Parameters.ChamferRadius + Normal * Radius;

            // UV映射
            const float U = static_cast<float>(s) / Parameters.BottomSides;
            const float V = 1.0f + Alpha;

            ChamferRing[c] = AddVertex(Section, Position, Normal, FVector2D(U, V));
        }

        // 连接倒角四边形
        for (int32 c = 0; c < Parameters.ChamferSections; ++c)
        {
            AddQuad(
                Section,
                ChamferRing[c],
                ChamferRing[c + 1],
                ChamferRing[c + 1],
                ChamferRing[c]
            );
        }
    }

    // 底部倒角
    for (int32 s = 0; s < Parameters.BottomSides; ++s)
    {
        const float Angle = s * AngleStep;
        const FVector RadialDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0);

        // 底部边缘位置
        const FVector BottomEdgePos = RadialDir * Parameters.BottomRadius + FVector(0, 0, -HalfHeight);

        // 生成倒角环
        TArray<int32> ChamferRing;
        ChamferRing.SetNum(Parameters.ChamferSections + 1);

        for (int32 c = 0; c <= Parameters.ChamferSections; ++c)
        {
            const float Alpha = static_cast<float>(c) / Parameters.ChamferSections;
            const float Radius = Parameters.ChamferRadius * (1.0f - Alpha);

            // 混合法线
            FVector Normal = FMath::Lerp(
                FVector(0, 0, -1),
                RadialDir,
                Alpha
            ).GetSafeNormal();

            // 计算位置
            const FVector Position = BottomEdgePos - Normal * Parameters.ChamferRadius + Normal * Radius;

            // UV映射
            const float U = static_cast<float>(s) / Parameters.BottomSides;
            const float V = Alpha; // 底部倒角UV从0开始

            ChamferRing[c] = AddVertex(Section, Position, Normal, FVector2D(U, V));
        }

        // 连接倒角四边形
        for (int32 c = 0; c < Parameters.ChamferSections; ++c)
        {
            AddQuad(
                Section,
                ChamferRing[c],
                ChamferRing[c],
                ChamferRing[c + 1],
                ChamferRing[c + 1]
            );
        }
    }
}

void AFrustum::CreateEndCaps(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float StartAngle = 0.0f;
    const float EndAngle = FMath::DegreesToRadians(Parameters.ArcAngle);

    // 起始端面
    const FVector StartNormal = FVector(-FMath::Sin(StartAngle), FMath::Cos(StartAngle), 0);
    TArray<FVector> StartCapVertices;

    // 生成端面顶点
    for (int32 h = 0; h <= Parameters.HeightSegments; ++h)
    {
        const float Z = -HalfHeight + h * (Parameters.Height / Parameters.HeightSegments);
        const float Alpha = static_cast<float>(h) / Parameters.HeightSegments;
        const float Radius = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha);

        // 边缘顶点
        const FVector EdgePos = FVector(Radius * FMath::Cos(StartAngle), Radius * FMath::Sin(StartAngle), Z);
        StartCapVertices.Add(EdgePos);

        // 内部顶点
        const FVector InnerPos = FVector(Parameters.CapThickness * FMath::Cos(StartAngle),
            Parameters.CapThickness * FMath::Sin(StartAngle),
            Z);
        StartCapVertices.Add(InnerPos);
    }

    // 添加端面网格
    for (int32 h = 0; h < Parameters.HeightSegments; ++h)
    {
        const int32 CurrentBase = h * 2;
        const int32 NextBase = (h + 1) * 2;

        const int32 V1 = AddVertex(Section, StartCapVertices[CurrentBase], StartNormal, FVector2D(0, h / (float)Parameters.HeightSegments));
        const int32 V2 = AddVertex(Section, StartCapVertices[CurrentBase + 1], StartNormal, FVector2D(1, h / (float)Parameters.HeightSegments));
        const int32 V3 = AddVertex(Section, StartCapVertices[NextBase], StartNormal, FVector2D(0, (h + 1) / (float)Parameters.HeightSegments));
        const int32 V4 = AddVertex(Section, StartCapVertices[NextBase + 1], StartNormal, FVector2D(1, (h + 1) / (float)Parameters.HeightSegments));

        AddQuad(Section, V1, V3, V4, V2);
    }

    // 结束端面
    const FVector EndNormal = FVector(-FMath::Sin(EndAngle), FMath::Cos(EndAngle), 0);
    TArray<FVector> EndCapVertices;

    // 生成端面顶点
    for (int32 h = 0; h <= Parameters.HeightSegments; ++h)
    {
        const float Z = -HalfHeight + h * (Parameters.Height / Parameters.HeightSegments);
        const float Alpha = static_cast<float>(h) / Parameters.HeightSegments;
        const float Radius = FMath::Lerp(Parameters.BottomRadius, Parameters.TopRadius, Alpha);

        // 边缘顶点
        const FVector EdgePos = FVector(Radius * FMath::Cos(EndAngle), Radius * FMath::Sin(EndAngle), Z);
        EndCapVertices.Add(EdgePos);

        // 内部顶点
        const FVector InnerPos = FVector(Parameters.CapThickness * FMath::Cos(EndAngle),
            Parameters.CapThickness * FMath::Sin(EndAngle),
            Z);
        EndCapVertices.Add(InnerPos);
    }

    // 添加端面网格
    for (int32 h = 0; h < Parameters.HeightSegments; ++h)
    {
        const int32 CurrentBase = h * 2;
        const int32 NextBase = (h + 1) * 2;

        const int32 V1 = AddVertex(Section, EndCapVertices[CurrentBase], EndNormal, FVector2D(0, h / (float)Parameters.HeightSegments));
        const int32 V2 = AddVertex(Section, EndCapVertices[CurrentBase + 1], EndNormal, FVector2D(1, h / (float)Parameters.HeightSegments));
        const int32 V3 = AddVertex(Section, EndCapVertices[NextBase], EndNormal, FVector2D(0, (h + 1) / (float)Parameters.HeightSegments));
        const int32 V4 = AddVertex(Section, EndCapVertices[NextBase + 1], EndNormal, FVector2D(1, (h + 1) / (float)Parameters.HeightSegments));

        AddQuad(Section, V1, V2, V4, V3);
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
