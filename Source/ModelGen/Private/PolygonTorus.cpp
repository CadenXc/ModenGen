#include "PolygonTorus.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

APolygonTorus::APolygonTorus()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;
    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 设置默认材质
    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (MaterialFinder.Succeeded())
    {
        ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
    }

    GeneratePolygonTorus(MajorRadius, MinorRadius, MajorSegments, MinorSegments, TorusAngle, bSmoothCrossSection, bSmoothVerticalSection);
}

void APolygonTorus::BeginPlay()
{
    Super::BeginPlay();
    GeneratePolygonTorus(MajorRadius, MinorRadius, MajorSegments, MinorSegments, TorusAngle, bSmoothCrossSection, bSmoothVerticalSection);
}

void APolygonTorus::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GeneratePolygonTorus(MajorRadius, MinorRadius, MajorSegments, MinorSegments, TorusAngle, bSmoothCrossSection, bSmoothVerticalSection);
}

// 生成多边形截面顶点
void APolygonTorus::GeneratePolygonVertices(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UVs,
    TArray<FProcMeshTangent>& Tangents,
    const FVector& Center,
    const FVector& Direction,
    const FVector& UpVector,
    float Radius,
    int32 Segments,
    float UOffset)
{
    // 计算垂直向量
    FVector RightVector = FVector::CrossProduct(Direction, UpVector).GetSafeNormal();

    for (int32 i = 0; i < Segments; i++)
    {
        float Angle = 2 * PI * i / Segments;
        float CosA = FMath::Cos(Angle);
        float SinA = FMath::Sin(Angle);

        // 计算顶点位置
        FVector VertexPos = Center +
            (RightVector * CosA * Radius) +
            (UpVector * SinA * Radius);

        // 计算法线（从中心指向顶点，确保指向外部）
        FVector Normal = (VertexPos - Center).GetSafeNormal();
        
        // 确保法线指向外部（对于圆环，法线应该指向远离中心的方向）
        // 这里我们检查法线是否指向正确的方向
        FVector ExpectedDirection = (VertexPos - Center);
        if (FVector::DotProduct(ExpectedDirection, Normal) < 0)
        {
            Normal = -Normal;
        }

        // 设置UV坐标
        FVector2D UV(UOffset + static_cast<float>(i) / Segments, 0.0f);

        // 切线方向（沿圆环方向）
        FVector Tangent = Direction;

        // 添加顶点
        Vertices.Add(VertexPos);
        Normals.Add(Normal);
        UVs.Add(UV);
        Tangents.Add(FProcMeshTangent(Tangent, false));
    }
}

void APolygonTorus::AddQuad(TArray<int32>& Triangles, int32 V0, int32 V1, int32 V2, int32 V3)
{
    Triangles.Add(V0); Triangles.Add(V1); Triangles.Add(V2);
    Triangles.Add(V0); Triangles.Add(V2); Triangles.Add(V3);
}

// 验证和约束参数
void APolygonTorus::ValidateParameters(float& MajorRad, float& MinorRad, int32& MajorSegs, int32& MinorSegs, float& Angle)
{
    MajorSegs = FMath::Max(3, MajorSegs);
    MinorSegs = FMath::Max(3, MinorSegs);
    MajorRad = FMath::Max(1.0f, MajorRad);
    MinorRad = FMath::Clamp(MinorRad, 1.0f, MajorRad * 0.9f);
    Angle = FMath::Clamp(Angle, 1.0f, 360.0f);
}

// 生成截面顶点
void APolygonTorus::GenerateSectionVertices(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UVs,
    TArray<FProcMeshTangent>& Tangents,
    TArray<int32>& SectionStartIndices,
    float MajorRad,
    float MinorRad,
    int32 MajorSegs,
    int32 MinorSegs,
    float AngleStep)
{
    for (int32 i = 0; i <= MajorSegs; i++)
    {
        float CurrentAngle = i * AngleStep;
        float UOffset = static_cast<float>(i) / MajorSegs;

        // 计算当前截面的中心位置
        FVector SectionCenter(
            FMath::Cos(CurrentAngle) * MajorRad,
            FMath::Sin(CurrentAngle) * MajorRad,
            0.0f
        );

        // 计算当前截面的方向（圆环的切线方向）
        FVector SectionDirection(
            -FMath::Sin(CurrentAngle),
            FMath::Cos(CurrentAngle),
            0.0f
        );

        // 记录当前截面的起始顶点索引
        SectionStartIndices.Add(Vertices.Num());

        // 生成多边形截面
        GeneratePolygonVertices(
            Vertices, Normals, UVs, Tangents,
            SectionCenter,
            SectionDirection,
            FVector::UpVector,
            MinorRad,
            MinorSegs,
            UOffset
        );
    }
}

// 生成侧面三角形
void APolygonTorus::GenerateSideTriangles(
    TArray<int32>& Triangles,
    const TArray<int32>& SectionStartIndices,
    int32 MajorSegs,
    int32 MinorSegs)
{
    for (int32 i = 0; i < MajorSegs; i++)
    {
        int32 CurrentSectionStart = SectionStartIndices[i];
        int32 NextSectionStart = SectionStartIndices[i + 1];

        for (int32 j = 0; j < MinorSegs; j++)
        {
            int32 NextJ = (j + 1) % MinorSegs;

            int32 V0 = CurrentSectionStart + j;
            int32 V1 = CurrentSectionStart + NextJ;
            int32 V2 = NextSectionStart + NextJ;
            int32 V3 = NextSectionStart + j;

            AddQuad(Triangles, V0, V1, V2, V3);
        }
    }
}

// 生成端面
void APolygonTorus::GenerateEndCaps(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UVs,
    TArray<FProcMeshTangent>& Tangents,
    TArray<int32>& Triangles,
    const TArray<int32>& SectionStartIndices,
    float MajorRad,
    float AngleRad,
    int32 MajorSegs,
    int32 MinorSegs)
{
    // 添加端盖中心顶点并记录索引
    int32 StartCenterIndex = Vertices.Num();
    FVector StartCenter(FMath::Cos(0) * MajorRad, FMath::Sin(0) * MajorRad, 0);
    Vertices.Add(StartCenter);
    Normals.Add(-FVector(-FMath::Sin(0), FMath::Cos(0), 0)); // 法线指向内侧
    UVs.Add(FVector2D(0.5f, 0.5f));
    Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));

    int32 EndCenterIndex = Vertices.Num();
    FVector EndCenter(FMath::Cos(AngleRad) * MajorRad, FMath::Sin(AngleRad) * MajorRad, 0);
    Vertices.Add(EndCenter);
    Normals.Add(FVector(-FMath::Sin(AngleRad), FMath::Cos(AngleRad), 0)); // 法线指向外侧
    UVs.Add(FVector2D(0.5f, 0.5f));
    Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));

    // 生成起始端盖
    int32 StartSection = SectionStartIndices[0];
    for (int32 j = 0; j < MinorSegs; j++)
    {
        int32 NextJ = (j + 1) % MinorSegs;

        Triangles.Add(StartSection + j);
        Triangles.Add(StartCenterIndex);
        Triangles.Add(StartSection + NextJ);
    }

    // 生成结束端盖
    int32 EndSection = SectionStartIndices[MajorSegs];
    for (int32 j = 0; j < MinorSegs; j++)
    {
        int32 NextJ = (j + 1) % MinorSegs;

        Triangles.Add(EndSection + j);
        Triangles.Add(EndSection + NextJ);
        Triangles.Add(EndCenterIndex);
    }
}

// 计算平滑法线
void APolygonTorus::CalculateSmoothNormals(
    TArray<FVector>& Normals,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    const TArray<int32>& SectionStartIndices,
    float MajorRad,
    float AngleStep,
    int32 MajorSegs,
    int32 MinorSegs,
    bool bSmoothCross,
    bool bSmoothVertical)
{
    // 重新计算所有顶点的法线
    TArray<FVector> NewNormals;
    NewNormals.SetNum(Vertices.Num());
    
    // 初始化法线为零向量
    for (int32 i = 0; i < NewNormals.Num(); i++)
    {
        NewNormals[i] = FVector::ZeroVector;
    }

    // 计算每个三角形的法线并累加到顶点
    for (int32 i = 0; i < Triangles.Num(); i += 3)
    {
        int32 V0 = Triangles[i];
        int32 V1 = Triangles[i + 1];
        int32 V2 = Triangles[i + 2];

        FVector Edge1 = Vertices[V1] - Vertices[V0];
        FVector Edge2 = Vertices[V2] - Vertices[V0];
        FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
        
        // 确保面法线指向外部
        // 对于圆环，法线应该指向远离圆环中心的方向
        FVector FaceCenter = (Vertices[V0] + Vertices[V1] + Vertices[V2]) / 3.0f;
        FVector ToCenter = FVector(FaceCenter.X, FaceCenter.Y, 0.0f).GetSafeNormal();
        
        // 如果面法线与指向中心的方向夹角小于90度，则翻转法线
        if (FVector::DotProduct(FaceNormal, ToCenter) > 0)
        {
            FaceNormal = -FaceNormal;
        }

        NewNormals[V0] += FaceNormal;
        NewNormals[V1] += FaceNormal;
        NewNormals[V2] += FaceNormal;
    }

    // 标准化所有法线
    for (int32 i = 0; i < NewNormals.Num(); i++)
    {
        NewNormals[i] = NewNormals[i].GetSafeNormal();
    }

    // 根据平滑设置调整法线
    for (int32 i = 0; i <= MajorSegs; i++)
    {
        int32 SectionStart = SectionStartIndices[i];
        
        for (int32 j = 0; j < MinorSegs; j++)
        {
            int32 VertexIndex = SectionStart + j;
            
            if (!bSmoothCross)
            {
                // 横切面不平滑：使用原始法线（从中心指向顶点）
                FVector OriginalNormal = (Vertices[VertexIndex] - FVector(
                    FMath::Cos(i * AngleStep) * MajorRad,
                    FMath::Sin(i * AngleStep) * MajorRad,
                    0.0f
                )).GetSafeNormal();
                NewNormals[VertexIndex] = OriginalNormal;
            }
            
            if (!bSmoothVertical)
            {
                // 竖面不平滑：使用截面法线
                FVector SectionNormal = FVector(
                    FMath::Cos(i * AngleStep),
                    FMath::Sin(i * AngleStep),
                    0.0f
                );
                NewNormals[VertexIndex] = SectionNormal;
            }
        }
    }

    // 更新法线数组
    Normals = NewNormals;
    
    // 验证法线方向
    ValidateNormalDirections(Vertices, Normals, SectionStartIndices, MajorRad, MajorSegs, MinorSegs);
}

// 主要的网格生成函数
void APolygonTorus::GeneratePolygonTorus(float MajorRad, float MinorRad, int32 MajorSegs, int32 MinorSegs, float Angle, bool bSmoothCross, bool bSmoothVertical)
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // 清除之前的网格
    ProceduralMesh->ClearAllMeshSections();

    // 验证参数
    ValidateParameters(MajorRad, MinorRad, MajorSegs, MinorSegs, Angle);

    // 准备网格数据
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    // 计算角度步长
    const float AngleRad = FMath::DegreesToRadians(Angle);
    const float AngleStep = AngleRad / MajorSegs;
    const bool bIsFullCircle = FMath::IsNearlyEqual(Angle, 360.0f);

    // 存储每个截面的顶点起始索引
    TArray<int32> SectionStartIndices;

    // 生成所有截面的顶点
    GenerateSectionVertices(Vertices, Normals, UVs, Tangents, SectionStartIndices, 
                           MajorRad, MinorRad, MajorSegs, MinorSegs, AngleStep);

    // 生成侧面（连接相邻截面）
    GenerateSideTriangles(Triangles, SectionStartIndices, MajorSegs, MinorSegs);

    // 生成端面（如果角度小于360度）
    if (!bIsFullCircle)
    {
        GenerateEndCaps(Vertices, Normals, UVs, Tangents, Triangles, 
                       SectionStartIndices, MajorRad, AngleRad, MajorSegs, MinorSegs);
    }

    // 重新计算法线以支持平滑着色
    if (bSmoothCross || bSmoothVertical)
    {
        CalculateSmoothNormals(Normals, Vertices, Triangles, SectionStartIndices,
                              MajorRad, AngleStep, MajorSegs, MinorSegs,
                              bSmoothCross, bSmoothVertical);
    }
    // 如果两个参数都为false，保持原始法线不变（GeneratePolygonVertices生成的硬边法线）

    // 验证法线方向（仅在调试模式下）
    #if WITH_EDITOR
    ValidateNormalDirections(Vertices, Normals, SectionStartIndices, MajorRad, MajorSegs, MinorSegs);
    #endif

    // 创建网格
    ProceduralMesh->CreateMeshSection_LinearColor(
        0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true
    );
    
    // 调试信息：输出法线统计
    if (Vertices.Num() > 0)
    {
        FVector AvgNormal = FVector::ZeroVector;
        for (const FVector& Normal : Normals)
        {
            AvgNormal += Normal;
        }
        AvgNormal = AvgNormal.GetSafeNormal();
        
        UE_LOG(LogTemp, Log, TEXT("PolygonTorus: Generated %d vertices, %d triangles, Average Normal: %s"), 
               Vertices.Num(), Triangles.Num() / 3, *AvgNormal.ToString());
    }
}

// 验证法线方向
void APolygonTorus::ValidateNormalDirections(
    const TArray<FVector>& Vertices,
    const TArray<FVector>& Normals,
    const TArray<int32>& SectionStartIndices,
    float MajorRad,
    int32 MajorSegs,
    int32 MinorSegs)
{
    if (Vertices.Num() != Normals.Num() || Vertices.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ValidateNormalDirections: Invalid vertex/normal count"));
        return;
    }
    
    int32 IncorrectNormals = 0;
    int32 TotalChecked = 0;
    
    // 检查每个顶点的法线方向
    for (int32 i = 0; i <= MajorSegs; i++)
    {
        if (i >= SectionStartIndices.Num()) break;
        
        int32 SectionStart = SectionStartIndices[i];
        for (int32 j = 0; j < MinorSegs; j++)
        {
            int32 VertexIndex = SectionStart + j;
            if (VertexIndex >= Vertices.Num()) break;
            
            const FVector& Vertex = Vertices[VertexIndex];
            const FVector& Normal = Normals[VertexIndex];
            
            // 计算从圆环中心到顶点的方向
            FVector ToCenter = FVector(Vertex.X, Vertex.Y, 0.0f).GetSafeNormal();
            
            // 法线应该指向远离中心的方向
            float DotProduct = FVector::DotProduct(Normal, ToCenter);
            
            TotalChecked++;
            if (DotProduct > 0.1f) // 允许一些误差
            {
                IncorrectNormals++;
                UE_LOG(LogTemp, Warning, TEXT("Vertex %d: Normal points inward (Dot: %.3f)"), 
                       VertexIndex, DotProduct);
            }
        }
    }
    
    if (IncorrectNormals > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Found %d incorrect normals out of %d checked vertices"), 
               IncorrectNormals, TotalChecked);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("All %d normals are correctly oriented"), TotalChecked);
    }
}
