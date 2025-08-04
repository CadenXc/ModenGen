// HollowPrism.cpp
#include "HollowPrism.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

AHollowPrism::AHollowPrism()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建网格组件
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PrismMesh"));
    RootComponent = MeshComponent;

    // 配置网格属性
    MeshComponent->bUseAsyncCooking = true;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetSimulatePhysics(false);

    // 初始生成
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

    // 清除现有网格
    MeshComponent->ClearAllMeshSections();

    // 验证参数
    Parameters.InnerRadius = FMath::Max(0.01f, Parameters.InnerRadius);
    Parameters.OuterRadius = FMath::Max(0.01f, Parameters.OuterRadius);
    Parameters.Height = FMath::Max(0.01f, Parameters.Height);
    Parameters.InnerSides = FMath::Max(3, Parameters.InnerSides);
    Parameters.OuterSides = FMath::Max(3, Parameters.OuterSides);
    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);

    // 创建新网格数据
    FMeshSection MeshData;

    // 预估内存需求
    const int32 TotalSides = FMath::Max(Parameters.InnerSides, Parameters.OuterSides);
    const int32 VertexCountEstimate = (TotalSides + 1) * 8; // 4层(内上/内下/外上/外下) * 2
    const int32 TriangleCountEstimate = TotalSides * 12; // 每边3个四边形 * 4面

    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate);

    // 生成几何部件
    GenerateSideGeometry(MeshData);
    GenerateTopGeometry(MeshData);
    GenerateBottomGeometry(MeshData);

    if (Parameters.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps(MeshData);
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
        UE_LOG(LogTemp, Warning, TEXT("Generated prism mesh has no vertices"));
    }
}

int32 AHollowPrism::AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV)
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

void AHollowPrism::GenerateSideGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);
    const int32 Sides = FMath::Max(Parameters.InnerSides, Parameters.OuterSides);

    // 存储顶点索引 [层][边]
    TArray<int32> InnerRingTop, InnerRingBottom;
    TArray<int32> OuterRingTop, OuterRingBottom;

    // 生成顶点环
    for (int32 s = 0; s <= Sides; ++s)
    {
        const float Angle = s * ArcRad / Sides;
        const float CosA = FMath::Cos(Angle);
        const float SinA = FMath::Sin(Angle);

        // 内环顶点 (上/下)
        const FVector InnerTopPos = FVector(
            Parameters.InnerRadius * CosA,
            Parameters.InnerRadius * SinA,
            HalfHeight
        );

        const FVector InnerBottomPos = FVector(
            Parameters.InnerRadius * CosA,
            Parameters.InnerRadius * SinA,
            -HalfHeight
        );

        // 外环顶点 (上/下)
        const FVector OuterTopPos = FVector(
            Parameters.OuterRadius * CosA,
            Parameters.OuterRadius * SinA,
            HalfHeight
        );

        const FVector OuterBottomPos = FVector(
            Parameters.OuterRadius * CosA,
            Parameters.OuterRadius * SinA,
            -HalfHeight
        );

        // 法线 (径向)
        const FVector RadialNormal = FVector(CosA, SinA, 0).GetSafeNormal();

        // UV映射
        const float U = static_cast<float>(s) / Sides;

        // 添加顶点
        InnerRingTop.Add(AddVertex(Section, InnerTopPos, -RadialNormal, FVector2D(U, 1.0f)));
        InnerRingBottom.Add(AddVertex(Section, InnerBottomPos, -RadialNormal, FVector2D(U, 0.0f)));
        OuterRingTop.Add(AddVertex(Section, OuterTopPos, RadialNormal, FVector2D(U, 1.0f)));
        OuterRingBottom.Add(AddVertex(Section, OuterBottomPos, RadialNormal, FVector2D(U, 0.0f)));
    }

    // 生成侧面四边形
    for (int32 s = 0; s < Sides; ++s)
    {
        // 内侧面 (法线朝内)
        AddQuad(Section,
            InnerRingTop[s],    // 当前边 上
            InnerRingBottom[s], // 当前边 下
            InnerRingBottom[s + 1], // 下一边 下
            InnerRingTop[s + 1]  // 下一边 上
        );

        // 外侧面 (法线朝外)
        AddQuad(Section,
            OuterRingTop[s],    // 当前边 上
            OuterRingTop[s + 1], // 下一边 上
            OuterRingBottom[s + 1], // 下一边 下
            OuterRingBottom[s]  // 当前边 下
        );
    }
}

void AHollowPrism::GenerateTopGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);
    const int32 Sides = FMath::Max(Parameters.InnerSides, Parameters.OuterSides);

    // 生成顶面四边形
    for (int32 s = 0; s < Sides; ++s)
    {
        const float Angle1 = s * ArcRad / Sides;
        const float Angle2 = (s + 1) * ArcRad / Sides;

        const FVector InnerTop1 = FVector(
            Parameters.InnerRadius * FMath::Cos(Angle1),
            Parameters.InnerRadius * FMath::Sin(Angle1),
            HalfHeight
        );

        const FVector InnerTop2 = FVector(
            Parameters.InnerRadius * FMath::Cos(Angle2),
            Parameters.InnerRadius * FMath::Sin(Angle2),
            HalfHeight
        );

        const FVector OuterTop1 = FVector(
            Parameters.OuterRadius * FMath::Cos(Angle1),
            Parameters.OuterRadius * FMath::Sin(Angle1),
            HalfHeight
        );

        const FVector OuterTop2 = FVector(
            Parameters.OuterRadius * FMath::Cos(Angle2),
            Parameters.OuterRadius * FMath::Sin(Angle2),
            HalfHeight
        );

        // 法线 (朝上)
        const FVector TopNormal = FVector(0, 0, 1);

        // UV映射
        const FVector2D UV_Inner1(0.5f + 0.5f * FMath::Cos(Angle1), 0.5f + 0.5f * FMath::Sin(Angle1));
        const FVector2D UV_Inner2(0.5f + 0.5f * FMath::Cos(Angle2), 0.5f + 0.5f * FMath::Sin(Angle2));
        const FVector2D UV_Outer1(0.5f + 0.5f * FMath::Cos(Angle1), 0.5f + 0.5f * FMath::Sin(Angle1));
        const FVector2D UV_Outer2(0.5f + 0.5f * FMath::Cos(Angle2), 0.5f + 0.5f * FMath::Sin(Angle2));

        // 添加顶点
        const int32 V_Inner1 = AddVertex(Section, InnerTop1, TopNormal, UV_Inner1);
        const int32 V_Inner2 = AddVertex(Section, InnerTop2, TopNormal, UV_Inner2);
        const int32 V_Outer1 = AddVertex(Section, OuterTop1, TopNormal, UV_Outer1);
        const int32 V_Outer2 = AddVertex(Section, OuterTop2, TopNormal, UV_Outer2);

        // 创建顶面四边形 (内->外)
        AddQuad(Section,
            V_Inner1,
            V_Inner2,
            V_Outer2,
            V_Outer1
        );
    }
}

void AHollowPrism::GenerateBottomGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);
    const int32 Sides = FMath::Max(Parameters.InnerSides, Parameters.OuterSides);

    // 生成底面四边形
    for (int32 s = 0; s < Sides; ++s)
    {
        const float Angle1 = s * ArcRad / Sides;
        const float Angle2 = (s + 1) * ArcRad / Sides;

        const FVector InnerBottom1 = FVector(
            Parameters.InnerRadius * FMath::Cos(Angle1),
            Parameters.InnerRadius * FMath::Sin(Angle1),
            -HalfHeight
        );

        const FVector InnerBottom2 = FVector(
            Parameters.InnerRadius * FMath::Cos(Angle2),
            Parameters.InnerRadius * FMath::Sin(Angle2),
            -HalfHeight
        );

        const FVector OuterBottom1 = FVector(
            Parameters.OuterRadius * FMath::Cos(Angle1),
            Parameters.OuterRadius * FMath::Sin(Angle1),
            -HalfHeight
        );

        const FVector OuterBottom2 = FVector(
            Parameters.OuterRadius * FMath::Cos(Angle2),
            Parameters.OuterRadius * FMath::Sin(Angle2),
            -HalfHeight
        );

        // 法线 (朝下)
        const FVector BottomNormal = FVector(0, 0, -1);

        // UV映射
        const FVector2D UV_Inner1(0.5f + 0.5f * FMath::Cos(Angle1), 0.5f + 0.5f * FMath::Sin(Angle1));
        const FVector2D UV_Inner2(0.5f + 0.5f * FMath::Cos(Angle2), 0.5f + 0.5f * FMath::Sin(Angle2));
        const FVector2D UV_Outer1(0.5f + 0.5f * FMath::Cos(Angle1), 0.5f + 0.5f * FMath::Sin(Angle1));
        const FVector2D UV_Outer2(0.5f + 0.5f * FMath::Cos(Angle2), 0.5f + 0.5f * FMath::Sin(Angle2));

        // 添加顶点
        const int32 V_Inner1 = AddVertex(Section, InnerBottom1, BottomNormal, UV_Inner1);
        const int32 V_Inner2 = AddVertex(Section, InnerBottom2, BottomNormal, UV_Inner2);
        const int32 V_Outer1 = AddVertex(Section, OuterBottom1, BottomNormal, UV_Outer1);
        const int32 V_Outer2 = AddVertex(Section, OuterBottom2, BottomNormal, UV_Outer2);

        // 创建底面四边形 (外->内)
        AddQuad(Section,
            V_Outer1,
            V_Outer2,
            V_Inner2,
            V_Inner1
        );
    }
}

void AHollowPrism::GenerateEndCaps(FMeshSection& Section)
{
    if (Parameters.ArcAngle >= 360.0f) return;

    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    // 起始端面
    const float StartAngle = 0.0f;
    const FVector StartNormal = FVector(-FMath::Cos(StartAngle), -FMath::Sin(StartAngle), 0).GetSafeNormal();

    // 结束端面
    const float EndAngle = ArcRad;
    const FVector EndNormal = FVector(FMath::Cos(EndAngle), FMath::Sin(EndAngle), 0).GetSafeNormal();

    // 起始端顶点
    const FVector InnerTopStart = FVector(
        Parameters.InnerRadius * FMath::Cos(StartAngle),
        Parameters.InnerRadius * FMath::Sin(StartAngle),
        HalfHeight
    );

    const FVector InnerBottomStart = FVector(
        Parameters.InnerRadius * FMath::Cos(StartAngle),
        Parameters.InnerRadius * FMath::Sin(StartAngle),
        -HalfHeight
    );

    const FVector OuterTopStart = FVector(
        Parameters.OuterRadius * FMath::Cos(StartAngle),
        Parameters.OuterRadius * FMath::Sin(StartAngle),
        HalfHeight
    );

    const FVector OuterBottomStart = FVector(
        Parameters.OuterRadius * FMath::Cos(StartAngle),
        Parameters.OuterRadius * FMath::Sin(StartAngle),
        -HalfHeight
    );

    // 结束端顶点
    const FVector InnerTopEnd = FVector(
        Parameters.InnerRadius * FMath::Cos(EndAngle),
        Parameters.InnerRadius * FMath::Sin(EndAngle),
        HalfHeight
    );

    const FVector InnerBottomEnd = FVector(
        Parameters.InnerRadius * FMath::Cos(EndAngle),
        Parameters.InnerRadius * FMath::Sin(EndAngle),
        -HalfHeight
    );

    const FVector OuterTopEnd = FVector(
        Parameters.OuterRadius * FMath::Cos(EndAngle),
        Parameters.OuterRadius * FMath::Sin(EndAngle),
        HalfHeight
    );

    const FVector OuterBottomEnd = FVector(
        Parameters.OuterRadius * FMath::Cos(EndAngle),
        Parameters.OuterRadius * FMath::Sin(EndAngle),
        -HalfHeight
    );

    // 添加起始端顶点
    const int32 V_InnerTopStart = AddVertex(Section, InnerTopStart, StartNormal, FVector2D(0, 1));
    const int32 V_InnerBottomStart = AddVertex(Section, InnerBottomStart, StartNormal, FVector2D(0, 0));
    const int32 V_OuterTopStart = AddVertex(Section, OuterTopStart, StartNormal, FVector2D(1, 1));
    const int32 V_OuterBottomStart = AddVertex(Section, OuterBottomStart, StartNormal, FVector2D(1, 0));

    // 添加结束端顶点
    const int32 V_InnerTopEnd = AddVertex(Section, InnerTopEnd, EndNormal, FVector2D(0, 1));
    const int32 V_InnerBottomEnd = AddVertex(Section, InnerBottomEnd, EndNormal, FVector2D(0, 0));
    const int32 V_OuterTopEnd = AddVertex(Section, OuterTopEnd, EndNormal, FVector2D(1, 1));
    const int32 V_OuterBottomEnd = AddVertex(Section, OuterBottomEnd, EndNormal, FVector2D(1, 0));

    // 创建起始端面
    AddQuad(Section,
        V_InnerTopStart,
        V_OuterTopStart,
        V_OuterBottomStart,
        V_InnerBottomStart
    );

    // 创建结束端面
    AddQuad(Section,
        V_OuterTopEnd,
        V_InnerTopEnd,
        V_InnerBottomEnd,
        V_OuterBottomEnd
    );
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
        UE_LOG(LogTemp, Warning, TEXT("Failed to find default material. Using fallback."));
        UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        if (FallbackMaterial)
        {
            MeshComponent->SetMaterial(0, FallbackMaterial);
        }
    }
}
