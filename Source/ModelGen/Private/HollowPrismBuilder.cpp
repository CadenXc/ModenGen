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
    
    // 计算是否启用倒角
    bEnableBevel = HollowPrism.BevelRadius > 0.0f && HollowPrism.BevelSegments > 0;

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
    GenerateWalls(HollowPrism.InnerRadius, HollowPrism.InnerSides, EInnerOuter::Inner);
    GenerateWalls(HollowPrism.OuterRadius, HollowPrism.OuterSides, EInnerOuter::Outer);

    // 生成顶盖和底盖
    TArray<int32> InnerVertices, OuterVertices;
    
    // 生成顶盖
    GenerateCapVertices(InnerVertices, OuterVertices, EHeightPosition::Top);
    GenerateCapTriangles(InnerVertices, OuterVertices, EHeightPosition::Top);
    
    // 清空数组并生成底盖
    InnerVertices.Empty();
    OuterVertices.Empty();
    GenerateCapVertices(InnerVertices, OuterVertices, EHeightPosition::Bottom);
    GenerateCapTriangles(InnerVertices, OuterVertices, EHeightPosition::Bottom);

    // 生成倒角几何体（如果启用倒角）
    if (bEnableBevel)
    {
        GenerateBevelGeometry(EHeightPosition::Top, EInnerOuter::Inner);   // 顶部内环倒角
        GenerateBevelGeometry(EHeightPosition::Top, EInnerOuter::Outer);  // 顶部外环倒角
        GenerateBevelGeometry(EHeightPosition::Bottom, EInnerOuter::Inner);   // 底部内环倒角
        GenerateBevelGeometry(EHeightPosition::Bottom, EInnerOuter::Outer);  // 底部外环倒角
    }

    // 如果不是一个完整的360度圆环，则需要生成端盖来封闭开口
    if (!HollowPrism.IsFullCircle())
    {
        // 使用记录的顶点索引生成端盖
        GenerateEndCapWithBevel(EEndCapType::Start);   // 起始端盖
        GenerateEndCapWithBevel(EEndCapType::End);  // 结束端盖
    }

    if (!ValidateGeneratedData())
    {
        return false;
    }

    // 计算正确的切线（用于法线贴图等）
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

void FHollowPrismBuilder::GenerateWalls(float Radius, int32 Sides, EInnerOuter InnerOuter)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Sides);
    const bool bFull = HollowPrism.IsFullCircle();

    TArray<int32> TopVertices, BottomVertices;

    for (int32 i = 0; i <= Sides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;

        // 如果启用倒角，墙体的高度需要缩减
        const float WallTopZ = bEnableBevel ? (HalfHeight - HollowPrism.BevelRadius) : HalfHeight;
        const float WallBottomZ = bEnableBevel ? (-HalfHeight + HollowPrism.BevelRadius) : -HalfHeight;

        const FVector TopPos = CalculateVertexPosition(Radius, Angle, WallTopZ);
        const FVector BottomPos = CalculateVertexPosition(Radius, Angle, WallBottomZ);

        // 法线方向：内壁向内，外壁向外
        const FVector Normal = (InnerOuter == EInnerOuter::Inner)
            ? FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f).GetSafeNormal()
            : FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f).GetSafeNormal();

        // 计算内外侧面UV坐标
        const FVector2D TopUV = CalculateWallUV(Angle, WallTopZ, InnerOuter);
        const FVector2D BottomUV = CalculateWallUV(Angle, WallBottomZ, InnerOuter);
        
        const int32 TopVertex = GetOrAddVertex(TopPos, Normal, TopUV);
        const int32 BottomVertex = GetOrAddVertex(BottomPos, Normal, BottomUV);

        TopVertices.Add(TopVertex);
        BottomVertices.Add(BottomVertex);

        // 记录与倒角连接的顶点（如果启用倒角）
        if (bEnableBevel)
        {
            if (InnerOuter == EInnerOuter::Inner)
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

        // 记录起始和结束位置的顶点索引（仅非完整圆形需要端盖）
        if (!bFull)
        {
            if (i == 0) // 起始位置
            {
                if (InnerOuter == EInnerOuter::Inner)
                {
                    StartInnerCapIndices.Add(TopVertex);    // 内壁顶部
                    StartInnerCapIndices.Add(BottomVertex); // 内壁底部
                }
                else
                {
                    StartOuterCapIndices.Add(TopVertex);    // 外壁顶部
                    StartOuterCapIndices.Add(BottomVertex); // 外壁底部
                }
            }
            else if (i == Sides) // 结束位置
            {
                if (InnerOuter == EInnerOuter::Inner)
                {
                    EndInnerCapIndices.Add(TopVertex);    // 内壁顶部
                    EndInnerCapIndices.Add(BottomVertex); // 内壁底部
                }
                else
                {
                    EndOuterCapIndices.Add(TopVertex);    // 外壁顶部
                    EndOuterCapIndices.Add(BottomVertex); // 外壁底部
                }
            }
        }
    }

    // 生成四边形面
    for (int32 i = 0; i < Sides; ++i)
    {
        if (InnerOuter == EInnerOuter::Inner)
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


void FHollowPrismBuilder::GenerateCapVertices(TArray<int32>& OutInnerVertices,
    TArray<int32>& OutOuterVertices,
    EHeightPosition HeightPosition)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    // 上下面的Z值保持不变
    const float CurrentHight = (HeightPosition == EHeightPosition::Top) ? HalfHeight : -HalfHeight;
    
    // 端盖面的法线应该垂直于端盖表面
    // 端盖面是水平的，法线应该垂直向上/向下
    const float ZSign = (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f;
    FVector Normal(0.0f, 0.0f, ZSign);

    const float StartAngle = CalculateStartAngle();
    const float InnerAngleStep = CalculateAngleStep(HollowPrism.InnerSides);
    const float OuterAngleStep = CalculateAngleStep(HollowPrism.OuterSides);

    // 生成内环顶点（调整半径以考虑倒角）
    for (int32 i = 0; i <= HollowPrism.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * InnerAngleStep;
        // 内环半径：启用倒角时增加倒角半径，否则使用原始半径
        const float AdjustedInnerRadius = bEnableBevel ? (HollowPrism.InnerRadius + HollowPrism.BevelRadius) : HollowPrism.InnerRadius;
        const FVector InnerPos = CalculateVertexPosition(AdjustedInnerRadius, Angle, CurrentHight);
        const FVector2D InnerUV = CalculateCapUV(Angle, AdjustedInnerRadius, HeightPosition);
        OutInnerVertices.Add(GetOrAddVertex(InnerPos, Normal, InnerUV));
    }

    // 生成外环顶点（调整半径以考虑倒角）
    for (int32 i = 0; i <= HollowPrism.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * OuterAngleStep;
        // 外环半径：启用倒角时减少倒角半径，否则使用原始半径
        const float AdjustedOuterRadius = bEnableBevel ? (HollowPrism.OuterRadius - HollowPrism.BevelRadius) : HollowPrism.OuterRadius;
        const FVector OuterPos = CalculateVertexPosition(AdjustedOuterRadius, Angle, CurrentHight);
        const FVector2D OuterUV = CalculateCapUV(Angle, AdjustedOuterRadius, HeightPosition);
        OutOuterVertices.Add(GetOrAddVertex(OuterPos, Normal, OuterUV));
    }

    // 如果是完整圆形，生成位置相同但UV不同的新顶点来闭合
    if (!HollowPrism.IsFullCircle())
    {
        // 记录顶面/底面的起始和结束顶点用于端盖生成
        if (OutInnerVertices.Num() > 0 && OutOuterVertices.Num() > 0)
        {
            if (HeightPosition == EHeightPosition::Top)
            {
                // 顶面：记录起始和结束顶点
                StartOuterCapIndices.Add(OutOuterVertices[0]);           // 外壁起始
                StartInnerCapIndices.Add(OutInnerVertices[0]);           // 内壁起始
                EndOuterCapIndices.Add(OutOuterVertices.Last());         // 外壁结束
                EndInnerCapIndices.Add(OutInnerVertices.Last());         // 内壁结束
            }
        }
    }
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
    // 将原点移动到最底面（底部Z=0）：所有顶点向上偏移HalfHeight
    const float HalfHeight = HollowPrism.GetHalfHeight();
    return FVector(
        Radius * FMath::Cos(Angle),
        Radius * FMath::Sin(Angle),
        Z + HalfHeight
    );
}

void FHollowPrismBuilder::GenerateCapTriangles(const TArray<int32>& InnerVertices, const TArray<int32>& OuterVertices, EHeightPosition HeightPosition)
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

        if (HeightPosition == EHeightPosition::Top)
        {
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV2], OuterVertices[OuterV1]);
            AddTriangle(InnerVertices[InnerV1], InnerVertices[InnerV2], OuterVertices[OuterV2]);
        }
        else // 底盖为逆时针 (Counter-Clockwise) - 从下方看
        {
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV1], OuterVertices[OuterV2]);
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV2], InnerVertices[InnerV2]);
        }
    }
}


void FHollowPrismBuilder::GenerateBevelGeometry(EHeightPosition HeightPosition, EInnerOuter InnerOuter)
{
    if (!bEnableBevel) return;

    TArray<int32> PrevRing;
    
    // 第一个环重新生成，不共用壁面顶点
    GenerateBevelRing(PrevRing, HeightPosition, InnerOuter, 0, HollowPrism.BevelSegments);
    
    for (int32 i = 1; i <= HollowPrism.BevelSegments; ++i)
    {
        TArray<int32> CurrentRing;
        GenerateBevelRing(CurrentRing, HeightPosition, InnerOuter, i, HollowPrism.BevelSegments);

        // 连接前一个环和当前环
        ConnectBevelRings(PrevRing, CurrentRing, InnerOuter, HeightPosition);
        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::GenerateBevelRing(TArray<int32>& OutCurrentRing,
    EHeightPosition HeightPosition, EInnerOuter InnerOuter,
    int32 RingIndex, int32 TotalRings)
{
    const float Alpha = static_cast<float>(RingIndex) / TotalRings;
    const float RingAngle = Alpha * HALF_PI; // 倒角从0到90度

    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float ZSign = (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f;

    float CurrentZ, CurrentRadius;
    
    if (RingIndex == 0)
    {
        // 第一个环：与壁面连接处，重新生成顶点位置
        CurrentZ = HalfHeight * ZSign - HollowPrism.BevelRadius * ZSign;
        CurrentRadius = (InnerOuter == EInnerOuter::Inner) ? HollowPrism.InnerRadius : HollowPrism.OuterRadius;
    }
    else
    {
        // 其他环：倒角圆弧上的点
        const float CenterZ = HalfHeight * ZSign - HollowPrism.BevelRadius * ZSign;
        const float CenterRadius = (InnerOuter == EInnerOuter::Inner) ?
            HollowPrism.InnerRadius + HollowPrism.BevelRadius :
            HollowPrism.OuterRadius - HollowPrism.BevelRadius;

        CurrentZ = CenterZ + FMath::Sin(RingAngle) * HollowPrism.BevelRadius * ZSign;
        const float RadiusFactor = FMath::Cos(RingAngle) * HollowPrism.BevelRadius;
        CurrentRadius = (InnerOuter == EInnerOuter::Inner) ? CenterRadius - RadiusFactor : CenterRadius + RadiusFactor;
    }

    const int32 Sides = (InnerOuter == EInnerOuter::Inner) ? HollowPrism.InnerSides : HollowPrism.OuterSides;
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Sides);

    OutCurrentRing.Reserve(Sides + 1);

    for (int32 s = 0; s <= Sides; ++s)
    {
        const float SideAngle = StartAngle + s * AngleStep;

        const FVector Position = CalculateVertexPosition(CurrentRadius, SideAngle, CurrentZ);
        const FVector Normal = CalculateBevelNormal(SideAngle, Alpha, InnerOuter, HeightPosition);
        const FVector2D UV = CalculateBevelUV(SideAngle, Alpha, InnerOuter, HeightPosition);
        const int32 VertexIndex = GetOrAddVertex(Position, Normal, UV);
        OutCurrentRing.Add(VertexIndex);

        // 记录倒角端盖顶点（仅非完整圆形需要端盖）
        if (!HollowPrism.IsFullCircle())
        {
            if (s == 0) // 起始位置
            {
                if (InnerOuter == EInnerOuter::Inner)
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
                if (InnerOuter == EInnerOuter::Inner)
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
}

FVector FHollowPrismBuilder::CalculateBevelNormal(float Angle, float Alpha, EInnerOuter InnerOuter, EHeightPosition HeightPosition) const
{
    // 计算倒角面的正确法线
    // 倒角面法线应该从倒角圆弧中心指向表面顶点
    const float RingAngle = Alpha * HALF_PI;
    const float ZSign = (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f;

    // 倒角圆弧的中心位置
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float CenterZ = HalfHeight * ZSign - HollowPrism.BevelRadius * ZSign;
    const float CenterRadius = (InnerOuter == EInnerOuter::Inner) ?
        HollowPrism.InnerRadius + HollowPrism.BevelRadius :
        HollowPrism.OuterRadius - HollowPrism.BevelRadius;

    // 当前顶点的位置
    const float CurrentZ = CenterZ + FMath::Sin(RingAngle) * HollowPrism.BevelRadius * ZSign;
    const float RadiusFactor = FMath::Cos(RingAngle) * HollowPrism.BevelRadius;
    const float CurrentRadius = (InnerOuter == EInnerOuter::Inner) ? CenterRadius - RadiusFactor : CenterRadius + RadiusFactor;

    // 计算从圆弧中心到顶点的方向向量
    FVector CenterPos(FMath::Cos(Angle) * CenterRadius, FMath::Sin(Angle) * CenterRadius, CenterZ);
    FVector VertexPos(FMath::Cos(Angle) * CurrentRadius, FMath::Sin(Angle) * CurrentRadius, CurrentZ);
    FVector Normal = (VertexPos - CenterPos).GetSafeNormal();

    return Normal;
}


void FHollowPrismBuilder::ConnectBevelRings(const TArray<int32>& PrevRing,
    const TArray<int32>& CurrentRing,
    EInnerOuter InnerOuter, EHeightPosition HeightPosition)
{
    const int32 Sides = (InnerOuter == EInnerOuter::Inner) ? HollowPrism.InnerSides : HollowPrism.OuterSides;

    for (int32 s = 0; s < Sides; ++s)
    {
        const int32 V00 = PrevRing[s];
        const int32 V10 = CurrentRing[s];
        const int32 V01 = PrevRing[s + 1];
        const int32 V11 = CurrentRing[s + 1];

        // 统一内外环和顶底的顶点顺序逻辑
        if ((HeightPosition == EHeightPosition::Top && InnerOuter == EInnerOuter::Outer) || 
            (HeightPosition == EHeightPosition::Bottom && InnerOuter == EInnerOuter::Inner))
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
void FHollowPrismBuilder::GenerateEndCapColumn(float Angle, const FVector& Normal, TArray<int32>& OutOrderedVertices, EEndCapType EndCapType)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float BevelRadius = HollowPrism.BevelRadius;
    const int32 BevelSegments = HollowPrism.BevelSegments;

    OutOrderedVertices.Empty();

    // Handle the case with no bevels separately for clarity and efficiency.
    if (!bEnableBevel)
    {
        const FVector OuterTop = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, HalfHeight);
        const FVector InnerTop = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, HalfHeight);
        const FVector OuterBottom = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, -HalfHeight);
        const FVector InnerBottom = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, -HalfHeight);

        // Add vertices in top-to-bottom, outer-then-inner order with UV.
        const FVector2D OuterTopUV = CalculateEndCapUVWithRadius(Angle, HalfHeight, HollowPrism.OuterRadius, EndCapType);
        const FVector2D InnerTopUV = CalculateEndCapUVWithRadius(Angle, HalfHeight, HollowPrism.InnerRadius, EndCapType);
        const FVector2D OuterBottomUV = CalculateEndCapUVWithRadius(Angle, -HalfHeight, HollowPrism.OuterRadius, EndCapType);
        const FVector2D InnerBottomUV = CalculateEndCapUVWithRadius(Angle, -HalfHeight, HollowPrism.InnerRadius, EndCapType);
        
        OutOrderedVertices.Add(GetOrAddVertex(OuterTop, Normal, OuterTopUV));
        OutOrderedVertices.Add(GetOrAddVertex(InnerTop, Normal, InnerTopUV));
        OutOrderedVertices.Add(GetOrAddVertex(OuterBottom, Normal, OuterBottomUV));
        OutOrderedVertices.Add(GetOrAddVertex(InnerBottom, Normal, InnerBottomUV));
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
        const FVector2D OuterUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Outer, EndCapType);
        const FVector2D InnerUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Inner, EndCapType);

        OutOrderedVertices.Add(GetOrAddVertex(OuterPos, Normal, OuterUV));
        OutOrderedVertices.Add(GetOrAddVertex(InnerPos, Normal, InnerUV));
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
        const FVector2D OuterUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Outer, EndCapType);
        const FVector2D InnerUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Inner, EndCapType);

        OutOrderedVertices.Add(GetOrAddVertex(OuterPos, Normal, OuterUV));
        OutOrderedVertices.Add(GetOrAddVertex(InnerPos, Normal, InnerUV));
    }
}


// [REFACTORED] -> This function is correct and does not need changes.
void FHollowPrismBuilder::GenerateEndCapTriangles(const TArray<int32>& OrderedVertices, EEndCapType EndCapType)
{
    // 顶点是以 [Outer0, Inner0, Outer1, Inner1, ...] 的顺序存储的
    for (int32 i = 0; i < OrderedVertices.Num() - 3; i += 2)
    {
        const int32 V_Outer_Curr = OrderedVertices[i];
        const int32 V_Inner_Curr = OrderedVertices[i + 1];
        const int32 V_Outer_Next = OrderedVertices[i + 2];
        const int32 V_Inner_Next = OrderedVertices[i + 3];

        if (EndCapType == EEndCapType::Start)
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



void FHollowPrismBuilder::GenerateEndCapWithBevel(EEndCapType EndCapType)
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float Angle = (EndCapType == EEndCapType::Start) ? -ArcAngleRadians / 2.0f : ArcAngleRadians / 2.0f;
    const FVector Normal = (EndCapType == EEndCapType::Start) ? 
        FVector(FMath::Sin(Angle), -FMath::Cos(Angle), 0.0f).GetSafeNormal() :
        FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0f).GetSafeNormal();
    
    TArray<int32> OrderedVertices;
    GenerateEndCapColumn(Angle, Normal, OrderedVertices, EndCapType);
    GenerateEndCapTriangles(OrderedVertices, EndCapType);
}


void FHollowPrismBuilder::GenerateEndCapWithBevelVertices(const TArray<int32>& RecordedVertices, EEndCapType EndCapType)
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
    if (EndCapType == EEndCapType::Start)
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

//~ Begin UV Functions

FVector2D FHollowPrismBuilder::CalculateWallUV(float Angle, float Z, EInnerOuter InnerOuter) const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    const float HalfHeight = HollowPrism.GetHalfHeight();
    
    float BaseU = 0.0f;
    if (ArcAngleRadians > 0.0f)
    {
        const float AngleRange = (Angle - StartAngle) / ArcAngleRadians;
        BaseU = FMath::Clamp(AngleRange, 0.0f, 1.0f);
    }
    else
    {
        BaseU = 0.5f;
    }
    
    const float TotalHeight = 2.0f * HalfHeight;
    // 只有在启用倒角时才计算倒角高度
    const float TopBevelHeight = bEnableBevel ? HollowPrism.BevelRadius : 0.0f;
    const float WallHeight = TotalHeight - 2.0f * TopBevelHeight;
    const float OuterCircumference = 2.0f * PI * HollowPrism.OuterRadius;
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    
    const float WallVScale = WallHeight / OuterCircumference;
    const float BevelVScale = TopBevelHeight / OuterCircumference;
    const float CapVScale = RadiusRange / OuterCircumference;
    
    const float OuterWallVOffset = 0.0f;
    const float OuterTopBevelVOffset = OuterWallVOffset + WallVScale;
    const float TopCapVOffset = OuterTopBevelVOffset + BevelVScale;
    const float InnerTopBevelVOffset = TopCapVOffset + CapVScale;
    const float InnerWallVOffset = InnerTopBevelVOffset + BevelVScale;
    const float InnerBottomBevelVOffset = InnerWallVOffset + WallVScale;
    const float BottomCapVOffset = InnerBottomBevelVOffset + BevelVScale;
    const float OuterBottomBevelVOffset = BottomCapVOffset + CapVScale;
    
    float V_Start, V_End;
    if (InnerOuter == EInnerOuter::Outer)
    {
        V_Start = OuterWallVOffset;
        V_End = OuterWallVOffset + WallVScale;
    }
    else
    {
        V_Start = InnerWallVOffset;
        V_End = InnerWallVOffset + WallVScale;
    }
    
    // 只有在启用倒角时才缩减墙体高度
    const float ActualWallHeight = bEnableBevel ? 2.0f * (HalfHeight - HollowPrism.BevelRadius) : 2.0f * HalfHeight;
    const float ActualWallBottom = bEnableBevel ? (-HalfHeight + HollowPrism.BevelRadius) : -HalfHeight;
    
    float V = 0.5f;
    if (ActualWallHeight > KINDA_SMALL_NUMBER)
    {
        const float HeightRatio = (Z - ActualWallBottom) / ActualWallHeight;
        V = HeightRatio;
    }
    
    const float ActualV = V_Start + FMath::Clamp(V, 0.0f, 1.0f) * (V_End - V_Start);
    
    return FVector2D(BaseU, ActualV);
}

FVector2D FHollowPrismBuilder::CalculateCapUV(float Angle, float Radius, EHeightPosition HeightPosition) const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    
    // 条状UV映射：将盖子展开成矩形条带
    // U坐标：沿着角度方向，从0到1
    const float U = (Angle - StartAngle) / ArcAngleRadians;
    const float NormalizedU = FMath::Clamp(U, 0.0f, 1.0f);
    
    // V坐标：沿着径向方向，从内半径到外半径
    // UV 应该基于原始半径范围计算，即使传入的 Radius 可能是调整后的（用于倒角）
    // 如果启用了倒角，将调整后的半径映射回原始半径范围
    float EffectiveRadius = Radius;
    
    if (bEnableBevel)
    {
        // 将调整后的半径映射回原始半径范围
        const float AdjustedInnerRadius = HollowPrism.InnerRadius + HollowPrism.BevelRadius;
        const float AdjustedOuterRadius = HollowPrism.OuterRadius - HollowPrism.BevelRadius;
        
        // 如果 Radius 接近调整后的内环半径，映射到原始内环（V=0）
        if (FMath::IsNearlyEqual(Radius, AdjustedInnerRadius, KINDA_SMALL_NUMBER))
        {
            EffectiveRadius = HollowPrism.InnerRadius;
        }
        // 如果 Radius 接近调整后的外环半径，映射到原始外环（V=1）
        else if (FMath::IsNearlyEqual(Radius, AdjustedOuterRadius, KINDA_SMALL_NUMBER))
        {
            EffectiveRadius = HollowPrism.OuterRadius;
        }
        // 如果 Radius 在调整范围内，线性映射回原始范围
        else if (Radius >= AdjustedInnerRadius && Radius <= AdjustedOuterRadius)
        {
            const float AdjustedRange = AdjustedOuterRadius - AdjustedInnerRadius;
            if (AdjustedRange > KINDA_SMALL_NUMBER)
            {
                const float Alpha = (Radius - AdjustedInnerRadius) / AdjustedRange;
                EffectiveRadius = HollowPrism.InnerRadius + Alpha * (HollowPrism.OuterRadius - HollowPrism.InnerRadius);
            }
        }
        // 如果 Radius 超出调整范围，可能是特殊情况，直接使用原始范围计算
        // （这种情况理论上不应该发生，但为了安全起见保留）
    }
    
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    const float V = (RadiusRange > KINDA_SMALL_NUMBER) ? 
        ((EffectiveRadius - HollowPrism.InnerRadius) / RadiusRange) : 0.5f;
    const float NormalizedV = FMath::Clamp(V, 0.0f, 1.0f);
    
    // 计算V值偏移，确保与其他部分连续
    // 只有在启用倒角时才使用倒角高度
    const float TotalHeight = 2.0f * HollowPrism.GetHalfHeight();
    const float BevelHeight = bEnableBevel ? HollowPrism.BevelRadius : 0.0f;
    const float WallHeight = TotalHeight - 2.0f * BevelHeight;
    const float OuterCircumference = 2.0f * PI * HollowPrism.OuterRadius;
    
    const float WallVScale = WallHeight / OuterCircumference;
    const float BevelVScale = BevelHeight / OuterCircumference;
    const float CapVScale = RadiusRange / OuterCircumference;
    
    const float OuterWallVOffset = 0.0f;
    const float OuterTopBevelVOffset = OuterWallVOffset + WallVScale;
    const float TopCapVOffset = OuterTopBevelVOffset + BevelVScale;
    const float InnerTopBevelVOffset = TopCapVOffset + CapVScale;
    const float InnerWallVOffset = InnerTopBevelVOffset + BevelVScale;
    const float InnerBottomBevelVOffset = InnerWallVOffset + WallVScale;
    const float BottomCapVOffset = InnerBottomBevelVOffset + BevelVScale;
    const float OuterBottomBevelVOffset = BottomCapVOffset + CapVScale;
    
    float V_Start, V_End;
    if (HeightPosition == EHeightPosition::Top)
    {
        V_Start = TopCapVOffset;
        V_End = TopCapVOffset + CapVScale;
    }
    else
    {
        V_Start = BottomCapVOffset;
        V_End = BottomCapVOffset + CapVScale;
    }
    
    // 条状映射：U使用完整范围[0,1]，V在分配的范围内连续分布
    const float ActualV = V_Start + NormalizedV * (V_End - V_Start);
    
    return FVector2D(NormalizedU, ActualV);
}

FVector2D FHollowPrismBuilder::CalculateBevelUV(float Angle, float Alpha, EInnerOuter InnerOuter, EHeightPosition HeightPosition) const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    
    float NormalizedAngle = (Angle - StartAngle) / ArcAngleRadians;
    NormalizedAngle = FMath::Clamp(NormalizedAngle, 0.0f, 1.0f);
    
    // 只有在启用倒角时才使用倒角高度
    const float TotalHeight = 2.0f * HollowPrism.GetHalfHeight();
    const float BevelHeight = bEnableBevel ? HollowPrism.BevelRadius : 0.0f;
    const float WallHeight = TotalHeight - 2.0f * BevelHeight;
    const float OuterCircumference = 2.0f * PI * HollowPrism.OuterRadius;
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    
    const float BevelVScale = BevelHeight / OuterCircumference;
    const float WallVScale = WallHeight / OuterCircumference;
    const float CapVScale = RadiusRange / OuterCircumference;
    
    const float OuterWallVOffset = 0.0f;
    const float OuterTopBevelVOffset = OuterWallVOffset + WallVScale;
    const float TopCapVOffset = OuterTopBevelVOffset + BevelVScale;
    const float InnerTopBevelVOffset = TopCapVOffset + CapVScale;
    const float InnerWallVOffset = InnerTopBevelVOffset + BevelVScale;
    const float InnerBottomBevelVOffset = InnerWallVOffset + WallVScale;
    const float BottomCapVOffset = InnerBottomBevelVOffset + BevelVScale;
    const float OuterBottomBevelVOffset = BottomCapVOffset + CapVScale;
    
    float V_Start, V_End;
    if (HeightPosition == EHeightPosition::Top)
    {
        if (InnerOuter == EInnerOuter::Outer)
        {
            V_Start = OuterTopBevelVOffset;
            V_End = OuterTopBevelVOffset + BevelVScale;
        }
        else
        {
            V_Start = InnerTopBevelVOffset;
            V_End = InnerTopBevelVOffset + BevelVScale;
        }
    }
    else
    {
        if (InnerOuter == EInnerOuter::Outer)
        {
            V_Start = OuterBottomBevelVOffset;
            V_End = OuterBottomBevelVOffset + BevelVScale;
        }
        else
        {
            V_Start = InnerBottomBevelVOffset;
            V_End = InnerBottomBevelVOffset + BevelVScale;
        }
    }
    
    const float U = NormalizedAngle;
    const float V = V_Start + FMath::Clamp(Alpha, 0.0f, 1.0f) * (V_End - V_Start);
    
    return FVector2D(U, V);
}

FVector2D FHollowPrismBuilder::CalculateEndCapUVWithRadius(float Angle, float Z, float Radius, EEndCapType EndCapType) const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    
    float NormalizedAngle;
    if (EndCapType == EEndCapType::Start)
    {
        NormalizedAngle = 0.0f;
    }
    else
    {
        NormalizedAngle = 1.0f;
    }
    
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float V = FMath::Clamp((Z + HalfHeight) / (2.0f * HalfHeight), 0.0f, 1.0f);
    
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    const float RadiusOffset = (Radius - HollowPrism.InnerRadius) / FMath::Max(RadiusRange, 0.001f);
    const float RadiusOffsetClamped = FMath::Clamp(RadiusOffset, 0.0f, 1.0f);
    
    float BaseU;
    if (EndCapType == EEndCapType::Start)
    {
        BaseU = V * 0.2f;
    }
    else
    {
        BaseU = V * 0.2f;
    }
    
    const float EndCapU = FMath::Clamp(BaseU, 0.0f, 1.0f);
    const float EndCapV = (EndCapType == EEndCapType::Start) ? 
        0.7f + RadiusOffsetClamped * 0.15f :
        0.85f + RadiusOffsetClamped * 0.15f;
    
    return FVector2D(EndCapU, EndCapV);
}