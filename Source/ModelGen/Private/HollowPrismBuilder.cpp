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
    if (!ValidateParameters())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    // 仅生成一次，避免重复添加三角形
    GenerateSideWalls();
    GenerateTopCapWithTriangles();
    GenerateBottomCapWithTriangles();

    if (HollowPrism.BevelRadius > 0.0f)
    {
        GenerateTopBevelGeometry();
        GenerateBottomBevelGeometry();
    }

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

bool FHollowPrismBuilder::ValidateParameters() const
{
    return HollowPrism.IsValid();
}

int32 FHollowPrismBuilder::CalculateVertexCountEstimate() const
{
    return HollowPrism.CalculateVertexCountEstimate();
}

int32 FHollowPrismBuilder::CalculateTriangleCountEstimate() const
{
    return HollowPrism.CalculateTriangleCountEstimate();
}

void FHollowPrismBuilder::GenerateSideWalls()
{
    GenerateInnerWalls();
    GenerateOuterWalls();
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
        
        const FVector InnerPos = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, HalfHeight - HollowPrism.BevelRadius);
        const FVector InnerNormal = FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f);
        
        const int32 InnerTopVertex = GetOrAddVertexWithDualUV(InnerPos, InnerNormal);
        InnerTopVertices.Add(InnerTopVertex);
        
        const FVector InnerBottomPos = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, -HalfHeight + HollowPrism.BevelRadius);
        const int32 InnerBottomVertex = GetOrAddVertexWithDualUV(InnerBottomPos, InnerNormal);
        InnerBottomVertices.Add(InnerBottomVertex);
    }
    // 满圆时让最后一个顶点复用第一个，避免缝隙
    if (bFull && InnerTopVertices.Num() > 0)
    {
        InnerTopVertices.Last() = InnerTopVertices[0];
    }
    if (bFull && InnerBottomVertices.Num() > 0)
    {
        InnerBottomVertices.Last() = InnerBottomVertices[0];
    }
    
    for (int32 i = 0; i < HollowPrism.InnerSides; ++i)
    {
        AddQuad(InnerTopVertices[i], InnerBottomVertices[i], 
               InnerBottomVertices[i + 1], InnerTopVertices[i + 1]);
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
        
        const FVector OuterPos = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, HalfHeight - HollowPrism.BevelRadius);
        const FVector OuterNormal = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        
        const int32 OuterTopVertex = GetOrAddVertexWithDualUV(OuterPos, OuterNormal);
        OuterTopVertices.Add(OuterTopVertex);
        
        const FVector OuterBottomPos = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, -HalfHeight + HollowPrism.BevelRadius);
        const int32 OuterBottomVertex = GetOrAddVertexWithDualUV(OuterBottomPos, OuterNormal);
        OuterBottomVertices.Add(OuterBottomVertex);
    }
    // 满圆时闭合
    if (bFull && OuterTopVertices.Num() > 0)
    {
        OuterTopVertices.Last() = OuterTopVertices[0];
    }
    if (bFull && OuterBottomVertices.Num() > 0)
    {
        OuterBottomVertices.Last() = OuterBottomVertices[0];
    }
    
    for (int32 i = 0; i < HollowPrism.OuterSides; ++i)
    {
        AddQuad(OuterTopVertices[i], OuterTopVertices[i + 1], 
               OuterBottomVertices[i + 1], OuterBottomVertices[i]);
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
    if (HollowPrism.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
    {
        return; // 完整环形，不需要端盖
    }

    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float EndAngle = ArcAngleRadians / 2.0f;

    // 生成起始端盖和结束端盖
    GenerateEndCap(StartAngle, FVector(-1, 0, 0), true);   // 起始端盖
    GenerateEndCap(EndAngle, FVector(1, 0, 0), false);      // 结束端盖
}

void FHollowPrismBuilder::GenerateEndCap(float Angle, const FVector& Normal, bool IsStart)
{
    TArray<int32> OrderedVertices;
    GenerateEndCapVertices(Angle, Normal, IsStart, OrderedVertices);
    GenerateEndCapTriangles(OrderedVertices, IsStart);
}

float FHollowPrismBuilder::CalculateStartAngle() const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    return -ArcAngleRadians / 2.0f;
}

float FHollowPrismBuilder::CalculateAngleStep(int32 Sides) const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    return ArcAngleRadians / Sides;
}

float FHollowPrismBuilder::CalculateInnerRadius(bool bIncludeBevel) const
{
    if (bIncludeBevel)
    {
        return HollowPrism.InnerRadius + HollowPrism.BevelRadius;
    }
    return HollowPrism.InnerRadius;
}

float FHollowPrismBuilder::CalculateOuterRadius(bool bIncludeBevel) const
{
    if (bIncludeBevel)
    {
        return HollowPrism.OuterRadius - HollowPrism.BevelRadius;
    }
    return HollowPrism.OuterRadius;
}

FVector FHollowPrismBuilder::CalculateVertexPosition(float Radius, float Angle, float Z) const
{
    return FVector(
        Radius * FMath::Cos(Angle),
        Radius * FMath::Sin(Angle),
        Z
    );
}

void FHollowPrismBuilder::GenerateCapTriangles(const TArray<int32>& InnerVertices, 
                                              const TArray<int32>& OuterVertices, 
                                              bool bIsTopCap)
{
    const int32 MaxSides = FMath::Max(HollowPrism.InnerSides, HollowPrism.OuterSides);
    
    for (int32 i = 0; i < MaxSides; ++i)
    {
        const float CurrentAngleRatio = static_cast<float>(i) / MaxSides;
        
        const float InnerIndex = CurrentAngleRatio * HollowPrism.InnerSides;
        const float OuterIndex = CurrentAngleRatio * HollowPrism.OuterSides;
        
        const float NextAngleRatio = static_cast<float>(i + 1) / MaxSides;
        const float NextInnerIndex = NextAngleRatio * HollowPrism.InnerSides;
        const float NextOuterIndex = NextAngleRatio * HollowPrism.OuterSides;
        
        const int32 InnerIndexFloor = FMath::FloorToInt(InnerIndex);
        const int32 InnerIndexCeil = FMath::Min(InnerIndexFloor + 1, HollowPrism.InnerSides);
        const int32 NextInnerIndexFloor = FMath::FloorToInt(NextInnerIndex);
        const int32 NextInnerIndexCeil = FMath::Min(NextInnerIndexFloor + 1, HollowPrism.InnerSides);
        
        const int32 OuterIndexFloor = FMath::FloorToInt(OuterIndex);
        const int32 OuterIndexCeil = FMath::Min(OuterIndexFloor + 1, HollowPrism.OuterSides);
        const int32 NextOuterIndexFloor = FMath::FloorToInt(NextOuterIndex);
        const int32 NextOuterIndexCeil = FMath::Min(NextOuterIndexFloor + 1, HollowPrism.OuterSides);
        
        const float InnerWeight = InnerIndex - InnerIndexFloor;
        const int32 BestInnerVertex = (InnerWeight < 0.5f) ? InnerIndexFloor : InnerIndexCeil;
        
        const float NextInnerWeight = NextInnerIndex - NextInnerIndexFloor;
        const int32 BestNextInnerVertex = (NextInnerWeight < 0.5f) ? NextInnerIndexFloor : NextInnerIndexCeil;
        
        const float OuterWeight = OuterIndex - OuterIndexFloor;
        const int32 BestOuterVertex = (OuterWeight < 0.5f) ? OuterIndexFloor : OuterIndexCeil;
        
        const float NextOuterWeight = NextOuterIndex - NextOuterIndexFloor;
        const int32 BestNextOuterVertex = (NextOuterWeight < 0.5f) ? NextOuterIndexFloor : NextOuterIndexCeil;
        
        if (bIsTopCap)
        {
            AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestNextOuterVertex], OuterVertices[BestOuterVertex]);
            AddTriangle(InnerVertices[BestInnerVertex], InnerVertices[BestNextInnerVertex], OuterVertices[BestNextOuterVertex]);
        }
        else
        {
            AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestOuterVertex], OuterVertices[BestNextOuterVertex]);
            AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestNextOuterVertex], InnerVertices[BestNextInnerVertex]);
        }
    }
}

void FHollowPrismBuilder::GenerateCapVertices(TArray<int32>& OutInnerVertices, 
                                             TArray<int32>& OutOuterVertices, 
                                             bool bIsTopCap)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float StartAngle = CalculateStartAngle();
    
    const float InnerAngleStep = CalculateAngleStep(HollowPrism.InnerSides);
    const float OuterAngleStep = CalculateAngleStep(HollowPrism.OuterSides);
    const bool bFull = HollowPrism.IsFullCircle();
    
    const FVector Normal(0.0f, 0.0f, bIsTopCap ? 1.0f : -1.0f);
    
    const float InnerRadius = CalculateInnerRadius(true);
    const float OuterRadius = CalculateOuterRadius(true);
    
    OutInnerVertices.Empty();
    OutOuterVertices.Empty();
    
    OutInnerVertices.Reserve(HollowPrism.InnerSides + 1);
    OutOuterVertices.Reserve(HollowPrism.OuterSides + 1);
    
    for (int32 i = 0; i <= HollowPrism.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * InnerAngleStep;
        
        const FVector InnerPos = CalculateVertexPosition(InnerRadius, Angle, bIsTopCap ? HalfHeight : -HalfHeight);
        const int32 InnerVertex = GetOrAddVertexWithDualUV(InnerPos, Normal);
        OutInnerVertices.Add(InnerVertex);
    }
    if (bFull && OutInnerVertices.Num() > 0)
    {
        OutInnerVertices.Last() = OutInnerVertices[0];
    }
    
    for (int32 i = 0; i <= HollowPrism.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * OuterAngleStep;
        
        const FVector OuterPos = CalculateVertexPosition(OuterRadius, Angle, bIsTopCap ? HalfHeight : -HalfHeight);
        const int32 OuterVertex = GetOrAddVertexWithDualUV(OuterPos, Normal);
        OutOuterVertices.Add(OuterVertex);
    }
    if (bFull && OutOuterVertices.Num() > 0)
    {
        OutOuterVertices.Last() = OutOuterVertices[0];
    }
}

void FHollowPrismBuilder::GenerateBevelGeometry(bool bIsTop, bool bIsInner)
{
    const float BevelRadiusLocal = HollowPrism.BevelRadius;
    const int32 BevelSectionsLocal = HollowPrism.BevelSegments;

    if (BevelRadiusLocal <= 0.0f || BevelSectionsLocal <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= BevelSectionsLocal; ++i)
    {
        TArray<int32> CurrentRing;
        GenerateBevelRing(PrevRing, CurrentRing, bIsTop, bIsInner, i, BevelSectionsLocal);

        if (i > 0 && PrevRing.Num() > 0)
        {
            ConnectBevelRings(PrevRing, CurrentRing, bIsInner, bIsTop);
        }

        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::GenerateBevelRing(const TArray<int32>& PrevRing, 
                                           TArray<int32>& OutCurrentRing,
                                           bool bIsTop, bool bIsInner, 
                                           int32 RingIndex, int32 TotalRings)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float BevelRadiusLocal = HollowPrism.BevelRadius;
    
    const float alpha = static_cast<float>(RingIndex) / TotalRings;
    
    const float ZOffset = bIsTop ? HalfHeight : -HalfHeight;
    const float ZDirection = bIsTop ? 1.0f : -1.0f;
    const float RadiusDirection = bIsInner ? 1.0f : -1.0f;
    
    const float StartRadius = bIsInner ? HollowPrism.InnerRadius : HollowPrism.OuterRadius;
    const float EndRadius = StartRadius + (RadiusDirection * BevelRadiusLocal);
    const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
    const float CurrentZ = FMath::Lerp(ZOffset - (ZDirection * BevelRadiusLocal), ZOffset, alpha);
    
    const int32 Sides = bIsInner ? HollowPrism.InnerSides : HollowPrism.OuterSides;
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float LocalAngleStep = ArcAngleRadians / Sides;
    
    OutCurrentRing.Empty();
    OutCurrentRing.Reserve(Sides + 1);
    
    for (int32 s = 0; s <= Sides; ++s)
    {
        const float angle = StartAngle + s * LocalAngleStep;

        const FVector Position = FVector(
            CurrentRadius * FMath::Cos(angle),
            CurrentRadius * FMath::Sin(angle),
            CurrentZ
        );

        const FVector Normal = CalculateBevelNormal(angle, alpha, bIsInner, bIsTop);

        OutCurrentRing.Add(GetOrAddVertexWithDualUV(Position, Normal));
    }
    if (HollowPrism.IsFullCircle() && OutCurrentRing.Num() > 0)
    {
        OutCurrentRing.Last() = OutCurrentRing[0];
    }
}

FVector FHollowPrismBuilder::CalculateBevelNormal(float Angle, float Alpha, bool bIsInner, bool bIsTop) const
{
    const FVector RadialDirection = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
    const FVector FaceNormal = FVector(0, 0, bIsTop ? 1.0f : -1.0f);
    
    // 根据是否为内环选择径向方向
    const FVector RadialComponent = bIsInner ? -RadialDirection : RadialDirection;
    
    // 在径向方向和面法线之间插值
    FVector Normal = FMath::Lerp(RadialComponent, FaceNormal, Alpha).GetSafeNormal();
    
    // 确保法线方向正确
    const float DotProduct = FVector::DotProduct(Normal, RadialDirection);
    if ((bIsInner && DotProduct > 0) || (!bIsInner && DotProduct < 0))
    {
        Normal = -Normal;
    }

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
        
        if (bIsInner)
        {
            if (bIsTop)
            {
                AddQuad(V00, V01, V11, V10);  // 顶面内环：逆时针
            }
            else
            {
                AddQuad(V00, V10, V11, V01);  // 底面内环：顺时针
            }
        }
        else
        {
            if (bIsTop)
            {
                AddQuad(V00, V10, V11, V01);  // 顶面外环：逆时针
            }
            else
            {
                AddQuad(V00, V01, V11, V10);  // 底面外环：顺时针
            }
        }
    }
}

void FHollowPrismBuilder::GenerateEndCapVertices(float Angle, const FVector& Normal, bool IsStart,
                                                TArray<int32>& OutOrderedVertices)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    
    OutOrderedVertices.Empty();
    
    const int32 TopCenterVertex = GetOrAddVertexWithDualUV(
        FVector(0, 0, HalfHeight),
        Normal
    );
    OutOrderedVertices.Add(TopCenterVertex);

    if (HollowPrism.BevelRadius > 0.0f)
    {
        GenerateEndCapBevelVertices(Angle, Normal, IsStart, true, OutOrderedVertices);
    }

    GenerateEndCapSideVertices(Angle, Normal, IsStart, OutOrderedVertices);

    if (HollowPrism.BevelRadius > 0.0f)
    {
        GenerateEndCapBevelVertices(Angle, Normal, IsStart, false, OutOrderedVertices);
    }

    const int32 BottomCenterVertex = GetOrAddVertexWithDualUV(
        FVector(0, 0, -HalfHeight),
        Normal
    );
    OutOrderedVertices.Add(BottomCenterVertex);
}

void FHollowPrismBuilder::GenerateEndCapBevelVertices(float Angle, const FVector& Normal, bool IsStart,
                                                     bool bIsTopBevel, TArray<int32>& OutVertices)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    float TopBevelHeight, BottomBevelHeight;
    CalculateEndCapBevelHeights(TopBevelHeight, BottomBevelHeight);
    
    float StartZ, EndZ;
    CalculateEndCapZRange(TopBevelHeight, BottomBevelHeight, StartZ, EndZ);
    
    const int32 BevelSections = HollowPrism.BevelSegments;
    const float StartZPos = bIsTopBevel ? HalfHeight : StartZ;
    const float EndZPos = bIsTopBevel ? EndZ : -HalfHeight;
    const int32 StartIndex = bIsTopBevel ? 0 : 1;
    const int32 EndIndex = bIsTopBevel ? BevelSections : BevelSections + 1;
    
    for (int32 i = StartIndex; i < EndIndex; ++i)
    {
        const float Alpha = static_cast<float>(i) / BevelSections;
        const float CurrentZ = FMath::Lerp(StartZPos, EndZPos, Alpha);
        
        float CurrentInnerRadius, CurrentOuterRadius;
        if (bIsTopBevel)
        {
            const float TopInnerRadius = HollowPrism.InnerRadius + HollowPrism.BevelRadius;
            const float TopOuterRadius = HollowPrism.OuterRadius - HollowPrism.BevelRadius;
            CurrentInnerRadius = FMath::Lerp(TopInnerRadius, HollowPrism.InnerRadius, Alpha);
            CurrentOuterRadius = FMath::Lerp(TopOuterRadius, HollowPrism.OuterRadius, Alpha);
        }
        else
        {
            CurrentInnerRadius = FMath::Lerp(HollowPrism.InnerRadius, HollowPrism.InnerRadius + HollowPrism.BevelRadius, Alpha);
            CurrentOuterRadius = FMath::Lerp(HollowPrism.OuterRadius, HollowPrism.OuterRadius - HollowPrism.BevelRadius, Alpha);
        }
        
        const FVector InnerBevelPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), 
                                            CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
        const int32 InnerBevelVertex = GetOrAddVertexWithDualUV(InnerBevelPos, Normal);
        OutVertices.Add(InnerBevelVertex);
        
        const FVector OuterBevelPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), 
                                            CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
        const int32 OuterBevelVertex = GetOrAddVertexWithDualUV(OuterBevelPos, Normal);
        OutVertices.Add(OuterBevelVertex);
    }
}

void FHollowPrismBuilder::GenerateEndCapSideVertices(float Angle, const FVector& Normal, bool IsStart,
                                                    TArray<int32>& OutVertices)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    float TopBevelHeight, BottomBevelHeight;
    CalculateEndCapBevelHeights(TopBevelHeight, BottomBevelHeight);
    
    float StartZ, EndZ;
    CalculateEndCapZRange(TopBevelHeight, BottomBevelHeight, StartZ, EndZ);
    
    for (int32 h = 0; h <= 1; ++h)
    {
        const float Z = FMath::Lerp(EndZ, StartZ, static_cast<float>(h) / 1.0f);

        const FVector InnerEdgePos = FVector(HollowPrism.InnerRadius * FMath::Cos(Angle), 
                                           HollowPrism.InnerRadius * FMath::Sin(Angle), Z);
        const int32 InnerEdgeVertex = GetOrAddVertexWithDualUV(InnerEdgePos, Normal);
        OutVertices.Add(InnerEdgeVertex);
        
        const FVector OuterEdgePos = FVector(HollowPrism.OuterRadius * FMath::Cos(Angle), 
                                           HollowPrism.OuterRadius * FMath::Sin(Angle), Z);
        const int32 OuterEdgeVertex = GetOrAddVertexWithDualUV(OuterEdgePos, Normal);
        OutVertices.Add(OuterEdgeVertex);
    }
}

void FHollowPrismBuilder::GenerateEndCapTriangles(const TArray<int32>& OrderedVertices, bool IsStart)
{
    for (int32 i = 0; i < OrderedVertices.Num() - 2; i += 2)
    {
        if (IsStart)
        {
            AddTriangle(OrderedVertices[i], OrderedVertices[i + 2], OrderedVertices[i + 1]);
            AddTriangle(OrderedVertices[i + 1], OrderedVertices[i + 2], OrderedVertices[i + 3]);
        }
        else
        {
            AddTriangle(OrderedVertices[i], OrderedVertices[i + 1], OrderedVertices[i + 2]);
            AddTriangle(OrderedVertices[i + 1], OrderedVertices[i + 3], OrderedVertices[i + 2]);
        }
    }
}

void FHollowPrismBuilder::CalculateEndCapBevelHeights(float& OutTopBevelHeight, float& OutBottomBevelHeight) const
{
    const float BevelRadiusLocal = HollowPrism.BevelRadius;
    const float MaxBevelHeight = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    
    OutTopBevelHeight = FMath::Min(BevelRadiusLocal, MaxBevelHeight);
    OutBottomBevelHeight = FMath::Min(BevelRadiusLocal, MaxBevelHeight);
}

void FHollowPrismBuilder::CalculateEndCapZRange(float TopBevelHeight, float BottomBevelHeight, 
                                               float& OutStartZ, float& OutEndZ) const
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    
    OutStartZ = -HalfHeight + BottomBevelHeight;
    OutEndZ = HalfHeight - TopBevelHeight;
}

FVector2D FHollowPrismBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    float Angle = FMath::Atan2(Y, X);
    if (Angle < 0) Angle += 2.0f * PI;
    
    float DistanceFromCenter = FMath::Sqrt(X * X + Y * Y);
    
    if (FMath::Abs(Normal.Z) > 0.9f)
    {
        // 顶底面：使用2U系统
        if (DistanceFromCenter < (HollowPrism.InnerRadius + HollowPrism.OuterRadius) * 0.5f)
        {
            // 内环：U从0.0到1.0
            float U = (Angle / (2.0f * PI));
            float V = 0.5f;
            return FVector2D(U, V);
        }
        else
        {
            // 外环：U从0.0到1.0
            float U = (Angle / (2.0f * PI));
            float V = 1.0f;
            return FVector2D(U, V);
        }
    }
    else
    {
        // 侧面：使用2U系统
        float U = (Angle / (2.0f * PI));  // 0到1
        float V = (Z + HollowPrism.GetHalfHeight()) / HollowPrism.Height;  // 0到1
        
        if (DistanceFromCenter < (HollowPrism.InnerRadius + HollowPrism.OuterRadius) * 0.5f)
        {
            // 内环：U从0.0到1.0
            return FVector2D(U, V);
        }
        else
        {
            // 外环：U从0.0到1.0
            return FVector2D(U, V);
        }
    }
}

FVector2D FHollowPrismBuilder::GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const
{
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    float Angle = FMath::Atan2(Y, X);
    if (Angle < 0) Angle += 2.0f * PI;
    
    float DistanceFromCenter = FMath::Sqrt(X * X + Y * Y);
    
    // 第二UV通道：使用传统UV系统（0-1范围）
    float U = Angle / (2.0f * PI);  // 0到1
    float V = (Z + HollowPrism.GetHalfHeight()) / HollowPrism.Height;  // 0到1
    
    // 根据半径调整V坐标，避免内外环重叠
    if (DistanceFromCenter < (HollowPrism.InnerRadius + HollowPrism.OuterRadius) * 0.5f)
    {
        V = V * 0.8f;  // 内环：0到0.8
    }
    else
    {
        V = 0.2f + V * 0.8f;  // 外环：0.2到1.0
    }
    
    return FVector2D(U, V);
}

int32 FHollowPrismBuilder::GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal)
{
    FVector2D MainUV = GenerateStableUVCustom(Pos, Normal);
    FVector2D SecondaryUV = GenerateSecondaryUV(Pos, Normal);
    
    return FModelGenMeshBuilder::GetOrAddVertexWithDualUV(Pos, Normal, MainUV, SecondaryUV);
}
