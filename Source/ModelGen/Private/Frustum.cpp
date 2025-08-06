// Frustum.cpp

#include "Frustum.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

// FFrustumParameters 实现
bool FFrustumParameters::IsValid() const
{
    return TopRadius > 0.01f && 
           BottomRadius > 0.01f && 
           Height > 0.01f && 
           Sides >= 3 && 
           HeightSegments >= 1 &&
           ChamferRadius >= 0.0f &&
           ChamferSections >= 1 &&
           ArcAngle >= 0.0f && ArcAngle <= 360.0f &&
           MinBendRadius >= 0.0f &&
           CapThickness >= 0.0f;
}

void FFrustumParameters::GetEstimatedCounts(int32& OutVertexCount, int32& OutTriangleCount) const
{
    // 预估顶点数量
    OutVertexCount = (HeightSegments + 1) * (Sides + 1) * 4 +
                    (ArcAngle < 360.0f ? HeightSegments * 4 : 0);

    // 预估三角形数量
    OutTriangleCount = HeightSegments * Sides * 6 +
                      (ArcAngle < 360.0f ? HeightSegments * 6 : 0);
}

// FFrustumGeometry 实现
void FFrustumGeometry::Clear()
{
    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UVs.Reset();
    VertexColors.Reset();
    Tangents.Reset();
}

void FFrustumGeometry::Reserve(int32 VertexCount, int32 TriangleCount)
{
    Vertices.Reserve(VertexCount);
    Triangles.Reserve(TriangleCount);
    Normals.Reserve(VertexCount);
    UVs.Reserve(VertexCount);
    VertexColors.Reserve(VertexCount);
    Tangents.Reserve(VertexCount);
}

bool FFrustumGeometry::IsValid() const
{
    return Vertices.Num() > 0 && 
           Triangles.Num() > 0 && 
           Normals.Num() == Vertices.Num() &&
           UVs.Num() == Vertices.Num() &&
           VertexColors.Num() == Vertices.Num() &&
           Tangents.Num() == Vertices.Num();
}

void FFrustumGeometry::GetStats(int32& OutVertexCount, int32& OutTriangleCount) const
{
    OutVertexCount = Vertices.Num();
    OutTriangleCount = Triangles.Num() / 3;
}

// FFrustumBuilder::FBuildParameters 实现
bool FFrustumBuilder::FBuildParameters::IsValid() const
{
    return TopRadius > 0.01f && 
           BottomRadius > 0.01f && 
           Height > 0.01f && 
           Sides >= 3 && 
           HeightSegments >= 1 &&
           ChamferRadius >= 0.0f &&
           ChamferSections >= 1 &&
           ArcAngle >= 0.0f && ArcAngle <= 360.0f &&
           MinBendRadius >= 0.0f &&
           CapThickness >= 0.0f;
}

// FFrustumBuilder 实现
FFrustumBuilder::FFrustumBuilder(const FBuildParameters& InParams)
    : Params(InParams)
{
}

bool FFrustumBuilder::Generate(FFrustumGeometry& OutGeometry)
{
    if (!Params.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid build parameters for Frustum"));
        return false;
    }

    // 清除之前的几何数据
    OutGeometry.Clear();
    UniqueVerticesMap.Empty();

    // 生成共享的顶点环
    FVertexRings Rings;
    GenerateAllVertexRings(OutGeometry, Rings);

    // 生成各个几何部分
    GenerateSideGeometry(OutGeometry, Rings);
    GenerateTopGeometry(OutGeometry, Rings);
    GenerateBottomGeometry(OutGeometry, Rings);

    if (Params.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps(OutGeometry, Rings);
    }

    return OutGeometry.IsValid();
}

int32 FFrustumBuilder::GetOrAddVertex(FFrustumGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    int32* FoundIndex = UniqueVerticesMap.Find(Pos);
    if (FoundIndex)
    {
        return *FoundIndex;
    }

    int32 NewIndex = AddVertex(Geometry, Pos, Normal, UV);
    UniqueVerticesMap.Add(Pos, NewIndex);
    return NewIndex;
}

int32 FFrustumBuilder::AddVertex(FFrustumGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    Geometry.Vertices.Add(Pos);
    Geometry.Normals.Add(Normal);
    Geometry.UVs.Add(UV);
    Geometry.VertexColors.Add(FLinearColor::White);
    Geometry.Tangents.Add(FProcMeshTangent(CalculateTangent(Normal), false));

    return Geometry.Vertices.Num() - 1;
}

FVector FFrustumBuilder::CalculateTangent(const FVector& Normal) const
{
    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    TangentDirection.Normalize();
    return TangentDirection;
}

void FFrustumBuilder::AddQuad(FFrustumGeometry& Geometry, int32 V1, int32 V2, int32 V3, int32 V4)
{
    AddTriangle(Geometry, V1, V2, V3);
    AddTriangle(Geometry, V1, V3, V4);
}

void FFrustumBuilder::AddTriangle(FFrustumGeometry& Geometry, int32 V1, int32 V2, int32 V3)
{
    Geometry.Triangles.Add(V1);
    Geometry.Triangles.Add(V2);
    Geometry.Triangles.Add(V3);
}

float FFrustumBuilder::CalculateBentRadius(float BaseRadius, float Alpha) const
{
    const float BendFactor = FMath::Sin(Alpha * PI);
    return FMath::Max(
        BaseRadius + Params.BendAmount * BendFactor * BaseRadius,
        Params.MinBendRadius
    );
}

FVector FFrustumBuilder::CalculateBentNormal(const FVector& BaseNormal, float Alpha) const
{
    if (FMath::IsNearlyZero(Params.BendAmount))
    {
        return BaseNormal;
    }

    const float NormalZ = -Params.BendAmount * FMath::Cos(Alpha * PI);
    return (BaseNormal + FVector(0, 0, NormalZ)).GetSafeNormal();
}

void FFrustumBuilder::GenerateAllVertexRings(FFrustumGeometry& Geometry, FVertexRings& Rings)
{
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.Sides;
    const float HalfHeight = Params.GetHalfHeight();

    // 生成侧面顶点环
    if (Params.ChamferRadius > 0.0f && Params.ChamferSections > 0)
    {
        // 有倒角时的侧面环
        const float SideHeight = Params.GetSideHeight();
        if (SideHeight > 0)
        {
            const float HeightStep = SideHeight / Params.HeightSegments;
            const float StartZ = -HalfHeight + Params.GetBottomChamferHeight();

            Rings.SideRings.SetNum(Params.HeightSegments + 1);

            for (int32 h = 0; h <= Params.HeightSegments; ++h)
            {
                const float Z = StartZ + h * HeightStep;
                const float Alpha = (Z + HalfHeight) / Params.Height;
                const float Radius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha);
                const float BentRadius = CalculateBentRadius(Radius, Alpha);

                TArray<int32>& Ring = Rings.SideRings[h];
                Ring.SetNum(Params.Sides);

                for (int32 s = 0; s < Params.Sides; ++s)
                {
                    const float Angle = s * AngleStep;
                    const float X = BentRadius * FMath::Cos(Angle);
                    const float Y = BentRadius * FMath::Sin(Angle);

                    FVector Normal = FVector(X, Y, 0).GetSafeNormal();
                    Normal = CalculateBentNormal(Normal, Alpha);

                    const float U = static_cast<float>(s) / Params.Sides;
                    const float V = Alpha;

                    Ring[s] = GetOrAddVertex(Geometry, FVector(X, Y, Z), Normal, FVector2D(U, V));
                }
            }
        }

        // 生成倒角顶点环（这些将连接到内圆面）
        GenerateTopChamferGeometry(Geometry, Rings);
        GenerateBottomChamferGeometry(Geometry, Rings);
        
        // 创建内圆环顶点，用于生成内部面
        
        // 顶部内圆环
        if (Rings.TopInnerRing.Num() == 0)
        {
            const float TopInnerRadius = Params.GetTopInnerRadius();
            const float TopZ = HalfHeight;
            
            Rings.TopInnerRing.SetNum(Params.Sides);
            for (int32 s = 0; s < Params.Sides; ++s)
            {
                const float Angle = s * AngleStep;
                const float X = TopInnerRadius * FMath::Cos(Angle);
                const float Y = TopInnerRadius * FMath::Sin(Angle);
                
                const float U = static_cast<float>(s) / Params.Sides;
                const float V = 0.0f;
                
                Rings.TopInnerRing[s] = GetOrAddVertex(
                    Geometry, 
                    FVector(X, Y, TopZ), 
                    FVector(0, 0, 1), 
                    FVector2D(U, V)
                );
            }
        }
        
        // 底部内圆环
        if (Rings.BottomInnerRing.Num() == 0)
        {
            const float BottomInnerRadius = Params.GetBottomInnerRadius();
            const float BottomZ = -HalfHeight;
            
            Rings.BottomInnerRing.SetNum(Params.Sides);
            for (int32 s = 0; s < Params.Sides; ++s)
            {
                const float Angle = s * AngleStep;
                const float X = BottomInnerRadius * FMath::Cos(Angle);
                const float Y = BottomInnerRadius * FMath::Sin(Angle);
                
                const float U = static_cast<float>(s) / Params.Sides;
                const float V = 1.0f;
                
                Rings.BottomInnerRing[s] = GetOrAddVertex(
                    Geometry, 
                    FVector(X, Y, BottomZ), 
                    FVector(0, 0, -1), 
                    FVector2D(U, V)
                );
            }
        }
    }
    else
    {
        // 无倒角时，生成完整的侧面环
        const float HeightStep = Params.Height / Params.HeightSegments;

        Rings.SideRings.SetNum(Params.HeightSegments + 1);

        for (int32 h = 0; h <= Params.HeightSegments; ++h)
        {
            const float Z = -HalfHeight + h * HeightStep;
            const float Alpha = (Z + HalfHeight) / Params.Height;
            const float Radius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha);
            const float BentRadius = CalculateBentRadius(Radius, Alpha);

            TArray<int32>& Ring = Rings.SideRings[h];
            Ring.SetNum(Params.Sides);

            for (int32 s = 0; s < Params.Sides; ++s)
            {
                const float Angle = s * AngleStep;
                const float X = BentRadius * FMath::Cos(Angle);
                const float Y = BentRadius * FMath::Sin(Angle);

                FVector Normal = FVector(X, Y, 0).GetSafeNormal();
                Normal = CalculateBentNormal(Normal, Alpha);

                const float U = static_cast<float>(s) / Params.Sides;
                const float V = Alpha;

                Ring[s] = GetOrAddVertex(Geometry, FVector(X, Y, Z), Normal, FVector2D(U, V));
            }
        }

        // 顶部和底部环就是侧面环的首尾
        Rings.TopRing = Rings.SideRings.Last();
        Rings.BottomRing = Rings.SideRings[0];
    }
}

void FFrustumBuilder::GenerateSideGeometry(FFrustumGeometry& Geometry, const FVertexRings& Rings)
{
    if (Rings.SideRings.Num() < 2) return;

    // 生成侧面四边形，使用共享的顶点环
    for (int32 h = 0; h < Rings.SideRings.Num() - 1; ++h)
    {
        for (int32 s = 0; s < Params.Sides; ++s)
        {
            const int32 V00 = Rings.SideRings[h][s];
            const int32 V10 = Rings.SideRings[h + 1][s];
            const int32 V01 = Rings.SideRings[h][(s + 1) % Params.Sides];
            const int32 V11 = Rings.SideRings[h + 1][(s + 1) % Params.Sides];

            AddQuad(Geometry, V00, V10, V11, V01);
        }
    }
}

void FFrustumBuilder::GenerateTopGeometry(FFrustumGeometry& Geometry, const FVertexRings& Rings)
{
    // 如果有倒角，使用内圆环；否则使用侧面环的顶部
    const TArray<int32>& TopRing = (Params.ChamferRadius > 0.0f && Params.ChamferSections > 0) 
                                   ? Rings.TopInnerRing 
                                   : Rings.TopRing;

    if (TopRing.Num() < 3) return;

    // 创建多边形面（三角形扇形从第一个顶点开始）
    for (int32 s = 1; s < TopRing.Num() - 1; ++s)
    {
        AddTriangle(
            Geometry,
            TopRing[0],
            TopRing[s + 1],
            TopRing[s]
        );
    }
}

void FFrustumBuilder::GenerateTopChamferGeometry(FFrustumGeometry& Geometry, FVertexRings& Rings)
{
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.Sides;
    const float HalfHeight = Params.GetHalfHeight();
    const float ChamferRadius = Params.ChamferRadius;
    const int32 ChamferSections = Params.ChamferSections;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 倒角起点：主侧面几何体的最顶层边缘顶点
        const float StartZ = HalfHeight - Params.GetTopChamferHeight();
        const float Alpha_Height = (StartZ + HalfHeight) / Params.Height;
        const float RadiusAtZ = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Height);
        const float BentRadius = CalculateBentRadius(RadiusAtZ, Alpha_Height);
        const FVector ChamferStartPoint = FVector(BentRadius, 0, StartZ);

        // 倒角终点：在顶面上，半径减去倒角半径
        const float TopRadius = Params.GetTopInnerRadius();
        const FVector ChamferEndPoint = FVector(TopRadius, 0, HalfHeight);

        // 计算倒角弧线控制点
        FVector ControlPoint = FVector(ChamferStartPoint.X, 0, ChamferEndPoint.Z);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.Sides + 1);

        for (int32 s = 0; s <= Params.Sides; ++s)
        {
            const float angle = s * AngleStep;

            // 计算倒角弧线上的点
            const float t2 = alpha * alpha;
            const float mt = 1.0f - alpha;
            const float mt2 = mt * mt;
            
            const FVector LocalChamferPos = ChamferStartPoint * mt2 + 
                                          ControlPoint * (2.0f * mt * alpha) + 
                                          ChamferEndPoint * t2;
            
            const FVector Position = FVector(
                LocalChamferPos.X * FMath::Cos(angle),
                LocalChamferPos.X * FMath::Sin(angle),
                LocalChamferPos.Z
            );

            // 计算法线
            const FVector Tangent = (ChamferEndPoint - ChamferStartPoint) * (2.0f * alpha) + 
                                  (ControlPoint - ChamferStartPoint) * (2.0f * mt);
            const FVector Normal = FVector(
                Tangent.X * FMath::Cos(angle),
                Tangent.X * FMath::Sin(angle),
                Tangent.Z
            ).GetSafeNormal();

            const float U = static_cast<float>(s) / Params.Sides;
            const float V = (Position.Z + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Geometry, Position, Normal, FVector2D(U, V)));
        }

        if (i > 0)
        {
            for (int32 s = 0; s < Params.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                AddQuad(Geometry, V00, V10, V11, V01);
            }
        }
        PrevRing = CurrentRing;
        
        // 保存最后一个环作为内圆环（用于生成顶部面）
        if (i == ChamferSections)
        {
            Rings.TopInnerRing.SetNum(Params.Sides);
            for (int32 s = 0; s < Params.Sides; ++s)
            {
                Rings.TopInnerRing[s] = CurrentRing[s];
            }
        }
    }
}

void FFrustumBuilder::GenerateBottomGeometry(FFrustumGeometry& Geometry, const FVertexRings& Rings)
{
    // 如果有倒角，使用内圆环；否则使用侧面环的底部
    const TArray<int32>& BottomRing = (Params.ChamferRadius > 0.0f && Params.ChamferSections > 0) 
                                      ? Rings.BottomInnerRing 
                                      : Rings.BottomRing;

    if (BottomRing.Num() < 3) return;

    // 创建多边形面（三角形扇形从第一个顶点开始，注意底面的三角形顺序）
    for (int32 s = 1; s < BottomRing.Num() - 1; ++s)
    {
        AddTriangle(
            Geometry,
            BottomRing[0],
            BottomRing[s],
            BottomRing[s + 1]
        );
    }
}

void FFrustumBuilder::GenerateBottomChamferGeometry(FFrustumGeometry& Geometry, FVertexRings& Rings)
{
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.Sides;
    const float HalfHeight = Params.GetHalfHeight();
    const float ChamferRadius = Params.ChamferRadius;
    const int32 ChamferSections = Params.ChamferSections;

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 倒角起点：主侧面几何体的最底层边缘顶点
        const float StartZ = -HalfHeight + Params.GetBottomChamferHeight();
        const float Alpha_Height = (StartZ + HalfHeight) / Params.Height;
        const float RadiusAtZ = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Height);
        const float BentRadius = CalculateBentRadius(RadiusAtZ, Alpha_Height);
        const FVector ChamferStartPoint = FVector(BentRadius, 0, StartZ);

        // 倒角终点：在底面上，半径减去倒角半径
        const float BottomRadius = Params.GetBottomInnerRadius();
        const FVector ChamferEndPoint = FVector(BottomRadius, 0, -HalfHeight);

        // 计算倒角弧线控制点
        FVector ControlPoint = FVector(ChamferStartPoint.X, 0, ChamferEndPoint.Z);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.Sides + 1);

        for (int32 s = 0; s <= Params.Sides; ++s)
        {
            const float angle = s * AngleStep;

            // 计算倒角弧线上的点
            const float t2 = alpha * alpha;
            const float mt = 1.0f - alpha;
            const float mt2 = mt * mt;
            
            const FVector LocalChamferPos = ChamferStartPoint * mt2 + 
                                          ControlPoint * (2.0f * mt * alpha) + 
                                          ChamferEndPoint * t2;
            
            const FVector Position = FVector(
                LocalChamferPos.X * FMath::Cos(angle),
                LocalChamferPos.X * FMath::Sin(angle),
                LocalChamferPos.Z
            );

            // 计算法线
            const FVector Tangent = (ChamferEndPoint - ChamferStartPoint) * (2.0f * alpha) + 
                                  (ControlPoint - ChamferStartPoint) * (2.0f * mt);
            const FVector Normal = FVector(
                Tangent.X * FMath::Cos(angle),
                Tangent.X * FMath::Sin(angle),
                Tangent.Z
            ).GetSafeNormal();

            const float U = static_cast<float>(s) / Params.Sides;
            const float V = (Position.Z + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Geometry, Position, Normal, FVector2D(U, V)));
        }

        if (i > 0)
        {
            for (int32 s = 0; s < Params.Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];

                AddQuad(Geometry, V00, V01, V11, V10);
            }
        }
        PrevRing = CurrentRing;
        
        // 保存最后一个环作为内圆环（用于生成底部面）
        if (i == ChamferSections)
        {
            Rings.BottomInnerRing.SetNum(Params.Sides);
            for (int32 s = 0; s < Params.Sides; ++s)
            {
                Rings.BottomInnerRing[s] = CurrentRing[s];
            }
        }
    }
}

void FFrustumBuilder::GenerateEndCaps(FFrustumGeometry& Geometry, const FVertexRings& Rings)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float StartAngle = 0.0f;
    const float EndAngle = FMath::DegreesToRadians(Params.ArcAngle);

    const FVector StartNormal = FVector(FMath::Sin(StartAngle), -FMath::Cos(StartAngle), 0.0f);
    const FVector EndNormal = FVector(FMath::Sin(EndAngle), -FMath::Cos(EndAngle), 0.0f);

    TArray<FVector> StartCapVerticesOuter;
    TArray<FVector> StartCapVerticesInner;
    TArray<FVector> EndCapVerticesOuter;
    TArray<FVector> EndCapVerticesInner;

    // 生成端面顶点环
    for (int32 h = 0; h <= Params.HeightSegments; ++h)
    {
        const float Z = -HalfHeight + h * (Params.Height / Params.HeightSegments);
        const float Alpha = (Z + HalfHeight) / Params.Height;

        const float Radius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha);
        const float BentRadius = CalculateBentRadius(Radius, Alpha);

        const FVector StartEdgePos = FVector(BentRadius * FMath::Cos(StartAngle), BentRadius * FMath::Sin(StartAngle), Z);
        StartCapVerticesOuter.Add(StartEdgePos);
        const FVector EndEdgePos = FVector(BentRadius * FMath::Cos(EndAngle), BentRadius * FMath::Sin(EndAngle), Z);
        EndCapVerticesOuter.Add(EndEdgePos);

        const FVector StartInnerPos = FVector(0, 0, Z);
        StartCapVerticesInner.Add(StartInnerPos);
        const FVector EndInnerPos = FVector(0, 0, Z);
        EndCapVerticesInner.Add(EndInnerPos);
    }

    // 构建起始端面
    for (int32 h = 0; h < Params.HeightSegments; ++h)
    {
        const FVector2D UVCorners[4] = {
            FVector2D(0, (float)h / Params.HeightSegments),
            FVector2D(1, (float)h / Params.HeightSegments),
            FVector2D(0, (float)(h + 1) / Params.HeightSegments),
            FVector2D(1, (float)(h + 1) / Params.HeightSegments)
        };

        const int32 V1 = GetOrAddVertex(Geometry, StartCapVerticesOuter[h], StartNormal, UVCorners[0]);
        const int32 V2 = GetOrAddVertex(Geometry, StartCapVerticesInner[h], StartNormal, UVCorners[1]);
        const int32 V3 = GetOrAddVertex(Geometry, StartCapVerticesOuter[h + 1], StartNormal, UVCorners[2]);
        const int32 V4 = GetOrAddVertex(Geometry, StartCapVerticesInner[h + 1], StartNormal, UVCorners[3]);

        AddQuad(Geometry, V1, V2, V4, V3);
    }

    // 构建结束端面
    for (int32 h = 0; h < Params.HeightSegments; ++h)
    {
        const FVector2D UVCorners[4] = {
            FVector2D(0, (float)h / Params.HeightSegments),
            FVector2D(1, (float)h / Params.HeightSegments),
            FVector2D(0, (float)(h + 1) / Params.HeightSegments),
            FVector2D(1, (float)(h + 1) / Params.HeightSegments)
        };

        const int32 V1 = GetOrAddVertex(Geometry, EndCapVerticesOuter[h], EndNormal, UVCorners[0]);
        const int32 V2 = GetOrAddVertex(Geometry, EndCapVerticesInner[h], EndNormal, UVCorners[1]);
        const int32 V3 = GetOrAddVertex(Geometry, EndCapVerticesOuter[h + 1], EndNormal, UVCorners[2]);
        const int32 V4 = GetOrAddVertex(Geometry, EndCapVerticesInner[h + 1], EndNormal, UVCorners[3]);

        AddQuad(Geometry, V1, V3, V4, V2);
    }
}

// AFrustum 实现
AFrustum::AFrustum()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建网格组件
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FrustumMesh"));
    RootComponent = MeshComponent;

    // 配置网格属性
    MeshComponent->bUseAsyncCooking = bUseAsyncCooking;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetSimulatePhysics(false);

    // 初始生成
    GenerateGeometry();
}

void AFrustum::BeginPlay()
{
    Super::BeginPlay();
    GenerateGeometry();
}

void AFrustum::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerateGeometry();
}

#if WITH_EDITOR
void AFrustum::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    static const TArray<FName> RelevantProperties = {
        "TopRadius", "BottomRadius", "Height",
        "Sides", "HeightSegments",
        "ChamferRadius", "ChamferSections",
        "BendAmount", "MinBendRadius", "ArcAngle",
        "CapThickness"
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

void AFrustum::RegenerateMesh()
{
    Regenerate();
}

bool AFrustum::IsGeometryValid() const
{
    return CurrentGeometry.IsValid();
}

void AFrustum::GetGeometryStats(int32& OutVertexCount, int32& OutTriangleCount) const
{
    CurrentGeometry.GetStats(OutVertexCount, OutTriangleCount);
}

void AFrustum::SetupCollision()
{
    if (MeshComponent)
    {
        MeshComponent->bUseAsyncCooking = bUseAsyncCooking;
        
        if (bGenerateCollision)
        {
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
        else
        {
            MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        
        MeshComponent->SetSimulatePhysics(false);
    }
}

void AFrustum::GenerateGeometry()
{
    if (!MeshComponent)
    {
        LogError(TEXT("Mesh component is missing!"));
        return;
    }

    if (!ValidateParameters())
    {
        LogError(TEXT("Invalid parameters!"));
        return;
    }

    // 清除现有网格
    MeshComponent->ClearAllMeshSections();

    // 创建几何生成器
    FFrustumBuilder::FBuildParameters BuildParams;
    BuildParams.TopRadius = Parameters.TopRadius;
    BuildParams.BottomRadius = Parameters.BottomRadius;
    BuildParams.Height = Parameters.Height;
    BuildParams.Sides = Parameters.Sides;
    BuildParams.HeightSegments = Parameters.HeightSegments;
    BuildParams.ChamferRadius = Parameters.ChamferRadius;
    BuildParams.ChamferSections = Parameters.ChamferSections;
    BuildParams.BendAmount = Parameters.BendAmount;
    BuildParams.MinBendRadius = Parameters.MinBendRadius;
    BuildParams.ArcAngle = Parameters.ArcAngle;
    BuildParams.CapThickness = Parameters.CapThickness;

    GeometryBuilder = MakeUnique<FFrustumBuilder>(BuildParams);

    // 生成新几何体
    if (GeometryBuilder->Generate(CurrentGeometry))
    {
        // 提交网格
        if (CurrentGeometry.Vertices.Num() > 0)
        {
            MeshComponent->CreateMeshSection_LinearColor(
                0,
                CurrentGeometry.Vertices,
                CurrentGeometry.Triangles,
                CurrentGeometry.Normals,
                CurrentGeometry.UVs,
                CurrentGeometry.VertexColors,
                CurrentGeometry.Tangents,
                bGenerateCollision
            );

            ApplyMaterial();
        }
        else
        {
            LogError(TEXT("Generated frustum mesh has no vertices"));
        }
    }
    else
    {
        LogError(TEXT("Geometry generation failed"));
    }
}

void AFrustum::ApplyMaterial()
{
    if (Material)
    {
        MeshComponent->SetMaterial(0, Material);
    }
    else
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
}

bool AFrustum::ValidateParameters() const
{
    return Parameters.IsValid();
}

void AFrustum::LogError(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("Frustum: %s"), *ErrorMessage);
}
