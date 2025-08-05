#include "HollowPrism.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

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

void AHollowPrism::GenerateGeometry()
{
    if (!MeshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("Mesh component is missing!"));
        return;
    }

    MeshComponent->ClearAllMeshSections();

    Parameters.InnerRadius = FMath::Max(0.01f, Parameters.InnerRadius);
    Parameters.OuterRadius = FMath::Max(0.01f, Parameters.OuterRadius);
    Parameters.Height = FMath::Max(0.01f, Parameters.Height);
    Parameters.InnerSides = FMath::Max(3, Parameters.InnerSides);
    Parameters.OuterSides = FMath::Max(3, Parameters.OuterSides);
    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);

    FMeshSection MeshData;

    const int32 VertexCountEstimate = (Parameters.InnerSides + Parameters.OuterSides) * 4 + 16;
    const int32 TriangleCountEstimate = (Parameters.InnerSides + Parameters.OuterSides) * 12;
    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate);

    GenerateSideGeometry(MeshData);
    GenerateTopGeometry(MeshData);
    GenerateBottomGeometry(MeshData);

    if (Parameters.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps(MeshData);
    }

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

int32 AHollowPrism::AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV)
{
    const int32 Index = Section.Vertices.Add(Position);
    Section.Normals.Add(Normal);
    Section.UVs.Add(UV);
    Section.VertexColors.Add(FLinearColor::White);

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
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V2);
    Section.Triangles.Add(V3);

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

void AHollowPrism::ConnectRingsWithTriangles(FMeshSection& Section, const TArray<int32>& InnerRing, const TArray<int32>& OuterRing, bool bReverse)
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
            // 修复底面三角形连接顺序
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
            // 顶面连接顺序保持不变
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

void AHollowPrism::GenerateSideGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    TArray<int32> InnerRingTop, InnerRingBottom;
    TArray<int32> OuterRingTop, OuterRingBottom;

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

void AHollowPrism::GenerateTopGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    TArray<int32> InnerRing, OuterRing;
    const FVector TopNormal(0, 0, 1);

    // 内环顶点
    for (int32 s = 0; s <= Parameters.InnerSides; s++)
    {
        const float Angle = s * ArcRad / Parameters.InnerSides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        const FVector InnerPos(Parameters.InnerRadius * CosA, Parameters.InnerRadius * SinA, HalfHeight);
        const FVector2D UV(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA);
        InnerRing.Add(AddVertex(Section, InnerPos, TopNormal, UV));
    }

    // 外环顶点
    for (int32 s = 0; s <= Parameters.OuterSides; s++)
    {
        const float Angle = s * ArcRad / Parameters.OuterSides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        const FVector OuterPos(Parameters.OuterRadius * CosA, Parameters.OuterRadius * SinA, HalfHeight);
        const FVector2D UV(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA);
        OuterRing.Add(AddVertex(Section, OuterPos, TopNormal, UV));
    }

    // 使用三角剖分连接内环和外环
    ConnectRingsWithTriangles(Section, InnerRing, OuterRing);
}

void AHollowPrism::GenerateBottomGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    TArray<int32> InnerRing, OuterRing;
    const FVector BottomNormal(0, 0, -1);

    // 内环顶点
    for (int32 s = 0; s <= Parameters.InnerSides; s++)
    {
        const float Angle = s * ArcRad / Parameters.InnerSides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        const FVector InnerPos(Parameters.InnerRadius * CosA, Parameters.InnerRadius * SinA, -HalfHeight);
        const FVector2D UV(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA);
        InnerRing.Add(AddVertex(Section, InnerPos, BottomNormal, UV));
    }

    // 外环顶点
    for (int32 s = 0; s <= Parameters.OuterSides; s++)
    {
        const float Angle = s * ArcRad / Parameters.OuterSides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        const FVector OuterPos(Parameters.OuterRadius * CosA, Parameters.OuterRadius * SinA, -HalfHeight);
        const FVector2D UV(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA);
        OuterRing.Add(AddVertex(Section, OuterPos, BottomNormal, UV));
    }

    // 使用三角剖分连接内环和外环 (反转方向)
    ConnectRingsWithTriangles(Section, InnerRing, OuterRing, true);
}

void AHollowPrism::GenerateEndCaps(FMeshSection& Section)
{
    if (Parameters.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) return;

    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    // 计算起始端(0度)和结束端(ArcAngle)的法线方向
    const FVector StartNormal(-FMath::Cos(0), -FMath::Sin(0), 0); // 指向外侧的法线
    const FVector EndNormal(FMath::Cos(ArcRad), FMath::Sin(ArcRad), 0);

    // 添加顶部和底部中心点（沿Z轴）
    const int32 TopCenter = AddVertex(Section, FVector(0, 0, HalfHeight), FVector(0, 0, 1), FVector2D(0.5f, 0.5f));
    const int32 BottomCenter = AddVertex(Section, FVector(0, 0, -HalfHeight), FVector(0, 0, -1), FVector2D(0.5f, 0.5f));

    // 起始端顶点（0度位置）
    const FVector StartInnerTopPos(Parameters.InnerRadius * FMath::Cos(0), Parameters.InnerRadius * FMath::Sin(0), HalfHeight);
    const FVector StartInnerBottomPos(Parameters.InnerRadius * FMath::Cos(0), Parameters.InnerRadius * FMath::Sin(0), -HalfHeight);
    const FVector StartOuterTopPos(Parameters.OuterRadius * FMath::Cos(0), Parameters.OuterRadius * FMath::Sin(0), HalfHeight);
    const FVector StartOuterBottomPos(Parameters.OuterRadius * FMath::Cos(0), Parameters.OuterRadius * FMath::Sin(0), -HalfHeight);

    const int32 V_InnerTopStart = AddVertex(Section, StartInnerTopPos, StartNormal, FVector2D(0, 1));
    const int32 V_InnerBottomStart = AddVertex(Section, StartInnerBottomPos, StartNormal, FVector2D(0, 0));
    const int32 V_OuterTopStart = AddVertex(Section, StartOuterTopPos, StartNormal, FVector2D(1, 1));
    const int32 V_OuterBottomStart = AddVertex(Section, StartOuterBottomPos, StartNormal, FVector2D(1, 0));

    // 结束端顶点（ArcAngle位置）
    const float EndCos = FMath::Cos(ArcRad);
    const float EndSin = FMath::Sin(ArcRad);
    const FVector EndInnerTopPos(Parameters.InnerRadius * EndCos, Parameters.InnerRadius * EndSin, HalfHeight);
    const FVector EndInnerBottomPos(Parameters.InnerRadius * EndCos, Parameters.InnerRadius * EndSin, -HalfHeight);
    const FVector EndOuterTopPos(Parameters.OuterRadius * EndCos, Parameters.OuterRadius * EndSin, HalfHeight);
    const FVector EndOuterBottomPos(Parameters.OuterRadius * EndCos, Parameters.OuterRadius * EndSin, -HalfHeight);

    const int32 V_InnerTopEnd = AddVertex(Section, EndInnerTopPos, EndNormal, FVector2D(0, 1));
    const int32 V_InnerBottomEnd = AddVertex(Section, EndInnerBottomPos, EndNormal, FVector2D(0, 0));
    const int32 V_OuterTopEnd = AddVertex(Section, EndOuterTopPos, EndNormal, FVector2D(1, 1));
    const int32 V_OuterBottomEnd = AddVertex(Section, EndOuterBottomPos, EndNormal, FVector2D(1, 0));

    // 构建起始端封闭面
    // 顶部四边形（连接中心点和内外环）
    AddQuad(Section,
        TopCenter,
        V_InnerTopStart,
        V_OuterTopStart,
        TopCenter
    );
    // 底部四边形（连接中心点和内外环）
    AddQuad(Section,
        BottomCenter,
        V_OuterBottomStart,
        V_InnerBottomStart,
        BottomCenter
    );
    // 侧面四边形（连接上下）
    AddQuad(Section,
        V_InnerTopStart,
        V_InnerBottomStart,
        V_OuterBottomStart,
        V_OuterTopStart
    );

    // 构建结束端封闭面
    // 顶部四边形（连接中心点和内外环）
    AddQuad(Section,
        TopCenter,
        V_OuterTopEnd,
        V_InnerTopEnd,
        TopCenter
    );
    // 底部四边形（连接中心点和内外环）
    AddQuad(Section,
        BottomCenter,
        V_InnerBottomEnd,
        V_OuterBottomEnd,
        BottomCenter
    );
    // 侧面四边形（连接上下）
    AddQuad(Section,
        V_OuterTopEnd,
        V_OuterBottomEnd,
        V_InnerBottomEnd,
        V_InnerTopEnd
    );

    // 连接中心点与内外环形成完整切面
    AddTriangle(Section, TopCenter, V_InnerTopEnd, V_InnerTopStart);
    AddTriangle(Section, BottomCenter, V_InnerBottomStart, V_InnerBottomEnd);
    AddTriangle(Section, TopCenter, V_OuterTopStart, V_OuterTopEnd);
    AddTriangle(Section, BottomCenter, V_OuterBottomEnd, V_OuterBottomStart);
}

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