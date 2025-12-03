// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"
#include "Frustum.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"

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

TArray<int32> FFrustumBuilder::CreateBevelRing(const FRingContext& Context, float VCoord, float NormalAlpha, bool bIsTopBevel, float OverrideRadius)
{
    TArray<int32> Indices;
    Indices.Reserve(Context.Sides + 1);

    const float HeightRatio = Context.Z / Frustum.Height;
    const float AngleStep = (Context.Sides > 0) ? (ArcAngleRadians / Context.Sides) : 0.0f;

    FVector VerticalNormal(0.0f, 0.0f, bIsTopBevel ? 1.0f : -1.0f);

    for (int32 i = 0; i <= Context.Sides; ++i)
    {
        const float Angle = StartAngle + (i * AngleStep);
        float SinA, CosA;
        FMath::SinCos(&SinA, &CosA, Angle);

        FVector Pos(Context.Radius * CosA, Context.Radius * SinA, Context.Z);
        Pos = ApplyBend(Pos, Context.Radius, HeightRatio);

        FVector HorizontalNormal(CosA, SinA, 0.0f);

        if (FMath::Abs(Frustum.BendAmount) > KINDA_SMALL_NUMBER)
        {
            float BendNormalZ = Frustum.BendAmount * FMath::Cos(HeightRatio * PI);
            HorizontalNormal.Z += BendNormalZ;
            HorizontalNormal.Normalize();
        }

        FVector SmoothNormal = FMath::Lerp(HorizontalNormal, VerticalNormal, NormalAlpha).GetSafeNormal();

        float UVRadius = (OverrideRadius > 0.0f) ? OverrideRadius : FVector2D(Pos.X, Pos.Y).Size();
        float U = (Angle - StartAngle) * UVRadius;
        FVector2D UV(U * ModelGenConstants::GLOBAL_UV_SCALE, VCoord * ModelGenConstants::GLOBAL_UV_SCALE);

        Indices.Add(GetOrAddVertex(Pos, SmoothNormal, UV));
    }

    return Indices;
}

void FFrustumBuilder::GenerateSides()
{
    float TopZ = Frustum.Height;
    float BottomZ = 0.0f;
    float TopR = Frustum.TopRadius;
    float BottomR = Frustum.BottomRadius;

    float TopBevelHeight = 0.0f;
    float BottomBevelHeight = 0.0f;
    float TopBevelArc = 0.0f;

    if (bEnableBevel)
    {
        TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
        BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);

        // 【核心修复】：恢复旧版逻辑，沿着侧边棱移动计算倒角起点
        const float RadiusDiff = Frustum.TopRadius - Frustum.BottomRadius;
        const float SideLength = FMath::Sqrt(RadiusDiff * RadiusDiff + Frustum.Height * Frustum.Height);

        if (SideLength > KINDA_SMALL_NUMBER)
        {
            const float RadiusDir = RadiusDiff / SideLength;
            const float HeightDir = Frustum.Height / SideLength;

            // 顶部：沿着棱向下移动
            TopR = Frustum.TopRadius - TopBevelHeight * RadiusDir;
            TopZ = Frustum.Height - TopBevelHeight * HeightDir;

            // 底部：沿着棱向上移动
            BottomR = Frustum.BottomRadius + BottomBevelHeight * RadiusDir;
            BottomZ = BottomBevelHeight * HeightDir;
        }
        else
        {
            // 圆柱体情况
            const float MinRadius = FMath::Min(Frustum.TopRadius, Frustum.BottomRadius);
            const float EffectiveBevel = FMath::Min(Frustum.BevelRadius, MinRadius);
            TopZ -= EffectiveBevel;
            BottomZ += EffectiveBevel;
        }

        // 倒角弧长 (用于UV偏移)
        TopBevelArc = (PI * TopBevelHeight) * 0.5f;
    }

    // 计算侧面几何长度
    float SideHeight = TopZ - BottomZ;
    float SideRadiusDiff = TopR - BottomR;
    float TotalSideLength = FMath::Sqrt(SideHeight * SideHeight + SideRadiusDiff * SideRadiusDiff);

    // V 从高值开始递减
    float CurrentV = TopBevelArc + TotalSideLength;

    // 统一 UV 参考半径
    float UVReferenceRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);

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
        const float HeightRatio = CurrentZ / Frustum.Height;

        const float CurrentBaseRadius = FMath::Lerp(BottomR, TopR, Alpha);

        if (h > 0)
        {
            const float dZ = CurrentZ - PrevZ;
            const float dR = CurrentBaseRadius - PrevRadius;
            const float SlantDist = FMath::Sqrt(dZ * dZ + dR * dR);
            CurrentV -= SlantDist;
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

            float CurrentAngle = StartAngle + i * AngleStep;
            float U = (CurrentAngle - StartAngle) * UVReferenceRadius;
            FVector2D UV(U * ModelGenConstants::GLOBAL_UV_SCALE, CurrentV * ModelGenConstants::GLOBAL_UV_SCALE);

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
    const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
    const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);
    const float MinRadius = FMath::Min(Frustum.TopRadius, Frustum.BottomRadius);
    const float BevelR = FMath::Min(Frustum.BevelRadius, MinRadius);
    const int32 Segments = Frustum.BevelSegments;

    const float BevelArcLength = (PI * BevelR) * 0.5f;
    const float V_Step = BevelArcLength / Segments;

    float UVReferenceRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);

    // 计算侧边棱信息 (用于几何计算)
    const float RadiusDiff = Frustum.TopRadius - Frustum.BottomRadius;
    const float SideLength = FMath::Sqrt(RadiusDiff * RadiusDiff + Frustum.Height * Frustum.Height);

    // 计算 WallTopZ 和 WallTopR（在 Top Bevel 和 Bottom Bevel 中都需要使用）
    float WallTopZ, WallTopR;
    if (SideLength > KINDA_SMALL_NUMBER)
    {
        const float HeightDir = Frustum.Height / SideLength;
        const float RadiusDir = RadiusDiff / SideLength;
        WallTopZ = Frustum.Height - TopBevelHeight * HeightDir;
        WallTopR = Frustum.TopRadius - TopBevelHeight * RadiusDir;
    }
    else
    {
        WallTopZ = Frustum.Height - TopBevelHeight;
        WallTopR = Frustum.TopRadius - TopBevelHeight;
    }

    // --- Top Bevel ---
    {
        TArray<int32> PreviousTopRing = TopSideRing;
        float CapTopR = Frustum.TopRadius - TopBevelHeight;

        float CurrentV_Top = (PI * TopBevelHeight) * 0.5f;

        for (int32 i = 1; i <= Segments; ++i)
        {
            CurrentV_Top -= V_Step;

            const float Alpha = static_cast<float>(i) / Segments;
            const float Angle = Alpha * HALF_PI;

            float DiffR = WallTopR - CapTopR;
            float CurrentR = CapTopR + DiffR * FMath::Cos(Angle);
            float CurrentZ = WallTopZ + TopBevelHeight * FMath::Sin(Angle);

            FRingContext Ctx;
            Ctx.Z = CurrentZ;
            Ctx.Radius = CurrentR;
            Ctx.Sides = Frustum.TopSides;

            TArray<int32> CurrentRing = CreateBevelRing(Ctx, CurrentV_Top, Alpha, true, UVReferenceRadius);

            StitchRings(PreviousTopRing, CurrentRing);

            if (CurrentRing.Num() > 0) {
                StartSliceIndices.Add(CurrentRing[0]);
                EndSliceIndices.Add(CurrentRing.Last());
            }

            PreviousTopRing = CurrentRing;
        }
        TopCapRing = PreviousTopRing;
    }

    // --- Bottom Bevel ---
    {
        TArray<int32> PreviousBottomRing = BottomSideRing;

        // 【核心修复】：恢复旧版几何计算
        float WallBotZ, WallBotR;
        if (SideLength > KINDA_SMALL_NUMBER)
        {
            const float HeightDir = Frustum.Height / SideLength;
            const float RadiusDir = RadiusDiff / SideLength;
            WallBotZ = BottomBevelHeight * HeightDir;
            WallBotR = Frustum.BottomRadius + BottomBevelHeight * RadiusDir;
        }
        else
        {
            WallBotZ = BottomBevelHeight;
            WallBotR = Frustum.BottomRadius + BottomBevelHeight;
        }
        float CapBotR = Frustum.BottomRadius - BottomBevelHeight;

        // V 计算
        const float TopBevelArc = (PI * TopBevelHeight) * 0.5f;

        // 需要计算侧面缩短后的实际长度
        float SideH_Projected = WallTopZ - WallBotZ;
        float SideR_Projected = WallTopR - WallBotR;
        float ActualSideLen = FMath::Sqrt(SideH_Projected * SideH_Projected + SideR_Projected * SideR_Projected);

        float CurrentV_Bottom = TopBevelArc + ActualSideLen;

        for (int32 i = 1; i <= Segments; ++i)
        {
            CurrentV_Bottom += V_Step;

            const float Alpha = static_cast<float>(i) / Segments;
            const float Angle = Alpha * HALF_PI;

            float DiffR = WallBotR - CapBotR;
            float CurrentR = CapBotR + DiffR * FMath::Cos(Angle);
            float CurrentZ = WallBotZ - BottomBevelHeight * FMath::Sin(Angle);

            FRingContext Ctx;
            Ctx.Z = CurrentZ;
            Ctx.Radius = CurrentR;
            Ctx.Sides = Frustum.BottomSides;

            TArray<int32> CurrentRing = CreateBevelRing(Ctx, CurrentV_Bottom, Alpha, false, UVReferenceRadius);

            StitchRings(CurrentRing, PreviousBottomRing);

            if (CurrentRing.Num() > 0) {
                StartSliceIndices.Insert(CurrentRing[0], 0);
                EndSliceIndices.Insert(CurrentRing.Last(), 0);
            }

            PreviousBottomRing = CurrentRing;
        }
        BottomCapRing = PreviousBottomRing;
    }
}

// ... (GenerateCaps, CreateCapDisk, GenerateCutPlanes, CreateCutPlaneSurface, CreateVertexRing, ApplyBend, CalculateBevelHeight 保持不变) ...
// 确保 CalculateBevelHeight 存在
float FFrustumBuilder::CalculateBevelHeight(float Radius) const
{
    return FMath::Min(Frustum.BevelRadius, Radius);
}

void FFrustumBuilder::GenerateCaps()
{
    if (TopCapRing.Num() >= 3 && Frustum.TopRadius > KINDA_SMALL_NUMBER)
    {
        CreateCapDisk(Frustum.Height, TopCapRing, true);
    }

    if (BottomCapRing.Num() >= 3 && Frustum.BottomRadius > KINDA_SMALL_NUMBER)
    {
        CreateCapDisk(0.0f, BottomCapRing, false);
    }
}

void FFrustumBuilder::CreateCapDisk(float Z, const TArray<int32>& BoundaryRing, bool bIsTop)
{
    FVector CenterPos(0.0f, 0.0f, Z);
    if (FMath::Abs(Frustum.BendAmount) > KINDA_SMALL_NUMBER)
    {
        float H = Z / Frustum.Height;
        CenterPos = ApplyBend(CenterPos, 0.0f, H);
    }

    FVector Normal(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);

    FVector2D CenterUV(CenterPos.X * ModelGenConstants::GLOBAL_UV_SCALE, CenterPos.Y * ModelGenConstants::GLOBAL_UV_SCALE);
    int32 CenterIndex = AddVertex(CenterPos, Normal, CenterUV);

    TArray<int32> CapVertices;
    CapVertices.Reserve(BoundaryRing.Num());

    for (int32 SrcIdx : BoundaryRing)
    {
        FVector Pos = GetPosByIndex(SrcIdx);
        FVector2D UV(Pos.X * ModelGenConstants::GLOBAL_UV_SCALE, Pos.Y * ModelGenConstants::GLOBAL_UV_SCALE);

        CapVertices.Add(GetOrAddVertex(Pos, Normal, UV));
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

    // 1. 计算两个切面的面法线
    const float StartNormalAngle = StartAngle - HALF_PI; // StartFace 法线向外
    const FVector StartFaceNormal(FMath::Cos(StartNormalAngle), FMath::Sin(StartNormalAngle), 0.0f);

    const float EndAngleVal = StartAngle + ArcAngleRadians;
    const float EndNormalAngle = EndAngleVal + HALF_PI;  // EndFace 法线向外
    const FVector EndFaceNormal(FMath::Cos(EndNormalAngle), FMath::Sin(EndNormalAngle), 0.0f);

    // 2. 准备内侧轴线 (0,0,Z) 的法线
    FVector StartInnerNormal = StartFaceNormal;
    FVector EndInnerNormal = EndFaceNormal;

    // 3. 如果开启倒角，计算平滑法线 (两个面法线的平均值/角平分线)
    if (bEnableBevel)
    {
        // 两个向量相加即为角平分方向 (因为长度都是1)
        FVector SmoothCenterNormal = (StartFaceNormal + EndFaceNormal).GetSafeNormal();

        // 极特殊情况：如果 ArcAngle 接近 360 或 0，向量可能抵消，需做保护
        if (SmoothCenterNormal.IsNearlyZero())
        {
            // 指向圆弧开口的反方向
            float BisectAngle = StartAngle + ArcAngleRadians * 0.5f + PI; 
            SmoothCenterNormal = FVector(FMath::Cos(BisectAngle), FMath::Sin(BisectAngle), 0.0f);
        }

        StartInnerNormal = SmoothCenterNormal;
        EndInnerNormal = SmoothCenterNormal;
    }

    // 4. 生成切面，传入计算好的内侧法线
    CreateCutPlaneSurface(StartAngle, StartSliceIndices, true, StartInnerNormal);
    CreateCutPlaneSurface(EndAngleVal, EndSliceIndices, false, EndInnerNormal);
}

void FFrustumBuilder::CreateCutPlaneSurface(float Angle, const TArray<int32>& ProfileIndices, bool bIsStartFace, const FVector& InnerNormal)
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

        // 内侧点位于中心轴
        FVector P1_Inner(0.0f, 0.0f, P1_Outer.Z);
        FVector P2_Inner(0.0f, 0.0f, P2_Outer.Z);

        // 外侧法线 (如果有 Bevel 则沿用 MeshData 中的平滑法线，否则用平面法线)
        FVector N1_Outer = PlaneNormal;
        FVector N2_Outer = PlaneNormal;

        if (bEnableBevel)
        {
            if (Idx1 < MeshData.Normals.Num()) N1_Outer = MeshData.Normals[Idx1];
            if (Idx2 < MeshData.Normals.Num()) N2_Outer = MeshData.Normals[Idx2];
        }

        auto GetCutUV = [&](const FVector& P) {
            float R = FVector2D(P.X, P.Y).Size();
            float U = bIsStartFace ? -R : R;
            float V = Frustum.Height - P.Z;
            return FVector2D(U * ModelGenConstants::GLOBAL_UV_SCALE, V * ModelGenConstants::GLOBAL_UV_SCALE);
        };

        // 关键修改：内侧顶点 (V_In1, V_In2) 使用传入的 InnerNormal
        int32 V_In1 = AddVertex(P1_Inner, InnerNormal, GetCutUV(P1_Inner));
        int32 V_Out1 = AddVertex(P1_Outer, N1_Outer, GetCutUV(P1_Outer));
        int32 V_Out2 = AddVertex(P2_Outer, N2_Outer, GetCutUV(P2_Outer));
        int32 V_In2 = AddVertex(P2_Inner, InnerNormal, GetCutUV(P2_Inner));

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
    return CreateBevelRing(Context, VCoord, 0.0f, false, 0.0f);
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

    bool bIsBevelRegion = false;
    if (bEnableBevel && Frustum.Height > KINDA_SMALL_NUMBER)
    {
        const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
        const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);

        const float BottomBevelRatio = BottomBevelHeight / Frustum.Height;
        const float TopBevelRatio = 1.0f - (TopBevelHeight / Frustum.Height);

        bIsBevelRegion = (HeightRatio <= BottomBevelRatio) || (HeightRatio >= TopBevelRatio);
    }

    if (!bIsCapRing && !bIsBevelRegion && Frustum.MinBendRadius > KINDA_SMALL_NUMBER)
    {
        BentRadius = FMath::Max(BentRadius, Frustum.MinBendRadius);
    }

    if (FMath::IsNearlyEqual(BentRadius, BaseRadius))
    {
        return BasePos;
    }

    const float Scale = BentRadius / BaseRadius;
    return FVector(BasePos.X * Scale, BasePos.Y * Scale, BasePos.Z);
}