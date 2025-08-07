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
    Parameters.ChamferRadius = FMath::Max(0.0f, Parameters.ChamferRadius);
    Parameters.ChamferSections = FMath::Max(1, Parameters.ChamferSections);

    // 确保外半径大于内半径
    if (Parameters.OuterRadius <= Parameters.InnerRadius)
    {
        Parameters.OuterRadius = Parameters.InnerRadius + 1.0f;
    }

    FMeshSection MeshData;

    // 顶点和三角形数量估算
    const int32 VertexCountEstimate =
        Parameters.Sides * 8 +  // 侧面（内外各4个顶点）
        Parameters.Sides * 4 +  // 顶面/底面（内外各2个顶点）
        (Parameters.ArcAngle < 360.0f ? 8 : 0);  // 端盖

    const int32 TriangleCountEstimate =
        Parameters.Sides * 8 +  // 侧面
        Parameters.Sides * 4 +  // 顶面/底面
        (Parameters.ArcAngle < 360.0f ? 8 : 0);  // 端盖

    MeshData.Reserve(VertexCountEstimate, TriangleCountEstimate * 3);

    // 生成基础几何
    GenerateSideWalls(MeshData);
    GenerateTopCapWithQuads(MeshData);
    GenerateBottomCapWithQuads(MeshData);

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



// 生成侧面
void AHollowPrism::GenerateSideWalls(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点（顶部和底部）
    TArray<FVector> InnerTopVertices, InnerBottomVertices;
    TArray<FVector2D> InnerTopUVs, InnerBottomUVs;
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, HalfHeight - Parameters.ChamferRadius, ArcAngle, InnerTopVertices, InnerTopUVs);
    CalculateRingVertices(Parameters.InnerRadius, Parameters.Sides, -HalfHeight + Parameters.ChamferRadius, ArcAngle, InnerBottomVertices, InnerBottomUVs);

    // 生成外环顶点（顶部和底部）
    TArray<FVector> OuterTopVertices, OuterBottomVertices;
    TArray<FVector2D> OuterTopUVs, OuterBottomUVs;
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, HalfHeight - Parameters.ChamferRadius, ArcAngle, OuterTopVertices, OuterTopUVs, 0.5f);
    CalculateRingVertices(Parameters.OuterRadius, Parameters.Sides, -HalfHeight + Parameters.ChamferRadius, ArcAngle, OuterBottomVertices, OuterBottomUVs, 0.5f);

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



// 生成顶面
void AHollowPrism::GenerateTopCapWithQuads(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(Parameters.InnerRadius + Parameters.ChamferRadius, Parameters.Sides, HalfHeight, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(Parameters.OuterRadius - Parameters.ChamferRadius, Parameters.Sides, HalfHeight, ArcAngle, OuterVertices, OuterUVs);

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
            InnerIndices[i + 1],
            OuterIndices[i + 1],
            OuterIndices[i]
        );
    }
}

// 生成底面
void AHollowPrism::GenerateBottomCapWithQuads(FMeshSection& Section)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngle = Parameters.ArcAngle;

    // 生成内环顶点
    TArray<FVector> InnerVertices;
    TArray<FVector2D> InnerUVs;
    CalculateRingVertices(Parameters.InnerRadius + Parameters.ChamferRadius, Parameters.Sides, -HalfHeight, ArcAngle, InnerVertices, InnerUVs);

    // 生成外环顶点
    TArray<FVector> OuterVertices;
    TArray<FVector2D> OuterUVs;
    CalculateRingVertices(Parameters.OuterRadius - Parameters.ChamferRadius, Parameters.Sides, -HalfHeight, ArcAngle, OuterVertices, OuterUVs);

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

// 生成顶部倒角几何
void AHollowPrism::GenerateTopChamferGeometry(FMeshSection& Section, float StartZ)
{
    // 暂时不实现倒角几何生成，保持标准几何
    // 这个方法保留用于将来的倒角功能实现
}

// 生成底部倒角几何
void AHollowPrism::GenerateBottomChamferGeometry(FMeshSection& Section, float StartZ)
{
    // 暂时不实现倒角几何生成，保持标准几何
    // 这个方法保留用于将来的倒角功能实现
}

// 生成端盖
void AHollowPrism::GenerateEndCaps(FMeshSection& Section)
{
    if (Parameters.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
    {
        return; // 完整环形，不需要端盖
    }

    const float HalfHeight = Parameters.Height / 2.0f;
    const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float EndAngle = ArcAngleRadians / 2.0f;

    // 生成端盖顶点
    TArray<FVector> TopVertices, BottomVertices;
    TArray<FVector2D> TopUVs, BottomUVs;

    // 顶部端盖
    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        const float angle = StartAngle + (EndAngle - StartAngle) * i / Parameters.Sides;
        
        // 内环顶点
        const FVector InnerVertex = FVector(
            Parameters.InnerRadius * FMath::Cos(angle),
            Parameters.InnerRadius * FMath::Sin(angle),
            HalfHeight
        );
        TopVertices.Add(InnerVertex);
        TopUVs.Add(FVector2D(0.0f, static_cast<float>(i) / Parameters.Sides));

        // 外环顶点
        const FVector OuterVertex = FVector(
            Parameters.OuterRadius * FMath::Cos(angle),
            Parameters.OuterRadius * FMath::Sin(angle),
            HalfHeight
        );
        TopVertices.Add(OuterVertex);
        TopUVs.Add(FVector2D(1.0f, static_cast<float>(i) / Parameters.Sides));
    }

    // 底部端盖
    for (int32 i = 0; i <= Parameters.Sides; i++)
    {
        const float angle = StartAngle + (EndAngle - StartAngle) * i / Parameters.Sides;
        
        // 内环顶点
        const FVector InnerVertex = FVector(
            Parameters.InnerRadius * FMath::Cos(angle),
            Parameters.InnerRadius * FMath::Sin(angle),
            -HalfHeight
        );
        BottomVertices.Add(InnerVertex);
        BottomUVs.Add(FVector2D(0.0f, static_cast<float>(i) / Parameters.Sides));

        // 外环顶点
        const FVector OuterVertex = FVector(
            Parameters.OuterRadius * FMath::Cos(angle),
            Parameters.OuterRadius * FMath::Sin(angle),
            -HalfHeight
        );
        BottomVertices.Add(OuterVertex);
        BottomUVs.Add(FVector2D(1.0f, static_cast<float>(i) / Parameters.Sides));
    }

    // 添加顶部端盖顶点
    TArray<int32> TopIndices;
    const FVector TopNormal(0, 0, 1);
    for (int32 i = 0; i < TopVertices.Num(); i++)
    {
        TopIndices.Add(AddVertex(Section, TopVertices[i], TopNormal, TopUVs[i]));
    }

    // 添加底部端盖顶点
    TArray<int32> BottomIndices;
    const FVector BottomNormal(0, 0, -1);
    for (int32 i = 0; i < BottomVertices.Num(); i++)
    {
        BottomIndices.Add(AddVertex(Section, BottomVertices[i], BottomNormal, BottomUVs[i]));
    }

    // 生成顶部端盖面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        const int32 BaseIndex = i * 2;
        AddQuad(Section,
            TopIndices[BaseIndex],
            TopIndices[BaseIndex + 1],
            TopIndices[BaseIndex + 3],
            TopIndices[BaseIndex + 2]
        );
    }

    // 生成底部端盖面
    for (int32 i = 0; i < Parameters.Sides; i++)
    {
        const int32 BaseIndex = i * 2;
        AddQuad(Section,
            BottomIndices[BaseIndex + 1],
            BottomIndices[BaseIndex],
            BottomIndices[BaseIndex + 2],
            BottomIndices[BaseIndex + 3]
        );
    }
}

// 计算环形顶点
void AHollowPrism::CalculateRingVertices(float Radius, int32 Sides, float Z, float ArcAngle,
                                        TArray<FVector>& OutVertices, TArray<FVector2D>& OutUVs, float UVScale)
{
    OutVertices.Empty();
    OutUVs.Empty();

    const float ArcAngleRadians = FMath::DegreesToRadians(ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Sides;

    for (int32 i = 0; i <= Sides; i++)
    {
        const float angle = StartAngle + i * AngleStep;
        const FVector Vertex = FVector(
            Radius * FMath::Cos(angle),
            Radius * FMath::Sin(angle),
            Z
        );
        OutVertices.Add(Vertex);

        // UV映射
        const float U = (static_cast<float>(i) / Sides) * UVScale;
        const float V = (Z + Parameters.Height / 2.0f) / Parameters.Height;
        OutUVs.Add(FVector2D(U, V));
    }
}

// 添加顶点
int32 AHollowPrism::AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV)
{
    const int32 VertexIndex = Section.Vertices.Num();
    
    Section.Vertices.Add(Position);
    Section.Normals.Add(Normal);
    Section.UVs.Add(UV);
    Section.VertexColors.Add(FLinearColor::White);
    Section.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
    
    return VertexIndex;
}

// 添加四边形
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

// 添加三角形
void AHollowPrism::AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3)
{
    Section.Triangles.Add(V1);
    Section.Triangles.Add(V2);
    Section.Triangles.Add(V3);
}

// 计算顶点法线
FVector AHollowPrism::CalculateVertexNormal(const FVector& Vertex)
{
    // 简单的径向法线计算
    const FVector2D XY(Vertex.X, Vertex.Y);
    const float Length = XY.Size();
    
    if (Length > KINDA_SMALL_NUMBER)
    {
        return FVector(XY.X / Length, XY.Y / Length, 0.0f);
    }
    
    return FVector::UpVector;
}