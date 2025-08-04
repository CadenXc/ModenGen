#include "PolygonTorus.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

APolygonTorus::APolygonTorus()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;
    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 设置默认材质
    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (MaterialFinder.Succeeded())
    {
        ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
    }

    GeneratePolygonTorus(MajorRadius, MinorRadius, MajorSegments, MinorSegments, TorusAngle);
}

void APolygonTorus::BeginPlay()
{
    Super::BeginPlay();
    GeneratePolygonTorus(MajorRadius, MinorRadius, MajorSegments, MinorSegments, TorusAngle);
}

void APolygonTorus::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GeneratePolygonTorus(MajorRadius, MinorRadius, MajorSegments, MinorSegments, TorusAngle);
}

// 生成多边形截面顶点
void APolygonTorus::GeneratePolygonVertices(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UVs,
    TArray<FProcMeshTangent>& Tangents,
    const FVector& Center,
    const FVector& Direction,
    const FVector& UpVector,
    float Radius,
    int32 Segments,
    float UOffset)
{
    // 计算垂直向量
    FVector RightVector = FVector::CrossProduct(Direction, UpVector).GetSafeNormal();

    for (int32 i = 0; i < Segments; i++)
    {
        float Angle = 2 * PI * i / Segments;
        float CosA = FMath::Cos(Angle);
        float SinA = FMath::Sin(Angle);

        // 计算顶点位置
        FVector VertexPos = Center +
            (RightVector * CosA * Radius) +
            (UpVector * SinA * Radius);

        // 计算法线（从中心指向顶点）
        FVector Normal = (VertexPos - Center).GetSafeNormal();

        // 设置UV坐标
        FVector2D UV(UOffset + static_cast<float>(i) / Segments, 0.0f);

        // 切线方向（沿圆环方向）
        FVector Tangent = Direction;

        // 添加顶点
        Vertices.Add(VertexPos);
        Normals.Add(Normal);
        UVs.Add(UV);
        Tangents.Add(FProcMeshTangent(Tangent, false));
    }
}

void APolygonTorus::AddQuad(TArray<int32>& Triangles, int32 V0, int32 V1, int32 V2, int32 V3)
{
    Triangles.Add(V0); Triangles.Add(V1); Triangles.Add(V2);
    Triangles.Add(V0); Triangles.Add(V2); Triangles.Add(V3);
}

// 主要的网格生成函数
void APolygonTorus::GeneratePolygonTorus(float MajorRad, float MinorRad, int32 MajorSegs, int32 MinorSegs, float Angle)
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // 清除之前的网格
    ProceduralMesh->ClearAllMeshSections();

    // 验证参数
    MajorSegs = FMath::Max(3, MajorSegs);
    MinorSegs = FMath::Max(3, MinorSegs);
    MajorRad = FMath::Max(1.0f, MajorRad);
    MinorRad = FMath::Clamp(MinorRad, 1.0f, MajorRad * 0.9f);
    Angle = FMath::Clamp(Angle, 1.0f, 360.0f);

    // 准备网格数据
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    // 计算角度步长
    const float AngleRad = FMath::DegreesToRadians(Angle);
    const float AngleStep = AngleRad / MajorSegs;
    const bool bIsFullCircle = FMath::IsNearlyEqual(Angle, 360.0f);

    // 存储每个截面的顶点起始索引
    TArray<int32> SectionStartIndices;

    // 生成所有截面的顶点
    for (int32 i = 0; i <= MajorSegs; i++)
    {
        float CurrentAngle = i * AngleStep;
        float UOffset = static_cast<float>(i) / MajorSegs;

        // 计算当前截面的中心位置
        FVector SectionCenter(
            FMath::Cos(CurrentAngle) * MajorRad,
            FMath::Sin(CurrentAngle) * MajorRad,
            0.0f
        );

        // 计算当前截面的方向（圆环的切线方向）
        FVector SectionDirection(
            -FMath::Sin(CurrentAngle),
            FMath::Cos(CurrentAngle),
            0.0f
        );

        // 记录当前截面的起始顶点索引
        SectionStartIndices.Add(Vertices.Num());

        // 生成多边形截面
        GeneratePolygonVertices(
            Vertices, Normals, UVs, Tangents,
            SectionCenter,
            SectionDirection,
            FVector::UpVector,
            MinorRad,
            MinorSegs,
            UOffset
        );
    }

    // 生成侧面（连接相邻截面）
    for (int32 i = 0; i < MajorSegs; i++)
    {
        // 如果是部分圆环且是最后一段，跳过
        if (!bIsFullCircle && i == MajorSegs - 1) break;

        int32 CurrentSectionStart = SectionStartIndices[i];
        int32 NextSectionStart = SectionStartIndices[(i + 1) % (MajorSegs + (bIsFullCircle ? 0 : 1))];

        for (int32 j = 0; j < MinorSegs; j++)
        {
            int32 NextJ = (j + 1) % MinorSegs;

            int32 V0 = CurrentSectionStart + j;
            int32 V1 = CurrentSectionStart + NextJ;
            int32 V2 = NextSectionStart + NextJ;
            int32 V3 = NextSectionStart + j;

            AddQuad(Triangles, V0, V1, V2, V3);
        }
    }

    // 生成端面（如果角度小于360度）
    if (!bIsFullCircle)
    {
        // 起始端面
        int32 StartSection = SectionStartIndices[0];
        for (int32 j = 0; j < MinorSegs; j++)
        {
            int32 NextJ = (j + 1) % MinorSegs;

            Triangles.Add(StartSection + j);
            Triangles.Add(StartSection + NextJ);
            Triangles.Add(StartSection + MinorSegs); // 中心顶点
        }

        // 结束端面
        int32 EndSection = SectionStartIndices[MajorSegs];
        for (int32 j = 0; j < MinorSegs; j++)
        {
            int32 NextJ = (j + 1) % MinorSegs;

            Triangles.Add(EndSection + j);
            Triangles.Add(EndSection + MinorSegs); // 中心顶点
            Triangles.Add(EndSection + NextJ);
        }
    }

    // 创建网格
    ProceduralMesh->CreateMeshSection_LinearColor(
        0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true
    );
}
