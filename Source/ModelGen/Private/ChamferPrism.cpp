#include "ChamferPrism.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h" 

AChamferPrism::AChamferPrism()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;

    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ProceduralMesh->SetSimulatePhysics(false);

    GeneratePrism(BottomRadius, TopRadius, Height, Sides, ChamferSize, ChamferSections, RotationOffset);

    // 设置默认材质
    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (MaterialFinder.Succeeded())
    {
        ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to find material. Using default material."));
    }
}

void AChamferPrism::BeginPlay()
{
    Super::BeginPlay();
    GeneratePrism(BottomRadius, TopRadius, Height, Sides, ChamferSize, ChamferSections, RotationOffset);
}

void AChamferPrism::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

int32 AChamferPrism::AddVertexInternal(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UV0,
    TArray<FLinearColor>& VertexColors,
    TArray<FProcMeshTangent>& Tangents,
    const FVector& Pos,
    const FVector& Normal,
    const FVector2D& UV)
{
    Vertices.Add(Pos);
    Normals.Add(Normal);
    UV0.Add(UV);
    VertexColors.Add(FLinearColor::White);

    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    TangentDirection.Normalize();
    Tangents.Add(FProcMeshTangent(TangentDirection, false));

    return Vertices.Num() - 1;
}

void AChamferPrism::AddQuadInternal(
    TArray<int32>& Triangles,
    int32 V1,
    int32 V2,
    int32 V3,
    int32 V4)
{
    Triangles.Add(V1); Triangles.Add(V2); Triangles.Add(V3);
    Triangles.Add(V1); Triangles.Add(V3); Triangles.Add(V4);
}

void AChamferPrism::AddTriangleInternal(
    TArray<int32>& Triangles,
    int32 V1,
    int32 V2,
    int32 V3)
{
    Triangles.Add(V1);
    Triangles.Add(V2);
    Triangles.Add(V3);
}

int32 AChamferPrism::GetOrAddVertex(
    TMap<FVector, int32>& UniqueVerticesMap,
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UV0,
    TArray<FLinearColor>& VertexColors,
    TArray<FProcMeshTangent>& Tangents,
    const FVector& Pos,
    const FVector& Normal,
    const FVector2D& UV)
{
    int32* FoundIndex = UniqueVerticesMap.Find(Pos);
    if (FoundIndex)
    {
        return *FoundIndex;
    }

    int32 NewIndex = AddVertexInternal(Vertices, Normals, UV0, VertexColors, Tangents, Pos, Normal, UV);
    UniqueVerticesMap.Add(Pos, NewIndex);
    return NewIndex;
}

void AChamferPrism::GeneratePolygonCap(
    TMap<FVector, int32>& UniqueVerticesMap,
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UV0,
    TArray<FLinearColor>& VertexColors,
    TArray<FProcMeshTangent>& Tangents,
    TArray<int32>& Triangles,
    float ZHeight,
    float Radius,
    int32 NumSides,
    bool bIsTop,
    float InChamferSize,
    int32 InChamferSections,
    TArray<FVector>& OutCapPoints)
{
    const float AngleStep = 2.0f * PI / NumSides;
    const FVector Normal = bIsTop ? FVector(0, 0, 1) : FVector(0, 0, -1);

    // 中心点
    FVector Center(0, 0, ZHeight);
    int32 CenterIndex = GetOrAddVertex(
        UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        Center, Normal, FVector2D(0.5f, 0.5f)
    );

    // 生成原始角点
    TArray<FVector> CornerPoints;
    for (int32 i = 0; i < NumSides; i++)
    {
        float Angle = AngleStep * i;
        float X = Radius * FMath::Cos(Angle);
        float Y = Radius * FMath::Sin(Angle);
        FVector Point(X, Y, ZHeight);
        CornerPoints.Add(Point);
    }

    // 生成圆角点
    TArray<FVector> ChamferedPoints;
    TArray<int32> ChamferedIndices;
    for (int32 i = 0; i < NumSides; i++)
    {
        int32 PrevIndex = (i - 1 + NumSides) % NumSides;
        int32 NextIndex = (i + 1) % NumSides;

        FVector Current = CornerPoints[i];
        FVector Prev = CornerPoints[PrevIndex];
        FVector Next = CornerPoints[NextIndex];

        // 计算方向向量
        FVector ToPrev = (Prev - Current).GetSafeNormal();
        FVector ToNext = (Next - Current).GetSafeNormal();

        // 计算角平分线
        FVector Bisector = (ToPrev + ToNext).GetSafeNormal();

        // 计算圆弧半径和中心
        float Angle = FMath::Acos(FVector::DotProduct(ToPrev, ToNext)) / 2.0f;
        float ChamferRadius = InChamferSize / FMath::Sin(Angle);
        FVector ArcCenter = Current + Bisector * ChamferRadius;

        // 圆弧起点和终点
        FVector ArcStart = Current + ToPrev * InChamferSize;
        FVector ArcEnd = Current + ToNext * InChamferSize;

        // 计算圆弧角度范围
        FVector StartDir = (ArcStart - ArcCenter).GetSafeNormal();
        FVector EndDir = (ArcEnd - ArcCenter).GetSafeNormal();
        float StartAngle = FMath::Atan2(StartDir.Y, StartDir.X);
        float EndAngle = FMath::Atan2(EndDir.Y, EndDir.X);
        if (EndAngle < StartAngle)
        {
            EndAngle += 2 * PI;
        }

        // 生成圆弧上的点
        for (int32 s = 0; s <= InChamferSections; s++)
        {
            float Alpha = static_cast<float>(s) / InChamferSections;
            float AngleT = StartAngle + Alpha * (EndAngle - StartAngle);
            FVector Point = ArcCenter + ChamferRadius * FVector(FMath::Cos(AngleT), FMath::Sin(AngleT), 0);
            ChamferedPoints.Add(Point);
            OutCapPoints.Add(Point); // 添加到输出点集

            // 计算UV坐标
            FVector2D UV(
                0.5f + 0.5f * Point.X / Radius,
                0.5f + 0.5f * Point.Y / Radius
            );

            // 添加顶点
            int32 VertIndex = GetOrAddVertex(
                UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
                Point, Normal, UV
            );
            ChamferedIndices.Add(VertIndex);
        }
    }

    // 连接中心点到圆弧
    for (int32 i = 0; i < NumSides; i++)
    {
        int32 StartIndex = i * (InChamferSections + 1);
        int32 NextStart = ((i + 1) % NumSides) * (InChamferSections + 1);

        // 中心到当前圆弧起点到下一圆弧起点
        AddTriangleInternal(
            Triangles,
            CenterIndex,
            ChamferedIndices[StartIndex],
            ChamferedIndices[NextStart]
        );
    }

    // 连接圆弧段之间
    for (int32 i = 0; i < NumSides; i++)
    {
        for (int32 s = 0; s < InChamferSections; s++)
        {
            int32 Current = i * (InChamferSections + 1) + s;
            int32 Next = Current + 1;
            int32 NextCorner = ((i + 1) % NumSides) * (InChamferSections + 1) + s;

            AddTriangleInternal(
                Triangles,
                ChamferedIndices[Current],
                ChamferedIndices[Next],
                ChamferedIndices[NextCorner]
            );
        }
    }
}

void AChamferPrism::GenerateSides(
    TMap<FVector, int32>& UniqueVerticesMap,
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UV0,
    TArray<FLinearColor>& VertexColors,
    TArray<FProcMeshTangent>& Tangents,
    TArray<int32>& Triangles,
    const TArray<FVector>& BottomPoints,
    const TArray<FVector>& TopPoints,
    float InChamferSize,
    int32 InChamferSections)
{
    int32 PointsPerSide = InChamferSections + 1;
    int32 NumSides = BottomPoints.Num() / PointsPerSide;

    for (int32 i = 0; i < NumSides; i++)
    {
        int32 NextSide = (i + 1) % NumSides;

        for (int32 s = 0; s < PointsPerSide; s++)
        {
            int32 BottomCurrentIdx = i * PointsPerSide + s;
            int32 BottomNextIdx = i * PointsPerSide + ((s + 1) % PointsPerSide);
            int32 TopCurrentIdx = i * PointsPerSide + s;
            int32 TopNextIdx = i * PointsPerSide + ((s + 1) % PointsPerSide);

            int32 BottomNextSideStart = NextSide * PointsPerSide;
            int32 TopNextSideStart = NextSide * PointsPerSide;

            // 确保索引有效
            if (BottomCurrentIdx < BottomPoints.Num() && BottomNextIdx < BottomPoints.Num() &&
                TopCurrentIdx < TopPoints.Num() && TopNextIdx < TopPoints.Num() &&
                BottomNextSideStart < BottomPoints.Num() && TopNextSideStart < TopPoints.Num())
            {
                FVector BottomCurrent = BottomPoints[BottomCurrentIdx];
                FVector BottomNext = BottomPoints[BottomNextIdx];
                FVector TopCurrent = TopPoints[TopCurrentIdx];
                FVector TopNext = TopPoints[TopNextIdx];
                FVector BottomNextSide = BottomPoints[BottomNextSideStart + s];
                FVector TopNextSide = TopPoints[TopNextSideStart + s];

                // 计算法线
                FVector SideNormal = FVector::CrossProduct(
                    TopCurrent - BottomCurrent,
                    BottomNext - BottomCurrent
                ).GetSafeNormal();

                // 添加四边形
                int32 V1 = GetOrAddVertex(
                    UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
                    BottomCurrent, SideNormal, FVector2D(static_cast<float>(i) / NumSides, static_cast<float>(s) / PointsPerSide)
                );
                int32 V2 = GetOrAddVertex(
                    UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
                    TopCurrent, SideNormal, FVector2D(static_cast<float>(i) / NumSides, static_cast<float>(s + 1) / PointsPerSide)
                );
                int32 V3 = GetOrAddVertex(
                    UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
                    TopNext, SideNormal, FVector2D(static_cast<float>(i + 1) / NumSides, static_cast<float>(s + 1) / PointsPerSide)
                );
                int32 V4 = GetOrAddVertex(
                    UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
                    BottomNext, SideNormal, FVector2D(static_cast<float>(i + 1) / NumSides, static_cast<float>(s) / PointsPerSide)
                );

                AddQuadInternal(Triangles, V1, V2, V3, V4);
            }
        }
    }
}

void AChamferPrism::GeneratePrism(
    float BottomRad,
    float TopRad,
    float H,
    int32 NumSides,
    float Chamfer,
    int32 ChamferSects,
    float RotationOff)
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // 参数验证
    NumSides = FMath::Max(3, NumSides);
    ChamferSects = FMath::Max(1, ChamferSects);
    BottomRad = FMath::Max(0.0f, BottomRad);
    TopRad = FMath::Max(0.0f, TopRad);
    H = FMath::Max(0.0f, H);
    Chamfer = FMath::Clamp(Chamfer, 0.0f, FMath::Min(BottomRad, TopRad) * 0.5f);

    // 清除旧网格
    ProceduralMesh->ClearAllMeshSections();

    // 初始化网格数据
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    // 顶点映射（避免重复）
    TMap<FVector, int32> UniqueVerticesMap;

    // 生成底面
    TArray<FVector> BottomPoints;
    GeneratePolygonCap(
        UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents, Triangles,
        0.0f, BottomRad, NumSides, false, Chamfer, ChamferSects, BottomPoints
    );

    // 生成顶面
    TArray<FVector> TopPoints;
    GeneratePolygonCap(
        UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents, Triangles,
        H, TopRad, NumSides, true, Chamfer, ChamferSects, TopPoints
    );

    // 生成侧面
    GenerateSides(
        UniqueVerticesMap,
        Vertices,
        Normals,
        UV0,
        VertexColors,
        Tangents,
        Triangles,
        BottomPoints,
        TopPoints,
        Chamfer,
        ChamferSects
    );

    // 创建网格
    ProceduralMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, true);
}