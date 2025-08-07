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

    GenerateTorusWithSmoothing(ETorusSmoothMode::Both, 30.0f);
}

void APolygonTorus::BeginPlay()
{
    Super::BeginPlay();
    GenerateTorusWithSmoothing(ETorusSmoothMode::Both, 30.0f);
}

void APolygonTorus::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerateTorusWithSmoothing(ETorusSmoothMode::Both, 30.0f);
}

// 优化的参数验证和约束
void APolygonTorus::ValidateAndClampParameters(float& MajorRad, float& MinorRad, int32& MajorSegs, 
                                              int32& MinorSegs, float& Angle)
{
    // 约束分段数
    MajorSegs = FMath::Clamp(MajorSegs, 3, 256);
    MinorSegs = FMath::Clamp(MinorSegs, 3, 256);
    
    // 约束半径
    MajorRad = FMath::Max(MajorRad, 1.0f);
    MinorRad = FMath::Clamp(MinorRad, 1.0f, MajorRad * 0.9f);
    
    // 约束角度
    Angle = FMath::Clamp(Angle, 1.0f, 360.0f);
}

// 优化的顶点生成（基于Blender的实现）
void APolygonTorus::GenerateOptimizedVertices(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UVs,
    TArray<FProcMeshTangent>& Tangents,
    float MajorRad,
    float MinorRad,
    int32 MajorSegs,
    int32 MinorSegs,
    float AngleRad)
{
    Vertices.Empty();
    Normals.Empty();
    UVs.Empty();
    Tangents.Empty();

    const float MajorStep = AngleRad / MajorSegs;
    const float MinorStep = 2.0f * PI / MinorSegs;

    for (int32 majorIndex = 0; majorIndex <= MajorSegs; ++majorIndex)
    {
        const float majorAngle = majorIndex * MajorStep;
        const float majorCos = FMath::Cos(majorAngle);
        const float majorSin = FMath::Sin(majorAngle);

        // 计算当前截面的中心位置
        const FVector sectionCenter(majorCos * MajorRad, majorSin * MajorRad, 0.0f);
        
        // 计算当前截面的方向（圆环的切线方向）
        const FVector sectionDirection(-majorSin, majorCos, 0.0f);

        for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
        {
            const float minorAngle = minorIndex * MinorStep;
            const float minorCos = FMath::Cos(minorAngle);
            const float minorSin = FMath::Sin(minorAngle);

            // 计算顶点位置（基于Blender的算法）
            const FVector vertexPos = sectionCenter + 
                FVector(minorCos * MinorRad * majorCos, 
                       minorCos * MinorRad * majorSin, 
                       minorSin * MinorRad);

            // 计算法线（从截面中心指向顶点）
            const FVector normal = (vertexPos - sectionCenter).GetSafeNormal();

            // 计算UV坐标
            const float u = static_cast<float>(majorIndex) / MajorSegs;
            const float v = static_cast<float>(minorIndex) / MinorSegs;
            const FVector2D uv(u, v);

            // 计算切线（沿圆环方向）
            const FVector tangent = sectionDirection;

            Vertices.Add(vertexPos);
            Normals.Add(normal);
            UVs.Add(uv);
            Tangents.Add(FProcMeshTangent(tangent, false));
        }
    }
}

// 优化的三角形生成
void APolygonTorus::GenerateOptimizedTriangles(
    TArray<int32>& Triangles,
    int32 MajorSegs,
    int32 MinorSegs,
    bool bIsFullCircle)
{
    Triangles.Empty();

    for (int32 majorIndex = 0; majorIndex < MajorSegs; ++majorIndex)
    {
        for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
        {
            const int32 currentMajor = majorIndex;
            const int32 nextMajor = (majorIndex + 1) % (MajorSegs + 1);
            const int32 currentMinor = minorIndex;
            const int32 nextMinor = (minorIndex + 1) % MinorSegs;

            const int32 v0 = currentMajor * MinorSegs + currentMinor;
            const int32 v1 = currentMajor * MinorSegs + nextMinor;
            const int32 v2 = nextMajor * MinorSegs + nextMinor;
            const int32 v3 = nextMajor * MinorSegs + currentMinor;

            // 添加四边形（两个三角形）
            Triangles.Add(v0); Triangles.Add(v1); Triangles.Add(v2);
            Triangles.Add(v0); Triangles.Add(v2); Triangles.Add(v3);
        }
    }
}

// 优化的端面生成
void APolygonTorus::GenerateEndCapsOptimized(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UVs,
    TArray<FProcMeshTangent>& Tangents,
    TArray<int32>& Triangles,
    float MajorRad,
    float MinorRad,
    float AngleRad,
    int32 MajorSegs,
    int32 MinorSegs,
    ETorusFillType InFillType)
{
    if (FMath::IsNearlyEqual(AngleRad, 2.0f * PI))
    {
        return; // 完整圆环不需要端面
    }

    const int32 startCenterIndex = Vertices.Num();
    const int32 endCenterIndex = startCenterIndex + 1;

    // 计算起始和结束角度
    const float startAngle = 0.0f;
    const float endAngle = AngleRad;

    // 计算端面中心点
    const FVector startCenter(
        FMath::Cos(startAngle) * MajorRad,
        FMath::Sin(startAngle) * MajorRad,
        0.0f
    );
    
    const FVector endCenter(
        FMath::Cos(endAngle) * MajorRad,
        FMath::Sin(endAngle) * MajorRad,
        0.0f
    );

    // 计算端面法线（指向圆环内部）
    const FVector startNormal = FVector(-FMath::Sin(startAngle), FMath::Cos(startAngle), 0.0f);
    const FVector endNormal = FVector(-FMath::Sin(endAngle), FMath::Cos(endAngle), 0.0f);

    // 添加端面中心顶点
    Vertices.Add(startCenter);
    Normals.Add(startNormal);
    UVs.Add(FVector2D(0.5f, 0.5f));
    Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));

    Vertices.Add(endCenter);
    Normals.Add(endNormal);
    UVs.Add(FVector2D(0.5f, 0.5f));
    Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));

    // 生成端面三角形
    const int32 startSection = 0;
    const int32 endSection = MajorSegs * MinorSegs;

    // 根据填充类型生成端面
    switch (InFillType)
    {
    case ETorusFillType::NGon:
        {
            // NGon填充：使用扇形三角形
            for (int32 i = 0; i < MinorSegs; ++i)
            {
                const int32 nextI = (i + 1) % MinorSegs;
                
                // 起始端面（扇形三角形）
                Triangles.Add(startSection + i);
                Triangles.Add(startCenterIndex);
                Triangles.Add(startSection + nextI);

                // 结束端面（扇形三角形）
                Triangles.Add(endSection + i);
                Triangles.Add(endSection + nextI);
                Triangles.Add(endCenterIndex);
            }
        }
        break;

    case ETorusFillType::Triangles:
        {
            // 三角形填充：使用三角形网格
            // 对于起始端面
            for (int32 i = 0; i < MinorSegs - 2; ++i)
            {
                // 创建三角形扇形
                Triangles.Add(startSection + 0);
                Triangles.Add(startSection + i + 1);
                Triangles.Add(startSection + i + 2);
            }

            // 对于结束端面
            for (int32 i = 0; i < MinorSegs - 2; ++i)
            {
                // 创建三角形扇形
                Triangles.Add(endSection + 0);
                Triangles.Add(endSection + i + 1);
                Triangles.Add(endSection + i + 2);
            }
        }
        break;

    case ETorusFillType::None:
        {
            // 不填充端面，只添加边缘三角形
            for (int32 i = 0; i < MinorSegs; ++i)
            {
                const int32 nextI = (i + 1) % MinorSegs;
                
                // 起始端面边缘
                Triangles.Add(startSection + i);
                Triangles.Add(startCenterIndex);
                Triangles.Add(startSection + nextI);

                // 结束端面边缘
                Triangles.Add(endSection + i);
                Triangles.Add(endSection + nextI);
                Triangles.Add(endCenterIndex);
            }
        }
        break;
    }

    // 为端面顶点生成UV坐标
    for (int32 i = 0; i < MinorSegs; ++i)
    {
        const float angle = static_cast<float>(i) / MinorSegs * 2.0f * PI;
        const float u = 0.5f + 0.5f * FMath::Cos(angle);
        const float v = 0.5f + 0.5f * FMath::Sin(angle);
        
        // 更新起始端面顶点的UV
        if (startSection + i < UVs.Num())
        {
            UVs[startSection + i] = FVector2D(u, v);
        }
        
        // 更新结束端面顶点的UV
        if (endSection + i < UVs.Num())
        {
            UVs[endSection + i] = FVector2D(u, v);
        }
    }
}

// 高级端盖生成函数
void APolygonTorus::GenerateAdvancedEndCaps(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UVs,
    TArray<FProcMeshTangent>& Tangents,
    TArray<int32>& Triangles,
    float MajorRad,
    float MinorRad,
    float AngleRad,
    int32 MajorSegs,
    int32 MinorSegs,
    ETorusFillType InFillType,
    bool bGenerateInnerCaps,
    bool bGenerateOuterCaps)
{
    // 如果是完整圆环，不需要端面
    if (FMath::IsNearlyEqual(AngleRad, 2.0f * PI))
    {
        return;
    }

    // 计算起始和结束角度
    const float startAngle = 0.0f;
    const float endAngle = AngleRad;

    // 计算端面中心点
    const FVector startCenter(
        FMath::Cos(startAngle) * MajorRad,
        FMath::Sin(startAngle) * MajorRad,
        0.0f
    );
    
    const FVector endCenter(
        FMath::Cos(endAngle) * MajorRad,
        FMath::Sin(endAngle) * MajorRad,
        0.0f
    );

    // 计算端面法线（指向圆环内部）
    const FVector startNormal = FVector(-FMath::Sin(startAngle), FMath::Cos(startAngle), 0.0f);
    const FVector endNormal = FVector(-FMath::Sin(endAngle), FMath::Cos(endAngle), 0.0f);

    // 添加端面中心顶点
    const int32 startCenterIndex = Vertices.Num();
    const int32 endCenterIndex = startCenterIndex + 1;

    Vertices.Add(startCenter);
    Normals.Add(startNormal);
    UVs.Add(FVector2D(0.5f, 0.5f));
    Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));

    Vertices.Add(endCenter);
    Normals.Add(endNormal);
    UVs.Add(FVector2D(0.5f, 0.5f));
    Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));

    // 获取端面边缘顶点的索引
    const int32 startSectionStart = 0;
    const int32 startSectionEnd = MinorSegs;
    const int32 endSectionStart = MajorSegs * MinorSegs;
    const int32 endSectionEnd = endSectionStart + MinorSegs;

    // 根据填充类型生成端面
    switch (InFillType)
    {
    case ETorusFillType::NGon:
        {
            // NGon填充：使用扇形三角形
            for (int32 i = 0; i < MinorSegs; ++i)
            {
                const int32 nextI = (i + 1) % MinorSegs;
                
                // 起始端面（扇形三角形）
                if (bGenerateInnerCaps)
                {
                    Triangles.Add(startSectionStart + i);
                    Triangles.Add(startCenterIndex);
                    Triangles.Add(startSectionStart + nextI);
                }

                // 结束端面（扇形三角形）
                if (bGenerateOuterCaps)
                {
                    Triangles.Add(endSectionStart + i);
                    Triangles.Add(endSectionStart + nextI);
                    Triangles.Add(endCenterIndex);
                }
            }
        }
        break;

    case ETorusFillType::Triangles:
        {
            // 三角形填充：使用三角形网格
            if (bGenerateInnerCaps)
            {
                // 对于起始端面
                for (int32 i = 0; i < MinorSegs - 2; ++i)
                {
                    // 创建三角形扇形
                    Triangles.Add(startSectionStart + 0);
                    Triangles.Add(startSectionStart + i + 1);
                    Triangles.Add(startSectionStart + i + 2);
                }
            }

            if (bGenerateOuterCaps)
            {
                // 对于结束端面
                for (int32 i = 0; i < MinorSegs - 2; ++i)
                {
                    // 创建三角形扇形
                    Triangles.Add(endSectionStart + 0);
                    Triangles.Add(endSectionStart + i + 1);
                    Triangles.Add(endSectionStart + i + 2);
                }
            }
        }
        break;

    case ETorusFillType::None:
        {
            // 不填充端面，只添加边缘三角形
            for (int32 i = 0; i < MinorSegs; ++i)
            {
                const int32 nextI = (i + 1) % MinorSegs;
                
                // 起始端面边缘
                if (bGenerateInnerCaps)
                {
                    Triangles.Add(startSectionStart + i);
                    Triangles.Add(startCenterIndex);
                    Triangles.Add(startSectionStart + nextI);
                }

                // 结束端面边缘
                if (bGenerateOuterCaps)
                {
                    Triangles.Add(endSectionStart + i);
                    Triangles.Add(endSectionStart + nextI);
                    Triangles.Add(endCenterIndex);
                }
            }
        }
        break;
    }

    // 为端面顶点生成UV坐标
    for (int32 i = 0; i < MinorSegs; ++i)
    {
        const float angle = static_cast<float>(i) / MinorSegs * 2.0f * PI;
        const float u = 0.5f + 0.5f * FMath::Cos(angle);
        const float v = 0.5f + 0.5f * FMath::Sin(angle);
        
        // 更新起始端面顶点的UV
        if (startSectionStart + i < UVs.Num())
        {
            UVs[startSectionStart + i] = FVector2D(u, v);
        }
        
        // 更新结束端面顶点的UV
        if (endSectionStart + i < UVs.Num())
        {
            UVs[endSectionStart + i] = FVector2D(u, v);
        }
    }
}

// 生成圆形端盖（用于特殊形状）
void APolygonTorus::GenerateCircularEndCaps(
    TArray<FVector>& Vertices,
    TArray<FVector>& Normals,
    TArray<FVector2D>& UVs,
    TArray<FProcMeshTangent>& Tangents,
    TArray<int32>& Triangles,
    float MajorRad,
    float MinorRad,
    float AngleRad,
    int32 MajorSegs,
    int32 MinorSegs,
    int32 InCapSegments,
    bool bInGenerateStartCap,
    bool bInGenerateEndCap)
{
    // 如果是完整圆环，不需要端面
    if (FMath::IsNearlyEqual(AngleRad, 2.0f * PI))
    {
        return;
    }

    // 计算起始和结束角度
    const float startAngle = 0.0f;
    const float endAngle = AngleRad;

    // 计算端面中心点
    const FVector startCenter(
        FMath::Cos(startAngle) * MajorRad,
        FMath::Sin(startAngle) * MajorRad,
        0.0f
    );
    
    const FVector endCenter(
        FMath::Cos(endAngle) * MajorRad,
        FMath::Sin(endAngle) * MajorRad,
        0.0f
    );

    // 计算端面法线
    const FVector startNormal = FVector(-FMath::Sin(startAngle), FMath::Cos(startAngle), 0.0f);
    const FVector endNormal = FVector(-FMath::Sin(endAngle), FMath::Cos(endAngle), 0.0f);

    // 添加端面中心顶点
    const int32 startCenterIndex = Vertices.Num();
    const int32 endCenterIndex = startCenterIndex + 1;

    if (bInGenerateStartCap)
    {
        Vertices.Add(startCenter);
        Normals.Add(startNormal);
        UVs.Add(FVector2D(0.5f, 0.5f));
        Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));
    }

    if (bInGenerateEndCap)
    {
        Vertices.Add(endCenter);
        Normals.Add(endNormal);
        UVs.Add(FVector2D(0.5f, 0.5f));
        Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));
    }

    // 生成圆形端盖顶点
    if (bInGenerateStartCap)
    {
        for (int32 i = 0; i < InCapSegments; ++i)
        {
            const float angle = static_cast<float>(i) / InCapSegments * 2.0f * PI;
            const float cosAngle = FMath::Cos(angle);
            const float sinAngle = FMath::Sin(angle);
            
            const FVector capVertex(
                startCenter.X + cosAngle * MinorRad,
                startCenter.Y + sinAngle * MinorRad,
                0.0f
            );

            Vertices.Add(capVertex);
            Normals.Add(startNormal);
            UVs.Add(FVector2D(0.5f + 0.5f * cosAngle, 0.5f + 0.5f * sinAngle));
            Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));
        }
    }

    if (bInGenerateEndCap)
    {
        for (int32 i = 0; i < InCapSegments; ++i)
        {
            const float angle = static_cast<float>(i) / InCapSegments * 2.0f * PI;
            const float cosAngle = FMath::Cos(angle);
            const float sinAngle = FMath::Sin(angle);
            
            const FVector capVertex(
                endCenter.X + cosAngle * MinorRad,
                endCenter.Y + sinAngle * MinorRad,
                0.0f
            );

            Vertices.Add(capVertex);
            Normals.Add(endNormal);
            UVs.Add(FVector2D(0.5f + 0.5f * cosAngle, 0.5f + 0.5f * sinAngle));
            Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));
        }
    }

    // 生成端盖三角形
    if (bInGenerateStartCap)
    {
        const int32 startCapStartIndex = startCenterIndex + 1;
        for (int32 i = 0; i < InCapSegments; ++i)
        {
            const int32 nextI = (i + 1) % InCapSegments;
            
            Triangles.Add(startCenterIndex);
            Triangles.Add(startCapStartIndex + i);
            Triangles.Add(startCapStartIndex + nextI);
        }
    }

    if (bInGenerateEndCap)
    {
        const int32 endCapStartIndex = endCenterIndex + 1;
        for (int32 i = 0; i < InCapSegments; ++i)
        {
            const int32 nextI = (i + 1) % InCapSegments;
            
            Triangles.Add(endCenterIndex);
            Triangles.Add(endCapStartIndex + nextI);
            Triangles.Add(endCapStartIndex + i);
        }
    }
}

// 优化的法线计算
void APolygonTorus::CalculateOptimizedNormals(
    TArray<FVector>& Normals,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    float MajorRad,
    float MinorRad,
    int32 MajorSegs,
    int32 MinorSegs,
    bool bSmoothCross,
    bool bSmoothVertical)
{
    if (!bGenerateNormals)
    {
        return;
    }

    TArray<FVector> newNormals;
    newNormals.SetNum(Vertices.Num());
    
    // 初始化法线为零向量
    for (int32 i = 0; i < newNormals.Num(); i++)
    {
        newNormals[i] = FVector::ZeroVector;
    }

    // 计算面法线并累加到顶点
    for (int32 i = 0; i < Triangles.Num(); i += 3)
    {
        const int32 v0 = Triangles[i];
        const int32 v1 = Triangles[i + 1];
        const int32 v2 = Triangles[i + 2];

        const FVector edge1 = Vertices[v1] - Vertices[v0];
        const FVector edge2 = Vertices[v2] - Vertices[v0];
        FVector faceNormal = FVector::CrossProduct(edge1, edge2).GetSafeNormal();

        // 确保面法线指向外部
        const FVector faceCenter = (Vertices[v0] + Vertices[v1] + Vertices[v2]) / 3.0f;
        const FVector toCenter = FVector(faceCenter.X, faceCenter.Y, 0.0f).GetSafeNormal();
        
        if (FVector::DotProduct(faceNormal, toCenter) > 0)
        {
            faceNormal = -faceNormal;
        }

        newNormals[v0] += faceNormal;
        newNormals[v1] += faceNormal;
        newNormals[v2] += faceNormal;
    }

    // 标准化所有法线
    for (int32 i = 0; i < newNormals.Num(); i++)
    {
        newNormals[i] = newNormals[i].GetSafeNormal();
    }

    // 根据平滑设置调整法线
    if (!bSmoothCross || !bSmoothVertical)
    {
        for (int32 majorIndex = 0; majorIndex <= MajorSegs; ++majorIndex)
        {
            for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
            {
                const int32 vertexIndex = majorIndex * MinorSegs + minorIndex;
                if (vertexIndex >= Vertices.Num()) continue;

                if (!bSmoothCross)
                {
                    // 横切面不平滑：使用截面法线
                    const float majorAngle = static_cast<float>(majorIndex) * 2.0f * PI / MajorSegs;
                    const FVector sectionCenter(
                        FMath::Cos(majorAngle) * MajorRad,
                        FMath::Sin(majorAngle) * MajorRad,
                        0.0f
                    );
                    const FVector crossNormal = (Vertices[vertexIndex] - sectionCenter).GetSafeNormal();
                    newNormals[vertexIndex] = crossNormal;
                }

                if (!bSmoothVertical)
                {
                    // 竖面不平滑：使用径向法线
                    const FVector radialNormal = FVector(
                        Vertices[vertexIndex].X,
                        Vertices[vertexIndex].Y,
                        0.0f
                    ).GetSafeNormal();
                    newNormals[vertexIndex] = radialNormal;
                }
            }
        }
    }

    Normals = newNormals;
}

// 优化的UV生成
void APolygonTorus::GenerateUVsOptimized(
    TArray<FVector2D>& UVs,
    int32 MajorSegs,
    int32 MinorSegs,
    float AngleRad,
    ETorusUVMode InUVMode)
{
    if (!bGenerateUVs)
    {
        return;
    }

    UVs.Empty();
    const int32 totalVertices = (MajorSegs + 1) * MinorSegs;

    for (int32 majorIndex = 0; majorIndex <= MajorSegs; ++majorIndex)
    {
        for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
        {
            FVector2D uv;

            switch (InUVMode)
            {
            case ETorusUVMode::Standard:
                uv = FVector2D(
                    static_cast<float>(majorIndex) / MajorSegs,
                    static_cast<float>(minorIndex) / MinorSegs
                );
                break;

            case ETorusUVMode::Cylindrical:
                uv = FVector2D(
                    static_cast<float>(majorIndex) / MajorSegs,
                    static_cast<float>(minorIndex) / MinorSegs
                );
                break;

            case ETorusUVMode::Spherical:
                uv = FVector2D(
                    static_cast<float>(majorIndex) / MajorSegs,
                    static_cast<float>(minorIndex) / MinorSegs
                );
                break;
            }

            UVs.Add(uv);
        }
    }
}

// 优化的主要生成函数
void APolygonTorus::GenerateOptimizedTorus()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // 清除之前的网格
    ProceduralMesh->ClearAllMeshSections();

    // 验证和约束参数
    float majorRad = MajorRadius;
    float minorRad = MinorRadius;
    int32 majorSegs = MajorSegments;
    int32 minorSegs = MinorSegments;
    float angle = TorusAngle;

    ValidateAndClampParameters(majorRad, minorRad, majorSegs, minorSegs, angle);

    // 准备网格数据
    TArray<FVector> vertices;
    TArray<int32> triangles;
    TArray<FVector> normals;
    TArray<FVector2D> uvs;
    TArray<FLinearColor> vertexColors;
    TArray<FProcMeshTangent> tangents;

    const float angleRad = FMath::DegreesToRadians(angle);
    const bool bIsFullCircle = FMath::IsNearlyEqual(angle, 360.0f);

    // 生成优化的顶点
    GenerateOptimizedVertices(vertices, normals, uvs, tangents,
                             majorRad, minorRad, majorSegs, minorSegs, angleRad);

    // 生成优化的三角形
    GenerateOptimizedTriangles(triangles, majorSegs, minorSegs, bIsFullCircle);

    // 生成端面（如果不是完整圆环）
    if (!bIsFullCircle)
    {
        if (bUseCircularCaps)
        {
            // 使用圆形端盖
            GenerateCircularEndCaps(vertices, normals, uvs, tangents, triangles,
                                   majorRad, minorRad, angleRad, majorSegs, minorSegs, 
                                   CapSegments, bGenerateStartCap, bGenerateEndCap);
        }
        else
        {
            // 使用标准端盖
            GenerateAdvancedEndCaps(vertices, normals, uvs, tangents, triangles,
                                   majorRad, minorRad, angleRad, majorSegs, minorSegs, 
                                   this->FillType, bGenerateStartCap, bGenerateEndCap);
        }
    }

    // 重新计算法线
    CalculateOptimizedNormals(normals, vertices, triangles,
                             majorRad, minorRad, majorSegs, minorSegs,
                             bSmoothCrossSection, bSmoothVerticalSection);

    // 重新生成UV（如果需要）
    if (bGenerateUVs)
    {
        GenerateUVsOptimized(uvs, majorSegs, minorSegs, angleRad, this->UVMode);
    }

    // 验证网格拓扑
    ValidateMeshTopology(vertices, triangles);

    // 记录网格统计信息
    LogMeshStatistics(vertices, triangles);

    // 创建网格
    ProceduralMesh->CreateMeshSection_LinearColor(
        0, vertices, triangles, normals, uvs, vertexColors, tangents, true
    );
}

// 网格拓扑验证
void APolygonTorus::ValidateMeshTopology(const TArray<FVector>& Vertices, const TArray<int32>& Triangles)
{
    if (Vertices.Num() == 0 || Triangles.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ValidateMeshTopology: Empty mesh data"));
        return;
    }

    // 检查三角形索引是否有效
    for (int32 i = 0; i < Triangles.Num(); ++i)
    {
        if (Triangles[i] < 0 || Triangles[i] >= Vertices.Num())
        {
            UE_LOG(LogTemp, Error, TEXT("Invalid triangle index: %d (vertex count: %d)"), 
                   Triangles[i], Vertices.Num());
        }
    }

    // 检查三角形数量是否为3的倍数
    if (Triangles.Num() % 3 != 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Triangle count is not divisible by 3: %d"), Triangles.Num());
    }
}

// 记录网格统计信息
void APolygonTorus::LogMeshStatistics(const TArray<FVector>& Vertices, const TArray<int32>& Triangles)
{
    if (Vertices.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("PolygonTorus: Generated %d vertices, %d triangles"), 
               Vertices.Num(), Triangles.Num() / 3);
    }
}

// 高级光滑控制函数
void APolygonTorus::GenerateTorusWithSmoothing(ETorusSmoothMode InSmoothMode, float InSmoothingAngle)
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // 清除之前的网格
    ProceduralMesh->ClearAllMeshSections();

    // 验证和约束参数
    float majorRad = MajorRadius;
    float minorRad = MinorRadius;
    int32 majorSegs = MajorSegments;
    int32 minorSegs = MinorSegments;
    float angle = TorusAngle;

    ValidateAndClampParameters(majorRad, minorRad, majorSegs, minorSegs, angle);

    // 准备网格数据
    TArray<FVector> vertices;
    TArray<int32> triangles;
    TArray<FVector> normals;
    TArray<FVector2D> uvs;
    TArray<FLinearColor> vertexColors;
    TArray<FProcMeshTangent> tangents;
    TArray<int32> smoothGroups;
    TArray<bool> hardEdges;

    const float angleRad = FMath::DegreesToRadians(angle);
    const bool bIsFullCircle = FMath::IsNearlyEqual(angle, 360.0f);

    // 生成优化的顶点
    GenerateOptimizedVertices(vertices, normals, uvs, tangents,
                             majorRad, minorRad, majorSegs, minorSegs, angleRad);

    // 生成优化的三角形
    GenerateOptimizedTriangles(triangles, majorSegs, minorSegs, bIsFullCircle);

    // 生成端面（如果不是完整圆环）
    if (!bIsFullCircle)
    {
        if (bUseCircularCaps)
        {
            // 使用圆形端盖
            GenerateCircularEndCaps(vertices, normals, uvs, tangents, triangles,
                                   majorRad, minorRad, angleRad, majorSegs, minorSegs, 
                                   CapSegments, bGenerateStartCap, bGenerateEndCap);
        }
        else
        {
            // 使用标准端盖
            GenerateAdvancedEndCaps(vertices, normals, uvs, tangents, triangles,
                                   majorRad, minorRad, angleRad, majorSegs, minorSegs, 
                                   this->FillType, bGenerateStartCap, bGenerateEndCap);
        }
    }

    // 计算高级光滑
    CalculateAdvancedSmoothing(normals, smoothGroups, hardEdges, vertices, triangles,
                              majorRad, minorRad, majorSegs, minorSegs, InSmoothMode, InSmoothingAngle);

    // 重新生成UV（如果需要）
    if (bGenerateUVs)
    {
        GenerateUVsOptimized(uvs, majorSegs, minorSegs, angleRad, this->UVMode);
    }

    // 验证网格拓扑
    ValidateMeshTopology(vertices, triangles);

    // 记录网格统计信息
    LogMeshStatistics(vertices, triangles);

    // 创建网格（注意：UE的ProceduralMeshComponent不直接支持光滑组和硬边，这里我们通过法线来实现）
    ProceduralMesh->CreateMeshSection_LinearColor(
        0, vertices, triangles, normals, uvs, vertexColors, tangents, true
    );

    // 输出光滑信息
    UE_LOG(LogTemp, Log, TEXT("PolygonTorus: Applied smoothing mode %d with angle threshold %.1f degrees"), 
           static_cast<int32>(InSmoothMode), InSmoothingAngle);
}

// 高级光滑计算
void APolygonTorus::CalculateAdvancedSmoothing(
    TArray<FVector>& Normals,
    TArray<int32>& SmoothGroups,
    TArray<bool>& HardEdges,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    float MajorRad,
    float MinorRad,
    int32 MajorSegs,
    int32 MinorSegs,
    ETorusSmoothMode InSmoothMode,
    float InSmoothingAngle)
{
    if (!bGenerateNormals)
    {
        return;
    }

    // 初始化数组
    SmoothGroups.SetNum(Vertices.Num());
    HardEdges.SetNum(Triangles.Num() / 3);
    
    // 生成光滑组
    if (bGenerateSmoothGroups)
    {
        GenerateSmoothGroups(SmoothGroups, Vertices, Triangles, MajorSegs, MinorSegs, InSmoothMode);
    }

    // 生成硬边标记
    if (bGenerateHardEdges)
    {
        GenerateHardEdges(HardEdges, Vertices, Triangles, MajorSegs, MinorSegs, InSmoothMode, InSmoothingAngle);
    }

    // 根据光滑模式计算法线
    TArray<FVector> newNormals;
    newNormals.SetNum(Vertices.Num());
    
    // 初始化法线为零向量
    for (int32 i = 0; i < newNormals.Num(); i++)
    {
        newNormals[i] = FVector::ZeroVector;
    }

    // 计算面法线并累加到顶点
    for (int32 i = 0; i < Triangles.Num(); i += 3)
    {
        const int32 v0 = Triangles[i];
        const int32 v1 = Triangles[i + 1];
        const int32 v2 = Triangles[i + 2];

        const FVector edge1 = Vertices[v1] - Vertices[v0];
        const FVector edge2 = Vertices[v2] - Vertices[v0];
        FVector faceNormal = FVector::CrossProduct(edge1, edge2).GetSafeNormal();

        // 确保面法线指向外部
        const FVector faceCenter = (Vertices[v0] + Vertices[v1] + Vertices[v2]) / 3.0f;
        const FVector toCenter = FVector(faceCenter.X, faceCenter.Y, 0.0f).GetSafeNormal();
        
        if (FVector::DotProduct(faceNormal, toCenter) > 0)
        {
            faceNormal = -faceNormal;
        }

        // 根据光滑模式决定是否累加法线
        bool bShouldSmooth = false;
        switch (InSmoothMode)
        {
        case ETorusSmoothMode::None:
            bShouldSmooth = false;
            break;
        case ETorusSmoothMode::Cross:
            bShouldSmooth = true; // 横切面光滑
            break;
        case ETorusSmoothMode::Vertical:
            bShouldSmooth = true; // 竖面光滑
            break;
        case ETorusSmoothMode::Both:
            bShouldSmooth = true; // 两个方向都光滑
            break;
        case ETorusSmoothMode::Auto:
            // 自动检测：根据角度阈值决定
            const float angleRad = FMath::DegreesToRadians(InSmoothingAngle);
            const float cosThreshold = FMath::Cos(angleRad);
            
            // 检查相邻面的角度
            FVector avgNormal = faceNormal;
            int32 connectedFaces = 1;
            
            // 这里简化处理，实际应该检查所有相邻面
            bShouldSmooth = true; // 默认光滑
            break;
        }

        if (bShouldSmooth)
        {
            newNormals[v0] += faceNormal;
            newNormals[v1] += faceNormal;
            newNormals[v2] += faceNormal;
        }
        else
        {
            // 硬边：每个顶点使用面法线
            newNormals[v0] = faceNormal;
            newNormals[v1] = faceNormal;
            newNormals[v2] = faceNormal;
        }
    }

    // 标准化所有法线
    for (int32 i = 0; i < newNormals.Num(); i++)
    {
        newNormals[i] = newNormals[i].GetSafeNormal();
    }

    // 根据光滑模式进行特殊处理
    if (InSmoothMode != ETorusSmoothMode::None)
    {
        for (int32 majorIndex = 0; majorIndex <= MajorSegs; ++majorIndex)
        {
            for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
            {
                const int32 vertexIndex = majorIndex * MinorSegs + minorIndex;
                if (vertexIndex >= Vertices.Num()) continue;

                FVector finalNormal = newNormals[vertexIndex];

                // 根据光滑模式调整法线
                switch (InSmoothMode)
                {
                case ETorusSmoothMode::Cross:
                    // 只对横切面进行光滑处理
                    if (bSmoothCrossSection)
                    {
                        const float majorAngle = static_cast<float>(majorIndex) * 2.0f * PI / MajorSegs;
                        const FVector sectionCenter(
                            FMath::Cos(majorAngle) * MajorRad,
                            FMath::Sin(majorAngle) * MajorRad,
                            0.0f
                        );
                        const FVector crossNormal = (Vertices[vertexIndex] - sectionCenter).GetSafeNormal();
                        finalNormal = crossNormal;
                    }
                    break;

                case ETorusSmoothMode::Vertical:
                    // 只对竖面进行光滑处理
                    if (bSmoothVerticalSection)
                    {
                        const FVector radialNormal = FVector(
                            Vertices[vertexIndex].X,
                            Vertices[vertexIndex].Y,
                            0.0f
                        ).GetSafeNormal();
                        finalNormal = radialNormal;
                    }
                    break;

                case ETorusSmoothMode::Both:
                    // 两个方向都光滑，使用计算出的平滑法线
                    break;

                case ETorusSmoothMode::Auto:
                    // 自动模式：根据角度阈值决定
                    break;
                }

                newNormals[vertexIndex] = finalNormal;
            }
        }
    }

    Normals = newNormals;
}

// 生成光滑组
void APolygonTorus::GenerateSmoothGroups(
    TArray<int32>& SmoothGroups,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    int32 MajorSegs,
    int32 MinorSegs,
    ETorusSmoothMode InSmoothMode)
{
    // 初始化所有顶点为同一光滑组
    for (int32 i = 0; i < SmoothGroups.Num(); i++)
    {
        SmoothGroups[i] = 0;
    }

    // 根据光滑模式分配不同的光滑组
    switch (InSmoothMode)
    {
    case ETorusSmoothMode::None:
        // 每个面一个光滑组
        for (int32 i = 0; i < Triangles.Num(); i += 3)
        {
            const int32 groupId = i / 3;
            SmoothGroups[Triangles[i]] = groupId;
            SmoothGroups[Triangles[i + 1]] = groupId;
            SmoothGroups[Triangles[i + 2]] = groupId;
        }
        break;

    case ETorusSmoothMode::Cross:
        // 横切面光滑：每个横截面一个光滑组
        for (int32 majorIndex = 0; majorIndex <= MajorSegs; ++majorIndex)
        {
            const int32 groupId = majorIndex;
            for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
            {
                const int32 vertexIndex = majorIndex * MinorSegs + minorIndex;
                if (vertexIndex < SmoothGroups.Num())
                {
                    SmoothGroups[vertexIndex] = groupId;
                }
            }
        }
        break;

    case ETorusSmoothMode::Vertical:
        // 竖面光滑：每个竖截面一个光滑组
        for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
        {
            const int32 groupId = minorIndex;
            for (int32 majorIndex = 0; majorIndex <= MajorSegs; ++majorIndex)
            {
                const int32 vertexIndex = majorIndex * MinorSegs + minorIndex;
                if (vertexIndex < SmoothGroups.Num())
                {
                    SmoothGroups[vertexIndex] = groupId;
                }
            }
        }
        break;

    case ETorusSmoothMode::Both:
        // 两个方向都光滑：所有顶点一个光滑组
        for (int32 i = 0; i < SmoothGroups.Num(); i++)
        {
            SmoothGroups[i] = 0;
        }
        break;

    case ETorusSmoothMode::Auto:
        // 自动模式：根据几何特征分配光滑组
        // 这里简化处理，实际应该根据角度和曲率来分配
        for (int32 i = 0; i < SmoothGroups.Num(); i++)
        {
            SmoothGroups[i] = 0;
        }
        break;
    }
}

// 生成硬边标记
void APolygonTorus::GenerateHardEdges(
    TArray<bool>& HardEdges,
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    int32 MajorSegs,
    int32 MinorSegs,
    ETorusSmoothMode InSmoothMode,
    float InSmoothingAngle)
{
    const float angleThreshold = FMath::DegreesToRadians(InSmoothingAngle);
    const float cosThreshold = FMath::Cos(angleThreshold);

    // 初始化所有边为软边
    for (int32 i = 0; i < HardEdges.Num(); i++)
    {
        HardEdges[i] = false;
    }

    // 根据光滑模式标记硬边
    switch (InSmoothMode)
    {
    case ETorusSmoothMode::None:
        // 所有边都是硬边
        for (int32 i = 0; i < HardEdges.Num(); i++)
        {
            HardEdges[i] = true;
        }
        break;

    case ETorusSmoothMode::Cross:
        // 横切面边界是硬边
        for (int32 majorIndex = 0; majorIndex < MajorSegs; ++majorIndex)
        {
            for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
            {
                const int32 triangleIndex = (majorIndex * MinorSegs + minorIndex) * 2; // 每个四边形产生2个三角形
                if (triangleIndex < HardEdges.Num())
                {
                    HardEdges[triangleIndex] = true;
                    if (triangleIndex + 1 < HardEdges.Num())
                    {
                        HardEdges[triangleIndex + 1] = true;
                    }
                }
            }
        }
        break;

    case ETorusSmoothMode::Vertical:
        // 竖面边界是硬边
        for (int32 majorIndex = 0; majorIndex < MajorSegs; ++majorIndex)
        {
            for (int32 minorIndex = 0; minorIndex < MinorSegs; ++minorIndex)
            {
                const int32 triangleIndex = (majorIndex * MinorSegs + minorIndex) * 2;
                if (triangleIndex < HardEdges.Num())
                {
                    HardEdges[triangleIndex] = true;
                    if (triangleIndex + 1 < HardEdges.Num())
                    {
                        HardEdges[triangleIndex + 1] = true;
                    }
                }
            }
        }
        break;

    case ETorusSmoothMode::Both:
        // 根据角度阈值决定硬边
        for (int32 i = 0; i < Triangles.Num(); i += 3)
        {
            const int32 triangleIndex = i / 3;
            if (triangleIndex < HardEdges.Num())
            {
                // 计算面法线
                const int32 v0 = Triangles[i];
                const int32 v1 = Triangles[i + 1];
                const int32 v2 = Triangles[i + 2];

                const FVector edge1 = Vertices[v1] - Vertices[v0];
                const FVector edge2 = Vertices[v2] - Vertices[v0];
                const FVector faceNormal = FVector::CrossProduct(edge1, edge2).GetSafeNormal();

                // 检查与相邻面的角度（简化处理）
                HardEdges[triangleIndex] = false; // 默认软边
            }
        }
        break;

    case ETorusSmoothMode::Auto:
        // 自动检测硬边
        for (int32 i = 0; i < Triangles.Num(); i += 3)
        {
            const int32 triangleIndex = i / 3;
            if (triangleIndex < HardEdges.Num())
            {
                // 这里可以实现更复杂的自动检测逻辑
                HardEdges[triangleIndex] = false;
            }
        }
        break;
    }
}

// 实现 GeneratePolygonTorus 函数
void APolygonTorus::GeneratePolygonTorus(float MajorRad, float MinorRad, int32 MajorSegs, int32 MinorSegs, 
                                         float Angle, bool bSmoothCross, bool bSmoothVertical)
{
    // 验证和约束参数
    ValidateAndClampParameters(MajorRad, MinorRad, MajorSegs, MinorSegs, Angle);
    
    // 将角度转换为弧度
    const float AngleRad = FMath::DegreesToRadians(Angle);
    
    // 清空现有网格数据
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;
    
    // 生成优化的顶点
    GenerateOptimizedVertices(Vertices, Normals, UVs, Tangents, MajorRad, MinorRad, MajorSegs, MinorSegs, AngleRad);
    
    // 生成三角形
    GenerateOptimizedTriangles(Triangles, MajorSegs, MinorSegs, FMath::IsNearlyEqual(Angle, 360.0f));
    
    // 生成端面（如果需要）
    if (!FMath::IsNearlyEqual(Angle, 360.0f))
    {
        if (bUseCircularCaps)
        {
            // 使用圆形端盖
            GenerateCircularEndCaps(Vertices, Normals, UVs, Tangents, Triangles,
                                   MajorRad, MinorRad, AngleRad, MajorSegs, MinorSegs, 
                                   CapSegments, bGenerateStartCap, bGenerateEndCap);
        }
        else
        {
            // 使用标准端盖
            GenerateAdvancedEndCaps(Vertices, Normals, UVs, Tangents, Triangles,
                                   MajorRad, MinorRad, AngleRad, MajorSegs, MinorSegs, 
                                   FillType, bGenerateStartCap, bGenerateEndCap);
        }
    }
    
    // 计算法线
    CalculateOptimizedNormals(Normals, Vertices, Triangles, MajorRad, MinorRad, MajorSegs, MinorSegs, bSmoothCross, bSmoothVertical);
    
    // 生成UV坐标（如果需要）
    if (bGenerateUVs)
    {
        GenerateUVsOptimized(UVs, MajorSegs, MinorSegs, AngleRad, UVMode);
    }
    
    // 验证网格拓扑
    ValidateMeshTopology(Vertices, Triangles);
    
    // 记录网格统计信息
    LogMeshStatistics(Vertices, Triangles);
    
    // 创建网格
    ProceduralMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, true);
    
    // 设置碰撞
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

// 端盖生成函数
void APolygonTorus::GenerateEndCapsOnly()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // 验证和约束参数
    float majorRad = MajorRadius;
    float minorRad = MinorRadius;
    int32 majorSegs = MajorSegments;
    int32 minorSegs = MinorSegments;
    float angle = TorusAngle;

    ValidateAndClampParameters(majorRad, minorRad, majorSegs, minorSegs, angle);

    // 准备网格数据
    TArray<FVector> vertices;
    TArray<int32> triangles;
    TArray<FVector> normals;
    TArray<FVector2D> uvs;
    TArray<FLinearColor> vertexColors;
    TArray<FProcMeshTangent> tangents;

    const float angleRad = FMath::DegreesToRadians(angle);
    const bool bIsFullCircle = FMath::IsNearlyEqual(angle, 360.0f);

    if (!bIsFullCircle)
    {
        if (bUseCircularCaps)
        {
            // 使用圆形端盖
            GenerateCircularEndCaps(vertices, normals, uvs, tangents, triangles,
                                   majorRad, minorRad, angleRad, majorSegs, minorSegs, 
                                   CapSegments, bGenerateStartCap, bGenerateEndCap);
        }
        else
        {
            // 使用标准端盖
            GenerateAdvancedEndCaps(vertices, normals, uvs, tangents, triangles,
                                   majorRad, minorRad, angleRad, majorSegs, minorSegs, 
                                   this->FillType, bGenerateStartCap, bGenerateEndCap);
        }

        // 验证网格拓扑
        ValidateMeshTopology(vertices, triangles);

        // 记录网格统计信息
        LogMeshStatistics(vertices, triangles);

        // 创建端盖网格
        ProceduralMesh->CreateMeshSection_LinearColor(
            1, vertices, triangles, normals, uvs, vertexColors, tangents, true
        );
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateEndCapsOnly: Full circle torus doesn't need end caps"));
    }
}

// 重新生成端盖
void APolygonTorus::RegenerateEndCaps()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // 清除现有的端盖网格
    ProceduralMesh->ClearMeshSection(1);

    // 重新生成端盖
    GenerateEndCapsOnly();
}
