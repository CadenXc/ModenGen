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

	GenerateSideWalls(MeshData);
	GenerateTopCapWithQuads(MeshData);
	GenerateBottomCapWithQuads(MeshData);

    // 生成倒角几何
    if (Parameters.ChamferRadius > 0.0f)
    {
        const float HalfHeight = Parameters.Height / 2.0f;
        
        // 分别生成内外倒角
        GenerateTopInnerChamfer(MeshData, HalfHeight);
        GenerateTopOuterChamfer(MeshData, HalfHeight);
        GenerateBottomInnerChamfer(MeshData, HalfHeight);
        GenerateBottomOuterChamfer(MeshData, HalfHeight);
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



// 生成顶部内环倒角
void AHollowPrism::GenerateTopInnerChamfer(FMeshSection& Section, float HalfHeight)
{
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 内环倒角：从内半径开始，向外扩展
        const float StartRadius = Parameters.InnerRadius;
        const float EndRadius = Parameters.InnerRadius + ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(HalfHeight - ChamferRadius, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            // 使用与CalculateRingVertices相同的角度计算方式
            const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Parameters.Sides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 内环法线：指向内部（负径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector TopNormal = FVector(0, 0, 1.0f);
            FVector Normal = FMath::Lerp(-RadialDirection, TopNormal, alpha).GetSafeNormal();

            // 确保法线指向内部
            if (FVector::DotProduct(Normal, RadialDirection) > 0)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (CurrentZ + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(Section, V00, V01, V11, V10);
            }
        }

        PrevRing = CurrentRing;
    }
}

// 生成顶部外环倒角
void AHollowPrism::GenerateTopOuterChamfer(FMeshSection& Section, float HalfHeight)
{
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 外环倒角：从外半径开始，向内收缩
        const float StartRadius = Parameters.OuterRadius;
        const float EndRadius = Parameters.OuterRadius - ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(HalfHeight - ChamferRadius, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            // 使用与CalculateRingVertices相同的角度计算方式
            const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Parameters.Sides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 外环法线：指向外部（正径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector TopNormal = FVector(0, 0, 1.0f);
            FVector Normal = FMath::Lerp(RadialDirection, TopNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            if (FVector::DotProduct(Normal, RadialDirection) < 0)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (CurrentZ + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(Section, V00, V10, V11, V01);
            }
        }

        PrevRing = CurrentRing;
    }
}

// 生成底部内环倒角
void AHollowPrism::GenerateBottomInnerChamfer(FMeshSection& Section, float HalfHeight)
{
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 内环倒角：从内半径开始，向外扩展
        const float StartRadius = Parameters.InnerRadius;
        const float EndRadius = Parameters.InnerRadius + ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(-HalfHeight + ChamferRadius, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            // 使用与CalculateRingVertices相同的角度计算方式
            const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Parameters.Sides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 内环法线：指向内部（负径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector BottomNormal = FVector(0, 0, -1.0f);
            FVector Normal = FMath::Lerp(-RadialDirection, BottomNormal, alpha).GetSafeNormal();

            // 确保法线指向内部
            if (FVector::DotProduct(Normal, RadialDirection) > 0)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (CurrentZ + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(Section, V00, V10, V11, V01);
            }
        }

        PrevRing = CurrentRing;
    }
}

// 生成底部外环倒角
void AHollowPrism::GenerateBottomOuterChamfer(FMeshSection& Section, float HalfHeight)
{
    const float ChamferRadius = Parameters.ChamferRadius;
    const int32 ChamferSections = Parameters.ChamferSections;
    const float AngleStep = FMath::DegreesToRadians(Parameters.ArcAngle) / Parameters.Sides;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 外环倒角：从外半径开始，向内收缩
        const float StartRadius = Parameters.OuterRadius;
        const float EndRadius = Parameters.OuterRadius - ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(-HalfHeight + ChamferRadius, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Parameters.Sides + 1);

        for (int32 s = 0; s <= Parameters.Sides; ++s)
        {
            // 使用与CalculateRingVertices相同的角度计算方式
            const float ArcAngleRadians = FMath::DegreesToRadians(Parameters.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Parameters.Sides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 外环法线：指向外部（正径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector BottomNormal = FVector(0, 0, -1.0f);
            FVector Normal = FMath::Lerp(RadialDirection, BottomNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            if (FVector::DotProduct(Normal, RadialDirection) < 0)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Parameters.Sides;
            const float V = (CurrentZ + HalfHeight) / Parameters.Height;

            CurrentRing.Add(AddVertex(Section, Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Parameters.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(Section, V00, V01, V11, V10);
            }
        }

        PrevRing = CurrentRing;
    }
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

    // 生成起始端盖和结束端盖
    GenerateEndCap(Section, StartAngle, FVector(-1, 0, 0), true);   // 起始端盖
    GenerateEndCap(Section, EndAngle, FVector(1, 0, 0), false);      // 结束端盖
}

void AHollowPrism::GenerateEndCap(FMeshSection& Section, float Angle, const FVector& Normal, bool IsStart)
{
    const float HalfHeight = Parameters.Height / 2.0f;
    const float ChamferRadius = Parameters.ChamferRadius;

    // 计算倒角高度
    const float TopChamferHeight = FMath::Min(ChamferRadius, Parameters.OuterRadius - Parameters.InnerRadius);
    const float BottomChamferHeight = FMath::Min(ChamferRadius, Parameters.OuterRadius - Parameters.InnerRadius);

    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomChamferHeight;
    const float EndZ = HalfHeight - TopChamferHeight;

    // 按照指定顺序存储顶点：上底中心 -> 上倒角圆弧 -> 侧边 -> 下倒角圆弧 -> 下底中心
    TArray<int32> OrderedVertices;
    
    // 1. 上底中心顶点
    const int32 TopCenterVertex = AddVertex(Section,
        FVector(0, 0, HalfHeight),
        Normal,
        FVector2D(0.5f, 1.0f)
    );
    OrderedVertices.Add(TopCenterVertex);

    // 2. 上倒角弧线顶点（从顶面到侧边）
    if (Parameters.ChamferRadius > 0.0f)
    {
        const int32 TopChamferSections = Parameters.ChamferSections;
        for (int32 i = 0; i <= TopChamferSections; ++i) // 从顶面到侧边
        {
            const float Alpha = static_cast<float>(i) / TopChamferSections;
            
            // 从顶面开始（HalfHeight位置）到侧边（EndZ位置）
            const float CurrentZ = FMath::Lerp(HalfHeight, EndZ, Alpha);
            
            // 从顶面半径到侧边半径（减去倒角半径，与主体几何体保持一致）
            const float TopInnerRadius = Parameters.InnerRadius + Parameters.ChamferRadius;
            const float TopOuterRadius = Parameters.OuterRadius - Parameters.ChamferRadius;
            const float SideInnerRadius = Parameters.InnerRadius;
            const float SideOuterRadius = Parameters.OuterRadius;
            
            const float CurrentInnerRadius = FMath::Lerp(TopInnerRadius, SideInnerRadius, Alpha);
            const float CurrentOuterRadius = FMath::Lerp(TopOuterRadius, SideOuterRadius, Alpha);
            
            // 内环顶点
            const FVector InnerChamferPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
            const int32 InnerChamferVertex = AddVertex(Section, InnerChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height));
            OrderedVertices.Add(InnerChamferVertex);
            
            // 外环顶点
            const FVector OuterChamferPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
            const int32 OuterChamferVertex = AddVertex(Section, OuterChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height));
            OrderedVertices.Add(OuterChamferVertex);
        }
    }

    // 3. 侧边顶点（从上到下）
    for (int32 h = 1; h < 4; ++h) // 使用4个分段
    {
        const float Z = FMath::Lerp(EndZ, StartZ, static_cast<float>(h) / 4.0f);

        // 内环顶点
        const FVector InnerEdgePos = FVector(Parameters.InnerRadius * FMath::Cos(Angle), Parameters.InnerRadius * FMath::Sin(Angle), Z);
        const int32 InnerEdgeVertex = AddVertex(Section, InnerEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (Z + HalfHeight) / Parameters.Height));
        OrderedVertices.Add(InnerEdgeVertex);
        
        // 外环顶点
        const FVector OuterEdgePos = FVector(Parameters.OuterRadius * FMath::Cos(Angle), Parameters.OuterRadius * FMath::Sin(Angle), Z);
        const int32 OuterEdgeVertex = AddVertex(Section, OuterEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (Z + HalfHeight) / Parameters.Height));
        OrderedVertices.Add(OuterEdgeVertex);
    }

    // 4. 下倒角弧线顶点（从侧边到底面）
    if (Parameters.ChamferRadius > 0.0f)
    {
        const int32 BottomChamferSections = Parameters.ChamferSections;
        for (int32 i = 0; i <= BottomChamferSections; ++i) // 从侧边到底面
        {
            const float Alpha = static_cast<float>(i) / BottomChamferSections;
            
            // 从侧边开始（StartZ位置）到底面（-HalfHeight位置）
            const float CurrentZ = FMath::Lerp(StartZ, -HalfHeight, Alpha);
            
            // 从侧边半径到底面半径（减去倒角半径，与主体几何体保持一致）
            const float SideInnerRadius = Parameters.InnerRadius;
            const float SideOuterRadius = Parameters.OuterRadius;
            const float BottomInnerRadius = Parameters.InnerRadius + Parameters.ChamferRadius;
            const float BottomOuterRadius = Parameters.OuterRadius - Parameters.ChamferRadius;
            
            const float CurrentInnerRadius = FMath::Lerp(SideInnerRadius, BottomInnerRadius, Alpha);
            const float CurrentOuterRadius = FMath::Lerp(SideOuterRadius, BottomOuterRadius, Alpha);
            
            // 内环顶点
            const FVector InnerChamferPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
            const int32 InnerChamferVertex = AddVertex(Section, InnerChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height));
            OrderedVertices.Add(InnerChamferVertex);
            
            // 外环顶点
            const FVector OuterChamferPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
            const int32 OuterChamferVertex = AddVertex(Section, OuterChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Parameters.Height));
            OrderedVertices.Add(OuterChamferVertex);
        }
    }

    // 5. 下底中心顶点
    const int32 BottomCenterVertex = AddVertex(Section,
        FVector(0, 0, -HalfHeight),
        Normal,
        FVector2D(0.5f, 0.0f)
    );
    OrderedVertices.Add(BottomCenterVertex);

    // 生成端盖三角形
    for (int32 i = 0; i < OrderedVertices.Num() - 2; i += 2)
    {
        // 连接相邻的顶点对，形成三角形
        if (IsStart)
        {
			AddTriangle(Section, OrderedVertices[i], OrderedVertices[i + 2], OrderedVertices[i + 1]);
			AddTriangle(Section, OrderedVertices[i + 1], OrderedVertices[i + 2], OrderedVertices[i + 3]);
        }
        else
        {
			AddTriangle(Section, OrderedVertices[i], OrderedVertices[i + 1], OrderedVertices[i + 2]);
			AddTriangle(Section, OrderedVertices[i + 1], OrderedVertices[i + 3], OrderedVertices[i + 2]);
        }
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