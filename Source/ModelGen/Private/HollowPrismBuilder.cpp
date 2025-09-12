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
    StartCapIndices.Empty();
    EndCapIndices.Empty();
    
    // 清空倒角连接顶点记录
    TopInnerBevelVertices.Empty();
    TopOuterBevelVertices.Empty();
    BottomInnerBevelVertices.Empty();
    BottomOuterBevelVertices.Empty();

    // 生成主体几何体
    GenerateInnerWalls();
    GenerateOuterWalls();

    // 有倒角时，生成带倒角的顶盖和底盖
    GenerateTopCapWithBevel();
    GenerateBottomCapWithBevel();

    // 生成倒角几何体（如果有倒角）
    if (HollowPrism.BevelRadius > 0.0f)
    {
        GenerateTopBevelGeometry();
        GenerateBottomBevelGeometry();
    }

    // 如果不是一个完整的360度圆环，则需要生成端盖来封闭开口
    if (!HollowPrism.IsFullCircle())
    {
        GenerateEndCaps();
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

void FHollowPrismBuilder::GenerateWalls(float Radius, int32 Sides, bool bIsInner)
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

        const int32 TopVertex = GetOrAddVertex(TopPos, Normal);
        const int32 BottomVertex = GetOrAddVertex(BottomPos, Normal);

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
                // 统一顺序：外壁顶点在前，内壁顶点在后
                if (bIsInner)
                {
                    // 内壁：添加到末尾
                    StartCapIndices.Add(BottomVertex);
                    StartCapIndices.Add(TopVertex);
                }
                else
                {
                    // 外壁：插入到开头
                    StartCapIndices.Insert(TopVertex, 0);
                    StartCapIndices.Insert(BottomVertex, 1);
                }
            }
            else if (i == Sides) // 结束位置
            {
                // 统一顺序：外壁顶点在前，内壁顶点在后
                if (bIsInner)
                {
                    // 内壁：添加到末尾
                    EndCapIndices.Add(TopVertex);
                    EndCapIndices.Add(BottomVertex);
                }
                else
                {
                    // 外壁：插入到开头
                    EndCapIndices.Insert(BottomVertex, 0);
                    EndCapIndices.Insert(TopVertex, 1);
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

void FHollowPrismBuilder::GenerateInnerWalls()
{
    GenerateWalls(HollowPrism.InnerRadius, HollowPrism.InnerSides, true);
}

void FHollowPrismBuilder::GenerateOuterWalls()
{
    GenerateWalls(HollowPrism.OuterRadius, HollowPrism.OuterSides, false);
}

void FHollowPrismBuilder::GenerateTopCapWithBevel()
{
    TArray<int32> InnerVertices, OuterVertices;
    GenerateCapVerticesWithBevel(InnerVertices, OuterVertices, true);
    GenerateCapTriangles(InnerVertices, OuterVertices, true);
}

void FHollowPrismBuilder::GenerateBottomCapWithBevel()
{
    TArray<int32> InnerVertices, OuterVertices;
    GenerateCapVerticesWithBevel(InnerVertices, OuterVertices, false);
    GenerateCapTriangles(InnerVertices, OuterVertices, false);
}

void FHollowPrismBuilder::GenerateCapVerticesWithBevel(TArray<int32>& OutInnerVertices,
    TArray<int32>& OutOuterVertices,
    bool bIsTopCap)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    // 上下面的Z值保持不变
    const float Z = bIsTopCap ? HalfHeight : -HalfHeight;
    const FVector Normal(0.0f, 0.0f, bIsTopCap ? 1.0f : -1.0f);

    const float StartAngle = CalculateStartAngle();
    const float InnerAngleStep = CalculateAngleStep(HollowPrism.InnerSides);
    const float OuterAngleStep = CalculateAngleStep(HollowPrism.OuterSides);

    // 生成内环顶点（调整半径以考虑倒角）
    for (int32 i = 0; i <= HollowPrism.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * InnerAngleStep;
        // 内环半径增加倒角半径
        const float AdjustedInnerRadius = HollowPrism.InnerRadius + HollowPrism.BevelRadius;
        const FVector InnerPos = CalculateVertexPosition(AdjustedInnerRadius, Angle, Z);
        OutInnerVertices.Add(GetOrAddVertex(InnerPos, Normal));
    }

    // 生成外环顶点（调整半径以考虑倒角）
    for (int32 i = 0; i <= HollowPrism.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * OuterAngleStep;
        // 外环半径减少倒角半径
        const float AdjustedOuterRadius = HollowPrism.OuterRadius - HollowPrism.BevelRadius;
        const FVector OuterPos = CalculateVertexPosition(AdjustedOuterRadius, Angle, Z);
        OutOuterVertices.Add(GetOrAddVertex(OuterPos, Normal));
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
}

void FHollowPrismBuilder::GenerateTopBevelGeometry()
{
    GenerateBevelGeometry(true, true);   // 顶部内环倒角
    GenerateBevelGeometry(true, false);  // 顶部外环倒角
}

void FHollowPrismBuilder::GenerateBottomBevelGeometry()
{
    GenerateBevelGeometry(false, true);   // 底部内环倒角
    GenerateBevelGeometry(false, false);  // 底部外环倒角
}

void FHollowPrismBuilder::GenerateEndCaps()
{
    if (HollowPrism.IsFullCircle())
    {
        return;
    }

    // 使用记录的顶点索引生成端盖
    GenerateAdvancedEndCaps();
}

void FHollowPrismBuilder::GenerateAdvancedEndCaps()
{
    // 使用记录的顶点索引生成端盖
    if (StartCapIndices.Num() > 0)
    {
        GenerateEndCapFromRecordedVertices(StartCapIndices, true);
    }
    
    if (EndCapIndices.Num() > 0)
    {
        GenerateEndCapFromRecordedVertices(EndCapIndices, false);
    }
}

// 已移除 GenerateEndCapFromIndices 函数
// 已移除 GenerateEndCap 函数

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

void FHollowPrismBuilder::GenerateCapTriangles(const TArray<int32>& InnerVertices, const TArray<int32>& OuterVertices, bool bIsTopCap)
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

void FHollowPrismBuilder::GenerateCapVertices(TArray<int32>& OutInnerVertices,
    TArray<int32>& OutOuterVertices,
    bool bIsTopCap)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float Z = bIsTopCap ? HalfHeight : -HalfHeight;
    const FVector Normal(0.0f, 0.0f, bIsTopCap ? 1.0f : -1.0f);

    const float StartAngle = CalculateStartAngle();
    const float InnerAngleStep = CalculateAngleStep(HollowPrism.InnerSides);
    const float OuterAngleStep = CalculateAngleStep(HollowPrism.OuterSides);

    // 生成内环顶点
    for (int32 i = 0; i <= HollowPrism.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * InnerAngleStep;
        const FVector InnerPos = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, Z);
        OutInnerVertices.Add(GetOrAddVertex(InnerPos, Normal));
    }

    // 生成外环顶点
    for (int32 i = 0; i <= HollowPrism.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * OuterAngleStep;
        const FVector OuterPos = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, Z);
        OutOuterVertices.Add(GetOrAddVertex(OuterPos, Normal));
    }

    // 如果是完整圆环，闭合顶点
    if (HollowPrism.IsFullCircle())
    {
        if (OutInnerVertices.Num() > 0) OutInnerVertices.Last() = OutInnerVertices[0];
        if (OutOuterVertices.Num() > 0) OutOuterVertices.Last() = OutOuterVertices[0];
    }
}

void FHollowPrismBuilder::GenerateBevelGeometry(bool bIsTop, bool bIsInner)
{
    if (HollowPrism.BevelRadius <= 0.0f || HollowPrism.BevelSegments <= 0) return;

    TArray<int32> PrevRing;
    
    // 第一个环使用记录的侧面顶点
    if (bIsTop)
    {
        PrevRing = bIsInner ? TopInnerBevelVertices : TopOuterBevelVertices;
    }
    else
    {
        PrevRing = bIsInner ? BottomInnerBevelVertices : BottomOuterBevelVertices;
    }
    
    for (int32 i = 1; i <= HollowPrism.BevelSegments; ++i)
    {
        TArray<int32> CurrentRing;
        GenerateBevelRing(CurrentRing, bIsTop, bIsInner, i, HollowPrism.BevelSegments);

        // 连接前一个环和当前环
        ConnectBevelRings(PrevRing, CurrentRing, bIsInner, bIsTop);
        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::GenerateBevelRing(TArray<int32>& OutCurrentRing,
    bool bIsTop, bool bIsInner,
    int32 RingIndex, int32 TotalRings)
{
    const float Alpha = static_cast<float>(RingIndex) / TotalRings;
    const float RingAngle = Alpha * HALF_PI; // 倒角从0到90度

    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float ZSign = bIsTop ? 1.0f : -1.0f;

    // 倒角圆弧的中心
    const float CenterZ = HalfHeight * ZSign - HollowPrism.BevelRadius * ZSign;
    const float CenterRadius = bIsInner ?
        HollowPrism.InnerRadius + HollowPrism.BevelRadius :
        HollowPrism.OuterRadius - HollowPrism.BevelRadius;

    // 当前环上的点的位置和法线
    const float CurrentZ = CenterZ + FMath::Sin(RingAngle) * HollowPrism.BevelRadius * ZSign;
    const float RadiusFactor = FMath::Cos(RingAngle) * HollowPrism.BevelRadius;
    const float CurrentRadius = bIsInner ? CenterRadius - RadiusFactor : CenterRadius + RadiusFactor;

    const int32 Sides = bIsInner ? HollowPrism.InnerSides : HollowPrism.OuterSides;
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Sides);

    OutCurrentRing.Reserve(Sides + 1);

    for (int32 s = 0; s <= Sides; ++s)
    {
        const float SideAngle = StartAngle + s * AngleStep;

        const FVector Position = CalculateVertexPosition(CurrentRadius, SideAngle, CurrentZ);
        const FVector Normal = CalculateBevelNormal(SideAngle, Alpha, bIsInner, bIsTop);
        OutCurrentRing.Add(GetOrAddVertex(Position, Normal));
    }

    if (HollowPrism.IsFullCircle())
    {
        OutCurrentRing.Last() = OutCurrentRing[0];
    }
}

FVector FHollowPrismBuilder::CalculateBevelNormal(float Angle, float Alpha, bool bIsInner, bool bIsTop) const
{
    // [OPTIMIZATION]: 采用更直接的几何方法计算法线
    // 法线方向是从倒角圆弧的中心指向顶点
    const float RingAngle = Alpha * HALF_PI;
    const float ZSign = bIsTop ? 1.0f : -1.0f;

    // 径向分量
    float RadialComponent = FMath::Cos(RingAngle);
    if (!bIsInner) RadialComponent = -RadialComponent;

    // Z轴分量
    const float ZComponent = FMath::Sin(RingAngle) * ZSign;

    FVector RadialDir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);

    // 合成最终法线
    FVector Normal = (RadialDir * RadialComponent + FVector(0, 0, ZComponent));

    return Normal.GetSafeNormal();
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
void FHollowPrismBuilder::GenerateEndCapColumn(float Angle, const FVector& Normal, TArray<int32>& OutOrderedVertices)
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

        // Add vertices in top-to-bottom, outer-then-inner order.
        OutOrderedVertices.Add(GetOrAddVertex(OuterTop, Normal));
        OutOrderedVertices.Add(GetOrAddVertex(InnerTop, Normal));
        OutOrderedVertices.Add(GetOrAddVertex(OuterBottom, Normal));
        OutOrderedVertices.Add(GetOrAddVertex(InnerBottom, Normal));
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

        OutOrderedVertices.Add(GetOrAddVertex(OuterPos, Normal));
        OutOrderedVertices.Add(GetOrAddVertex(InnerPos, Normal));
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

        OutOrderedVertices.Add(GetOrAddVertex(OuterPos, Normal));
        OutOrderedVertices.Add(GetOrAddVertex(InnerPos, Normal));
    }
}


// [REFACTORED] -> This function is correct and does not need changes.
void FHollowPrismBuilder::GenerateEndCapTriangles(const TArray<int32>& OrderedVertices, bool IsStart)
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

void FHollowPrismBuilder::GenerateEndCapFromRecordedVertices(const TArray<int32>& RecordedVertices, bool IsStart)
{
    if (RecordedVertices.Num() < 4)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateEndCapFromRecordedVertices - 记录的顶点数量不足，无法生成端盖"));
        return;
    }

    // 对于有倒角的情况，我们需要生成完整的端盖顶点
    if (HollowPrism.BevelRadius > 0.0f && HollowPrism.BevelSegments > 0)
    {
        // 有倒角时，使用原来的 GenerateEndCapColumn 方法
        const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
        const float Angle = IsStart ? -ArcAngleRadians / 2.0f : ArcAngleRadians / 2.0f;
        const FVector Normal = IsStart ? 
            FVector(FMath::Sin(Angle), -FMath::Cos(Angle), 0.0f).GetSafeNormal() :
            FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0f).GetSafeNormal();
        
        TArray<int32> OrderedVertices;
        GenerateEndCapColumn(Angle, Normal, OrderedVertices);
        GenerateEndCapTriangles(OrderedVertices, IsStart);
    }
    else
    {
        // 无倒角时，使用记录的顶点
        // 记录的顶点顺序：[外壁顶部, 外壁底部, 内壁顶部, 内壁底部]
        // 需要重新排列为：[外壁顶部, 内壁顶部, 外壁底部, 内壁底部] 的顺序
        TArray<int32> OrderedVertices;
        OrderedVertices.Reserve(RecordedVertices.Num());
        
        // 按照端盖需要的顺序重新排列顶点
        OrderedVertices.Add(RecordedVertices[0]); // 外壁顶部
        OrderedVertices.Add(RecordedVertices[2]); // 内壁顶部
        OrderedVertices.Add(RecordedVertices[1]); // 外壁底部
        OrderedVertices.Add(RecordedVertices[3]); // 内壁底部

        // 使用现有的三角形生成函数
        GenerateEndCapTriangles(OrderedVertices, IsStart);
    }
}

// UV生成已移除 - 让UE4自动处理UV生成

int32 FHollowPrismBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal)
{
    // 不计算UV，让UE4自动生成
    return FModelGenMeshBuilder::GetOrAddVertex(Pos, Normal);
}