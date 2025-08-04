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
    TArray<FLinearColor> VertexColors; // 留空（使用默认白色）

    // 1. 计算底面顶点
    TArray<FVector> BaseVertices;
    for (int32 i = 0; i < InSides; i++)
    {
        float Angle = 2 * PI * i / InSides;
        BaseVertices.Add(FVector(
            InBaseRadius * FMath::Cos(Angle),
            InBaseRadius * FMath::Sin(Angle),
            0.0f
        ));
    }

    // 2. 生成侧面（三角形面）
    FVector Apex(0.0f, 0.0f, InHeight); // 顶点位置
    for (int32 i = 0; i < InSides; i++)
    {
        int32 NextIndex = (i + 1) % InSides;

        // 计算三角形法线（确保外法线）
        FVector Edge1 = BaseVertices[i] - Apex;
        FVector Edge2 = BaseVertices[NextIndex] - Apex;
        FVector Normal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        // 添加三个顶点（顶点、当前底点、下一个底点）
        int32 ApexIndex = AddVertex(Vertices, Normals, UV0, Tangents, Apex, Normal, FVector2D(0.5f, 1.0f));
        int32 CurrentBaseIndex = AddVertex(Vertices, Normals, UV0, Tangents, BaseVertices[i], Normal, FVector2D(static_cast<float>(i) / InSides, 0.0f));
        int32 NextBaseIndex = AddVertex(Vertices, Normals, UV0, Tangents, BaseVertices[NextIndex], Normal, FVector2D(static_cast<float>(i + 1) / InSides, 0.0f));

        // 添加三角形（确保逆时针顺序）
        AddTriangle(Triangles, ApexIndex, NextBaseIndex, CurrentBaseIndex);
    }

    // 3. 生成底面（如果需要）
    if (bInCreateBottom)
    {
        // 底面中心点
        int32 CenterIndex = AddVertex(Vertices, Normals, UV0, Tangents, FVector::ZeroVector, FVector(0, 0, -1), FVector2D(0.5f, 0.5f));

        for (int32 i = 0; i < InSides; i++)
        {
            int32 NextIndex = (i + 1) % InSides;

            // 添加底面顶点（法线向下）
            int32 CurrentBaseIndex = AddVertex(Vertices, Normals, UV0, Tangents, BaseVertices[i], FVector(0, 0, -1),
                FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * i / InSides), 0.5f + 0.5f * FMath::Sin(2 * PI * i / InSides)));
            int32 NextBaseIndex = AddVertex(Vertices, Normals, UV0, Tangents, BaseVertices[NextIndex], FVector(0, 0, -1),
                FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * NextIndex / InSides), 0.5f + 0.5f * FMath::Sin(2 * PI * NextIndex / InSides)));

            // 添加三角形（从底面看为逆时针）
            AddTriangle(Triangles, CenterIndex, CurrentBaseIndex, NextBaseIndex);
        }
    }

    // 创建网格
    ProceduralMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, TArray<FColor>(), Tangents, true);
}