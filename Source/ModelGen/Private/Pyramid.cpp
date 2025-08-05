#include "Pyramid.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

// ============================================================================
// FPyramidGeometry 实现
// ============================================================================

void FPyramidGeometry::Clear()
{
    Vertices.Empty();
    Triangles.Empty();
    Normals.Empty();
    UV0.Empty();
    VertexColors.Empty();
    Tangents.Empty();
}

bool FPyramidGeometry::IsValid() const
{
    return Vertices.Num() > 0 && 
           Triangles.Num() > 0 && 
           Triangles.Num() % 3 == 0 &&
           Normals.Num() == Vertices.Num() &&
           UV0.Num() == Vertices.Num() &&
           Tangents.Num() == Vertices.Num();
}

// ============================================================================
// FPyramidBuildParameters 实现
// ============================================================================

bool FPyramidBuildParameters::IsValid() const
{
    return BaseRadius > 0.0f && 
           Height > 0.0f && 
           Sides >= 3 && 
           Sides <= 100 &&
           BevelRadius >= 0.0f && 
           BevelRadius < Height;
}

float FPyramidBuildParameters::GetBevelTopRadius() const
{
    if (BevelRadius <= 0.0f) return BaseRadius;
    return FMath::Max(0.0f, BaseRadius - BaseRadius * BevelRadius / Height);
}

float FPyramidBuildParameters::GetTotalHeight() const
{
    return Height;
}

float FPyramidBuildParameters::GetPyramidBaseRadius() const
{
    return (BevelRadius > 0) ? GetBevelTopRadius() : BaseRadius;
}

float FPyramidBuildParameters::GetPyramidBaseHeight() const
{
    return (BevelRadius > 0) ? BevelRadius : 0.0f;
}

// ============================================================================
// FPyramidBuilder 实现
// ============================================================================

FPyramidBuilder::FPyramidBuilder(const FPyramidBuildParameters& InParams)
    : Params(InParams)
{
}

bool FPyramidBuilder::Generate(FPyramidGeometry& OutGeometry)
{
    if (!Params.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid build parameters for Pyramid"));
        return false;
    }

    // 清除之前的几何数据
    OutGeometry.Clear();

    // 生成各个几何部分
    if (Params.BevelRadius > 0.0f)
    {
        GeneratePrismSection(OutGeometry);
    }
    else if (Params.bCreateBottom)
    {
        GenerateBottomFace(OutGeometry);
    }

    GeneratePyramidSection(OutGeometry);

    return OutGeometry.IsValid();
}

void FPyramidBuilder::GeneratePrismSection(FPyramidGeometry& Geometry)
{
    TArray<FVector> BottomVerts = GenerateCircleVertices(Params.GetBevelTopRadius(), 0.0f, Params.Sides);
    TArray<FVector> TopVerts = GenerateCircleVertices(Params.GetBevelTopRadius(), Params.BevelRadius, Params.Sides);

    // 生成棱柱侧面
    GeneratePrismSides(Geometry, BottomVerts, TopVerts, false, 0.0f, 0.5f);

    // 生成棱柱底面（可选）
    if (Params.bCreateBottom)
    {
        GeneratePolygonFace(Geometry, BottomVerts, FVector(0, 0, -1), false);
    }

    // 生成棱柱顶面（金字塔底面）
    GeneratePolygonFace(Geometry, TopVerts, FVector(0, 0, 1), true);
}

void FPyramidBuilder::GeneratePyramidSection(FPyramidGeometry& Geometry)
{
    float PyramidBaseRadius = Params.GetPyramidBaseRadius();
    float PyramidBaseHeight = Params.GetPyramidBaseHeight();
    TArray<FVector> BaseVertices = GenerateCircleVertices(PyramidBaseRadius, PyramidBaseHeight, Params.Sides);
    FVector Apex(0.0f, 0.0f, Params.GetTotalHeight());

    for (int32 i = 0; i < Params.Sides; i++)
    {
        int32 NextIndex = (i + 1) % Params.Sides;

        // 计算三角形法线
        FVector Edge1 = BaseVertices[i] - Apex;
        FVector Edge2 = BaseVertices[NextIndex] - Apex;
        FVector Normal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        // 添加三个顶点
        int32 ApexIndex = GetOrAddVertex(Geometry, Apex, Normal, FVector2D(0.5f, 1.0f));
        int32 CurrentBaseIndex = GetOrAddVertex(Geometry, BaseVertices[i], Normal,
            FVector2D(static_cast<float>(i) / Params.Sides, (Params.BevelRadius > 0) ? 0.5f : 0.0f));
        int32 NextBaseIndex = GetOrAddVertex(Geometry, BaseVertices[NextIndex], Normal,
            FVector2D(static_cast<float>(i + 1) / Params.Sides, (Params.BevelRadius > 0) ? 0.5f : 0.0f));

        // 添加三角形 - 确保逆时针顺序以正确朝向
        AddTriangle(Geometry, ApexIndex, CurrentBaseIndex, NextBaseIndex);
    }
}

void FPyramidBuilder::GenerateBottomFace(FPyramidGeometry& Geometry)
{
    TArray<FVector> BaseVerts = GenerateCircleVertices(Params.BaseRadius, 0.0f, Params.Sides);
    GeneratePolygonFace(Geometry, BaseVerts, FVector(0, 0, -1), false);
}

int32 FPyramidBuilder::GetOrAddVertex(FPyramidGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    // 为了确保法线正确，我们总是创建新顶点
    // 这样可以避免不同面之间的法线冲突
    int32 Index = Geometry.Vertices.Add(Pos);
    Geometry.Normals.Add(Normal);
    Geometry.UV0.Add(UV);

    // 计算切线（垂直于法线）
    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    TangentDirection.Normalize();
    Geometry.Tangents.Add(FProcMeshTangent(TangentDirection, false));

    return Index;
}

void FPyramidBuilder::AddTriangle(FPyramidGeometry& Geometry, int32 V1, int32 V2, int32 V3)
{
    Geometry.Triangles.Add(V1);
    Geometry.Triangles.Add(V2);
    Geometry.Triangles.Add(V3);
}

TArray<FVector> FPyramidBuilder::GenerateCircleVertices(float Radius, float Z, int32 NumSides)
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
        int32 V0 = GetOrAddVertex(Geometry, PolygonVerts[i], Normal,
            FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * i / NumSides),
                0.5f + 0.5f * FMath::Sin(2 * PI * i / NumSides)));
        int32 V1 = GetOrAddVertex(Geometry, PolygonVerts[NextIndex], Normal,
            FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * NextIndex / NumSides),
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
    if (!ProceduralMesh) return false;

    // 创建构建参数
    FPyramidBuildParameters BuildParams;
    BuildParams.BaseRadius = BaseRadius;
    BuildParams.Height = Height;
    BuildParams.Sides = Sides;
    BuildParams.bCreateBottom = bCreateBottom;
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

void APyramid::GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides, bool bInCreateBottom)
{
    BaseRadius = InBaseRadius;
    Height = InHeight;
    Sides = InSides;
    bCreateBottom = bInCreateBottom;
    
    GenerateMeshInternal();
}
