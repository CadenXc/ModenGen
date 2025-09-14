// Copyright (c) 2024. All rights reserved.

#include "HollowPrismBuilder.h"
#include "HollowPrism.h"
#include "ModelGenMeshData.h"
#include "Engine/Engine.h"

FHollowPrismBuilder::FHollowPrismBuilder(const AHollowPrism& InHollowPrism)
    : HollowPrism(InHollowPrism)
{
    Clear();
}

bool FHollowPrismBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!HollowPrism.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    // 清空端盖顶点索引记录
    StartOuterCapIndices.Empty();
    StartInnerCapIndices.Empty();
    EndOuterCapIndices.Empty();
    EndInnerCapIndices.Empty();
    
    // 清空倒角连接顶点记录
    TopInnerBevelVertices.Empty();
    TopOuterBevelVertices.Empty();
    BottomInnerBevelVertices.Empty();
    BottomOuterBevelVertices.Empty();

    // 生成主体几何体
    CreateInnerWalls();
    CreateOuterWalls();

    // 生成顶盖和底盖
    CreateTopCap();
    CreateBottomCap();

    // 生成倒角几何体（如果有倒角）
    if (HollowPrism.BevelRadius > 0.0f)
    {
        CreateTopBevel();
        CreateBottomBevel();
    }

    // 如果不是一个完整的360度圆环，则需要生成端盖来封闭开口
    if (!HollowPrism.IsFullCircle())
    {
        CreateEndCaps();
    }

    if (!ValidateGeneratedData())
    {
        return false;
    }

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

void FHollowPrismBuilder::CreateWalls(float Radius, int32 Sides, bool bIsInner)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Sides);
    const bool bFull = HollowPrism.IsFullCircle();

    TArray<int32> TopVertices, BottomVertices;

    for (int32 i = 0; i <= Sides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;

        // 如果有倒角，墙体的高度需要缩减
        const float WallTopZ = HalfHeight - HollowPrism.BevelRadius;
        const float WallBottomZ = -HalfHeight + HollowPrism.BevelRadius;

        const FVector TopPos = CalculateVertexPosition(Radius, Angle, WallTopZ);
        const FVector BottomPos = CalculateVertexPosition(Radius, Angle, WallBottomZ);

        // 法线方向：内壁向内，外壁向外
        const FVector Normal = bIsInner
            ? FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f).GetSafeNormal()
            : FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f).GetSafeNormal();

        // 计算内外侧面UV坐标
        const FVector2D TopUV = CalculateWallUV(Angle, WallTopZ, !bIsInner);
        const FVector2D BottomUV = CalculateWallUV(Angle, WallBottomZ, !bIsInner);
        
        const int32 TopVertex = GetOrAddVertexWithUV(TopPos, Normal, TopUV);
        const int32 BottomVertex = GetOrAddVertexWithUV(BottomPos, Normal, BottomUV);

        TopVertices.Add(TopVertex);
        BottomVertices.Add(BottomVertex);

        // 记录与倒角连接的顶点（如果有倒角）
        if (HollowPrism.BevelRadius > 0.0f)
        {
            if (bIsInner)
            {
                TopInnerBevelVertices.Add(TopVertex);
                BottomInnerBevelVertices.Add(BottomVertex);
            }
            else
            {
                TopOuterBevelVertices.Add(TopVertex);
                BottomOuterBevelVertices.Add(BottomVertex);
            }
        }

        // 记录起始和结束位置的顶点索引（用于端盖生成）
        if (!bFull)
        {
            if (i == 0) // 起始位置
            {
                if (bIsInner)
                {
                    // 内壁：记录到起始内壁数组
                    StartInnerCapIndices.Add(TopVertex);    // 内壁顶部
                    StartInnerCapIndices.Add(BottomVertex); // 内壁底部
                }
                else
                {
                    // 外壁：记录到起始外壁数组
                    StartOuterCapIndices.Add(TopVertex);    // 外壁顶部
                    StartOuterCapIndices.Add(BottomVertex); // 外壁底部
                }
            }
            else if (i == Sides) // 结束位置
            {
                if (bIsInner)
                {
                    // 内壁：记录到结束内壁数组
                    EndInnerCapIndices.Add(TopVertex);    // 内壁顶部
                    EndInnerCapIndices.Add(BottomVertex); // 内壁底部
                }
                else
                {
                    // 外壁：记录到结束外壁数组
                    EndOuterCapIndices.Add(TopVertex);    // 外壁顶部
                    EndOuterCapIndices.Add(BottomVertex); // 外壁底部
                }
            }
        }
    }

    // 如果是完整圆形，最后一个顶点应该与第一个相同
    if (bFull && TopVertices.Num() > 0)
    {
        TopVertices.Last() = TopVertices[0];
        BottomVertices.Last() = BottomVertices[0];
    }

    // 生成四边形面
    for (int32 i = 0; i < Sides; ++i)
    {
        if (bIsInner)
        {
            // 内壁：逆时针顺序
            AddQuad(TopVertices[i], BottomVertices[i], BottomVertices[i + 1], TopVertices[i + 1]);
        }
        else
        {
            // 外壁：顺时针顺序
            AddQuad(TopVertices[i], TopVertices[i + 1], BottomVertices[i + 1], BottomVertices[i]);
        }
    }
}

void FHollowPrismBuilder::CreateInnerWalls()
{
    CreateWalls(HollowPrism.InnerRadius, HollowPrism.InnerSides, true);
}

void FHollowPrismBuilder::CreateOuterWalls()
{
    CreateWalls(HollowPrism.OuterRadius, HollowPrism.OuterSides, false);
}

void FHollowPrismBuilder::CreateTopCap()
{
    TArray<int32> InnerVertices, OuterVertices;
    CreateCapVertices(InnerVertices, OuterVertices, true);
    CreateCapTriangles(InnerVertices, OuterVertices, true);
}

void FHollowPrismBuilder::CreateBottomCap()
{
    TArray<int32> InnerVertices, OuterVertices;
    CreateCapVertices(InnerVertices, OuterVertices, false);
    CreateCapTriangles(InnerVertices, OuterVertices, false);
}

void FHollowPrismBuilder::CreateCapVertices(TArray<int32>& OutInnerVertices,
    TArray<int32>& OutOuterVertices,
    bool bIsTopCap)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    // 上下面的Z值保持不变
    const float CurrentHight = bIsTopCap ? HalfHeight : -HalfHeight;
    
    // 端盖面的法线应该垂直于端盖表面
    // 端盖面是水平的，法线应该垂直向上/向下
    const float ZSign = bIsTopCap ? 1.0f : -1.0f;
    FVector Normal(0.0f, 0.0f, ZSign);

    const float StartAngle = CalculateStartAngle();
    const float InnerAngleStep = CalculateAngleStep(HollowPrism.InnerSides);
    const float OuterAngleStep = CalculateAngleStep(HollowPrism.OuterSides);

    // 生成内环顶点（调整半径以考虑倒角）
    for (int32 i = 0; i <= HollowPrism.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * InnerAngleStep;
        // 内环半径增加倒角半径
        const float AdjustedInnerRadius = HollowPrism.InnerRadius + HollowPrism.BevelRadius;
        const FVector InnerPos = CalculateVertexPosition(AdjustedInnerRadius, Angle, CurrentHight);
        const FVector2D InnerUV = CalculateCapUV(Angle, AdjustedInnerRadius, bIsTopCap);
        OutInnerVertices.Add(GetOrAddVertexWithUV(InnerPos, Normal, InnerUV));
    }

    // 生成外环顶点（调整半径以考虑倒角）
    for (int32 i = 0; i <= HollowPrism.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * OuterAngleStep;
        // 外环半径减少倒角半径
        const float AdjustedOuterRadius = HollowPrism.OuterRadius - HollowPrism.BevelRadius;
        const FVector OuterPos = CalculateVertexPosition(AdjustedOuterRadius, Angle, CurrentHight);
        const FVector2D OuterUV = CalculateCapUV(Angle, AdjustedOuterRadius, bIsTopCap);
        OutOuterVertices.Add(GetOrAddVertexWithUV(OuterPos, Normal, OuterUV));
    }

    // 如果是完整圆形，确保最后一个顶点与第一个相同
    if (HollowPrism.IsFullCircle())
    {
        if (OutInnerVertices.Num() > 0)
        {
            OutInnerVertices.Last() = OutInnerVertices[0];
        }
        if (OutOuterVertices.Num() > 0)
        {
            OutOuterVertices.Last() = OutOuterVertices[0];
        }
    }
    else
    {
        // 记录顶面/底面的起始和结束顶点用于端盖生成
        if (OutInnerVertices.Num() > 0 && OutOuterVertices.Num() > 0)
        {
            if (bIsTopCap)
            {
                // 顶面：记录起始和结束顶点
                StartOuterCapIndices.Add(OutOuterVertices[0]);           // 外壁起始
                StartInnerCapIndices.Add(OutInnerVertices[0]);           // 内壁起始
                EndOuterCapIndices.Add(OutOuterVertices.Last());         // 外壁结束
                EndInnerCapIndices.Add(OutInnerVertices.Last());         // 内壁结束
            }
            else
            {
                // 底面：记录起始和结束顶点
                StartOuterCapIndices.Add(OutOuterVertices[0]);           // 外壁起始
                StartInnerCapIndices.Add(OutInnerVertices[0]);           // 内壁起始
                EndOuterCapIndices.Add(OutOuterVertices.Last());         // 外壁结束
                EndInnerCapIndices.Add(OutInnerVertices.Last());         // 内壁结束
            }
        }
    }
}

void FHollowPrismBuilder::CreateTopBevel()
{
    CreateBevelGeometry(true, true);   // 顶部内环倒角
    CreateBevelGeometry(true, false);  // 顶部外环倒角
}

void FHollowPrismBuilder::CreateBottomBevel()
{
    CreateBevelGeometry(false, true);   // 底部内环倒角
    CreateBevelGeometry(false, false);  // 底部外环倒角
}

void FHollowPrismBuilder::CreateEndCaps()
{
    if (HollowPrism.IsFullCircle())
    {
        return;
    }

    // 使用记录的顶点索引生成端盖
    CreateAdvancedEndCaps();
}

void FHollowPrismBuilder::CreateAdvancedEndCaps()
{
    // 使用正确的端盖生成方法，按高度顺序生成完整顶点列
    CreateEndCapWithBevel(true);   // 起始端盖
    CreateEndCapWithBevel(false);  // 结束端盖
}


float FHollowPrismBuilder::CalculateStartAngle() const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    return -ArcAngleRadians / 2.0f;
}

float FHollowPrismBuilder::CalculateAngleStep(int32 Sides) const
{
    // 确保边数不为0以避免除零错误
    if (Sides == 0) return 0.0f;
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    return ArcAngleRadians / Sides;
}

FVector FHollowPrismBuilder::CalculateVertexPosition(float Radius, float Angle, float Z) const
{
    return FVector(
        Radius * FMath::Cos(Angle),
        Radius * FMath::Sin(Angle),
        Z
    );
}

void FHollowPrismBuilder::CreateCapTriangles(const TArray<int32>& InnerVertices, const TArray<int32>& OuterVertices, bool bIsTopCap)
{
    const int32 MaxSides = FMath::Max(HollowPrism.InnerSides, HollowPrism.OuterSides);
    if (MaxSides == 0) return;

    for (int32 i = 0; i < MaxSides; ++i)
    {
        // 算法保持不变，但在顶点选择上进行插值
        const int32 InnerV1 = FMath::RoundToInt(static_cast<float>(i) / MaxSides * HollowPrism.InnerSides);
        const int32 OuterV1 = FMath::RoundToInt(static_cast<float>(i) / MaxSides * HollowPrism.OuterSides);
        const int32 InnerV2 = FMath::RoundToInt(static_cast<float>(i + 1) / MaxSides * HollowPrism.InnerSides);
        const int32 OuterV2 = FMath::RoundToInt(static_cast<float>(i + 1) / MaxSides * HollowPrism.OuterSides);

        if (bIsTopCap)
        {
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV2], OuterVertices[OuterV1]);
            AddTriangle(InnerVertices[InnerV1], InnerVertices[InnerV2], OuterVertices[OuterV2]);
        }
        else // 底盖为逆时针 (Counter-Clockwise)
        {
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV1], OuterVertices[OuterV2]);
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV2], InnerVertices[InnerV2]);
        }
    }
}


void FHollowPrismBuilder::CreateBevelGeometry(bool bIsTop, bool bIsInner)
{
    if (HollowPrism.BevelRadius <= 0.0f || HollowPrism.BevelSegments <= 0) return;

    TArray<int32> PrevRing;
    
    // 第一个环重新生成，不共用壁面顶点
    CreateBevelRing(PrevRing, bIsTop, bIsInner, 0, HollowPrism.BevelSegments);
    
    for (int32 i = 1; i <= HollowPrism.BevelSegments; ++i)
    {
        TArray<int32> CurrentRing;
        CreateBevelRing(CurrentRing, bIsTop, bIsInner, i, HollowPrism.BevelSegments);

        // 连接前一个环和当前环
        ConnectBevelRings(PrevRing, CurrentRing, bIsInner, bIsTop);
        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::CreateBevelRing(TArray<int32>& OutCurrentRing,
    bool bIsTop, bool bIsInner,
    int32 RingIndex, int32 TotalRings)
{
    const float Alpha = static_cast<float>(RingIndex) / TotalRings;
    const float RingAngle = Alpha * HALF_PI; // 倒角从0到90度

    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float ZSign = bIsTop ? 1.0f : -1.0f;

    float CurrentZ, CurrentRadius;
    
    if (RingIndex == 0)
    {
        // 第一个环：与壁面连接处，重新生成顶点位置
        CurrentZ = HalfHeight * ZSign - HollowPrism.BevelRadius * ZSign;
        CurrentRadius = bIsInner ? HollowPrism.InnerRadius : HollowPrism.OuterRadius;
    }
    else
    {
        // 其他环：倒角圆弧上的点
        const float CenterZ = HalfHeight * ZSign - HollowPrism.BevelRadius * ZSign;
        const float CenterRadius = bIsInner ?
            HollowPrism.InnerRadius + HollowPrism.BevelRadius :
            HollowPrism.OuterRadius - HollowPrism.BevelRadius;

        CurrentZ = CenterZ + FMath::Sin(RingAngle) * HollowPrism.BevelRadius * ZSign;
        const float RadiusFactor = FMath::Cos(RingAngle) * HollowPrism.BevelRadius;
        CurrentRadius = bIsInner ? CenterRadius - RadiusFactor : CenterRadius + RadiusFactor;
    }

    const int32 Sides = bIsInner ? HollowPrism.InnerSides : HollowPrism.OuterSides;
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Sides);

    OutCurrentRing.Reserve(Sides + 1);

    for (int32 s = 0; s <= Sides; ++s)
    {
        const float SideAngle = StartAngle + s * AngleStep;

        const FVector Position = CalculateVertexPosition(CurrentRadius, SideAngle, CurrentZ);
        const FVector Normal = CalculateBevelNormal(SideAngle, Alpha, bIsInner, bIsTop);
        const FVector2D UV = CalculateBevelUV(SideAngle, Alpha, bIsInner, bIsTop);
        const int32 VertexIndex = GetOrAddVertexWithUV(Position, Normal, UV);
        OutCurrentRing.Add(VertexIndex);

        // 记录倒角端盖顶点（如果不是完整圆环）
        if (!HollowPrism.IsFullCircle())
        {
            if (s == 0) // 起始位置
            {
                if (bIsInner)
                {
                    StartInnerCapIndices.Add(VertexIndex);
                }
                else
                {
                    StartOuterCapIndices.Add(VertexIndex);
                }
            }
            else if (s == Sides) // 结束位置
            {
                if (bIsInner)
                {
                    EndInnerCapIndices.Add(VertexIndex);
                }
                else
                {
                    EndOuterCapIndices.Add(VertexIndex);
                }
            }
        }
    }

    if (HollowPrism.IsFullCircle())
    {
        OutCurrentRing.Last() = OutCurrentRing[0];
    }
}

FVector FHollowPrismBuilder::CalculateBevelNormal(float Angle, float Alpha, bool bIsInner, bool bIsTop) const
{
    // 计算倒角面的正确法线
    // 倒角面法线应该从倒角圆弧中心指向表面顶点
    const float RingAngle = Alpha * HALF_PI;
    const float ZSign = bIsTop ? 1.0f : -1.0f;

    // 倒角圆弧的中心位置
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float CenterZ = HalfHeight * ZSign - HollowPrism.BevelRadius * ZSign;
    const float CenterRadius = bIsInner ?
        HollowPrism.InnerRadius + HollowPrism.BevelRadius :
        HollowPrism.OuterRadius - HollowPrism.BevelRadius;

    // 当前顶点的位置
    const float CurrentZ = CenterZ + FMath::Sin(RingAngle) * HollowPrism.BevelRadius * ZSign;
    const float RadiusFactor = FMath::Cos(RingAngle) * HollowPrism.BevelRadius;
    const float CurrentRadius = bIsInner ? CenterRadius - RadiusFactor : CenterRadius + RadiusFactor;

    // 计算从圆弧中心到顶点的方向向量
    FVector CenterPos(FMath::Cos(Angle) * CenterRadius, FMath::Sin(Angle) * CenterRadius, CenterZ);
    FVector VertexPos(FMath::Cos(Angle) * CurrentRadius, FMath::Sin(Angle) * CurrentRadius, CurrentZ);
    FVector Normal = (VertexPos - CenterPos).GetSafeNormal();

    return Normal;
}


void FHollowPrismBuilder::ConnectBevelRings(const TArray<int32>& PrevRing,
    const TArray<int32>& CurrentRing,
    bool bIsInner, bool bIsTop)
{
    const int32 Sides = bIsInner ? HollowPrism.InnerSides : HollowPrism.OuterSides;

    for (int32 s = 0; s < Sides; ++s)
    {
        const int32 V00 = PrevRing[s];
        const int32 V10 = CurrentRing[s];
        const int32 V01 = PrevRing[s + 1];
        const int32 V11 = CurrentRing[s + 1];

        // 统一内外环和顶底的顶点顺序逻辑
        if ((bIsTop && !bIsInner) || (!bIsTop && bIsInner))
        {
            AddQuad(V00, V10, V11, V01);
        }
        else
        {
            AddQuad(V00, V01, V11, V10);
        }
    }
}


// [FIXED] Correctly generates the end cap vertex column, accounting for bevels.
void FHollowPrismBuilder::CreateEndCapColumn(float Angle, const FVector& Normal, TArray<int32>& OutOrderedVertices, bool bIsStart)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float BevelRadius = HollowPrism.BevelRadius;
    const int32 BevelSegments = HollowPrism.BevelSegments;

    OutOrderedVertices.Empty();

    // Handle the case with no bevels separately for clarity and efficiency.
    if (BevelRadius <= 0.0f || BevelSegments <= 0)
    {
        const FVector OuterTop = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, HalfHeight);
        const FVector InnerTop = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, HalfHeight);
        const FVector OuterBottom = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, -HalfHeight);
        const FVector InnerBottom = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, -HalfHeight);

        // Add vertices in top-to-bottom, outer-then-inner order with UV.
        const FVector2D OuterTopUV = CalculateEndCapUVWithRadius(Angle, HalfHeight, HollowPrism.OuterRadius, bIsStart);
        const FVector2D InnerTopUV = CalculateEndCapUVWithRadius(Angle, HalfHeight, HollowPrism.InnerRadius, bIsStart);
        const FVector2D OuterBottomUV = CalculateEndCapUVWithRadius(Angle, -HalfHeight, HollowPrism.OuterRadius, bIsStart);
        const FVector2D InnerBottomUV = CalculateEndCapUVWithRadius(Angle, -HalfHeight, HollowPrism.InnerRadius, bIsStart);
        
        OutOrderedVertices.Add(GetOrAddVertexWithUV(OuterTop, Normal, OuterTopUV));
        OutOrderedVertices.Add(GetOrAddVertexWithUV(InnerTop, Normal, InnerTopUV));
        OutOrderedVertices.Add(GetOrAddVertexWithUV(OuterBottom, Normal, OuterBottomUV));
        OutOrderedVertices.Add(GetOrAddVertexWithUV(InnerBottom, Normal, InnerBottomUV));
        return;
    }

    // With bevels: Generate vertices by tracing the profile from top to bottom.
    // Reserve memory to avoid reallocations.
    OutOrderedVertices.Reserve((BevelSegments + 1) * 2 * 2);

    // === 1. Top Bevel ===
    // Generate vertices from the top flat cap down to the wall junction.
    for (int32 i = 0; i <= BevelSegments; ++i)
    {
        // Alpha goes from 1 (cap) down to 0 (wall).
        const float Alpha = 1.0f - (static_cast<float>(i) / BevelSegments);
        const float RingAngle = Alpha * HALF_PI; // Angle from 90 degrees down to 0.

        const float ZSign = 1.0f;
        const float CenterZ = HalfHeight * ZSign - BevelRadius * ZSign;
        const float RadiusFactor = FMath::Cos(RingAngle) * BevelRadius;

        // Calculate inner and outer radii at this point on the bevel curve.
        const float CenterRadius_Inner = HollowPrism.InnerRadius + BevelRadius;
        const float CurrentRadius_Inner = CenterRadius_Inner - RadiusFactor;
        const float CenterRadius_Outer = HollowPrism.OuterRadius - BevelRadius;
        const float CurrentRadius_Outer = CenterRadius_Outer + RadiusFactor;

        const float CurrentZ = CenterZ + FMath::Sin(RingAngle) * BevelRadius * ZSign;

        const FVector InnerPos = CalculateVertexPosition(CurrentRadius_Inner, Angle, CurrentZ);
        const FVector OuterPos = CalculateVertexPosition(CurrentRadius_Outer, Angle, CurrentZ);

        // 计算UV坐标
        const FVector2D OuterUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Outer, bIsStart);
        const FVector2D InnerUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Inner, bIsStart);

        OutOrderedVertices.Add(GetOrAddVertexWithUV(OuterPos, Normal, OuterUV));
        OutOrderedVertices.Add(GetOrAddVertexWithUV(InnerPos, Normal, InnerUV));
    }

    // === 2. Bottom Bevel ===
    // Generate vertices from the wall junction down to the bottom flat cap.
    // Start from i=0 to ensure we have the wall-bevel seam vertex.
    for (int32 i = 0; i <= BevelSegments; ++i)
    {
        // Alpha goes from 0 (wall) up to 1 (cap).
        const float Alpha = static_cast<float>(i) / BevelSegments;
        const float RingAngle = Alpha * HALF_PI; // Angle from 0 up to 90 degrees.

        const float ZSign = -1.0f;
        const float CenterZ = HalfHeight * ZSign - BevelRadius * ZSign;
        const float RadiusFactor = FMath::Cos(RingAngle) * BevelRadius;

        // Calculate inner and outer radii at this point on the bevel curve.
        const float CenterRadius_Inner = HollowPrism.InnerRadius + BevelRadius;
        const float CurrentRadius_Inner = CenterRadius_Inner - RadiusFactor;
        const float CenterRadius_Outer = HollowPrism.OuterRadius - BevelRadius;
        const float CurrentRadius_Outer = CenterRadius_Outer + RadiusFactor;

        const float CurrentZ = CenterZ + FMath::Sin(RingAngle) * BevelRadius * ZSign;

        const FVector InnerPos = CalculateVertexPosition(CurrentRadius_Inner, Angle, CurrentZ);
        const FVector OuterPos = CalculateVertexPosition(CurrentRadius_Outer, Angle, CurrentZ);

        // 计算UV坐标
        const FVector2D OuterUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Outer, bIsStart);
        const FVector2D InnerUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Inner, bIsStart);

        OutOrderedVertices.Add(GetOrAddVertexWithUV(OuterPos, Normal, OuterUV));
        OutOrderedVertices.Add(GetOrAddVertexWithUV(InnerPos, Normal, InnerUV));
    }
}


// [REFACTORED] -> This function is correct and does not need changes.
void FHollowPrismBuilder::CreateEndCapTriangles(const TArray<int32>& OrderedVertices, bool IsStart)
{
    // 顶点是以 [Outer0, Inner0, Outer1, Inner1, ...] 的顺序存储的
    for (int32 i = 0; i < OrderedVertices.Num() - 3; i += 2)
    {
        const int32 V_Outer_Curr = OrderedVertices[i];
        const int32 V_Inner_Curr = OrderedVertices[i + 1];
        const int32 V_Outer_Next = OrderedVertices[i + 2];
        const int32 V_Inner_Next = OrderedVertices[i + 3];

        if (IsStart)
        {
            // 起始端盖，法线朝外，逆时针
            AddQuad(V_Outer_Curr, V_Outer_Next, V_Inner_Next, V_Inner_Curr);
        }
        else
        {
            // 结束端盖，法线朝外，顺时针
            AddQuad(V_Outer_Curr, V_Inner_Curr, V_Inner_Next, V_Outer_Next);
        }
    }
}

void FHollowPrismBuilder::CreateEndCapFromVertices(const TArray<int32>& RecordedVertices, bool bIsStart)
{
    if (RecordedVertices.Num() < 4)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateEndCapFromVertices - 记录的顶点数量不足，无法生成端盖"));
        return;
    }

    // 直接使用记录的顶点生成端盖，避免重复计算
    CreateEndCapFromSimpleVertices(RecordedVertices, bIsStart);
}

void FHollowPrismBuilder::CreateEndCapFromSeparateArrays(const TArray<int32>& OuterVertices, const TArray<int32>& InnerVertices, bool bIsStart)
{
    if (OuterVertices.Num() < 2 || InnerVertices.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateEndCapFromSeparateArrays - 顶点数量不足"));
        return;
    }

    // 重新创建端盖顶点，参考Frustum的实现
    TArray<int32> EndCapOuterVertices;
    TArray<int32> EndCapInnerVertices;
    
    // 计算端盖法线（垂直于端盖表面）
    const FVector EndCapNormal = bIsStart ? 
        FVector(-1.0f, 0.0f, 0.0f) :  // 起始端盖法线朝外
        FVector(1.0f, 0.0f, 0.0f);    // 结束端盖法线朝外
    
    // 重新创建外壁顶点
    for (int32 i = 0; i < OuterVertices.Num(); ++i)
    {
        const FVector OuterPos = GetPosByIndex(OuterVertices[i]);
        
        // 基于索引计算U坐标，基于高度计算V坐标
        const float U = static_cast<float>(i) / FMath::Max(1.0f, static_cast<float>(OuterVertices.Num() - 1));
        const float V = (OuterPos.Z + HollowPrism.GetHalfHeight()) / (2.0f * HollowPrism.GetHalfHeight());
        
        // 端盖UV区域分配：[0.75, 1] x [0, 1]
        // 起始端盖：[0.75, 0.875] x [0, 1] - 使用镜像U映射
        // 结束端盖：[0.875, 1] x [0, 1] - 使用正常U映射
        const float EndCapU = bIsStart ? 
            0.75f + (1.0f - U) * 0.125f : // 起始端盖：镜像映射 0.75 到 0.875
            0.875f + U * 0.125f; // 结束端盖：正常映射 0.875 到 1
        const float EndCapV = FMath::Clamp(V, 0.0f, 1.0f);
        
        const FVector2D OuterUV(EndCapU, EndCapV);
        const int32 NewOuterVertex = GetOrAddVertexWithUV(OuterPos, EndCapNormal, OuterUV);
        EndCapOuterVertices.Add(NewOuterVertex);
    }
    
    // 重新创建内壁顶点
    for (int32 i = 0; i < InnerVertices.Num(); ++i)
    {
        const FVector InnerPos = GetPosByIndex(InnerVertices[i]);
        
        // 基于索引计算U坐标，基于高度计算V坐标
        const float U = static_cast<float>(i) / FMath::Max(1.0f, static_cast<float>(InnerVertices.Num() - 1));
        const float V = (InnerPos.Z + HollowPrism.GetHalfHeight()) / (2.0f * HollowPrism.GetHalfHeight());
        
        // 端盖UV区域分配：[0.75, 1] x [0, 1]
        // 起始端盖：[0.75, 0.875] x [0, 1] - 使用镜像U映射
        // 结束端盖：[0.875, 1] x [0, 1] - 使用正常U映射
        const float EndCapU = bIsStart ? 
            0.75f + (1.0f - U) * 0.125f : // 起始端盖：镜像映射 0.75 到 0.875
            0.875f + U * 0.125f; // 结束端盖：正常映射 0.875 到 1
        const float EndCapV = FMath::Clamp(V, 0.0f, 1.0f);
        
        const FVector2D InnerUV(EndCapU, EndCapV);
        const int32 NewInnerVertex = GetOrAddVertexWithUV(InnerPos, EndCapNormal, InnerUV);
        EndCapInnerVertices.Add(NewInnerVertex);
    }
    
    // 生成端盖三角形
    for (int32 i = 0; i < EndCapOuterVertices.Num() - 1; ++i)
    {
        const int32 OuterTop = EndCapOuterVertices[i];
        const int32 OuterBottom = EndCapOuterVertices[i + 1];
        const int32 InnerTop = EndCapInnerVertices[i];
        const int32 InnerBottom = EndCapInnerVertices[i + 1];
        
        if (bIsStart)
        {
            // 起始端盖：法线朝外，逆时针
            AddQuad(OuterTop, OuterBottom, InnerBottom, InnerTop);
        }
        else
        {
            // 结束端盖：法线朝外，顺时针
            AddQuad(OuterTop, InnerTop, InnerBottom, OuterBottom);
        }
    }
}

void FHollowPrismBuilder::CreateEndCapWithBevel(bool bIsStart)
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float Angle = bIsStart ? -ArcAngleRadians / 2.0f : ArcAngleRadians / 2.0f;
    const FVector Normal = bIsStart ? 
        FVector(FMath::Sin(Angle), -FMath::Cos(Angle), 0.0f).GetSafeNormal() :
        FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0f).GetSafeNormal();
    
    TArray<int32> OrderedVertices;
    CreateEndCapColumn(Angle, Normal, OrderedVertices, bIsStart);
    CreateEndCapTriangles(OrderedVertices, bIsStart);
}

void FHollowPrismBuilder::CreateEndCapFromSimpleVertices(const TArray<int32>& RecordedVertices, bool bIsStart)
{
    if (RecordedVertices.Num() < 4)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateEndCapFromSimpleVertices - 顶点数量不足"));
        return;
    }
    
    if (HollowPrism.BevelRadius > 0.0f && HollowPrism.BevelSegments > 0)
    {
        // 有倒角时，端盖包含8个顶点：[上外部倒角, 下外部倒角, 上内部倒角, 下内部倒角, 外壁顶部, 外壁底部, 内壁顶部, 内壁底部]
        if (RecordedVertices.Num() >= 8)
        {
            CreateEndCapWithBevelVertices(RecordedVertices, bIsStart);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("CreateEndCapFromSimpleVertices - 有倒角但顶点数量不足8个"));
        }
    }
    else
    {
        // 无倒角时，端盖包含4个顶点：[外壁顶部, 外壁底部, 内壁顶部, 内壁底部]
        const int32 OuterTop = RecordedVertices[0];
        const int32 OuterBottom = RecordedVertices[1];
        const int32 InnerTop = RecordedVertices[2];
        const int32 InnerBottom = RecordedVertices[3];
        
        // 生成端盖三角形
        if (bIsStart)
        {
            // 起始端盖：法线朝外，逆时针
            AddQuad(OuterTop, OuterBottom, InnerBottom, InnerTop);
        }
        else
        {
            // 结束端盖：法线朝外，顺时针
            AddQuad(OuterTop, InnerTop, InnerBottom, OuterBottom);
        }
    }
}

void FHollowPrismBuilder::CreateEndCapWithBevelVertices(const TArray<int32>& RecordedVertices, bool bIsStart)
{
    // 顶点顺序：[上外部倒角, 下外部倒角, 上内部倒角, 下内部倒角, 外壁顶部, 外壁底部, 内壁顶部, 内壁底部]
    const int32 OuterBevelTop = RecordedVertices[0];     // 上外部倒角
    const int32 OuterBevelBottom = RecordedVertices[1];  // 下外部倒角
    const int32 InnerBevelTop = RecordedVertices[2];     // 上内部倒角
    const int32 InnerBevelBottom = RecordedVertices[3];  // 下内部倒角
    const int32 OuterTop = RecordedVertices[4];          // 外壁顶部
    const int32 OuterBottom = RecordedVertices[5];       // 外壁底部
    const int32 InnerTop = RecordedVertices[6];          // 内壁顶部
    const int32 InnerBottom = RecordedVertices[7];       // 内壁底部
    
    // 生成端盖三角形（包含倒角部分）
    if (bIsStart)
    {
        // 起始端盖：法线朝外，逆时针
        // 倒角部分
        AddQuad(OuterBevelTop, OuterBevelBottom, InnerBevelBottom, InnerBevelTop);
        // 墙体部分
        AddQuad(OuterTop, OuterBottom, InnerBottom, InnerTop);
        // 连接倒角和墙体
        AddQuad(OuterBevelTop, OuterTop, InnerTop, InnerBevelTop);
        AddQuad(OuterBevelBottom, InnerBevelBottom, InnerBottom, OuterBottom);
    }
    else
    {
        // 结束端盖：法线朝外，顺时针
        // 倒角部分
        AddQuad(OuterBevelTop, InnerBevelTop, InnerBevelBottom, OuterBevelBottom);
        // 墙体部分
        AddQuad(OuterTop, InnerTop, InnerBottom, OuterBottom);
        // 连接倒角和墙体
        AddQuad(OuterBevelTop, InnerBevelTop, InnerTop, OuterTop);
        AddQuad(OuterBevelBottom, OuterBottom, InnerBottom, InnerBevelBottom);
    }
}


int32 FHollowPrismBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal)
{
    // 不计算UV，让UE4自动生成
    return FModelGenMeshBuilder::GetOrAddVertex(Pos, Normal);
}

int32 FHollowPrismBuilder::GetOrAddVertexWithUV(const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    return FModelGenMeshBuilder::GetOrAddVertex(Pos, Normal, UV);
}

//~ Begin UV Functions

FVector2D FHollowPrismBuilder::CalculateWallUV(float Angle, float Z, bool bIsOuter) const
{
    // 壁面UV映射：U基于角度，V基于高度
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    const float HalfHeight = HollowPrism.GetHalfHeight();
    
    // 计算基础角度和高度
    float BaseU = 0.0f;
    float BaseV = 0.0f;
    
    if (ArcAngleRadians > 0.0f)
    {
        const float AngleRange = (Angle - StartAngle) / ArcAngleRadians;
        const float ClampedRange = FMath::Clamp(AngleRange, 0.0f, 1.0f);
        BaseU = ClampedRange; // 0 到 1
    }
    else
    {
        BaseU = 0.5f; // 如果弧角为0，使用中间值
    }
    
    // 考虑倒角缩减后的实际壁面高度范围
    const float ActualWallHeight = 2.0f * (HalfHeight - HollowPrism.BevelRadius);
    const float ActualWallBottom = -HalfHeight + HollowPrism.BevelRadius;
    
    float HeightRange = 0.5f; // 默认中间值
    if (ActualWallHeight > KINDA_SMALL_NUMBER)
    {
        HeightRange = (Z - ActualWallBottom) / ActualWallHeight;
        HeightRange = FMath::Clamp(HeightRange, 0.0f, 1.0f);
    }
    BaseV = HeightRange;
    
    // 内外侧面UV区域分配：[0.2, 0.5] x [0, 1]
    // 外侧面：[0.2, 0.5] x [0, 0.5] - 上方
    // 内侧面：[0.2, 0.5] x [0.5, 1] - 下方
    
    if (bIsOuter)
    {
        // 外侧面：[0.2, 0.5] x [0, 0.5] - 上方
        const float U = 0.2f + BaseU * 0.3f; // 0.2 到 0.5
        const float V = BaseV * 0.5f; // 0 到 0.5
        return FVector2D(U, V);
    }
    else
    {
        // 内侧面：[0.2, 0.5] x [0.5, 1] - 下方
        const float U = 0.2f + BaseU * 0.3f; // 0.2 到 0.5
        const float V = 0.5f + BaseV * 0.5f; // 0.5 到 1
        return FVector2D(U, V);
    }
}

FVector2D FHollowPrismBuilder::CalculateCapUV(float Angle, float Radius, bool bIsTop) const
{
    // 端盖UV映射：基于角度和半径的极坐标映射
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    
    // 确保角度在正确范围内
    float NormalizedAngle = (Angle - StartAngle) / ArcAngleRadians;
    NormalizedAngle = FMath::Clamp(NormalizedAngle, 0.0f, 1.0f);
    
    // 使用简单的线性映射避免扭曲
    // U坐标：基于角度线性映射
    const float U = NormalizedAngle; // 0 到 1
    
    // V坐标：基于半径线性映射
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    const float V = RadiusRange > 0.0f ? 
        FMath::Clamp((Radius - HollowPrism.InnerRadius) / RadiusRange, 0.0f, 1.0f) : 0.5f;
    
    // 上下底Caps使用UV区域：[0, 0.2] x [0, 1]
    // 上底：[0, 0.2] x [0, 0.5]
    // 下底：[0, 0.2] x [0.5, 1]
    const float CapU = U * 0.2f; // 0 到 0.2
    const float CapV = bIsTop ? 
        V * 0.5f : // 上底：0 到 0.5
        0.5f + V * 0.5f; // 下底：0.5 到 1
    
    return FVector2D(CapU, CapV);
}

FVector2D FHollowPrismBuilder::CalculateBevelUV(float Angle, float Alpha, bool bIsInner, bool bIsTop) const
{
    // 倒角UV映射：U基于角度，V基于倒角参数Alpha
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    
    // 确保角度在正确范围内
    float NormalizedAngle = (Angle - StartAngle) / ArcAngleRadians;
    NormalizedAngle = FMath::Clamp(NormalizedAngle, 0.0f, 1.0f);
    
    const float U = NormalizedAngle;
    const float V = FMath::Clamp(Alpha, 0.0f, 1.0f); // Alpha从0到1
    
    // 倒角UV区域分配：[0.5, 0.7] x [0, 1]
    // 上倒角：[0.5, 0.7] x [0, 0.5]
    // 下倒角：[0.5, 0.7] x [0.5, 1]
    const float BevelU = 0.5f + U * 0.2f; // 0.5 到 0.7
    const float BevelV = bIsTop ? 
        V * 0.5f : // 上倒角：0 到 0.5
        0.5f + V * 0.5f; // 下倒角：0.5 到 1
    
    return FVector2D(BevelU, BevelV);
}

FVector2D FHollowPrismBuilder::CalculateEndCapUV(float Angle, float Z, bool bIsStart) const
{
    // 端盖面UV映射：基于角度和高度
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    
    // 确保角度在正确范围内
    // 对于端盖，角度应该映射到 [0, 1] 范围
    float NormalizedAngle;
    if (bIsStart)
    {
        // 起始端盖：角度从 -ArcAngle/2 到 -ArcAngle/2，映射到 [0, 1]
        NormalizedAngle = 0.0f; // 起始端盖始终使用角度0
    }
    else
    {
        // 结束端盖：角度从 ArcAngle/2 到 ArcAngle/2，映射到 [0, 1]
        NormalizedAngle = 1.0f; // 结束端盖始终使用角度1
    }
    
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float V = FMath::Clamp((Z + HalfHeight) / (2.0f * HalfHeight), 0.0f, 1.0f);
    
    // 端盖面UV区域分配：[0.7, 1] x [0, 1] - 重新分配避免重叠
    // 起始端盖：[0.7, 0.85] x [0, 1] - 基于角度分布U坐标
    // 结束端盖：[0.85, 1] x [0, 1] - 基于角度分布U坐标
    const float EndCapU = bIsStart ? 
        0.7f + (1.0f - NormalizedAngle) * 0.15f : // 起始端盖：镜像映射 0.7 到 0.85
        0.85f + NormalizedAngle * 0.15f; // 结束端盖：正常映射 0.85 到 1
    const float EndCapV = V; // 0 到 1
    
    return FVector2D(EndCapU, EndCapV);
}

FVector2D FHollowPrismBuilder::CalculateEndCapUVWithRadius(float Angle, float Z, float Radius, bool bIsStart) const
{
    // 端盖面UV映射：基于角度、高度和半径
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    
    // 确保角度在正确范围内
    // 对于端盖，角度应该映射到 [0, 1] 范围
    float NormalizedAngle;
    if (bIsStart)
    {
        // 起始端盖：角度从 -ArcAngle/2 到 -ArcAngle/2，映射到 [0, 1]
        NormalizedAngle = 0.0f; // 起始端盖始终使用角度0
    }
    else
    {
        // 结束端盖：角度从 ArcAngle/2 到 ArcAngle/2，映射到 [0, 1]
        NormalizedAngle = 1.0f; // 结束端盖始终使用角度1
    }
    
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float V = FMath::Clamp((Z + HalfHeight) / (2.0f * HalfHeight), 0.0f, 1.0f);
    
    // 基于半径计算U坐标的偏移，区分内壁和外壁
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    const float RadiusOffset = (Radius - HollowPrism.InnerRadius) / FMath::Max(RadiusRange, 0.001f);
    const float RadiusOffsetClamped = FMath::Clamp(RadiusOffset, 0.0f, 1.0f);
    
    // 端盖面UV区域分配：[0.7, 1] x [0, 1] - 重新分配避免重叠
    // 起始端盖：[0.7, 0.85] x [0, 1] - 基于半径分布U坐标
    // 结束端盖：[0.85, 1] x [0, 1] - 基于半径分布U坐标
    
    // 基于半径计算U坐标，让内壁和外壁有不同的U坐标
    float BaseU;
    if (bIsStart)
    {
        // 起始端盖：U坐标从0.7到0.85，基于半径分布
        BaseU = 0.7f + RadiusOffsetClamped * 0.15f;
    }
    else
    {
        // 结束端盖：U坐标从0.85到1，基于半径分布
        BaseU = 0.85f + RadiusOffsetClamped * 0.15f;
    }
    
    const float EndCapU = FMath::Clamp(BaseU, 0.0f, 1.0f);
    const float EndCapV = V; // 0 到 1
    
    return FVector2D(EndCapU, EndCapV);
}