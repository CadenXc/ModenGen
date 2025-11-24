// Copyright (c) 2024. All rights reserved.

#include "HollowPrismBuilder.h"
#include "HollowPrism.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"

FHollowPrismBuilder::FHollowPrismBuilder(const AHollowPrism& InHollowPrism)
    : HollowPrism(InHollowPrism)
{
    Clear();
}

void FHollowPrismBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();

    TopInnerCapRing.Empty();
    TopOuterCapRing.Empty();
    BottomInnerCapRing.Empty();
    BottomOuterCapRing.Empty();

    StartInnerCapIndices.Empty();
    StartOuterCapIndices.Empty();
    EndInnerCapIndices.Empty();
    EndOuterCapIndices.Empty();

    InnerAngleCache.Empty();
    OuterAngleCache.Empty();
}

bool FHollowPrismBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!HollowPrism.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    const float Thickness = FMath::Abs(HollowPrism.OuterRadius - HollowPrism.InnerRadius);
    const float MinDimension = FMath::Min(Thickness, HollowPrism.Height);

    BevelSegments = HollowPrism.BevelSegments;
    bEnableBevel = (HollowPrism.BevelRadius > KINDA_SMALL_NUMBER) &&
        (BevelSegments > 0) &&
        (HollowPrism.BevelRadius * 2.0f < MinDimension);

    PrecomputeMath();

    GenerateSideGeometry(EInnerOuter::Inner);
    GenerateSideGeometry(EInnerOuter::Outer);

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

int32 FHollowPrismBuilder::CalculateVertexCountEstimate() const
{
    return HollowPrism.CalculateVertexCountEstimate();
}

int32 FHollowPrismBuilder::CalculateTriangleCountEstimate() const
{
    return HollowPrism.CalculateTriangleCountEstimate();
}

void FHollowPrismBuilder::PrecomputeMath()
{
    ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    StartAngle = -ArcAngleRadians / 2.0f;

    {
        const int32 Sides = HollowPrism.OuterSides;
        OuterAngleCache.SetNum(Sides + 1);
        const float Step = (Sides > 0) ? (ArcAngleRadians / Sides) : 0.0f;
        for (int32 i = 0; i <= Sides; ++i)
        {
            float Angle = StartAngle + i * Step;
            FMath::SinCos(&OuterAngleCache[i].Sin, &OuterAngleCache[i].Cos, Angle);
        }
    }

    {
        const int32 Sides = HollowPrism.InnerSides;
        InnerAngleCache.SetNum(Sides + 1);
        const float Step = (Sides > 0) ? (ArcAngleRadians / Sides) : 0.0f;
        for (int32 i = 0; i <= Sides; ++i)
        {
            float Angle = StartAngle + i * Step;
            FMath::SinCos(&InnerAngleCache[i].Sin, &InnerAngleCache[i].Cos, Angle);
        }
    }
}

void FHollowPrismBuilder::ComputeVerticalProfile(EInnerOuter InnerOuter, TArray<FVerticalProfilePoint>& OutProfile)
{
    OutProfile.Empty();

    const float BevelR = bEnableBevel ? HollowPrism.BevelRadius : 0.0f;
    const int32 Segments = bEnableBevel ? BevelSegments : 0;

    const float BaseRadius = (InnerOuter == EInnerOuter::Inner) ? HollowPrism.InnerRadius : HollowPrism.OuterRadius;
    const float Sign = (InnerOuter == EInnerOuter::Inner) ? 1.0f : -1.0f;

    // 【核心修复】：调整Z坐标使中心点在底面（Z=0），与其他模型一致
    // 原来：顶部在Height/2，底部在-Height/2，中心在0
    // 现在：顶部在Height，底部在0，中心在底面
    const float TopArcCenterZ = HollowPrism.Height - BevelR;
    const float BottomArcCenterZ = BevelR;
    const float ArcCenterR = BaseRadius + (Sign * BevelR);

    float CurrentV = 0.0f;

    // Top Bevel
    for (int32 i = 0; i <= Segments; ++i)
    {
        float Alpha = (Segments > 0) ? (float)i / Segments : 1.0f;
        float Angle = Alpha * HALF_PI;

        FVerticalProfilePoint Point;
        Point.Z = TopArcCenterZ + BevelR * FMath::Cos(Angle);
        Point.Radius = ArcCenterR - (Sign * BevelR * FMath::Sin(Angle));

        float RadialComp = (InnerOuter == EInnerOuter::Outer) ? 1.0f : -1.0f;
        FVector NormalUp(0, 0, 1);
        FVector NormalSide(RadialComp, 0, 0);
        Point.Normal = NormalUp * FMath::Cos(Angle) + NormalSide * FMath::Sin(Angle);

        Point.bIsWallEdge = (i == Segments);

        if (i > 0)
        {
            float ArcLen = BevelR * (HALF_PI / (Segments > 0 ? Segments : 1));
            CurrentV += ArcLen;
        }
        Point.V = CurrentV;

        OutProfile.Add(Point);
    }

    // Wall
    {
        float WallHeight = HollowPrism.Height - 2.0f * BevelR;
        if (WallHeight > KINDA_SMALL_NUMBER)
        {
            CurrentV += WallHeight;

            FVerticalProfilePoint Point;
            Point.Z = BottomArcCenterZ;
            Point.Radius = BaseRadius;

            float RadialComp = (InnerOuter == EInnerOuter::Outer) ? 1.0f : -1.0f;
            Point.Normal = FVector(RadialComp, 0, 0);
            Point.bIsWallEdge = true;
            Point.V = CurrentV;

            OutProfile.Add(Point);
        }
    }

    // Bottom Bevel
    for (int32 i = 1; i <= Segments; ++i)
    {
        float Alpha = (float)i / Segments;
        float Angle = Alpha * HALF_PI;

        float ArcLen = BevelR * (HALF_PI / (Segments > 0 ? Segments : 1));
        CurrentV += ArcLen;

        FVerticalProfilePoint Point;
        Point.Z = BottomArcCenterZ - BevelR * FMath::Sin(Angle);
        Point.Radius = ArcCenterR - (Sign * BevelR * FMath::Cos(Angle));

        float RadialComp = (InnerOuter == EInnerOuter::Outer) ? 1.0f : -1.0f;
        FVector NormalSide(RadialComp, 0, 0);
        FVector NormalDown(0, 0, -1);

        Point.Normal = NormalSide * FMath::Cos(Angle) + NormalDown * FMath::Sin(Angle);
        Point.bIsWallEdge = false;
        Point.V = CurrentV;

        OutProfile.Add(Point);
    }
}

void FHollowPrismBuilder::GenerateSideGeometry(EInnerOuter InnerOuter)
{
    const TArray<FCachedTrig>& AngleCache = (InnerOuter == EInnerOuter::Inner) ? InnerAngleCache : OuterAngleCache;
    const int32 Sides = (InnerOuter == EInnerOuter::Inner) ? HollowPrism.InnerSides : HollowPrism.OuterSides;

    TArray<FVerticalProfilePoint> Profile;
    ComputeVerticalProfile(InnerOuter, Profile);

    if (Profile.Num() < 2) return;

    // 【核心修复】：确定 UV 参考半径（圆柱投影）
    // 内壁使用 InnerRadius，外壁使用 OuterRadius
    // 这保证了无论在倒角处半径如何变化，UV 展开都是基于主圆柱面的
    const float ReferenceRadius = (InnerOuter == EInnerOuter::Inner) ? HollowPrism.InnerRadius : HollowPrism.OuterRadius;

    TArray<TArray<int32>> GridIndices;
    GridIndices.SetNum(Sides + 1);

    TArray<int32>& TopCapRing = (InnerOuter == EInnerOuter::Inner) ? TopInnerCapRing : TopOuterCapRing;
    TArray<int32>& BottomCapRing = (InnerOuter == EInnerOuter::Inner) ? BottomInnerCapRing : BottomOuterCapRing;

    for (int32 s = 0; s <= Sides; ++s)
    {
        GridIndices[s].Reserve(Profile.Num());

        const float CosA = AngleCache[s].Cos;
        const float SinA = AngleCache[s].Sin;

        const float CurrentAngleDelta = s * (ArcAngleRadians / Sides);

        // 【核心修复】：U 坐标基于 ReferenceRadius 计算，消除梯形扭曲
        float U = CurrentAngleDelta * ReferenceRadius;

        for (int32 p = 0; p < Profile.Num(); ++p)
        {
            const FVerticalProfilePoint& Point = Profile[p];

            FVector Pos(Point.Radius * CosA, Point.Radius * SinA, Point.Z);
            FVector Normal(Point.Normal.X * CosA, Point.Normal.X * SinA, Point.Normal.Z);

            FVector2D UV(U * ModelGenConstants::GLOBAL_UV_SCALE, Point.V * ModelGenConstants::GLOBAL_UV_SCALE);

            int32 VertIdx = GetOrAddVertex(Pos, Normal, UV);
            GridIndices[s].Add(VertIdx);

            if (p == 0)
            {
                TopCapRing.Add(VertIdx);
            }
            else if (p == Profile.Num() - 1)
            {
                BottomCapRing.Add(VertIdx);
            }
        }
    }

    TArray<int32>& StartIndices = (InnerOuter == EInnerOuter::Inner) ? StartInnerCapIndices : StartOuterCapIndices;
    for (int32 p = 0; p < Profile.Num(); ++p)
    {
        StartIndices.Add(GridIndices[0][p]);
    }

    TArray<int32>& EndIndices = (InnerOuter == EInnerOuter::Inner) ? EndInnerCapIndices : EndOuterCapIndices;
    for (int32 p = 0; p < Profile.Num(); ++p)
    {
        EndIndices.Add(GridIndices[Sides][p]);
    }

    for (int32 s = 0; s < Sides; ++s)
    {
        for (int32 p = 0; p < Profile.Num() - 1; ++p)
        {
            int32 V00 = GridIndices[s][p];
            int32 V10 = GridIndices[s + 1][p];
            int32 V01 = GridIndices[s][p + 1];
            int32 V11 = GridIndices[s + 1][p + 1];

            if (InnerOuter == EInnerOuter::Outer)
            {
                AddQuad(V00, V10, V11, V01);
            }
            else
            {
                AddQuad(V00, V01, V11, V10);
            }
        }
    }
}

void FHollowPrismBuilder::GenerateCaps()
{
    CreateCapDisk(TopInnerCapRing, TopOuterCapRing, true);
    CreateCapDisk(BottomInnerCapRing, BottomOuterCapRing, false);
}

void FHollowPrismBuilder::CreateCapDisk(const TArray<int32>& InnerRing, const TArray<int32>& OuterRing, bool bIsTop)
{
    if (InnerRing.Num() < 2 || OuterRing.Num() < 2) return;

    FVector Normal(0, 0, bIsTop ? 1.0f : -1.0f);

    auto ProcessRing = [&](const TArray<int32>& SrcRing, TArray<int32>& OutRing)
        {
            OutRing.Reserve(SrcRing.Num());
            for (int32 SrcIdx : SrcRing)
            {
                FVector Pos = GetPosByIndex(SrcIdx);
                FVector2D UV(Pos.X * ModelGenConstants::GLOBAL_UV_SCALE, Pos.Y * ModelGenConstants::GLOBAL_UV_SCALE);
                OutRing.Add(AddVertex(Pos, Normal, UV));
            }
        };

    TArray<int32> NewInnerRing, NewOuterRing;
    ProcessRing(InnerRing, NewInnerRing);
    ProcessRing(OuterRing, NewOuterRing);

    const int32 CountIn = NewInnerRing.Num();
    const int32 CountOut = NewOuterRing.Num();
    const int32 MaxCount = FMath::Max(CountIn, CountOut);

    for (int32 i = 0; i < MaxCount - 1; ++i)
    {
        float RatioCurr = (float)i / (MaxCount - 1);
        float RatioNext = (float)(i + 1) / (MaxCount - 1);

        int32 IdxIn_Curr = FMath::Clamp(FMath::RoundToInt(RatioCurr * (CountIn - 1)), 0, CountIn - 1);
        int32 IdxIn_Next = FMath::Clamp(FMath::RoundToInt(RatioNext * (CountIn - 1)), 0, CountIn - 1);
        int32 IdxOut_Curr = FMath::Clamp(FMath::RoundToInt(RatioCurr * (CountOut - 1)), 0, CountOut - 1);
        int32 IdxOut_Next = FMath::Clamp(FMath::RoundToInt(RatioNext * (CountOut - 1)), 0, CountOut - 1);

        if (bIsTop)
        {
            AddQuad(NewInnerRing[IdxIn_Curr], NewInnerRing[IdxIn_Next], NewOuterRing[IdxOut_Next], NewOuterRing[IdxOut_Curr]);
        }
        else
        {
            AddQuad(NewInnerRing[IdxIn_Curr], NewOuterRing[IdxOut_Curr], NewOuterRing[IdxOut_Next], NewInnerRing[IdxIn_Next]);
        }
    }
}

void FHollowPrismBuilder::GenerateCutPlanes()
{
    if (HollowPrism.IsFullCircle()) return;

    if (StartInnerCapIndices.Num() != StartOuterCapIndices.Num()) return;

    CreateCutPlane(StartAngle, StartInnerCapIndices, StartOuterCapIndices, true);

    float EndAngle = StartAngle + ArcAngleRadians;
    CreateCutPlane(EndAngle, EndInnerCapIndices, EndOuterCapIndices, false);
}

void FHollowPrismBuilder::CreateCutPlane(float Angle, const TArray<int32>& InnerIndices, const TArray<int32>& OuterIndices, bool bIsStartFace)
{
    // 法线
    float NormalAngle = Angle + (bIsStartFace ? -HALF_PI : HALF_PI);
    FVector Normal(FMath::Cos(NormalAngle), FMath::Sin(NormalAngle), 0.0f);

    int32 NumPoints = InnerIndices.Num();

    // 必须创建新顶点 (硬边)
    TArray<int32> NewInner, NewOuter;
    NewInner.Reserve(NumPoints);
    NewOuter.Reserve(NumPoints);

    // 【核心修复】：调整V坐标计算，因为Z坐标已经从[-Height/2, Height/2]改为[0, Height]
    // 现在Z在[0, Height]范围内，V = Height - Z，使得顶部（Z=Height）的V=0，底部（Z=0）的V=Height
    const float Height = HollowPrism.Height;

    for (int32 i = 0; i < NumPoints; ++i)
    {
        FVector P_In = GetPosByIndex(InnerIndices[i]);
        FVector P_Out = GetPosByIndex(OuterIndices[i]);

        // UV = (Radius, V_flipped) * Scale
        float R_In = FVector2D(P_In.X, P_In.Y).Size();
        float R_Out = FVector2D(P_Out.X, P_Out.Y).Size();

        // 【核心修复】：反转V值 (Height - Z) -> (Top=0, Bottom=Height)
        float V_In = Height - P_In.Z;
        float V_Out = Height - P_Out.Z;

        FVector2D UV_In(-R_In * ModelGenConstants::GLOBAL_UV_SCALE, V_In * ModelGenConstants::GLOBAL_UV_SCALE);
        FVector2D UV_Out(-R_Out * ModelGenConstants::GLOBAL_UV_SCALE, V_Out * ModelGenConstants::GLOBAL_UV_SCALE);

        NewInner.Add(AddVertex(P_In, Normal, UV_In));
        NewOuter.Add(AddVertex(P_Out, Normal, UV_Out));
    }

    // 生成面
    for (int32 i = 0; i < NumPoints - 1; ++i)
    {
        if (bIsStartFace)
        {
            AddQuad(NewInner[i], NewOuter[i], NewOuter[i + 1], NewInner[i + 1]);
        }
        else
        {
            AddQuad(NewInner[i], NewInner[i + 1], NewOuter[i + 1], NewOuter[i]);
        }
    }
}