// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"
#include "Frustum.h"
#include "ModelGenMeshData.h"

// 全局 UV 缩放因子 (100 units = 1 UV tile)
static const float GLOBAL_UV_SCALE = 0.01f;

FFrustumBuilder::FFrustumBuilder(const AFrustum& InFrustum)
    : Frustum(InFrustum)
{
    Clear();
}

void FFrustumBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();

    TopSideRing.Empty();
    BottomSideRing.Empty();
    TopCapRing.Empty();
    BottomCapRing.Empty();

    StartSliceIndices.Empty();
    EndSliceIndices.Empty();
}

bool FFrustumBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Frustum.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    const float MinDimension = FMath::Min(Frustum.TopRadius, Frustum.BottomRadius);
    bEnableBevel = (Frustum.BevelRadius > KINDA_SMALL_NUMBER) &&
        (Frustum.BevelSegments > 0) &&
        (MinDimension > KINDA_SMALL_NUMBER);

    CalculateCommonParams();

    GenerateSides();

    if (bEnableBevel)
    {
        GenerateBevels();
    }
    else
    {
        TopCapRing = TopSideRing;
        BottomCapRing = BottomSideRing;
    }

    GenerateCaps();

    GenerateCutPlanes();

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;
    return true;
}

int32 FFrustumBuilder::CalculateVertexCountEstimate() const
{
    return Frustum.CalculateVertexCountEstimate();
}

int32 FFrustumBuilder::CalculateTriangleCountEstimate() const
{
    return Frustum.CalculateTriangleCountEstimate();
}

void FFrustumBuilder::CalculateCommonParams()
{
    ArcAngleRadians = FMath::DegreesToRadians(Frustum.ArcAngle);
    StartAngle = -ArcAngleRadians / 2.0f;
}

void FFrustumBuilder::StitchRings(const TArray<int32>& RingA, const TArray<int32>& RingB)
{
    if (RingA.Num() < 2 || RingB.Num() < 2) return;

    const int32 CountA = RingA.Num();
    const int32 CountB = RingB.Num();

    for (int32 i = 0; i < CountA - 1; ++i)
    {
        const float RatioCurrent = static_cast<float>(i) / (CountA - 1);
        const float RatioNext = static_cast<float>(i + 1) / (CountA - 1);

        const int32 IndexB_Current = FMath::Clamp(FMath::RoundToInt(RatioCurrent * (CountB - 1)), 0, CountB - 1);
        const int32 IndexB_Next = FMath::Clamp(FMath::RoundToInt(RatioNext * (CountB - 1)), 0, CountB - 1);

        AddQuad(RingA[i], RingB[IndexB_Current], RingB[IndexB_Next], RingA[i + 1]);
    }
}

TArray<FVector2D> FFrustumBuilder::GetRingPos2D(float Radius, int32 Sides) const
{
    TArray<FVector2D> Positions;
    Positions.Reserve(Sides + 1);

    const float Step = (Sides > 0) ? (ArcAngleRadians / Sides) : 0.0f;

    for (int32 i = 0; i <= Sides; ++i)
    {
        float Angle = StartAngle + i * Step;
        Positions.Add(FVector2D(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle)));
    }
    return Positions;
}

void FFrustumBuilder::GenerateSides()
{
    const float HalfHeight = Frustum.GetHalfHeight();

    // V 坐标累加器 (世界单位)
    float CurrentV = 0.0f;

    float TopZ = HalfHeight;
    float BottomZ = -HalfHeight;

    const float TopR = Frustum.TopRadius;
    const float BottomR = Frustum.BottomRadius;

    if (bEnableBevel)
    {
        const float MinRadius = FMath::Min(Frustum.TopRadius, Frustum.BottomRadius);
        const float EffectiveBevel = FMath::Min(Frustum.BevelRadius, MinRadius);

        TopZ -= EffectiveBevel;
        BottomZ += EffectiveBevel;

        // 如果有底部倒角，侧壁从倒角上方开始，V 偏移为倒角弧长
        CurrentV = (PI * EffectiveBevel) * 0.5f;
    }

    TArray<FVector2D> BottomRef = GetRingPos2D(BottomR, Frustum.BottomSides);
    TArray<FVector2D> TopRef = GetRingPos2D(TopR, Frustum.TopSides);

    const int32 Segments = FMath::Max(1, Frustum.HeightSegments + 1);
    TArray<TArray<int32>> Rings;
    Rings.Reserve(Segments + 1);

    float PrevRadius = 0.0f;
    float PrevZ = 0.0f;

    for (int32 h = 0; h <= Segments; ++h)
    {
        const float Alpha = static_cast<float>(h) / Segments;
        const float CurrentZ = FMath::Lerp(BottomZ, TopZ, Alpha);
        const float HeightRatio = (CurrentZ + Frustum.GetHalfHeight()) / Frustum.Height;

        // 计算插值半径用于 UV V 增量计算
        const float CurrentBaseRadius = FMath::Lerp(BottomR, TopR, Alpha);

        if (h > 0)
        {
            // 累加斜边距离
            const float dZ = CurrentZ - PrevZ;
            const float dR = CurrentBaseRadius - PrevRadius;
            const float SlantDist = FMath::Sqrt(dZ * dZ + dR * dR);
            CurrentV += SlantDist;
        }

        PrevZ = CurrentZ;
        PrevRadius = CurrentBaseRadius;

        int32 CurrentSides = (h == Segments) ? Frustum.TopSides : Frustum.BottomSides;
        TArray<int32> CurrentRingIndices;
        CurrentRingIndices.Reserve(CurrentSides + 1);

        const float AngleStep = (CurrentSides > 0) ? (ArcAngleRadians / CurrentSides) : 0.0f;

        for (int32 i = 0; i <= CurrentSides; ++i)
        {
            FVector FinalPos;
            FVector Normal;

            if (h == Segments)
            {
                FVector2D P = TopRef[FMath::Clamp(i, 0, TopRef.Num() - 1)];
                FinalPos = FVector(P.X, P.Y, CurrentZ);
                Normal = FVector(P.X, P.Y, 0.0f).GetSafeNormal();
            }
            else
            {
                FVector2D PosStart = BottomRef[i];
                float Ratio = static_cast<float>(i) / Frustum.BottomSides;
                int32 TopIndex = FMath::Clamp(FMath::RoundToInt(Ratio * Frustum.TopSides), 0, Frustum.TopSides);
                FVector2D PosEnd = TopRef[TopIndex];

                FVector2D LerpedPos = FMath::Lerp(PosStart, PosEnd, Alpha);
                FinalPos = FVector(LerpedPos.X, LerpedPos.Y, CurrentZ);
                Normal = FVector(LerpedPos.X, LerpedPos.Y, 0.0f).GetSafeNormal();
            }

            float CurrentRadius = FVector2D(FinalPos.X, FinalPos.Y).Size();
            FinalPos = ApplyBend(FinalPos, CurrentRadius, HeightRatio);

            if (FMath::Abs(Frustum.BendAmount) > KINDA_SMALL_NUMBER)
            {
                float NormalZ = Frustum.BendAmount * FMath::Cos(HeightRatio * PI);
                Normal.Z += NormalZ;
                Normal.Normalize();
            }

            // 【UV 缩放】：U = 弧长 * Scale, V = 距离 * Scale
            float FinalRadius = FVector2D(FinalPos.X, FinalPos.Y).Size();
            float CurrentAngle = StartAngle + i * AngleStep;
            float U = (CurrentAngle - StartAngle) * FinalRadius;

            FVector2D UV(U * GLOBAL_UV_SCALE, CurrentV * GLOBAL_UV_SCALE);

            CurrentRingIndices.Add(GetOrAddVertex(FinalPos, Normal, UV));
        }

        Rings.Add(CurrentRingIndices);

        if (CurrentRingIndices.Num() > 0)
        {
            StartSliceIndices.Add(CurrentRingIndices[0]);
            EndSliceIndices.Add(CurrentRingIndices.Last());
        }
    }

    if (Rings.Num() > 0)
    {
        BottomSideRing = Rings[0];
        TopSideRing = Rings.Last();
    }

    for (int32 i = 0; i < Rings.Num() - 1; ++i)
    {
        StitchRings(Rings[i], Rings[i + 1]);
    }
}

void FFrustumBuilder::GenerateBevels()
{
    const float HalfHeight = Frustum.GetHalfHeight();
    const float MinRadius = FMath::Min(Frustum.TopRadius, Frustum.BottomRadius);
    const float BevelR = FMath::Min(Frustum.BevelRadius, MinRadius);
    const int32 Segments = Frustum.BevelSegments;

    const float BevelArcLength = (PI * BevelR) * 0.5f;
    const float V_Step = BevelArcLength / Segments;

    // 计算侧壁的 V 长度，用于顶部倒角的起始 V
    float SideSlantH = Frustum.Height - 2.0f * BevelR;
    float SideSlantR = Frustum.TopRadius - Frustum.BottomRadius;
    float SideVLength = FMath::Sqrt(SideSlantH * SideSlantH + SideSlantR * SideSlantR);
    float BottomBevelArc = (PI * BevelR) * 0.5f;

    // --- Top Bevel ---
    TArray<int32> PreviousTopRing = TopSideRing;
    const float ArcCenterZ = HalfHeight - BevelR;
    const float ArcCenterR = Frustum.TopRadius - BevelR;

    float CurrentV_Top = BottomBevelArc + SideVLength;

    for (int32 i = 1; i <= Segments; ++i)
    {
        CurrentV_Top += V_Step;

        const float Alpha = static_cast<float>(i) / Segments;
        const float Angle = Alpha * HALF_PI;

        FRingContext Ctx;
        Ctx.Z = ArcCenterZ + BevelR * FMath::Sin(Angle);
        Ctx.Radius = ArcCenterR + BevelR * FMath::Cos(Angle);
        Ctx.Sides = Frustum.TopSides;

        TArray<int32> CurrentRing = CreateVertexRing(Ctx, CurrentV_Top);

        StitchRings(PreviousTopRing, CurrentRing);

        if (CurrentRing.Num() > 0) {
            StartSliceIndices.Add(CurrentRing[0]);
            EndSliceIndices.Add(CurrentRing.Last());
        }

        PreviousTopRing = CurrentRing;
    }
    TopCapRing = PreviousTopRing;

    // --- Bottom Bevel ---
    TArray<int32> PreviousBottomRing = BottomSideRing;
    const float BottomArcCenterZ = -HalfHeight + BevelR;
    const float BottomArcCenterR = Frustum.BottomRadius - BevelR;

    float CurrentV_Bottom = BottomBevelArc;

    for (int32 i = 1; i <= Segments; ++i)
    {
        CurrentV_Bottom -= V_Step;

        const float Alpha = static_cast<float>(i) / Segments;
        const float Angle = Alpha * HALF_PI;

        FRingContext Ctx;
        Ctx.Z = BottomArcCenterZ - BevelR * FMath::Sin(Angle);
        Ctx.Radius = BottomArcCenterR + BevelR * FMath::Cos(Angle);
        Ctx.Sides = Frustum.BottomSides;

        TArray<int32> CurrentRing = CreateVertexRing(Ctx, CurrentV_Bottom);

        StitchRings(CurrentRing, PreviousBottomRing);

        if (CurrentRing.Num() > 0) {
            StartSliceIndices.Insert(CurrentRing[0], 0);
            EndSliceIndices.Insert(CurrentRing.Last(), 0);
        }

        PreviousBottomRing = CurrentRing;
    }
    BottomCapRing = PreviousBottomRing;
}

void FFrustumBuilder::GenerateCaps()
{
    if (TopCapRing.Num() >= 3 && Frustum.TopRadius > KINDA_SMALL_NUMBER)
    {
        CreateCapDisk(Frustum.GetHalfHeight(), TopCapRing, true);
    }

    if (BottomCapRing.Num() >= 3 && Frustum.BottomRadius > KINDA_SMALL_NUMBER)
    {
        CreateCapDisk(-Frustum.GetHalfHeight(), BottomCapRing, false);
    }
}

void FFrustumBuilder::CreateCapDisk(float Z, const TArray<int32>& BoundaryRing, bool bIsTop)
{
    FVector CenterPos(0.0f, 0.0f, Z);
    if (FMath::Abs(Frustum.BendAmount) > KINDA_SMALL_NUMBER)
    {
        float H = (Z + Frustum.GetHalfHeight()) / Frustum.Height;
        CenterPos = ApplyBend(CenterPos, 0.0f, H);
    }

    FVector Normal(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);

    // 【UV 缩放】：World XY * Scale
    FVector2D CenterUV(CenterPos.X * GLOBAL_UV_SCALE, CenterPos.Y * GLOBAL_UV_SCALE);
    int32 CenterIndex = AddVertex(CenterPos, Normal, CenterUV);

    TArray<int32> CapVertices;
    CapVertices.Reserve(BoundaryRing.Num());

    for (int32 SrcIdx : BoundaryRing)
    {
        FVector Pos = GetPosByIndex(SrcIdx);
        // 【UV 缩放】：World XY * Scale
        FVector2D UV(Pos.X * GLOBAL_UV_SCALE, Pos.Y * GLOBAL_UV_SCALE);

        CapVertices.Add(AddVertex(Pos, Normal, UV));
    }

    for (int32 i = 0; i < CapVertices.Num() - 1; ++i)
    {
        if (bIsTop)
        {
            AddTriangle(CenterIndex, CapVertices[i + 1], CapVertices[i]);
        }
        else
        {
            AddTriangle(CenterIndex, CapVertices[i], CapVertices[i + 1]);
        }
    }
}

void FFrustumBuilder::GenerateCutPlanes()
{
    if (Frustum.ArcAngle >= 360.0f - 0.01f)
    {
        return;
    }

    CreateCutPlaneSurface(StartAngle, StartSliceIndices, true);

    const float EndAngleVal = StartAngle + ArcAngleRadians;
    CreateCutPlaneSurface(EndAngleVal, EndSliceIndices, false);
}

void FFrustumBuilder::CreateCutPlaneSurface(float Angle, const TArray<int32>& ProfileIndices, bool bIsStartFace)
{
    if (ProfileIndices.Num() < 2) return;

    TArray<int32> SortedIndices = ProfileIndices;
    SortedIndices.Sort([this](const int32& A, const int32& B) {
        return GetPosByIndex(A).Z < GetPosByIndex(B).Z;
        });

    const float NormalAngle = Angle + (bIsStartFace ? -HALF_PI : HALF_PI);
    const FVector PlaneNormal(FMath::Cos(NormalAngle), FMath::Sin(NormalAngle), 0.0f);

    for (int32 i = 0; i < SortedIndices.Num() - 1; ++i)
    {
        int32 Idx1 = SortedIndices[i];
        int32 Idx2 = SortedIndices[i + 1];

        FVector P1_Outer = GetPosByIndex(Idx1);
        FVector P2_Outer = GetPosByIndex(Idx2);

        FVector P1_Inner(0.0f, 0.0f, P1_Outer.Z);
        FVector P2_Inner(0.0f, 0.0f, P2_Outer.Z);

        // 【UV 缩放】：World Coordinates * Scale
        auto GetCutUV = [&](const FVector& P) {
            float R = FVector2D(P.X, P.Y).Size();
            return FVector2D(R * GLOBAL_UV_SCALE, P.Z * GLOBAL_UV_SCALE);
            };

        int32 V_In1 = AddVertex(P1_Inner, PlaneNormal, GetCutUV(P1_Inner));
        int32 V_Out1 = AddVertex(P1_Outer, PlaneNormal, GetCutUV(P1_Outer));
        int32 V_Out2 = AddVertex(P2_Outer, PlaneNormal, GetCutUV(P2_Outer));
        int32 V_In2 = AddVertex(P2_Inner, PlaneNormal, GetCutUV(P2_Inner));

        if (bIsStartFace)
        {
            AddQuad(V_In1, V_In2, V_Out2, V_Out1);
        }
        else
        {
            AddQuad(V_In1, V_Out1, V_Out2, V_In2);
        }
    }
}

TArray<int32> FFrustumBuilder::CreateVertexRing(const FRingContext& Context, float VCoord)
{
    TArray<int32> Indices;
    Indices.Reserve(Context.Sides + 1);

    const float HeightRatio = (Context.Z + Frustum.GetHalfHeight()) / Frustum.Height;
    const float AngleStep = (Context.Sides > 0) ? (ArcAngleRadians / Context.Sides) : 0.0f;

    for (int32 i = 0; i <= Context.Sides; ++i)
    {
        const float Angle = StartAngle + (i * AngleStep);
        float SinA, CosA;
        FMath::SinCos(&SinA, &CosA, Angle);

        FVector Pos(Context.Radius * CosA, Context.Radius * SinA, Context.Z);

        Pos = ApplyBend(Pos, Context.Radius, HeightRatio);

        FVector Normal(CosA, SinA, 0.0f);

        if (FMath::Abs(Frustum.BendAmount) > KINDA_SMALL_NUMBER)
        {
            float NormalZ = Frustum.BendAmount * FMath::Cos(HeightRatio * PI);
            Normal.Z += NormalZ;
            Normal.Normalize();
        }

        // U = ArcLength = Angle * Radius
        float CurrentRadius = FVector2D(Pos.X, Pos.Y).Size();
        float U = (Angle - StartAngle) * CurrentRadius;

        // 【UV 缩放】：Scale applied here
        FVector2D UV(U * GLOBAL_UV_SCALE, VCoord * GLOBAL_UV_SCALE);

        Indices.Add(GetOrAddVertex(Pos, Normal, UV));
    }

    return Indices;
}

FVector FFrustumBuilder::ApplyBend(const FVector& BasePos, float BaseRadius, float HeightRatio) const
{
    if (FMath::Abs(Frustum.BendAmount) < KINDA_SMALL_NUMBER)
    {
        return BasePos;
    }

    if (BaseRadius < KINDA_SMALL_NUMBER)
    {
        return BasePos;
    }

    const float BendFactor = FMath::Sin(HeightRatio * PI);

    float BentRadius = BaseRadius * (1.0f - Frustum.BendAmount * BendFactor);

    const bool bIsCapRing = (HeightRatio < KINDA_SMALL_NUMBER) || (HeightRatio > (1.0f - KINDA_SMALL_NUMBER));

    if (!bIsCapRing && Frustum.MinBendRadius > KINDA_SMALL_NUMBER)
    {
        BentRadius = FMath::Max(BentRadius, Frustum.MinBendRadius);
    }

    const float Scale = BentRadius / BaseRadius;
    return FVector(BasePos.X * Scale, BasePos.Y * Scale, BasePos.Z);
}