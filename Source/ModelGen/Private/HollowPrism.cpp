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
        "Sides", "ArcAngle", "bUseTriangleMethod",
        "ChamferRadius", "bUseChamfer", "ChamferSections", "ChamferSegments"
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

    // 参数验证
    Parameters.InnerRadius = FMath::Max(0.01f, Parameters.InnerRadius);
    Parameters.OuterRadius = FMath::Max(0.01f, Parameters.OuterRadius);
    Parameters.Height = FMath::Max(0.01f, Parameters.Height);
    Parameters.Sides = FMath::Max(3, Parameters.Sides);
    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);

    // 确保外半径大于内半径
    if (Parameters.OuterRadius <= Parameters.InnerRadius)
    {
        Parameters.OuterRadius = Parameters.InnerRadius + 1.0f;
    }

    // 倒角参数验证
    Parameters.ChamferRadius = FMath::Max(0.0f, Parameters.ChamferRadius);
    Parameters.ChamferSections = FMath::Max(1, Parameters.ChamferSections);
    Parameters.ChamferSegments = FMath::Max(1, Parameters.ChamferSegments);
    if (Parameters.bUseChamfer)
    {
        // 确保倒角不会导致内外环重叠
        const float MaxChamferRadius = (Parameters.OuterRadius - Parameters.InnerRadius) * 0.5f;
        Parameters.ChamferRadius = FMath::Min(Parameters.ChamferRadius, MaxChamferRadius);
    }

    FMeshSection MeshData;

    // 顶点和三角形数量估算
    const int32 VertexCountEstimate =
        Parameters.Sides * 8 +  // 侧面（内外各4个顶点）
        Parameters.Sides * 4 +  // 顶面/底面（内外各2个顶点）
        (Parameters.ArcAngle < 360.0f ? 8 : 0) +  // 端盖
        (Parameters.bUseChamfer ? Parameters.ChamferSegments * Parameters.Sides * 8 : 0);  // 倒角

    const int32 TriangleCountEstimate =
        Parameters.Sides * 8 +  // 侧面
        Parameters.Sides * 4 +  // 顶面/底面
        (Parameters.ArcAngle < 360.0f ? 8 : 0) +  // 端盖
        (Parameters.bUseChamfer ? Parameters.ChamferSegments * Parameters.Sides * 6 : 0);  // 倒角

    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate * 3);

    // 生成各部件几何
    if (Parameters.bUseChamfer && Parameters.ChamferRadius > 0.0f)
    {
        GenerateChamferedGeometry(MeshData);
    }
    else
    {
        GenerateSideWalls(MeshData);

        if (Parameters.bUseTriangleMethod)
        {
            GenerateTopCapWithTriangles(MeshData);
            GenerateBottomCapWithTriangles(MeshData);
        }
        else
        {
            GenerateTopCapWithQuads(MeshData);
            GenerateBottomCapWithQuads(MeshData);
        }
    }

    if (Parameters.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps(MeshData);
    }

    // 创建网格
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

        // 应用材质
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



void AHollowPrism::CalculateRingVertices(float Radius, int32 Sides, float Z, float ArcAngle,
    TArray<FVector>& OutVertices, TArray<FVector2D>& OutUVs, float UOffset)
{
    const float ArcRad = FMath::DegreesToRadians(ArcAngle);
    const float AngleStep = ArcRad / FMath::Max(1, Sides);

    for (int32 i = 0; i <= Sides; i++)
    {
        const float Angle = i * AngleStep;
        const float X = Radius * FMath::Cos(Angle);
        const float Y = Radius * FMath::Sin(Angle);

        OutVertices.Add(FVector(X, Y, Z));

        // UV映射：U沿圆周方向，V沿高度方向
        const float U = UOffset + (static_cast<float>(i) / Sides);
        OutUVs.Add(FVector2D(U, Z > 0 ? 1.0f : 0.0f));
    }
}

void AHollowPrism::GenerateSideWalls(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点（顶部和底部）
    TArray<FVector> InnerTopVertices, InnerBottomVertices;
    TArray<FVector2D> InnerTopUVs, InnerBottomUVs;
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, HalfHeight, ArcAngle, InnerTopVertices, InnerTopUVs);
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, -HalfHeight, ArcAngle, InnerBottomVertices, InnerBottomUVs);

    // 生成外环顶点（顶部和底部）
    TArray<FVector> OuterTopVertices, OuterBottomVertices;
    TArray<FVector2D> OuterTopUVs, OuterBottomUVs;
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, HalfHeight, ArcAngle, OuterTopVertices, OuterTopUVs, 0.5f);
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, -HalfHeight, ArcAngle, OuterBottomVertices, OuterBottomUVs, 0.5f);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerTopIndices, InnerBottomIndices;
    TArray<int32> OuterTopIndices, OuterBottomIndices;

    // 内环顶点
    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        const FVector RadialNormal = (InnerTopVertices[i] - FVector(0, 0, InnerTopVertices[i].Z)).GetSafeNormal();
        InnerTopIndices.Add(AddVertex(Section, InnerTopVertices[i], -RadialNormal, InnerTopUVs[i]));
        InnerBottomIndices.Add(AddVertex(Section, InnerBottomVertices[i], -RadialNormal, InnerBottomUVs[i]));
    }

    // 外环顶点
    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        const FVector RadialNormal = (OuterTopVertices[i] - FVector(0, 0, OuterTopVertices[i].Z)).GetSafeNormal();
        OuterTopIndices.Add(AddVertex(Section, OuterTopVertices[i], RadialNormal, OuterTopUVs[i]));
        OuterBottomIndices.Add(AddVertex(Section, OuterBottomVertices[i], RadialNormal, OuterBottomUVs[i]));
    }

    // 生成内侧面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            InnerTopIndices[i],
            InnerBottomIndices[i],
            InnerBottomIndices[i + 1],
            InnerTopIndices[i + 1]
        );
    }

    // 生成外侧面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            OuterTopIndices[i],
            OuterTopIndices[i + 1],
            OuterBottomIndices[i + 1],
            OuterBottomIndices[i]
        );
    }
}

void AHollowPrism::GenerateTopCapWithTriangles(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, HalfHeight, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, HalfHeight, ArcAngle, OuterVertices, OuterUVs);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerIndices, OuterIndices;
    const FVector TopNormal(0, 0, 1);

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (InnerVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (InnerVertices[i].Y / Parameters.OuterRadius)
        );
        InnerIndices.Add(AddVertex(Section, InnerVertices[i], TopNormal, UV));
    }

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (OuterVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (OuterVertices[i].Y / Parameters.OuterRadius)
        );
        OuterIndices.Add(AddVertex(Section, OuterVertices[i], TopNormal, UV));
    }

    // 使用三角形方法连接内外环（不添加中心点，避免遮挡内孔）
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        // 连接外环到内环，形成梯形
        AddQuad(Section,
            OuterIndices[i],
            InnerIndices[i],
            InnerIndices[i + 1],
            OuterIndices[i + 1]
        );
    }
}

void AHollowPrism::GenerateBottomCapWithTriangles(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, -HalfHeight, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, -HalfHeight, ArcAngle, OuterVertices, OuterUVs);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerIndices, OuterIndices;
    const FVector BottomNormal(0, 0, -1);

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (InnerVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (InnerVertices[i].Y / Parameters.OuterRadius)
        );
        InnerIndices.Add(AddVertex(Section, InnerVertices[i], BottomNormal, UV));
    }

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (OuterVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (OuterVertices[i].Y / Parameters.OuterRadius)
        );
        OuterIndices.Add(AddVertex(Section, OuterVertices[i], BottomNormal, UV));
    }

    // 使用三角形方法连接内外环（注意顶点顺序，确保法线正确）
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        // 连接外环到内环，形成梯形（注意顶点顺序）
        AddQuad(Section,
            OuterIndices[i + 1],
            OuterIndices[i],
            InnerIndices[i],
            InnerIndices[i + 1]
        );
    }
}

void AHollowPrism::GenerateTopCapWithQuads(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, HalfHeight, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, HalfHeight, ArcAngle, OuterVertices, OuterUVs);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerIndices, OuterIndices;
    const FVector TopNormal(0, 0, 1);

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (InnerVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (InnerVertices[i].Y / Parameters.OuterRadius)
        );
        InnerIndices.Add(AddVertex(Section, InnerVertices[i], TopNormal, UV));
    }

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (OuterVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (OuterVertices[i].Y / Parameters.OuterRadius)
        );
        OuterIndices.Add(AddVertex(Section, OuterVertices[i], TopNormal, UV));
    }

    // 连接内外环形成顶面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            InnerIndices[i],
            OuterIndices[i],
            OuterIndices[i + 1],
            InnerIndices[i + 1]
        );
    }
}

void AHollowPrism::GenerateBottomCapWithQuads(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, -HalfHeight, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, -HalfHeight, ArcAngle, OuterVertices, OuterUVs);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerIndices, OuterIndices;
    const FVector BottomNormal(0, 0, -1);

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (InnerVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (InnerVertices[i].Y / Parameters.OuterRadius)
        );
        InnerIndices.Add(AddVertex(Section, InnerVertices[i], BottomNormal, UV));
    }

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (OuterVertices[i].X / Parameters.OuterRadius),
            0.5f + 0.5f * (OuterVertices[i].Y / Parameters.OuterRadius)
        );
        OuterIndices.Add(AddVertex(Section, OuterVertices[i], BottomNormal, UV));
    }

    // 连接内外环形成底面（注意顶点顺序反转）
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            OuterIndices[i],
            InnerIndices[i],
            InnerIndices[i + 1],
            OuterIndices[i + 1]
        );
    }
}

void AHollowPrism::GenerateTopCap(FMeshSection& Section)
{
    if (Parameters.bUseTriangleMethod)
    {
        GenerateTopCapWithTriangles(Section);
    }
    else
    {
        GenerateTopCapWithQuads(Section);
    }
}

void AHollowPrism::GenerateBottomCap(FMeshSection& Section)
{
    if (Parameters.bUseTriangleMethod)
    {
        GenerateBottomCapWithTriangles(Section);
    }
    else
    {
        GenerateBottomCapWithQuads(Section);
    }
}

void AHollowPrism::GenerateEndCaps(FMeshSection& Section)
{
    if (Parameters.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
        return;

    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcRad = FMath::DegreesToRadians(Parameters.ArcAngle);

    // 计算法线方向（垂直于端面）
    const FVector StartNormal(-FMath::Cos(0.0f), -FMath::Sin(0.0f), 0);
    const FVector EndNormal(FMath::Cos(ArcRad), FMath::Sin(ArcRad), 0);

    // 起始端顶点（0度位置）
    const int32 V_InnerTopStart = AddVertex(Section,
        FVector(Parameters.InnerRadius, 0, HalfHeight),
        StartNormal,
        FVector2D(0, 1));

    const int32 V_InnerBottomStart = AddVertex(Section,
        FVector(Parameters.InnerRadius, 0, -HalfHeight),
        StartNormal,
        FVector2D(0, 0));

    const int32 V_OuterTopStart = AddVertex(Section,
        FVector(Parameters.OuterRadius, 0, HalfHeight),
        StartNormal,
        FVector2D(1, 1));

    const int32 V_OuterBottomStart = AddVertex(Section,
        FVector(Parameters.OuterRadius, 0, -HalfHeight),
        StartNormal,
        FVector2D(1, 0));

    // 结束端顶点（ArcAngle位置）
    const int32 V_InnerTopEnd = AddVertex(Section,
        FVector(Parameters.InnerRadius * FMath::Cos(ArcRad), Parameters.InnerRadius * FMath::Sin(ArcRad), HalfHeight),
        EndNormal,
        FVector2D(0, 1));

    const int32 V_InnerBottomEnd = AddVertex(Section,
        FVector(Parameters.InnerRadius * FMath::Cos(ArcRad), Parameters.InnerRadius * FMath::Sin(ArcRad), -HalfHeight),
        EndNormal,
        FVector2D(0, 0));

    const int32 V_OuterTopEnd = AddVertex(Section,
        FVector(Parameters.OuterRadius * FMath::Cos(ArcRad), Parameters.OuterRadius * FMath::Sin(ArcRad), HalfHeight),
        EndNormal,
        FVector2D(1, 1));

    const int32 V_OuterBottomEnd = AddVertex(Section,
        FVector(Parameters.OuterRadius * FMath::Cos(ArcRad), Parameters.OuterRadius * FMath::Sin(ArcRad), -HalfHeight),
        EndNormal,
        FVector2D(1, 0));

    // 构建起始端封闭面
    AddQuad(Section,
        V_InnerTopStart,
        V_OuterTopStart,
        V_OuterBottomStart,
        V_InnerBottomStart
    );

    // 构建结束端封闭面
    AddQuad(Section,
        V_OuterTopEnd,
        V_InnerTopEnd,
        V_InnerBottomEnd,
        V_OuterBottomEnd
    );
}

void AHollowPrism::GenerateChamferedGeometry(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferedTopZ = HalfHeight - Parameters.ChamferRadius;
    const float ChamferedBottomZ = -HalfHeight + Parameters.ChamferRadius;
    const float ChamferedInnerRadius = Parameters.InnerRadius - Parameters.ChamferRadius;
    const float ChamferedOuterRadius = Parameters.OuterRadius - Parameters.ChamferRadius;

    // 生成几何部件
    GenerateChamferedSideWalls(Section, ChamferedTopZ, ChamferedBottomZ);
    CreateTopChamferGeometry(Section, HalfHeight, ChamferedTopZ);
    CreateBottomChamferGeometry(Section, -HalfHeight, ChamferedBottomZ);
    GenerateChamferedTopCap(Section, ChamferedTopZ, ChamferedInnerRadius, ChamferedOuterRadius);
    GenerateChamferedBottomCap(Section, ChamferedBottomZ, ChamferedInnerRadius, ChamferedOuterRadius);
    GenerateEdgeChamfers(Section, ChamferedTopZ, ChamferedBottomZ, ChamferedInnerRadius, ChamferedOuterRadius);
    GenerateCornerChamfers(Section, ChamferedTopZ, ChamferedBottomZ, ChamferedInnerRadius, ChamferedOuterRadius);
}

void AHollowPrism::GenerateChamferedSideWalls(FMeshSection& Section, float ChamferedTopZ, float ChamferedBottomZ)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 使用原始半径，倒角将从这里开始
    const float InnerRadius = Parameters.InnerRadius;
    const float OuterRadius = Parameters.OuterRadius;

    // 生成内环顶点（顶部和底部）
    TArray<FVector> InnerTopVertices, InnerBottomVertices;
    TArray<FVector2D> InnerTopUVs, InnerBottomUVs;
    CalculateRingVertices(InnerRadius, Parameters.Sides, ChamferedTopZ, ArcAngle, InnerTopVertices, InnerTopUVs);
    CalculateRingVertices(InnerRadius, Parameters.Sides, ChamferedBottomZ, ArcAngle, InnerBottomVertices, InnerBottomUVs);

    // 生成外环顶点（顶部和底部）
    TArray<FVector> OuterTopVertices, OuterBottomVertices;
    TArray<FVector2D> OuterTopUVs, OuterBottomUVs;
    CalculateRingVertices(OuterRadius, Parameters.Sides, ChamferedTopZ, ArcAngle, OuterTopVertices, OuterTopUVs, 0.5f);
    CalculateRingVertices(OuterRadius, Parameters.Sides, ChamferedBottomZ, ArcAngle, OuterBottomVertices, OuterBottomUVs, 0.5f);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerTopIndices, InnerBottomIndices;
    TArray<int32> OuterTopIndices, OuterBottomIndices;

    // 内环顶点
    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        const FVector RadialNormal = (InnerTopVertices[i] - FVector(0, 0, InnerTopVertices[i].Z)).GetSafeNormal();
        InnerTopIndices.Add(AddVertex(Section, InnerTopVertices[i], -RadialNormal, InnerTopUVs[i]));
        InnerBottomIndices.Add(AddVertex(Section, InnerBottomVertices[i], -RadialNormal, InnerBottomUVs[i]));
    }

    // 外环顶点
    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        const FVector RadialNormal = (OuterTopVertices[i] - FVector(0, 0, OuterTopVertices[i].Z)).GetSafeNormal();
        OuterTopIndices.Add(AddVertex(Section, OuterTopVertices[i], RadialNormal, OuterTopUVs[i]));
        OuterBottomIndices.Add(AddVertex(Section, OuterBottomVertices[i], RadialNormal, OuterBottomUVs[i]));
    }

    // 生成内侧面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            InnerTopIndices[i],
            InnerBottomIndices[i],
            InnerBottomIndices[i + 1],
            InnerTopIndices[i + 1]
        );
    }

    // 生成外侧面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            OuterTopIndices[i],
            OuterTopIndices[i + 1],
            OuterBottomIndices[i + 1],
            OuterBottomIndices[i]
        );
    }
}

void AHollowPrism::GenerateChamferedTopCap(FMeshSection& Section, float ChamferedTopZ, float ChamferedInnerRadius, float ChamferedOuterRadius)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(ChamferedInnerRadius, Parameters.Sides, ChamferedTopZ, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(ChamferedOuterRadius, Parameters.Sides, ChamferedTopZ, ArcAngle, OuterVertices, OuterUVs);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerIndices, OuterIndices;
    const FVector TopNormal(0, 0, 1);

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (InnerVertices[i].X / ChamferedOuterRadius),
            0.5f + 0.5f * (InnerVertices[i].Y / ChamferedOuterRadius)
        );
        InnerIndices.Add(AddVertex(Section, InnerVertices[i], TopNormal, UV));
    }

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (OuterVertices[i].X / ChamferedOuterRadius),
            0.5f + 0.5f * (OuterVertices[i].Y / ChamferedOuterRadius)
        );
        OuterIndices.Add(AddVertex(Section, OuterVertices[i], TopNormal, UV));
    }

    // 连接内外环形成顶面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            InnerIndices[i],
            OuterIndices[i],
            OuterIndices[i + 1],
            InnerIndices[i + 1]
        );
    }
}

void AHollowPrism::GenerateChamferedBottomCap(FMeshSection& Section, float ChamferedBottomZ, float ChamferedInnerRadius, float ChamferedOuterRadius)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(ChamferedInnerRadius, Parameters.Sides, ChamferedBottomZ, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(ChamferedOuterRadius, Parameters.Sides, ChamferedBottomZ, ArcAngle, OuterVertices, OuterUVs);

    // 添加顶点到网格并记录索引
    TArray<int32> InnerIndices, OuterIndices;
    const FVector BottomNormal(0, 0, 1);

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (InnerVertices[i].X / ChamferedOuterRadius),
            0.5f + 0.5f * (InnerVertices[i].Y / ChamferedOuterRadius)
        );
        InnerIndices.Add(AddVertex(Section, InnerVertices[i], BottomNormal, UV));
    }

    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        // 径向UV映射
        const FVector2D UV(
            0.5f + 0.5f * (OuterVertices[i].X / ChamferedOuterRadius),
            0.5f + 0.5f * (OuterVertices[i].Y / ChamferedOuterRadius)
        );
        OuterIndices.Add(AddVertex(Section, OuterVertices[i], BottomNormal, UV));
    }

    // 连接内外环形成底面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        AddQuad(Section,
            OuterIndices[i],
            OuterIndices[i + 1],
            InnerIndices[i + 1],
            InnerIndices[i]
        );
    }
}

void AHollowPrism::GenerateEdgeChamfers(FMeshSection& Section, float ChamferedTopZ, float ChamferedBottomZ, float ChamferedInnerRadius, float ChamferedOuterRadius)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    // 为每个角度生成边缘倒角
    for (int32 s = 0; s < Parameters.Sides; ++s)
    {
        const float Angle = s * AngleStep;
        const FVector RadialDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0);

        // 外环顶部边缘倒角
        GenerateEdgeChamfer(Section, RadialDir, Parameters.OuterRadius, ChamferedOuterRadius,
            HalfHeight, ChamferedTopZ, true, true, s);

        // 内环顶部边缘倒角
        GenerateEdgeChamfer(Section, RadialDir, Parameters.InnerRadius, ChamferedInnerRadius,
            HalfHeight, ChamferedTopZ, true, false, s);

        // 外环底部边缘倒角
        GenerateEdgeChamfer(Section, RadialDir, Parameters.OuterRadius, ChamferedOuterRadius,
            -HalfHeight, ChamferedBottomZ, false, true, s);

        // 内环底部边缘倒角
        GenerateEdgeChamfer(Section, RadialDir, Parameters.InnerRadius, ChamferedInnerRadius,
            -HalfHeight, ChamferedBottomZ, false, false, s);
    }
}

void AHollowPrism::GenerateEdgeChamfer(FMeshSection& Section, const FVector& RadialDir,
    float OriginalRadius, float ChamferedRadius,
    float OriginalZ, float ChamferedZ,
    bool bIsTop, bool bIsOuter, int32 SideIndex)
{
    TArray<int32> PrevStripIndices;

    for (int32 seg = 0; seg <= Parameters.ChamferSegments; ++seg)
    {
        const float Alpha = static_cast<float>(seg) / Parameters.ChamferSegments;

        // 计算当前段的法线插值
        FVector BaseNormal = bIsTop ? FVector(0, 0, 1) : FVector(0, 0, -1);
        FVector RadialNormal = bIsOuter ? RadialDir : -RadialDir;
        FVector CurrentNormal = FMath::Lerp(BaseNormal, RadialNormal, Alpha).GetSafeNormal();

        // 计算位置插值
        const float CurrentRadius = FMath::Lerp(OriginalRadius, ChamferedRadius, Alpha);
        const float CurrentZ = FMath::Lerp(OriginalZ, ChamferedZ, Alpha);
        FVector CurrentPos = RadialDir * CurrentRadius + FVector(0, 0, CurrentZ);

        // UV映射
        const float U = static_cast<float>(SideIndex) / Parameters.Sides;
        const float V = Alpha;
        FVector2D UV(U, V);

        int32 CurrentVertex = AddVertex(Section, CurrentPos, CurrentNormal, UV);

        // 连接到前一个段
        if (seg > 0 && PrevStripIndices.Num() > 0)
        {
            AddQuad(Section, PrevStripIndices[0], CurrentVertex, CurrentVertex, PrevStripIndices[0]);
        }

        PrevStripIndices = { CurrentVertex };
    }
}

void AHollowPrism::GenerateCornerChamfers(FMeshSection& Section, float ChamferedTopZ, float ChamferedBottomZ, float ChamferedInnerRadius, float ChamferedOuterRadius)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    // 为每个角度生成角落倒角
    for (int32 s = 0; s < Parameters.Sides; ++s)
    {
        const float Angle = s * AngleStep;
        const FVector RadialDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0);

        // 顶部外环角落倒角
        GenerateCornerChamfer(Section, RadialDir, Parameters.OuterRadius, ChamferedOuterRadius,
            HalfHeight, ChamferedTopZ, true, true, s);

        // 顶部内环角落倒角
        GenerateCornerChamfer(Section, RadialDir, Parameters.InnerRadius, ChamferedInnerRadius,
            HalfHeight, ChamferedTopZ, true, false, s);

        // 底部外环角落倒角
        GenerateCornerChamfer(Section, RadialDir, Parameters.OuterRadius, ChamferedOuterRadius,
            -HalfHeight, ChamferedBottomZ, false, true, s);

        // 底部内环角落倒角
        GenerateCornerChamfer(Section, RadialDir, Parameters.InnerRadius, ChamferedInnerRadius,
            -HalfHeight, ChamferedBottomZ, false, false, s);
    }
}

void AHollowPrism::GenerateCornerChamfer(FMeshSection& Section, const FVector& RadialDir,
    float OriginalRadius, float ChamferedRadius,
    float OriginalZ, float ChamferedZ,
    bool bIsTop, bool bIsOuter, int32 SideIndex)
{
    // 创建顶点网格
    TArray<TArray<int32>> CornerVerticesGrid;
    CornerVerticesGrid.SetNum(Parameters.ChamferSegments + 1);

    for (int32 Lat = 0; Lat <= Parameters.ChamferSegments; ++Lat)
    {
        CornerVerticesGrid[Lat].SetNum(Parameters.ChamferSegments + 1 - Lat);
    }

    // 生成四分之一球体的顶点
    for (int32 Lat = 0; Lat <= Parameters.ChamferSegments; ++Lat)
    {
        const float LatAlpha = static_cast<float>(Lat) / Parameters.ChamferSegments;

        for (int32 Lon = 0; Lon <= Parameters.ChamferSegments - Lat; ++Lon)
        {
            const float LonAlpha = static_cast<float>(Lon) / Parameters.ChamferSegments;

            // 计算法线插值
            FVector BaseNormal = bIsTop ? FVector(0, 0, 1) : FVector(0, 0, -1);
            FVector RadialNormal = bIsOuter ? RadialDir : -RadialDir;
            FVector CurrentNormal = FMath::Lerp(BaseNormal, RadialNormal, LatAlpha + LonAlpha).GetSafeNormal();

            // 计算位置插值
            const float CurrentRadius = FMath::Lerp(OriginalRadius, ChamferedRadius, LatAlpha + LonAlpha);
            const float CurrentZ = FMath::Lerp(OriginalZ, ChamferedZ, LatAlpha + LonAlpha);
            FVector CurrentPos = RadialDir * CurrentRadius + FVector(0, 0, CurrentZ);

            // UV映射
            const float U = static_cast<float>(SideIndex) / Parameters.Sides;
            const float V = LatAlpha + LonAlpha;
            FVector2D UV(U, V);

            CornerVerticesGrid[Lat][Lon] = AddVertex(Section, CurrentPos, CurrentNormal, UV);
        }
    }

    // 生成四分之一球体的三角形
    for (int32 Lat = 0; Lat < Parameters.ChamferSegments; ++Lat)
    {
        for (int32 Lon = 0; Lon < Parameters.ChamferSegments - Lat; ++Lon)
        {
            const int32 V00 = CornerVerticesGrid[Lat][Lon];
            const int32 V10 = CornerVerticesGrid[Lat + 1][Lon];
            const int32 V01 = CornerVerticesGrid[Lat][Lon + 1];

            AddTriangle(Section, V00, V10, V01);

            if (Lon + 1 < CornerVerticesGrid[Lat + 1].Num())
            {
                const int32 V11 = CornerVerticesGrid[Lat + 1][Lon + 1];
                AddTriangle(Section, V10, V11, V01);
            }
        }
    }
}

// 贝塞尔曲线倒角实现
AHollowPrism::FChamferArcControlPoints AHollowPrism::CalculateChamferControlPoints(const FVector& SideVertex, const FVector& TopBottomVertex)
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

FVector AHollowPrism::CalculateChamferArcPoint(const FChamferArcControlPoints& ControlPoints, float t)
{
    // 二次贝塞尔曲线：B(t) = (1-t)²P₀ + 2(1-t)tP₁ + t²P₂
    float t2 = t * t;
    float mt = 1.0f - t;
    float mt2 = mt * mt;

    return ControlPoints.StartPoint * mt2 +
        ControlPoints.ControlPoint * (2.0f * mt * t) +
        ControlPoints.EndPoint * t2;
}

FVector AHollowPrism::CalculateChamferArcTangent(const FChamferArcControlPoints& ControlPoints, float t)
{
    // 二次贝塞尔曲线切线：B'(t) = 2(1-t)(P₁-P₀) + 2t(P₂-P₁)
    float mt = 1.0f - t;
    return (ControlPoints.ControlPoint - ControlPoints.StartPoint) * (2.0f * mt) +
        (ControlPoints.EndPoint - ControlPoints.ControlPoint) * (2.0f * t);
}

void AHollowPrism::CreateTopChamferGeometry(FMeshSection& Section, float StartZ, float ChamferedTopZ)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevOuterRing, PrevInnerRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 外环倒角：从侧面边缘到顶面
        const FVector OuterChamferStartPoint = FVector(Parameters.OuterRadius, 0, StartZ);
        const float OuterEndRadius = FMath::Max(0.0f, Parameters.OuterRadius - ChamferRadius);
        const FVector OuterChamferEndPoint = FVector(OuterEndRadius, 0, ChamferedTopZ);
        FChamferArcControlPoints OuterControlPoints = CalculateChamferControlPoints(OuterChamferStartPoint, OuterChamferEndPoint);

        // 内环倒角：从侧面内边缘到顶面内边缘
        const FVector InnerChamferStartPoint = FVector(Parameters.InnerRadius, 0, StartZ);
        const float InnerEndRadius = FMath::Max(0.0f, Parameters.InnerRadius - ChamferRadius);
        const FVector InnerChamferEndPoint = FVector(InnerEndRadius, 0, ChamferedTopZ);
        FChamferArcControlPoints InnerControlPoints = CalculateChamferControlPoints(InnerChamferStartPoint, InnerChamferEndPoint);

        TArray<int32> CurrentOuterRing, CurrentInnerRing;
        CurrentOuterRing.Reserve(Parameters.Sides + 1);
        CurrentInnerRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            const float angle = s * AngleStep;

            // 外环倒角点
            const FVector OuterLocalChamferPos = CalculateChamferArcPoint(OuterControlPoints, alpha);
            const FVector OuterPosition = FVector(
                OuterLocalChamferPos.X * FMath::Cos(angle),
                OuterLocalChamferPos.X * FMath::Sin(angle),
                OuterLocalChamferPos.Z
            );

            const FVector OuterTangent = CalculateChamferArcTangent(OuterControlPoints, alpha);
            const FVector OuterNormal = FVector(
                OuterTangent.X * FMath::Cos(angle),
                OuterTangent.X * FMath::Sin(angle),
                OuterTangent.Z
            ).GetSafeNormal();

            const float OuterU = static_cast<float>(s) / Parameters.Sides;
            const float OuterV = (OuterPosition.Z + HalfHeight) / Parameters.Height;
            CurrentOuterRing.Add(AddVertex(Section, OuterPosition, OuterNormal, FVector2D(OuterU, OuterV)));

            // 内环倒角点
            const FVector InnerLocalChamferPos = CalculateChamferArcPoint(InnerControlPoints, alpha);
            const FVector InnerPosition = FVector(
                InnerLocalChamferPos.X * FMath::Cos(angle),
                InnerLocalChamferPos.X * FMath::Sin(angle),
                InnerLocalChamferPos.Z
            );

            const FVector InnerTangent = CalculateChamferArcTangent(InnerControlPoints, alpha);
            const FVector InnerNormal = FVector(
                InnerTangent.X * FMath::Cos(angle),
                InnerTangent.X * FMath::Sin(angle),
                InnerTangent.Z
            ).GetSafeNormal();

            const float InnerU = static_cast<float>(s) / Parameters.Sides;
            const float InnerV = (InnerPosition.Z + HalfHeight) / Parameters.Height;
            CurrentInnerRing.Add(AddVertex(Section, InnerPosition, InnerNormal, FVector2D(InnerU, InnerV)));
        }

        if (i > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                // 外环四边形
                const int32 V00 = PrevOuterRing[s];
                const int32 V10 = CurrentOuterRing[s];
                const int32 V01 = PrevOuterRing[s + 1];
                const int32 V11 = CurrentOuterRing[s + 1];
                AddQuad(Section, V00, V10, V11, V01);

                // 内环四边形
                const int32 V20 = PrevInnerRing[s];
                const int32 V30 = CurrentInnerRing[s];
                const int32 V21 = PrevInnerRing[s + 1];
                const int32 V31 = CurrentInnerRing[s + 1];
                AddQuad(Section, V20, V30, V31, V21);

                // 连接内外环的四边形
                AddQuad(Section, V00, V20, V21, V01);
                AddQuad(Section, V10, V11, V31, V30);
            }
        }
        PrevOuterRing = CurrentOuterRing;
        PrevInnerRing = CurrentInnerRing;
    }
}

void AHollowPrism::CreateBottomChamferGeometry(FMeshSection& Section, float StartZ, float ChamferedBottomZ)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevOuterRing, PrevInnerRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 外环倒角：从侧面边缘到底面
        const FVector OuterChamferStartPoint = FVector(Parameters.OuterRadius, 0, StartZ);
        const float OuterEndRadius = FMath::Max(0.0f, Parameters.OuterRadius - ChamferRadius);
        const FVector OuterChamferEndPoint = FVector(OuterEndRadius, 0, ChamferedBottomZ);
        FChamferArcControlPoints OuterControlPoints = CalculateChamferControlPoints(OuterChamferStartPoint, OuterChamferEndPoint);

        // 内环倒角：从侧面内边缘到底面内边缘
        const FVector InnerChamferStartPoint = FVector(Parameters.InnerRadius, 0, StartZ);
        const float InnerEndRadius = FMath::Max(0.0f, Parameters.InnerRadius - ChamferRadius);
        const FVector InnerChamferEndPoint = FVector(InnerEndRadius, 0, ChamferedBottomZ);
        FChamferArcControlPoints InnerControlPoints = CalculateChamferControlPoints(InnerChamferStartPoint, InnerChamferEndPoint);

        TArray<int32> CurrentOuterRing, CurrentInnerRing;
        CurrentOuterRing.Reserve(Parameters.Sides + 1);
        CurrentInnerRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            const float angle = s * AngleStep;

            // 外环倒角点
            const FVector OuterLocalChamferPos = CalculateChamferArcPoint(OuterControlPoints, alpha);
            const FVector OuterPosition = FVector(
                OuterLocalChamferPos.X * FMath::Cos(angle),
                OuterLocalChamferPos.X * FMath::Sin(angle),
                OuterLocalChamferPos.Z
            );

            const FVector OuterTangent = CalculateChamferArcTangent(OuterControlPoints, alpha);
            const FVector OuterNormal = FVector(
                OuterTangent.X * FMath::Cos(angle),
                OuterTangent.X * FMath::Sin(angle),
                OuterTangent.Z
            ).GetSafeNormal();

            const float OuterU = static_cast<float>(s) / Parameters.Sides;
            const float OuterV = (OuterPosition.Z + HalfHeight) / Parameters.Height;
            CurrentOuterRing.Add(AddVertex(Section, OuterPosition, OuterNormal, FVector2D(OuterU, OuterV)));

            // 内环倒角点
            const FVector InnerLocalChamferPos = CalculateChamferArcPoint(InnerControlPoints, alpha);
            const FVector InnerPosition = FVector(
                InnerLocalChamferPos.X * FMath::Cos(angle),
                InnerLocalChamferPos.X * FMath::Sin(angle),
                InnerLocalChamferPos.Z
            );

            const FVector InnerTangent = CalculateChamferArcTangent(InnerControlPoints, alpha);
            const FVector InnerNormal = FVector(
                InnerTangent.X * FMath::Cos(angle),
                InnerTangent.X * FMath::Sin(angle),
                InnerTangent.Z
            ).GetSafeNormal();

            const float InnerU = static_cast<float>(s) / Parameters.Sides;
            const float InnerV = (InnerPosition.Z + HalfHeight) / Parameters.Height;
            CurrentInnerRing.Add(AddVertex(Section, InnerPosition, InnerNormal, FVector2D(InnerU, InnerV)));
        }

        if (i > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                // 外环四边形
                const int32 V00 = PrevOuterRing[s];
                const int32 V10 = CurrentOuterRing[s];
                const int32 V01 = PrevOuterRing[s + 1];
                const int32 V11 = CurrentOuterRing[s + 1];
                AddQuad(Section, V00, V01, V11, V10);

                // 内环四边形
                const int32 V20 = PrevInnerRing[s];
                const int32 V30 = CurrentInnerRing[s];
                const int32 V21 = PrevInnerRing[s + 1];
                const int32 V31 = CurrentInnerRing[s + 1];
                AddQuad(Section, V20, V21, V31, V30);

                // 连接内外环的四边形
                AddQuad(Section, V00, V01, V21, V20);
                AddQuad(Section, V10, V30, V31, V11);
            }
        }
        PrevOuterRing = CurrentOuterRing;
        PrevInnerRing = CurrentInnerRing;
    }
}