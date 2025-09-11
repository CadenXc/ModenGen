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

    // 生成主体几何体
    GenerateInnerWalls();
    GenerateOuterWalls();
    
    // 如果没有倒角，则直接生成平坦的顶盖和底盖
    if (HollowPrism.BevelRadius <= 0.0f)
    {
        GenerateTopCapWithTriangles();
        GenerateBottomCapWithTriangles();
    }
    // 如果有倒角，则生成倒角几何体，它会与墙体连接
    else
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

void FHollowPrismBuilder::GenerateInnerWalls()
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(HollowPrism.InnerSides);
    const bool bFull = HollowPrism.IsFullCircle();
    
    TArray<int32> InnerTopVertices, InnerBottomVertices;
    
    for (int32 i = 0; i <= HollowPrism.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 如果有倒角，墙体的高度需要缩减
        const float WallTopZ = HalfHeight - HollowPrism.BevelRadius;
        const float WallBottomZ = -HalfHeight + HollowPrism.BevelRadius;

        const FVector InnerPos = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, WallTopZ);
        const FVector InnerNormal = FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f).GetSafeNormal();
        
        const int32 InnerTopVertex = GetOrAddVertex(InnerPos, InnerNormal);
        InnerTopVertices.Add(InnerTopVertex);
        
        const FVector InnerBottomPos = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, WallBottomZ);
        const int32 InnerBottomVertex = GetOrAddVertex(InnerBottomPos, InnerNormal);
        InnerBottomVertices.Add(InnerBottomVertex);
    }
    
    if (bFull && InnerTopVertices.Num() > 0)
    {
        InnerTopVertices.Last() = InnerTopVertices[0];
        InnerBottomVertices.Last() = InnerBottomVertices[0];
    }
    
    for (int32 i = 0; i < HollowPrism.InnerSides; ++i)
    {
        AddQuad(InnerTopVertices[i], InnerBottomVertices[i], InnerBottomVertices[i + 1], InnerTopVertices[i + 1]);
    }
}

void FHollowPrismBuilder::GenerateOuterWalls()
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(HollowPrism.OuterSides);
    const bool bFull = HollowPrism.IsFullCircle();
    
    TArray<int32> OuterTopVertices, OuterBottomVertices;
    
    for (int32 i = 0; i <= HollowPrism.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;

        const float WallTopZ = HalfHeight - HollowPrism.BevelRadius;
        const float WallBottomZ = -HalfHeight + HollowPrism.BevelRadius;
        
        const FVector OuterPos = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, WallTopZ);
        const FVector OuterNormal = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f).GetSafeNormal();
        
        const int32 OuterTopVertex = GetOrAddVertex(OuterPos, OuterNormal);
        OuterTopVertices.Add(OuterTopVertex);
        
        const FVector OuterBottomPos = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, WallBottomZ);
        const int32 OuterBottomVertex = GetOrAddVertex(OuterBottomPos, OuterNormal);
        OuterBottomVertices.Add(OuterBottomVertex);
    }

    if (bFull && OuterTopVertices.Num() > 0)
    {
        OuterTopVertices.Last() = OuterTopVertices[0];
        OuterBottomVertices.Last() = OuterBottomVertices[0];
    }
    
    for (int32 i = 0; i < HollowPrism.OuterSides; ++i)
    {
        // 确保外墙的顶点顺序是顺时针的
        AddQuad(OuterTopVertices[i], OuterTopVertices[i + 1], OuterBottomVertices[i + 1], OuterBottomVertices[i]);
    }
}

void FHollowPrismBuilder::GenerateTopCapWithTriangles()
{
    TArray<int32> InnerVertices, OuterVertices;
    GenerateCapVertices(InnerVertices, OuterVertices, true);
    GenerateCapTriangles(InnerVertices, OuterVertices, true);
}

void FHollowPrismBuilder::GenerateBottomCapWithTriangles()
{
    TArray<int32> InnerVertices, OuterVertices;
    GenerateCapVertices(InnerVertices, OuterVertices, false);
    GenerateCapTriangles(InnerVertices, OuterVertices, false);
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
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float EndAngle = ArcAngleRadians / 2.0f;

    // 生成起始端盖和结束端盖
    GenerateEndCap(StartAngle, true);
    GenerateEndCap(EndAngle, false);
}

void FHollowPrismBuilder::GenerateEndCap(float Angle, bool IsStart)
{
    // [FIX]: 计算正确的端盖法线，它应该与棱柱的弧线相切
    const FVector Normal = IsStart ?
        FVector(FMath::Sin(Angle), -FMath::Cos(Angle), 0.0f) :
        FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0f);

    TArray<int32> OrderedVertices;
    GenerateEndCapColumn(Angle, Normal.GetSafeNormal(), OrderedVertices);
    GenerateEndCapTriangles(OrderedVertices, IsStart);
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
    for (int32 i = 0; i <= HollowPrism.BevelSegments; ++i)
    {
        TArray<int32> CurrentRing;
        GenerateBevelRing(CurrentRing, bIsTop, bIsInner, i, HollowPrism.BevelSegments);

        if (i > 0)
        {
            ConnectBevelRings(PrevRing, CurrentRing, bIsInner, bIsTop);
        }
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

// [REFACTORED]
void FHollowPrismBuilder::GenerateEndCapColumn(float Angle, const FVector& Normal, TArray<int32>& OutOrderedVertices)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float BevelRadius = HollowPrism.BevelRadius;
    const int32 BevelSegments = HollowPrism.BevelSegments;

    // 定义端盖需要生成的垂直分段总数，包括倒角部分
    const int32 NumCoreSegments = 1; 
    const int32 TotalVerticalSegments = BevelRadius > 0 ? (BevelSegments * 2 + NumCoreSegments) : NumCoreSegments;

    OutOrderedVertices.Empty();
    OutOrderedVertices.Reserve((TotalVerticalSegments + 1) * 2);

    for (int32 i = 0; i <= TotalVerticalSegments; ++i)
    {
        const float t = static_cast<float>(i) / TotalVerticalSegments; // 从上到下 (0 to 1) 的插值因子
        const float CurrentZ = FMath::Lerp(HalfHeight, -HalfHeight, t);

        float CurrentInnerRadius = HollowPrism.InnerRadius;
        float CurrentOuterRadius = HollowPrism.OuterRadius;
        
        // 如果存在倒角，根据Z值计算当前截面的内外半径
        if (BevelRadius > 0.0f)
        {
            const float TopBevelLimit = HalfHeight - BevelRadius;
            const float BottomBevelLimit = -HalfHeight + BevelRadius;

            if (CurrentZ > TopBevelLimit) // 顶部倒角区域
            {
                const float BevelAlpha = (HalfHeight - CurrentZ) / BevelRadius;
                const float BevelAngle = BevelAlpha * HALF_PI;
                CurrentInnerRadius = (HollowPrism.InnerRadius + BevelRadius) - FMath::Cos(BevelAngle) * BevelRadius;
                CurrentOuterRadius = (HollowPrism.OuterRadius - BevelRadius) + FMath::Cos(BevelAngle) * BevelRadius;
            }
            else if (CurrentZ < BottomBevelLimit) // 底部倒角区域
            {
                const float BevelAlpha = (CurrentZ - (-HalfHeight)) / BevelRadius;
                const float BevelAngle = BevelAlpha * HALF_PI;
                CurrentInnerRadius = HollowPrism.InnerRadius + FMath::Sin(BevelAngle) * BevelRadius;
                CurrentOuterRadius = HollowPrism.OuterRadius - FMath::Sin(BevelAngle) * BevelRadius;
            }
        }
        
        // 根据计算出的半径和Z值创建内外顶点
        const FVector InnerPos = CalculateVertexPosition(CurrentInnerRadius, Angle, CurrentZ);
        const FVector OuterPos = CalculateVertexPosition(CurrentOuterRadius, Angle, CurrentZ);

        // 按 外侧 -> 内侧 的顺序添加，方便后续三角化
        OutOrderedVertices.Add(GetOrAddVertex(OuterPos, Normal));
        OutOrderedVertices.Add(GetOrAddVertex(InnerPos, Normal));
    }
}

// [REFACTORED]
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

// UV生成已移除 - 让UE4自动处理UV生成

int32 FHollowPrismBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal)
{
    // 不计算UV，让UE4自动生成
    return FModelGenMeshBuilder::GetOrAddVertex(Pos, Normal);
}
