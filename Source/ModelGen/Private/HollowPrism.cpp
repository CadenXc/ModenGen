#include "HollowPrism.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

// ============================================================================
// 构造函数和生命周期
// ============================================================================

AHollowPrism::AHollowPrism()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PrismMesh"));
    RootComponent = MeshComponent;

    MeshComponent->bUseAsyncCooking = true;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetSimulatePhysics(false);

    GenerateGeometry();
}

void AHollowPrism::BeginPlay()
{
    Super::BeginPlay();
    GenerateGeometry();
}

#if WITH_EDITOR
void AHollowPrism::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    static const TArray<FName> RelevantProperties = {
        "InnerRadius", "OuterRadius", "Height",
        "InnerSides", "OuterSides", "ArcAngle"
    };

    if (RelevantProperties.Contains(PropertyName))
    {
        GenerateGeometry();
    }
}
#endif

void AHollowPrism::Regenerate()
{
    GenerateGeometry();
}

// ============================================================================
// 主要几何生成函数
// ============================================================================

void AHollowPrism::GenerateGeometry()
{
    if (!MeshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("Mesh component is missing!"));
        return;
    }

    MeshComponent->ClearAllMeshSections();

    // 验证和约束参数
    ValidateAndClampParameters();

    FMeshSection MeshData;
    ReserveMeshMemory(MeshData);

    // 生成各个几何部分
    GenerateSideGeometry(MeshData);
    GenerateTopGeometry(MeshData);
    GenerateBottomGeometry(MeshData);

    // 如果弧角小于360度，生成端面
    if (Parameters.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps(MeshData);
    }

    // 创建网格
    CreateMeshFromData(MeshData);
}

// ============================================================================
// 参数验证和内存分配
// ============================================================================

void AHollowPrism::ValidateAndClampParameters()
{
    Parameters.InnerRadius = FMath::Max(0.01f, Parameters.InnerRadius);
    Parameters.OuterRadius = FMath::Max(0.01f, Parameters.OuterRadius);
    Parameters.Height = FMath::Max(0.01f, Parameters.Height);
    Parameters.InnerSides = FMath::Max(3, Parameters.InnerSides);
    Parameters.OuterSides = FMath::Max(3, Parameters.OuterSides);
    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);
}

void AHollowPrism::ReserveMeshMemory(FMeshSection& MeshData)
{
    const int32 VertexCountEstimate = (Parameters.InnerSides + Parameters.OuterSides) * 4 + 16;
    const int32 TriangleCountEstimate = (Parameters.InnerSides + Parameters.OuterSides) * 12;
    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate);
}

void AHollowPrism::CreateMeshFromData(const FMeshSection& MeshData)
{
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
            true
        );
        ApplyMaterial();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Generated prism mesh has no vertices"));
    }
}

// ============================================================================
// 顶点和面片操作
// ============================================================================

int32 AHollowPrism::AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV)
{
    const int32 Index = Section.Vertices.Add(Position);
    Section.Normals.Add(Normal);
    Section.UVs.Add(UV);
    Section.VertexColors.Add(FLinearColor::White);

    // 计算切线
    FVector Tangent = FVector::CrossProduct(Normal, FVector::UpVector);
    if (Tangent.IsNearlyZero())
    {
        Tangent = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    Tangent.Normalize();
    Section.Tangents.Add(FProcMeshTangent(Tangent, false));

    return Index;
}

void AHollowPrism::AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4)
{
    // 第一个三角形
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V2);
    Section.Triangles.Add(V3);

    // 第二个三角形
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V3);
    Section.Triangles.Add(V4);
}

void AHollowPrism::AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3)
{
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V2);
    Section.Triangles.Add(V3);
}

// ============================================================================
// 侧面几何生成
// ============================================================================

void AHollowPrism::GenerateSideGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    // 生成顶点环
    TArray<int32> InnerRingTop, InnerRingBottom;
    TArray<int32> OuterRingTop, OuterRingBottom;
    
    GenerateVertexRings(Section, HalfHeight, ArcRad, InnerRingTop, InnerRingBottom, OuterRingTop, OuterRingBottom);

    // 生成侧面四边形
    GenerateSideQuads(Section, InnerRingTop, InnerRingBottom, OuterRingTop, OuterRingBottom);
}

void AHollowPrism::GenerateVertexRings(FMeshSection& Section, float HalfHeight, float ArcRad,
    TArray<int32>& InnerRingTop, TArray<int32>& InnerRingBottom,
    TArray<int32>& OuterRingTop, TArray<int32>& OuterRingBottom)
{
    // 内环顶点 (顶部和底部)
    for (int32 s = 0; s <= Parameters.InnerSides; s++)
    {
        const float Angle = s * ArcRad / Parameters.InnerSides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        const FVector InnerTopPos(Parameters.InnerRadius * CosA, Parameters.InnerRadius * SinA, HalfHeight);
        const FVector InnerBottomPos(Parameters.InnerRadius * CosA, Parameters.InnerRadius * SinA, -HalfHeight);
        const FVector RadialNormal(CosA, SinA, 0.0f);
        const float U = static_cast<float>(s) / Parameters.InnerSides;

        InnerRingTop.Add(AddVertex(Section, InnerTopPos, -RadialNormal, FVector2D(U, 1.0f)));
        InnerRingBottom.Add(AddVertex(Section, InnerBottomPos, -RadialNormal, FVector2D(U, 0.0f)));
    }

    // 外环顶点 (顶部和底部)
    for (int32 s = 0; s <= Parameters.OuterSides; s++)
    {
        const float Angle = s * ArcRad / Parameters.OuterSides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        const FVector OuterTopPos(Parameters.OuterRadius * CosA, Parameters.OuterRadius * SinA, HalfHeight);
        const FVector OuterBottomPos(Parameters.OuterRadius * CosA, Parameters.OuterRadius * SinA, -HalfHeight);
        const FVector RadialNormal(CosA, SinA, 0.0f);
        const float U = static_cast<float>(s) / Parameters.OuterSides;

        OuterRingTop.Add(AddVertex(Section, OuterTopPos, RadialNormal, FVector2D(U, 1.0f)));
        OuterRingBottom.Add(AddVertex(Section, OuterBottomPos, RadialNormal, FVector2D(U, 0.0f)));
    }
}

void AHollowPrism::GenerateSideQuads(FMeshSection& Section, 
    const TArray<int32>& InnerRingTop, const TArray<int32>& InnerRingBottom,
    const TArray<int32>& OuterRingTop, const TArray<int32>& OuterRingBottom)
{
    // 内侧面
    for (int32 s = 0; s < Parameters.InnerSides; s++)
    {
        AddQuad(Section,
            InnerRingTop[s],
            InnerRingBottom[s],
            InnerRingBottom[s + 1],
            InnerRingTop[s + 1]
        );
    }

    // 外侧面
    for (int32 s = 0; s < Parameters.OuterSides; s++)
    {
        AddQuad(Section,
            OuterRingTop[s],
            OuterRingTop[s + 1],
            OuterRingBottom[s + 1],
            OuterRingBottom[s]
        );
    }
}

// ============================================================================
// 顶面和底面几何生成
// ============================================================================

void AHollowPrism::GenerateTopGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    TArray<int32> InnerRing, OuterRing;
    const FVector TopNormal(0, 0, 1);

    GenerateRingVertices(Section, HalfHeight, ArcRad, TopNormal, InnerRing, OuterRing);
    ConnectRingsWithTriangles(Section, InnerRing, OuterRing, false);
}

void AHollowPrism::GenerateBottomGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    TArray<int32> InnerRing, OuterRing;
    const FVector BottomNormal(0, 0, -1);

    GenerateRingVertices(Section, -HalfHeight, ArcRad, BottomNormal, InnerRing, OuterRing);
    ConnectRingsWithTriangles(Section, InnerRing, OuterRing, true);
}

void AHollowPrism::GenerateRingVertices(FMeshSection& Section, float Height, float ArcRad, 
    const FVector& Normal, TArray<int32>& InnerRing, TArray<int32>& OuterRing)
{
    // 内环顶点
    for (int32 s = 0; s <= Parameters.InnerSides; s++)
    {
        const float Angle = s * ArcRad / Parameters.InnerSides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        const FVector InnerPos(Parameters.InnerRadius * CosA, Parameters.InnerRadius * SinA, Height);
        const FVector2D UV(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA);
        InnerRing.Add(AddVertex(Section, InnerPos, Normal, UV));
    }

    // 外环顶点
    for (int32 s = 0; s <= Parameters.OuterSides; s++)
    {
        const float Angle = s * ArcRad / Parameters.OuterSides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        const FVector OuterPos(Parameters.OuterRadius * CosA, Parameters.OuterRadius * SinA, Height);
        const FVector2D UV(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA);
        OuterRing.Add(AddVertex(Section, OuterPos, Normal, UV));
    }
}

void AHollowPrism::ConnectRingsWithTriangles(FMeshSection& Section, const TArray<int32>& InnerRing, 
    const TArray<int32>& OuterRing, bool bReverse)
{
    const int32 InnerCount = InnerRing.Num();
    const int32 OuterCount = OuterRing.Num();
    const int32 TotalSegments = FMath::Max(InnerCount, OuterCount);

    for (int32 i = 0; i < TotalSegments; i++)
    {
        const float Ratio = static_cast<float>(i) / TotalSegments;
        const int32 InnerIndex = FMath::FloorToInt(Ratio * (InnerCount - 1));
        const int32 OuterIndex = FMath::FloorToInt(Ratio * (OuterCount - 1));

        const int32 NextInnerIndex = (InnerIndex + 1) % (InnerCount - 1);
        const int32 NextOuterIndex = (OuterIndex + 1) % (OuterCount - 1);

        if (bReverse)
        {
            // 底面三角形连接顺序
            AddTriangle(Section,
                InnerRing[InnerIndex],
                OuterRing[OuterIndex],
                OuterRing[NextOuterIndex]);

            AddTriangle(Section,
                InnerRing[InnerIndex],
                OuterRing[NextOuterIndex],
                InnerRing[NextInnerIndex]);
        }
        else
        {
            // 顶面连接顺序
            AddTriangle(Section,
                InnerRing[InnerIndex],
                OuterRing[NextOuterIndex],
                OuterRing[OuterIndex]);

            AddTriangle(Section,
                InnerRing[InnerIndex],
                InnerRing[NextInnerIndex],
                OuterRing[NextOuterIndex]);
        }
    }
}

// ============================================================================
// 端面几何生成
// ============================================================================

void AHollowPrism::GenerateEndCaps(FMeshSection& Section)
{
    // 完全移除端面生成 - 当ArcAngle < 360度时保持完全开放
    // 不生成任何端面顶点或连接，避免孤立的线条
}

// ============================================================================
// 材质应用
// ============================================================================

void AHollowPrism::ApplyMaterial()
{
    static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (DefaultMaterial.Succeeded())
    {
        MeshComponent->SetMaterial(0, DefaultMaterial.Object);
    }
    else
    {
        UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        if (FallbackMaterial)
        {
            MeshComponent->SetMaterial(0, FallbackMaterial);
        }
    }
}