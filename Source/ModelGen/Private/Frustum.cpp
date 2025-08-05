// Frustum.cpp

#include "Frustum.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/UnrealMathUtility.h"

// ============================================================================
// FMeshBuilder 实现
// ============================================================================

FMeshBuilder::FMeshBuilder()
{
}

FMeshBuilder::~FMeshBuilder()
{
}

int32 FMeshBuilder::AddVertex(const FVector& Position, const FVector& Normal, const FVector2D& UV)
{
    FVertex Vertex(Position, Normal, UV);
    return AddVertex(Vertex);
}

int32 FMeshBuilder::AddVertex(const FVertex& Vertex)
{
    const int32 Index = Vertices.Add(Vertex);
    return Index;
}

void FMeshBuilder::AddTriangle(int32 A, int32 B, int32 C)
{
    Triangles.Add(FTriangle(A, B, C));
}

void FMeshBuilder::AddQuad(int32 A, int32 B, int32 C, int32 D)
{
    // 将四边形分解为两个三角形
    AddTriangle(A, B, C);
    AddTriangle(A, C, D);
}

void FMeshBuilder::AddTriangle(const FTriangle& Triangle)
{
    Triangles.Add(Triangle);
}

void FMeshBuilder::AddQuad(const FQuad& Quad)
{
    AddQuad(Quad.Indices[0], Quad.Indices[1], Quad.Indices[2], Quad.Indices[3]);
}

void FMeshBuilder::AddTriangles(const TArray<FTriangle>& InTriangles)
{
    Triangles.Append(InTriangles);
}

void FMeshBuilder::AddQuads(const TArray<FQuad>& InQuads)
{
    for (const FQuad& Quad : InQuads)
    {
        AddQuad(Quad);
    }
}

void FMeshBuilder::BuildMesh(UProceduralMeshComponent* MeshComponent)
{
    if (!MeshComponent || Vertices.Num() == 0)
    {
        return;
    }

    // 计算切线
    CalculateTangents();

    // 提取数据
    TArray<FVector> Positions;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Indices;

    Positions.Reserve(Vertices.Num());
    Normals.Reserve(Vertices.Num());
    UVs.Reserve(Vertices.Num());
    Colors.Reserve(Vertices.Num());
    Tangents.Reserve(Vertices.Num());
    Indices.Reserve(Triangles.Num() * 3);

    // 转换顶点数据
    for (const FVertex& Vertex : Vertices)
    {
        Positions.Add(Vertex.Position);
        Normals.Add(Vertex.Normal);
        UVs.Add(Vertex.UV);
        Colors.Add(Vertex.Color);
        Tangents.Add(Vertex.Tangent);
    }

    // 转换三角形数据
    for (const FTriangle& Triangle : Triangles)
    {
        Indices.Add(Triangle.Indices[0]);
        Indices.Add(Triangle.Indices[1]);
        Indices.Add(Triangle.Indices[2]);
    }

    // 创建网格
    MeshComponent->CreateMeshSection_LinearColor(
        0,
        Positions,
        Indices,
        Normals,
        UVs,
        Colors,
        Tangents,
        true
    );
}

void FMeshBuilder::Clear()
{
    Vertices.Empty();
    Triangles.Empty();
}

void FMeshBuilder::Reserve(int32 VertexCount, int32 TriangleCount)
{
    Vertices.Reserve(VertexCount);
    Triangles.Reserve(TriangleCount);
}

void FMeshBuilder::CalculateTangents()
{
    // 为每个顶点计算切线
    for (FVertex& Vertex : Vertices)
    {
        FVector Tangent = FVector::CrossProduct(Vertex.Normal, FVector::UpVector);
        if (Tangent.IsNearlyZero())
        {
            Tangent = FVector::CrossProduct(Vertex.Normal, FVector::RightVector);
        }
        Tangent.Normalize();
        Vertex.Tangent = FProcMeshTangent(Tangent, false);
    }
}

// ============================================================================
// FGeometryGenerator 实现
// ============================================================================

// FSideGeometryGenerator 实现
void FSideGeometryGenerator::Generate(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    const float HalfHeight = Params.Height / 2.0f;
    const float ChamferHeight = Params.bEnableChamfer ? Params.ChamferRadius : 0.0f;
    const float AdjustedHeight = Params.Height - ChamferHeight;
    const float HeightStep = AdjustedHeight / Params.HeightSegments;
    
    // 计算角度步长
    const float BottomAngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.BottomSides;
    const float TopAngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.TopSides;
    
    // 确保上底边数 <= 下底边数
    const int32 TopSides = FMath::Min(Params.TopSides, Params.BottomSides);
    const int32 BottomSides = Params.BottomSides;

    // 生成顶点环
    TArray<TArray<int32>> VertexRings;
    VertexRings.SetNum(Params.HeightSegments + 1);

    for (int32 h = 0; h <= Params.HeightSegments; ++h)
    {
        const float Z = -HalfHeight + ChamferHeight + h * HeightStep;
        const float Alpha = static_cast<float>(h) / Params.HeightSegments;
        
        TArray<int32>& Ring = VertexRings[h];
        Ring.SetNum(BottomSides + 1);
        
        for (int32 s = 0; s <= BottomSides; ++s)
        {
            const float Angle = s * BottomAngleStep;
            const float Radius = CalculateRadius(Params, Alpha);
            const float X = Radius * FMath::Cos(Angle);
            const float Y = Radius * FMath::Sin(Angle);
            
            const FVector Position(X, Y, Z);
            const FVector Normal = CalculateNormal(Params, Alpha, Position);
            const FVector2D UV(static_cast<float>(s) / BottomSides, Alpha);
            
            Ring[s] = Builder.AddVertex(Position, Normal, UV);
        }
    }

    // 生成不规则棱柱的三角形网格
    for (int32 h = 0; h < Params.HeightSegments; ++h)
    {
        for (int32 s = 0; s < BottomSides; ++s)
        {
            const int32 V00 = VertexRings[h][s];
            const int32 V10 = VertexRings[h + 1][s];
            const int32 V01 = VertexRings[h][s + 1];
            const int32 V11 = VertexRings[h + 1][s + 1];

            if (TopSides < BottomSides)
            {
                // 计算对应的上底顶点索引
                const float TopS = static_cast<float>(s) * TopSides / BottomSides;
                const int32 TopS0 = FMath::FloorToInt(TopS);
                const int32 TopS1 = FMath::Min(TopS0 + 1, TopSides);
                
                if (s < TopSides)
                {
                    // 上底边连接下底顶点形成三角形
                    Builder.AddTriangle(V00, V10, V01);
                    Builder.AddTriangle(V01, V10, V11);
                }
                else
                {
                    // 下底边连接上底顶点形成三角形
                    // 需要计算正确的上底顶点位置
                    const float TopAngle = TopS0 * TopAngleStep;
                    const float TopRadius = CalculateRadius(Params, static_cast<float>(h + 1) / Params.HeightSegments);
                    const float TopX = TopRadius * FMath::Cos(TopAngle);
                    const float TopY = TopRadius * FMath::Sin(TopAngle);
                    const float TopZ = -HalfHeight + ChamferHeight + (h + 1) * HeightStep;
                    
                    const FVector TopPosition(TopX, TopY, TopZ);
                    const FVector TopNormal = CalculateNormal(Params, static_cast<float>(h + 1) / Params.HeightSegments, TopPosition);
                    const FVector2D TopUV(static_cast<float>(TopS0) / TopSides, static_cast<float>(h + 1) / Params.HeightSegments);
                    
                    const int32 TopVertex = Builder.AddVertex(TopPosition, TopNormal, TopUV);
                    
                    Builder.AddTriangle(V00, TopVertex, V01);
                    Builder.AddTriangle(V01, TopVertex, V11);
                }
            }
            else
            {
                // 上下底边数相等，使用标准四边形分解
                Builder.AddTriangle(V00, V10, V01);
                Builder.AddTriangle(V01, V10, V11);
            }
        }
    }
}

void FSideGeometryGenerator::GenerateVertexRing(FMeshBuilder& Builder, const FFrustumParameters& Params, float Height, float Alpha)
{
    // 这个方法可以用于生成特定的顶点环
    // 当前实现中，顶点环的生成在Generate方法中完成
}

float FSideGeometryGenerator::CalculateRadius(const FFrustumParameters& Params, float Alpha)
{
    float Radius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha);
    
    // 考虑弯曲效果
    const float BendFactor = FMath::Sin(Alpha * PI);
    float BentRadius = FMath::Max(
        Radius + Params.BendAmount * BendFactor * Radius,
        Params.MinBendRadius
    );
    

    
    return BentRadius;
}

FVector FSideGeometryGenerator::CalculateNormal(const FFrustumParameters& Params, float Alpha, const FVector& Position)
{
    FVector Normal = FVector(Position.X, Position.Y, 0).GetSafeNormal();
    
    if (!FMath::IsNearlyZero(Params.BendAmount))
    {
        const float NormalZ = -Params.BendAmount * FMath::Cos(Alpha * PI);
        Normal = (Normal + FVector(0, 0, NormalZ)).GetSafeNormal();
    }
    
    return Normal;
}

// FTopGeometryGenerator 实现
void FTopGeometryGenerator::Generate(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    // 如果有倒角，顶部表面由倒角生成器处理
    if (Params.bEnableChamfer && Params.ChamferRadius > 0.0f)
    {
        return;
    }
    
    const float HalfHeight = Params.Height / 2.0f;
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.TopSides;

    // 中心顶点
    const int32 CenterVertex = Builder.AddVertex(
        FVector(0, 0, HalfHeight),
        FVector(0, 0, 1),
        FVector2D(0.5f, 0.5f)
    );

    // 顶部顶点环
    TArray<int32> TopRing;
    TopRing.SetNum(Params.TopSides + 1);

    for (int32 s = 0; s <= Params.TopSides; ++s)
    {
        const float Angle = s * AngleStep;
        
        const float TopRadius = Params.TopRadius;
        
        const float X = TopRadius * FMath::Cos(Angle);
        const float Y = TopRadius * FMath::Sin(Angle);

        // UV映射到圆形
        const float U = 0.5f + 0.5f * FMath::Cos(Angle);
        const float V = 0.5f + 0.5f * FMath::Sin(Angle);

        TopRing[s] = Builder.AddVertex(
            FVector(X, Y, HalfHeight),
            FVector(0, 0, 1),
            FVector2D(U, V)
        );
    }

    // 创建顶部三角扇
    for (int32 s = 0; s < Params.TopSides; ++s)
    {
        Builder.AddTriangle(
            CenterVertex,
            TopRing[s + 1],
            TopRing[s]
        );
    }
}

// FBottomGeometryGenerator 实现
void FBottomGeometryGenerator::Generate(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    // 如果有倒角，底部表面由倒角生成器处理
    if (Params.bEnableChamfer && Params.ChamferRadius > 0.0f)
    {
        return;
    }
    
    const float HalfHeight = Params.Height / 2.0f;
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.BottomSides;

    // 中心顶点
    const int32 CenterVertex = Builder.AddVertex(
        FVector(0, 0, -HalfHeight),
        FVector(0, 0, -1),
        FVector2D(0.5f, 0.5f)
    );

    // 底部顶点环
    TArray<int32> BottomRing;
    BottomRing.SetNum(Params.BottomSides + 1);

    for (int32 s = 0; s <= Params.BottomSides; ++s)
    {
        const float Angle = s * AngleStep;
        
        const float BottomRadius = Params.BottomRadius;
        
        const float X = BottomRadius * FMath::Cos(Angle);
        const float Y = BottomRadius * FMath::Sin(Angle);

        // UV映射到圆形
        const float U = 0.5f + 0.5f * FMath::Cos(Angle);
        const float V = 0.5f + 0.5f * FMath::Sin(Angle);

        BottomRing[s] = Builder.AddVertex(
            FVector(X, Y, -HalfHeight),
            FVector(0, 0, -1),
            FVector2D(U, V)
        );
    }

    // 创建底部三角扇
    for (int32 s = 0; s < Params.BottomSides; ++s)
    {
        Builder.AddTriangle(
            CenterVertex,
            BottomRing[s],
            BottomRing[s + 1]
        );
    }
}



// FEndCapGeometryGenerator 实现
void FEndCapGeometryGenerator::Generate(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    if (Params.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER || !Params.bEnableEndCaps)
    {
        return;
    }

    const float StartAngle = 0.0f;
    const float EndAngle = FMath::DegreesToRadians(Params.ArcAngle);

    // 起始端面 (0度)
    GenerateEndCap(Builder, Params, StartAngle, true);

    // 结束端面 (ArcAngle度)
    GenerateEndCap(Builder, Params, EndAngle, false);
}

void FEndCapGeometryGenerator::GenerateEndCap(FMeshBuilder& Builder, const FFrustumParameters& Params, float Angle, bool bIsStart)
{
    const float HalfHeight = Params.Height / 2.0f;
    const float ChamferRadius = Params.bEnableChamfer ? Params.ChamferRadius : 0.0f;
    
    // 计算端面法线
    const FVector Normal = bIsStart ? 
        FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0) : 
        FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0);
    
    // 计算端面的半径范围（考虑倒角收缩）
    const float StartRadius = Params.BottomRadius - ChamferRadius;
    const float EndRadius = Params.TopRadius - ChamferRadius;
    
    // 使用等腰三角形结构生成端面
    const int32 HeightSegments = Params.HeightSegments;
    
    // 生成端面顶点
    TArray<TArray<int32>> EndCapVertices;
    EndCapVertices.SetNum(HeightSegments + 1);
    
    for (int32 h = 0; h <= HeightSegments; ++h)
    {
        const float HeightAlpha = static_cast<float>(h) / HeightSegments;
        const float CurrentHeight = FMath::Lerp(-HalfHeight, HalfHeight, HeightAlpha);
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, HeightAlpha);
        
        TArray<int32>& Ring = EndCapVertices[h];
        Ring.SetNum(2); // 只使用两个顶点：内顶点和外顶点
        
        // 内顶点（中心点）
        const FVector InnerPosition(0, 0, CurrentHeight);
        const float InnerU = 0.0f;
        const float InnerV = HeightAlpha;
        const FVector2D InnerUV(InnerU, InnerV);
        
        Ring[0] = Builder.AddVertex(InnerPosition, Normal, InnerUV);
        
        // 外顶点（边缘点）
        const float X = CurrentRadius * FMath::Cos(Angle);
        const float Y = CurrentRadius * FMath::Sin(Angle);
        const FVector OuterPosition(X, Y, CurrentHeight);
        const float OuterU = 1.0f;
        const float OuterV = HeightAlpha;
        const FVector2D OuterUV(OuterU, OuterV);
        
        Ring[1] = Builder.AddVertex(OuterPosition, Normal, OuterUV);
    }
    
    // 生成等腰三角形网格
    for (int32 h = 0; h < HeightSegments; ++h)
    {
        const int32 V00 = EndCapVertices[h][0];     // 当前层内顶点
        const int32 V10 = EndCapVertices[h + 1][0]; // 下一层内顶点
        const int32 V01 = EndCapVertices[h][1];     // 当前层外顶点
        const int32 V11 = EndCapVertices[h + 1][1]; // 下一层外顶点
        
        // 添加两个等腰三角形
        Builder.AddTriangle(V00, V10, V01); // 第一个三角形
        Builder.AddTriangle(V01, V10, V11); // 第二个三角形
    }
}

// ============================================================================
// FChamferGeometryGenerator 实现
// ============================================================================

void FChamferGeometryGenerator::Generate(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    if (!Params.bEnableChamfer || Params.ChamferRadius <= 0.0f)
    {
        return;
    }

    // 生成收缩后的主面
    GenerateShrunkTopSurface(Builder, Params);
    GenerateShrunkBottomSurface(Builder, Params);
    
    // 生成倒角弧面
    GenerateTopChamferArc(Builder, Params);
    GenerateBottomChamferArc(Builder, Params);
}



void FChamferGeometryGenerator::GenerateShrunkTopSurface(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    const float HalfHeight = Params.Height / 2.0f;
    const float ChamferRadius = Params.ChamferRadius;
    const float TopRadius = Params.TopRadius;
    const float InnerRadius = TopRadius - ChamferRadius;
    const float InnerHeight = HalfHeight - ChamferRadius;
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.TopSides;
    
    // 中心顶点
    const int32 CenterVertex = Builder.AddVertex(
        FVector(0, 0, InnerHeight),
        FVector(0, 0, 1),
        FVector2D(0.5f, 0.5f)
    );

    // 收缩后的顶部顶点环
    TArray<int32> TopRing;
    TopRing.SetNum(Params.TopSides + 1);

    for (int32 s = 0; s <= Params.TopSides; ++s)
    {
        const float Angle = s * AngleStep;
        const float X = InnerRadius * FMath::Cos(Angle);
        const float Y = InnerRadius * FMath::Sin(Angle);

        // UV映射到圆形
        const float U = 0.5f + 0.5f * FMath::Cos(Angle);
        const float V = 0.5f + 0.5f * FMath::Sin(Angle);

        TopRing[s] = Builder.AddVertex(
            FVector(X, Y, InnerHeight),
            FVector(0, 0, 1),
            FVector2D(U, V)
        );
    }

    // 创建收缩后的顶部三角扇
    for (int32 s = 0; s < Params.TopSides; ++s)
    {
        Builder.AddTriangle(
            CenterVertex,
            TopRing[s + 1],
            TopRing[s]
        );
    }
}

void FChamferGeometryGenerator::GenerateShrunkBottomSurface(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    const float HalfHeight = Params.Height / 2.0f;
    const float ChamferRadius = Params.ChamferRadius;
    const float BottomRadius = Params.BottomRadius;
    const float InnerRadius = BottomRadius - ChamferRadius;
    const float InnerHeight = -HalfHeight; // 底部高度不变
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.BottomSides;
    
    // 中心顶点
    const int32 CenterVertex = Builder.AddVertex(
        FVector(0, 0, InnerHeight),
        FVector(0, 0, -1),
        FVector2D(0.5f, 0.5f)
    );

    // 收缩后的底部顶点环
    TArray<int32> BottomRing;
    BottomRing.SetNum(Params.BottomSides + 1);

    for (int32 s = 0; s <= Params.BottomSides; ++s)
    {
        const float Angle = s * AngleStep;
        const float X = InnerRadius * FMath::Cos(Angle);
        const float Y = InnerRadius * FMath::Sin(Angle);

        // UV映射到圆形
        const float U = 0.5f + 0.5f * FMath::Cos(Angle);
        const float V = 0.5f + 0.5f * FMath::Sin(Angle);

        BottomRing[s] = Builder.AddVertex(
            FVector(X, Y, InnerHeight),
            FVector(0, 0, -1),
            FVector2D(U, V)
        );
    }

    // 创建收缩后的底部三角扇
    for (int32 s = 0; s < Params.BottomSides; ++s)
    {
        Builder.AddTriangle(
            CenterVertex,
            BottomRing[s],
            BottomRing[s + 1]
        );
    }
}

void FChamferGeometryGenerator::GenerateTopChamferArc(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    const float HalfHeight = Params.Height / 2.0f;
    const float ChamferRadius = Params.ChamferRadius;
    const float TopRadius = Params.TopRadius;
    const float InnerRadius = TopRadius - ChamferRadius;
    const float InnerHeight = HalfHeight; // 顶部高度不变
    const float OuterHeight = HalfHeight + ChamferRadius; // 向外连接到侧面
    const int32 ChamferSegments = Params.ChamferSegments;
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.TopSides;
    
    // 生成倒角弧面的顶点环
    TArray<TArray<int32>> ArcRings;
    ArcRings.SetNum(ChamferSegments + 1);
    
    for (int32 c = 0; c <= ChamferSegments; ++c)
    {
        const float T = static_cast<float>(c) / ChamferSegments;
        const float CurrentRadius = FMath::Lerp(InnerRadius, TopRadius, T);
        const float CurrentHeight = FMath::Lerp(InnerHeight, OuterHeight, T);
        
        TArray<int32>& Ring = ArcRings[c];
        Ring.SetNum(Params.TopSides + 1);
        
        for (int32 s = 0; s <= Params.TopSides; ++s)
        {
            const float Angle = (Params.TopSides - s) * AngleStep;
            const float X = CurrentRadius * FMath::Cos(Angle);
            const float Y = CurrentRadius * FMath::Sin(Angle);
            const FVector Position(X, Y, CurrentHeight);
            
            // 计算法向量（指向外侧）
            const float NormalAngle = Angle + PI / 2;
            const FVector Normal = FVector(FMath::Cos(NormalAngle), FMath::Sin(NormalAngle), 0);
            
            // UV坐标
            const float U = static_cast<float>(s) / Params.TopSides;
            const float V = static_cast<float>(c) / ChamferSegments;
            const FVector2D UV(U, V);
            
            Ring[s] = Builder.AddVertex(Position, Normal, UV);
        }
    }
    
    // 生成倒角弧面的四边形
    for (int32 c = 0; c < ChamferSegments; ++c)
    {
        for (int32 s = 0; s < Params.TopSides; ++s)
        {
            const int32 V00 = ArcRings[c][s];
            const int32 V10 = ArcRings[c + 1][s];
            const int32 V01 = ArcRings[c][s + 1];
            const int32 V11 = ArcRings[c + 1][s + 1];
            
            Builder.AddQuad(V00, V10, V11, V01);
        }
    }
}

void FChamferGeometryGenerator::GenerateBottomChamferArc(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    const float HalfHeight = Params.Height / 2.0f;
    const float ChamferRadius = Params.ChamferRadius;
    const float BottomRadius = Params.BottomRadius;
    const float InnerRadius = BottomRadius - ChamferRadius;
    const float InnerHeight = -HalfHeight; // 底部高度不变
    const float OuterHeight = -HalfHeight + ChamferRadius; // 连接到侧面
    const int32 ChamferSegments = Params.ChamferSegments;
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.BottomSides;
    
    // 生成倒角弧面的顶点环
    TArray<TArray<int32>> ArcRings;
    ArcRings.SetNum(ChamferSegments + 1);
    
    for (int32 c = 0; c <= ChamferSegments; ++c)
    {
        const float T = static_cast<float>(c) / ChamferSegments;
        const float CurrentRadius = FMath::Lerp(InnerRadius, BottomRadius, T);
        const float CurrentHeight = FMath::Lerp(InnerHeight, OuterHeight, T); // 从底部表面向下连接到侧面
        
        TArray<int32>& Ring = ArcRings[c];
        Ring.SetNum(Params.BottomSides + 1);
        
        for (int32 s = 0; s <= Params.BottomSides; ++s)
        {
            const float Angle = (Params.BottomSides - s) * AngleStep;
            const float X = CurrentRadius * FMath::Cos(Angle);
            const float Y = CurrentRadius * FMath::Sin(Angle);
            const FVector Position(X, Y, CurrentHeight);
            
            // 计算法向量（指向外侧）
            const float NormalAngle = Angle + PI / 2;
            const FVector Normal = FVector(FMath::Cos(NormalAngle), FMath::Sin(NormalAngle), 0);
            
            // UV坐标
            const float U = static_cast<float>(s) / Params.BottomSides;
            const float V = static_cast<float>(c) / ChamferSegments;
            const FVector2D UV(U, V);
            
            Ring[s] = Builder.AddVertex(Position, Normal, UV);
        }
    }
    
    // 生成倒角弧面的四边形
    for (int32 c = 0; c < ChamferSegments; ++c)
    {
        for (int32 s = 0; s < Params.BottomSides; ++s)
        {
            const int32 V00 = ArcRings[c][s];
            const int32 V10 = ArcRings[c + 1][s];
            const int32 V01 = ArcRings[c][s + 1];
            const int32 V11 = ArcRings[c + 1][s + 1];
            
            Builder.AddQuad(V00, V10, V11, V01);
        }
    }
}

// ============================================================================
// FGeometryManager 实现
// ============================================================================

FGeometryManager::FGeometryManager()
{
    InitializeGenerators();
}

FGeometryManager::~FGeometryManager()
{
}

void FGeometryManager::InitializeGenerators()
{
    Generators.Add(MakeUnique<FChamferGeometryGenerator>());
    Generators.Add(MakeUnique<FSideGeometryGenerator>());
    Generators.Add(MakeUnique<FTopGeometryGenerator>());
    Generators.Add(MakeUnique<FBottomGeometryGenerator>());
    Generators.Add(MakeUnique<FEndCapGeometryGenerator>());
}

void FGeometryManager::GenerateFrustum(FMeshBuilder& Builder, const FFrustumParameters& Params)
{
    // 预估内存需求
    const int32 TotalSides = FMath::Max(Params.TopSides, Params.BottomSides);
    const int32 VertexCountEstimate =
        (Params.HeightSegments + 1) * (TotalSides + 1) * 4 +
        (Params.ArcAngle < 360.0f && Params.bEnableEndCaps ? 
            (Params.bFullCapCoverage ? Params.HeightSegments * 6 : Params.HeightSegments * 4) : 0);

    const int32 TriangleCountEstimate =
        Params.HeightSegments * TotalSides * 6 +
        (Params.ArcAngle < 360.0f && Params.bEnableEndCaps ? 
            (Params.bFullCapCoverage ? Params.HeightSegments * 12 : Params.HeightSegments * 6) : 0);

    Builder.Reserve(VertexCountEstimate, TriangleCountEstimate);

    // 使用所有生成器创建几何体
    for (const TUniquePtr<FGeometryGenerator>& Generator : Generators)
    {
        Generator->Generate(Builder, Params);
    }
}

// ============================================================================
// AFrustum 实现
// ============================================================================

AFrustum::AFrustum()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建网格组件
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FrustumMesh"));
    RootComponent = MeshComponent;

    // 配置网格属性
    MeshComponent->bUseAsyncCooking = true;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetSimulatePhysics(false);

    // 初始化几何管理器
    GeometryManager = MakeUnique<FGeometryManager>();
    MeshBuilder = MakeUnique<FMeshBuilder>();

    // 初始生成
    GenerateGeometry();
}

void AFrustum::BeginPlay()
{
    Super::BeginPlay();
    GenerateGeometry();
}

void AFrustum::PostLoad()
{
    Super::PostLoad();
    GenerateGeometry();
}

#if WITH_EDITOR
void AFrustum::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    static const TArray<FName> RelevantProperties = {
        "TopRadius", "BottomRadius", "Height",
        "TopSides", "BottomSides", "HeightSegments",

        "BendAmount", "MinBendRadius", "ArcAngle",
        "CapThickness", "bEnableEndCaps", "CapUVScale", "bFullCapCoverage",
        "ChamferRadius", "ChamferSegments", "bEnableChamfer"
    };

    if (RelevantProperties.Contains(PropertyName))
    {
        bGeometryDirty = true;
        GenerateGeometry();
    }
}
#endif

void AFrustum::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bGeometryDirty)
    {
        GenerateGeometry();
        bGeometryDirty = false;
    }
}

void AFrustum::Regenerate()
{
    GenerateGeometry();
}

void AFrustum::GenerateGeometry()
{
    if (!MeshComponent || !GeometryManager || !MeshBuilder)
    {
        UE_LOG(LogTemp, Error, TEXT("Required components are missing!"));
        return;
    }

    // 清除现有网格
    MeshComponent->ClearAllMeshSections();

    // 验证参数
    Parameters.TopRadius = FMath::Max(0.01f, Parameters.TopRadius);
    Parameters.BottomRadius = FMath::Max(0.01f, Parameters.BottomRadius);
    Parameters.Height = FMath::Max(0.01f, Parameters.Height);
    Parameters.TopSides = FMath::Max(3, Parameters.TopSides);
    Parameters.BottomSides = FMath::Max(3, Parameters.BottomSides);
    Parameters.HeightSegments = FMath::Max(1, Parameters.HeightSegments);

    Parameters.ArcAngle = FMath::Clamp(Parameters.ArcAngle, 0.0f, 360.0f);
    Parameters.MinBendRadius = FMath::Max(1.0f, Parameters.MinBendRadius);
    Parameters.CapThickness = FMath::Max(0.0f, Parameters.CapThickness);
    Parameters.CapUVScale = FMath::Max(0.1f, Parameters.CapUVScale);
    Parameters.ChamferRadius = FMath::Max(0.0f, Parameters.ChamferRadius);
    Parameters.ChamferSegments = FMath::Max(1, Parameters.ChamferSegments);

    // 确保顶部边数不超过底部
    Parameters.TopSides = FMath::Min(Parameters.TopSides, Parameters.BottomSides);

    // 清除构建器
    MeshBuilder->Clear();

    // 生成几何体
    GeometryManager->GenerateFrustum(*MeshBuilder, Parameters);

    // 构建网格
    MeshBuilder->BuildMesh(MeshComponent);

    // 应用材质
    ApplyMaterial();
}

void AFrustum::ApplyMaterial()
{
    static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (DefaultMaterial.Succeeded())
    {
        MeshComponent->SetMaterial(0, DefaultMaterial.Object);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to find default material. Using fallback."));

        // 创建简单回退材质
        UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        if (FallbackMaterial)
        {
            MeshComponent->SetMaterial(0, FallbackMaterial);
        }
    }
}
