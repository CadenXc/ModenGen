#include "Pyramid.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

APyramid::APyramid()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建ProceduralMesh组件
    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;

    // 设置碰撞
    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 设置默认材质
    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (MaterialFinder.Succeeded())
    {
        ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
    }

    // 初始生成
    GeneratePyramid(BaseRadius, Height, Sides, bCreateBottom);
}

void APyramid::BeginPlay()
{
    Super::BeginPlay();
    GeneratePyramid(BaseRadius, Height, Sides, bCreateBottom);
}

void APyramid::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GeneratePyramid(BaseRadius, Height, Sides, bCreateBottom);
}

int32 APyramid::AddVertex(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UV0,
    TArray<FProcMeshTangent>& Tangents,
    const FVector& Position,
    const FVector& Normal,
    const FVector2D& UV)
{
    int32 Index = Vertices.Add(Position);
    Normals.Add(Normal);
    UV0.Add(UV);

    // 计算切线（垂直于法线）
    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    TangentDirection.Normalize();
    Tangents.Add(FProcMeshTangent(TangentDirection, false));

    return Index;
}

void APyramid::AddTriangle(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3)
{
    Triangles.Add(V1);
    Triangles.Add(V2);
    Triangles.Add(V3);
}

TArray<FVector> APyramid::GenerateCircleVertices(float Radius, float Z, int32 NumSides)
{
    TArray<FVector> Vertices;
    Vertices.Reserve(NumSides);

    for (int32 i = 0; i < NumSides; i++)
    {
        float Angle = 2 * PI * i / NumSides;
        Vertices.Add(FVector(
            Radius * FMath::Cos(Angle),
            Radius * FMath::Sin(Angle),
            Z
        ));
    }
    return Vertices;
}

void APyramid::GeneratePrismSides(
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UV0,
    TArray<FProcMeshTangent>& Tangents,
    const TArray<FVector>& BottomVerts,
    const TArray<FVector>& TopVerts,
    bool bReverseNormal,
    float UVOffsetY,
    float UVScaleY)
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
        int32 V0 = AddVertex(Vertices, Normals, UV0, Tangents, BottomVerts[i], Normal,
            FVector2D(static_cast<float>(i) / NumSides, UVOffsetY));
        int32 V1 = AddVertex(Vertices, Normals, UV0, Tangents, BottomVerts[NextIndex], Normal,
            FVector2D(static_cast<float>(i + 1) / NumSides, UVOffsetY));
        int32 V2 = AddVertex(Vertices, Normals, UV0, Tangents, TopVerts[NextIndex], Normal,
            FVector2D(static_cast<float>(i + 1) / NumSides, UVOffsetY + UVScaleY));
        int32 V3 = AddVertex(Vertices, Normals, UV0, Tangents, TopVerts[i], Normal,
            FVector2D(static_cast<float>(i) / NumSides, UVOffsetY + UVScaleY));

        if (bReverseNormal)
        {
            AddTriangle(Triangles, V0, V2, V3);
            AddTriangle(Triangles, V0, V1, V2);
        }
        else
        {
            AddTriangle(Triangles, V0, V2, V1);
            AddTriangle(Triangles, V0, V3, V2);
        }
    }
}

void APyramid::GeneratePolygonFace(
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UV0,
    TArray<FProcMeshTangent>& Tangents,
    const TArray<FVector>& PolygonVerts,
    const FVector& Normal,
    bool bReverseOrder,
    float UVOffsetZ)
{
    const int32 NumSides = PolygonVerts.Num();
    const FVector Center = FVector(0, 0, PolygonVerts[0].Z);
    int32 CenterIndex = AddVertex(Vertices, Normals, UV0, Tangents, Center, Normal, FVector2D(0.5f, 0.5f));

    for (int32 i = 0; i < NumSides; i++)
    {
        int32 NextIndex = (i + 1) % NumSides;
        int32 V0 = AddVertex(Vertices, Normals, UV0, Tangents, PolygonVerts[i], Normal,
            FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * i / NumSides),
                0.5f + 0.5f * FMath::Sin(2 * PI * i / NumSides)));
        int32 V1 = AddVertex(Vertices, Normals, UV0, Tangents, PolygonVerts[NextIndex], Normal,
            FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * NextIndex / NumSides),
                0.5f + 0.5f * FMath::Sin(2 * PI * NextIndex / NumSides)));

        if (bReverseOrder)
        {
            AddTriangle(Triangles, CenterIndex, V1, V0);
        }
        else
        {
            AddTriangle(Triangles, CenterIndex, V0, V1);
        }
    }
}

void APyramid::GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides, bool bInCreateBottom)
{
    if (!ProceduralMesh || InSides < 3) return;
    ProceduralMesh->ClearAllMeshSections();

    // 初始化网格数据
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FProcMeshTangent> Tangents;

    // 计算棱柱顶面半径和高度
    float BevelTopRadius = FMath::Max(0.0f, InBaseRadius - InBaseRadius * BevelRadius / InHeight);
    //float BevelTopRadius = FMath::Max(0.0f, InBaseRadius - BevelRadius);
    float TotalHeight = InHeight;

    // 1. 生成棱柱部分（倒角）
    if (BevelRadius > 0.0f)
    {
        TArray<FVector> BottomVerts = GenerateCircleVertices(BevelTopRadius, 0.0f, InSides);
        TArray<FVector> TopVerts = GenerateCircleVertices(BevelTopRadius, BevelRadius, InSides);

        // 生成棱柱侧面
        GeneratePrismSides(Vertices, Triangles, Normals, UV0, Tangents,
            BottomVerts, TopVerts, false, 0.0f, 0.5f);

        // 生成棱柱底面（可选）
        if (bInCreateBottom)
        {
            GeneratePolygonFace(Vertices, Triangles, Normals, UV0, Tangents,
                BottomVerts, FVector(0, 0, -1), true);
        }

        // 生成棱柱顶面（金字塔底面）
        GeneratePolygonFace(Vertices, Triangles, Normals, UV0, Tangents,
            TopVerts, FVector(0, 0, 1));
    }
    else
    {
        // 无倒角时生成底面
        if (bInCreateBottom)
        {
            TArray<FVector> BaseVerts = GenerateCircleVertices(InBaseRadius, 0.0f, InSides);
            GeneratePolygonFace(Vertices, Triangles, Normals, UV0, Tangents,
                BaseVerts, FVector(0, 0, -1), true);
        }
    }

    // 2. 生成金字塔主体部分
    float PyramidBaseRadius = (BevelRadius > 0) ? BevelTopRadius : InBaseRadius;
    float PyramidBaseHeight = (BevelRadius > 0) ? BevelRadius : 0.0f;
    TArray<FVector> BaseVertices = GenerateCircleVertices(PyramidBaseRadius, PyramidBaseHeight, InSides);
    FVector Apex(0.0f, 0.0f, TotalHeight);

    for (int32 i = 0; i < InSides; i++)
    {
        int32 NextIndex = (i + 1) % InSides;

        // 计算三角形法线
        FVector Edge1 = BaseVertices[i] - Apex;
        FVector Edge2 = BaseVertices[NextIndex] - Apex;
        FVector Normal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        // 添加三个顶点
        int32 ApexIndex = AddVertex(Vertices, Normals, UV0, Tangents, Apex, Normal, FVector2D(0.5f, 1.0f));
        int32 CurrentBaseIndex = AddVertex(Vertices, Normals, UV0, Tangents, BaseVertices[i], Normal,
            FVector2D(static_cast<float>(i) / InSides, (BevelRadius > 0) ? 0.5f : 0.0f));
        int32 NextBaseIndex = AddVertex(Vertices, Normals, UV0, Tangents, BaseVertices[NextIndex], Normal,
            FVector2D(static_cast<float>(i + 1) / InSides, (BevelRadius > 0) ? 0.5f : 0.0f));

        // 添加三角形
        AddTriangle(Triangles, ApexIndex, NextBaseIndex, CurrentBaseIndex);
    }

    // 创建网格
    ProceduralMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, TArray<FColor>(), Tangents, true);
}
