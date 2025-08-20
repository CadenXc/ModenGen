// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"
#include "Frustum.h"
#include "ModelGenMeshData.h"

FFrustumBuilder::FFrustumBuilder(const AFrustum& InFrustum)
    : Frustum(InFrustum)
{
    Clear();
    CalculateAngles();
}

void FFrustumBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    ClearEndCapConnectionPoints();
}

bool FFrustumBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!ValidateParameters())
    {
        UE_LOG(LogTemp, Error, TEXT("FFrustumBuilder::Generate - Parameters validation failed"));
        return false;
    }

    Clear();
    ReserveMemory();

    GenerateBaseGeometry();
    CreateSideGeometry();
    GenerateTopGeometry();
    GenerateBottomGeometry();

    if (Frustum.BevelRadius > 0.0f)
    {
        GenerateTopBevelGeometry();
        GenerateBottomBevelGeometry();
    }

    if (Frustum.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps();
    }

    if (!ValidateGeneratedData())
    {
        UE_LOG(LogTemp, Error, TEXT("FFrustumBuilder::Generate - Generated data validation failed"));
        return false;
    }

    OutMeshData = MeshData;
    return true;
}

bool FFrustumBuilder::ValidateParameters() const
{
    return Frustum.IsValid();
}

int32 FFrustumBuilder::CalculateVertexCountEstimate() const
{
    return Frustum.CalculateVertexCountEstimate();
}

int32 FFrustumBuilder::CalculateTriangleCountEstimate() const
{
    return Frustum.CalculateTriangleCountEstimate();
}

void FFrustumBuilder::GenerateBaseGeometry()
{
	CreateSideGeometry();
    GenerateTopGeometry();
	GenerateTopBevelGeometry();
	GenerateBottomBevelGeometry();
    GenerateBottomGeometry();
	GenerateEndCaps();
}

void FFrustumBuilder::CreateSideGeometry()
{
    const float HalfHeight = Frustum.GetHalfHeight();
    const float TopBevelStartZ = HalfHeight - CalculateBevelHeight(Frustum.TopRadius);
    const float BottomBevelStartZ = -HalfHeight + CalculateBevelHeight(Frustum.BottomRadius);
    
    TArray<int32> TopRing = GenerateVertexRing(
        Frustum.TopRadius, 
        TopBevelStartZ, 
        Frustum.TopSides
    );
    
    TArray<int32> BottomRing = GenerateVertexRing(
        Frustum.BottomRadius, 
        BottomBevelStartZ, 
        Frustum.BottomSides
    );

    SideTopRing = TopRing;
    SideBottomRing = BottomRing;

    TArray<int32> TopRingOrigin = GenerateVertexRing(Frustum.TopRadius, HalfHeight, Frustum.TopSides);
    TArray<int32> BottomRingOrigin = GenerateVertexRing(Frustum.BottomRadius, -HalfHeight, Frustum.BottomSides);

    TArray<int32> BottomToTopMapping;
    BottomToTopMapping.SetNum(BottomRingOrigin.Num());
    for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
    {
        const float BottomRatio = static_cast<float>(BottomIndex) / BottomRingOrigin.Num();
        const int32 TopIndex = FMath::Clamp(FMath::RoundToInt(BottomRatio * TopRingOrigin.Num()), 0, TopRingOrigin.Num() - 1);
        BottomToTopMapping[BottomIndex] = TopIndex;
    }

    TArray<TArray<int32>> VertexRings;
    VertexRings.Add(BottomRing);

    if (Frustum.HeightSegments > 1)
    {
        const float HeightStep = Frustum.Height / Frustum.HeightSegments;

        for (int32 h = Frustum.HeightSegments - 1; h > 0; --h)
        {
            const float CurrentHeight = HalfHeight - h * HeightStep;
            const float HeightRatio = static_cast<float>(Frustum.HeightSegments - h) / Frustum.HeightSegments;

            TArray<int32> CurrentRing;

            for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
            {
                const int32 TopIndex = BottomToTopMapping[BottomIndex];

                const float XR = FMath::Lerp(
                    GetPosByIndex(BottomRingOrigin[BottomIndex]).X, GetPosByIndex(TopRingOrigin[TopIndex]).X, HeightRatio);
                const float YR = FMath::Lerp(
                    GetPosByIndex(BottomRingOrigin[BottomIndex]).Y, GetPosByIndex(TopRingOrigin[TopIndex]).Y, HeightRatio);

                const float BaseRadius = FMath::Lerp(Frustum.BottomRadius, Frustum.TopRadius, HeightRatio);
                const float BentRadius = CalculateBentRadius(BaseRadius, HeightRatio);

                const float Scale = BentRadius / BaseRadius;
                const float X = XR * Scale;
                const float Y = YR * Scale;

                const FVector InterpolatedPos(X, Y, CurrentHeight);

                FVector Normal = FVector(X, Y, 0.0f).GetSafeNormal();
                if (Normal.IsNearlyZero())
                {
                    Normal = FVector(1.0f, 0.0f, 0.0f);
                }
                
                if (Frustum.BendAmount > KINDA_SMALL_NUMBER)
                {
                    const float BendFactor = FMath::Sin(HeightRatio * PI);
                    const float NormalZ = -Frustum.BendAmount * FMath::Cos(HeightRatio * PI);
                    Normal = (Normal + FVector(0, 0, NormalZ)).GetSafeNormal();
                }

                const int32 VertexIndex = GetOrAddVertex(InterpolatedPos, Normal, GenerateStableUVCustom(InterpolatedPos, Normal));
                CurrentRing.Add(VertexIndex);
            }

            VertexRings.Add(CurrentRing);
        }
    }
    VertexRings.Add(TopRing);

    for (int32 i = VertexRings.Num() - 1; i >= 0; i--)
    {
        RecordEndCapConnectionPoint(VertexRings[i][0]);
    }

    for (int i = 0; i < VertexRings.Num() - 1; i++)
    {
        TArray<int32> CurrentRing = VertexRings[i];
        TArray<int32> NextRing = VertexRings[i + 1];

        for (int32 CurrentIndex = 0; CurrentIndex < CurrentRing.Num(); ++CurrentIndex)
        {
            const int32 NextCurrentIndex =
                (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? (CurrentIndex + 1) % CurrentRing.Num() : (CurrentIndex + 1);

            if (NextCurrentIndex >= CurrentRing.Num())
                continue;

            const float CurrentRatio = static_cast<float>(CurrentIndex) / CurrentRing.Num();
            const float NextCurrentRatio = static_cast<float>(NextCurrentIndex) / CurrentRing.Num();

            const int32 NextRingIndex = FMath::Clamp(FMath::RoundToInt(CurrentRatio * NextRing.Num()), 0, NextRing.Num() - 1);
            const int32 NextRingNextIndex =
                FMath::Clamp(FMath::RoundToInt(NextCurrentRatio * NextRing.Num()), 0, NextRing.Num() - 1);

            AddQuad(CurrentRing[CurrentIndex], NextRing[NextRingIndex], NextRing[NextRingNextIndex], CurrentRing[NextCurrentIndex]);
        }
    }
}

void FFrustumBuilder::GenerateTopGeometry()
{
    GenerateCapGeometry(Frustum.GetHalfHeight(), Frustum.TopSides, Frustum.TopRadius, true);
}

void FFrustumBuilder::GenerateBottomGeometry()
{
    GenerateCapGeometry(-Frustum.GetHalfHeight(), Frustum.BottomSides, Frustum.BottomRadius, false);
}

void FFrustumBuilder::GenerateTopBevelGeometry()
{
    if (Frustum.BevelRadius <= 0.0f)
    {
        return;
    }

    GenerateBevelGeometry(true);
}

void FFrustumBuilder::GenerateBottomBevelGeometry()
{
    if (Frustum.BevelRadius <= 0.0f)
    {
        return;
    }

    GenerateBevelGeometry(false);
}

void FFrustumBuilder::GenerateEndCaps()
{
    if (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
    {
        return;
    }
    
    GenerateEndCap(StartAngle, FVector(-1, 0, 0), true);   
    GenerateEndCap(EndAngle, FVector(1, 0, 0), false);
}

void FFrustumBuilder::GenerateEndCap(float Angle, const FVector& Normal, bool IsStart)
{
    if (EndCapConnectionPoints.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateEndCap - %s端面连接点不足，无法生成端面"), IsStart ? TEXT("起始") : TEXT("结束"));
        return;
    }

    TArray<int32> RotatedConnectionPoints;
    RotatedConnectionPoints.Reserve(EndCapConnectionPoints.Num());

    for (int32 VertexIndex : EndCapConnectionPoints)
    {
        FVector OriginalPos = GetPosByIndex(VertexIndex);
        
        FVector EndCapPos;
        if (IsStart)
        {
            EndCapPos = OriginalPos;
        }
        else
        {
            const float Radius = FMath::Sqrt(OriginalPos.X * OriginalPos.X + OriginalPos.Y * OriginalPos.Y);
            const float CurrentAngle = FMath::Atan2(OriginalPos.Y, OriginalPos.X);
            
            const float RotationAngle = EndAngle - StartAngle;
            const float NewAngle = CurrentAngle + RotationAngle;
            
            const float NewX = Radius * FMath::Cos(NewAngle);
            const float NewY = Radius * FMath::Sin(NewAngle);
            EndCapPos = FVector(NewX, NewY, OriginalPos.Z);
        }
        
        FVector EndCapNormal = Normal;
        
        if (Frustum.BendAmount > KINDA_SMALL_NUMBER)
        {
            const float Z = EndCapPos.Z;
            const float HeightRatio = (Z + Frustum.GetHalfHeight()) / Frustum.Height;
            const float BendInfluence = FMath::Sin(HeightRatio * PI);
            
            FVector BendNormal = FVector(0, 0, -BendInfluence).GetSafeNormal();
            EndCapNormal = (EndCapNormal + BendNormal * Frustum.BendAmount).GetSafeNormal();
        }
        
        int32 NewVertexIndex = GetOrAddVertex(EndCapPos, EndCapNormal, GenerateStableUVCustom(EndCapPos, EndCapNormal));
        RotatedConnectionPoints.Add(NewVertexIndex);
    }

    GenerateEndCapTrianglesFromVertices(RotatedConnectionPoints, IsStart, Angle);
}

TArray<int32> FFrustumBuilder::GenerateVertexRing(float Radius, float Z, int32 Sides)
{
    TArray<int32> VertexRing;
    const float AngleStep = CalculateAngleStep(Sides);

    const int32 VertexCount = (Frustum.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides;

    for (int32 i = 0; i < VertexCount; ++i)
    {
        const float Angle = StartAngle + (i * AngleStep);
        
        const float X = Radius * FMath::Cos(Angle);
        const float Y = Radius * FMath::Sin(Angle);
        const FVector Pos(X, Y, Z);
        FVector Normal = FVector(X, Y, 0.0f).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector(1.0f, 0.0f, 0.0f);
        }

        const int32 VertexIndex = GetOrAddVertex(Pos, Normal, GenerateStableUVCustom(Pos, Normal));
        VertexRing.Add(VertexIndex);
    }

    return VertexRing;
}

void FFrustumBuilder::GenerateCapGeometry(float Z, int32 Sides, float Radius, bool bIsTop)
{
    FVector Normal(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);

    const FVector CenterPos(0.0f, 0.0f, Z);
    const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, GenerateStableUVCustom(CenterPos, Normal));

    const float AngleStep = CalculateAngleStep(Sides);

    for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
    {
        const float CurrentAngle = StartAngle + (SideIndex * AngleStep);
        const float NextAngle = StartAngle + ((SideIndex + 1) * AngleStep);

        const float CurrentRadius = Radius - Frustum.BevelRadius;

        const FVector CurrentPos(CurrentRadius * FMath::Cos(CurrentAngle), CurrentRadius * FMath::Sin(CurrentAngle), Z);
        const FVector NextPos(CurrentRadius * FMath::Cos(NextAngle), CurrentRadius * FMath::Sin(NextAngle), Z);

        FVector AdjustedNormal = Normal;
        if (Frustum.BevelRadius > KINDA_SMALL_NUMBER)
        {
            const float EdgeDistance = (Radius - CurrentRadius) / Frustum.BevelRadius;
            const float NormalBlend = FMath::Clamp(EdgeDistance, 0.0f, 1.0f);
            
            FVector SideNormal = FVector(FMath::Cos(CurrentAngle), FMath::Sin(CurrentAngle), 0.0f);
            AdjustedNormal = FMath::Lerp(Normal, SideNormal, NormalBlend).GetSafeNormal();
        }

        const int32 V1 = GetOrAddVertex(CurrentPos, AdjustedNormal, GenerateStableUVCustom(CurrentPos, AdjustedNormal));
        const int32 V2 = GetOrAddVertex(NextPos, AdjustedNormal, GenerateStableUVCustom(NextPos, AdjustedNormal));

        if (SideIndex == 0)
        {
            RecordEndCapConnectionPoint(V1);
        }

        if (bIsTop)
        {
            AddTriangle(CenterVertex, V2, V1);
        }
        else
        {
            AddTriangle(CenterVertex, V1, V2);
        }
    }
}

void FFrustumBuilder::GenerateBevelGeometry(bool bIsTop)
{
    const float HalfHeight = Frustum.GetHalfHeight();
    const float BevelRadius = Frustum.BevelRadius;
    const int32 BevelSections = Frustum.BevelSegments;
    
    if (BevelRadius <= 0.0f || BevelSections <= 0)
    {
        return;
    }

    const float Radius = bIsTop ? Frustum.TopRadius : Frustum.BottomRadius;
    const int32 Sides = bIsTop ? Frustum.TopSides : Frustum.BottomSides;
    const TArray<int32>& SideRing = bIsTop ? SideTopRing : SideBottomRing;
    
    const float StartZ = GetPosByIndex(SideRing[0]).Z;
    const float EndZ = bIsTop ? HalfHeight : -HalfHeight;

    TArray<int32> PrevRing;
    const float AngleStep = CalculateAngleStep(Sides);
    TArray<int32> StartRingForEndCap;

    for (int32 i = 0; i <= BevelSections; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSections;

        const float HeightRatio = (StartZ + HalfHeight) / Frustum.Height;
        const float RadiusAtZ = FMath::Lerp(Frustum.BottomRadius, Frustum.TopRadius, HeightRatio);
        const float StartRadius = FMath::Max(RadiusAtZ, KINDA_SMALL_NUMBER);
        
        const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);
        const float CurrentRadius = FMath::Lerp(StartRadius, CapRadius, alpha);
        const float CurrentZ = FMath::Lerp(StartZ, EndZ, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Sides + 1);

        const int32 VertexCount = (Frustum.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides;

        for (int32 s = 0; s <= VertexCount; ++s)
        {
            FVector Position;
            
            if (i == 0 && s < SideRing.Num())
            {
                Position = GetPosByIndex(SideRing[s]);
            }
            else
            {
                const float angle = StartAngle + (s * AngleStep);
                Position = FVector(CurrentRadius * FMath::Cos(angle), CurrentRadius * FMath::Sin(angle), CurrentZ);
            }

            FVector Normal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
            if (Normal.IsNearlyZero())
            {
                Normal = FVector(1.0f, 0.0f, 0.0f);
            }
            
            if (Frustum.BevelRadius > KINDA_SMALL_NUMBER)
            {
                const float VertexHeightRatio = (Position.Z + HalfHeight) / Frustum.Height;
                const float BevelInfluence = FMath::Clamp(VertexHeightRatio, 0.0f, 1.0f);
                
                FVector SideNormal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
                FVector CapNormal = FVector(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);
                
                Normal = FMath::Lerp(SideNormal, CapNormal, BevelInfluence).GetSafeNormal();
            }
            
            CurrentRing.Add(GetOrAddVertex(Position, Normal, GenerateStableUVCustom(Position, Normal)));
        }

        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];

                if (bIsTop)
                {
                    AddQuad(V00, V10, V11, V01);
                }
                else
                {
                    AddQuad(V00, V01, V11, V10);
                }
            }
        }
        PrevRing = CurrentRing;
        StartRingForEndCap.Add(CurrentRing[0]);
    }

    if (bIsTop)
    {
        for (int i = StartRingForEndCap.Num() - 1; i >= 0; i--)
        {
            RecordEndCapConnectionPoint(StartRingForEndCap[i]);
        }
    }
    else
    {
        for (int i = 0; i < StartRingForEndCap.Num(); i++)
        {
            RecordEndCapConnectionPoint(StartRingForEndCap[i]);
        }
    }
}



float FFrustumBuilder::CalculateBentRadius(float BaseRadius, float HeightRatio)
{
    const float BendFactor = FMath::Sin(HeightRatio * PI);
    const float BentRadius = BaseRadius + Frustum.BendAmount * BendFactor * BaseRadius;

    if (Frustum.MinBendRadius > KINDA_SMALL_NUMBER)
    {
        return FMath::Max(BentRadius, Frustum.MinBendRadius);
    }

    return FMath::Max(BentRadius, KINDA_SMALL_NUMBER);
}

float FFrustumBuilder::CalculateBevelHeight(float Radius)
{
    return FMath::Min(Frustum.BevelRadius, Radius);
}

float FFrustumBuilder::CalculateHeightRatio(float Z)
{
    const float HalfHeight = Frustum.GetHalfHeight();
    return (Z + HalfHeight) / Frustum.Height;
}

float FFrustumBuilder::CalculateAngleStep(int32 Sides)
{
    return ArcAngleRadians / Sides;
}



void FFrustumBuilder::GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, bool IsStart, float Angle)
{
    if (OrderedVertices.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateEndCapTrianglesFromVertices - 顶点数量不足，无法生成面"));
        return;
    }

    TArray<int32> SortedVertices = OrderedVertices;
    SortedVertices.Sort([this](const int32& A, const int32& B) -> bool
    {
        FVector PosA = GetPosByIndex(A);
        FVector PosB = GetPosByIndex(B);
        return PosA.Z > PosB.Z;
    });

    const FVector EndCapNormal = IsStart ? FVector(-1, 0, 0) : FVector(1, 0, 0);

    if (SortedVertices.Num() == 2)
    {
        const int32 V1 = SortedVertices[0];
        const int32 V2 = SortedVertices[1];
        
        FVector Pos1 = GetPosByIndex(V1);
        FVector Pos2 = GetPosByIndex(V2);
        
        const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), EndCapNormal, GenerateStableUVCustom(FVector(0, 0, Pos1.Z), EndCapNormal));
        const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), EndCapNormal, GenerateStableUVCustom(FVector(0, 0, Pos2.Z), EndCapNormal));
        
        if (IsStart)
        {
            AddTriangle(V1, V2, CenterV1);
            AddTriangle(V2, CenterV2, CenterV1);
        }
        else
        {
            AddTriangle(V2, V1, CenterV1);
            AddTriangle(CenterV1, CenterV2, V2);
        }
    }
    else
    {
        for (int32 i = 0; i < SortedVertices.Num() - 1; ++i)
        {
            const int32 V1 = SortedVertices[i];
            const int32 V2 = SortedVertices[i + 1];
            
            FVector Pos1 = GetPosByIndex(V1);
            FVector Pos2 = GetPosByIndex(V2);
            
            const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), EndCapNormal, GenerateStableUVCustom(FVector(0, 0, Pos1.Z), EndCapNormal));
            const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), EndCapNormal, GenerateStableUVCustom(FVector(0, 0, Pos2.Z), EndCapNormal));
            
            if (IsStart)
            {
                AddTriangle(V1, V2, CenterV1);
                AddTriangle(V2, CenterV2, CenterV1);
            }
            else
            {
                AddTriangle(V2, V1, CenterV1);
                AddTriangle(CenterV1, CenterV2, V2);
            }
        }
    }
}

void FFrustumBuilder::RecordEndCapConnectionPoint(int32 VertexIndex)
{
    EndCapConnectionPoints.Add(VertexIndex);
}

const TArray<int32>& FFrustumBuilder::GetEndCapConnectionPoints() const
{
    return EndCapConnectionPoints;
}

void FFrustumBuilder::ClearEndCapConnectionPoints()
{
    EndCapConnectionPoints.Empty();
}

FVector2D FFrustumBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    const float X = Position.X;
    const float Y = Position.Y;
    const float Z = Position.Z;
    
    float Angle = FMath::Atan2(Y, X);
    if (Angle < 0) Angle += 2.0f * PI;
    
    if (FMath::Abs(Normal.Z) > 0.9f)
    {
        // 顶底面：使用单UV系统（0-1范围）
        float U = Angle / (2.0f * PI);  // 0.0 到 1.0
        float V = (Z + Frustum.GetHalfHeight()) / Frustum.Height;  // 0.0 到 1.0
        
        if (Z > 0)
        {
            // 顶面：V = 0.5
            V = 0.5f;
        }
        else
        {
            // 底面：V = 1.0
            V = 1.0f;
        }
        
        return FVector2D(U, V);
    }
    else if (FMath::Abs(Normal.X) > 0.9f || FMath::Abs(Normal.Y) > 0.9f)
    {
        // 侧面：使用单UV系统（0-1范围）
        const float BaseV = (Z + Frustum.GetHalfHeight()) / Frustum.Height;
        const float AdjustedV = 0.1f + BaseV * 0.8f;  // 0.1 到 0.9
        
        if (FMath::Abs(Normal.X) > 0.9f)
        {
            if (Normal.X < 0)
            {
                // 左面：U从0.0到0.25
                const float U = (Angle / (2.0f * PI)) * 0.25f;
                return FVector2D(U, AdjustedV);
            }
            else
            {
                // 右面：U从0.75到1.0
                const float U = 0.75f + (Angle / (2.0f * PI)) * 0.25f;
                return FVector2D(U, AdjustedV);
            }
        }
        else
        {
            if (Normal.Y < 0)
            {
                // 后面：U从0.25到0.5
                const float U = 0.25f + (Angle / (2.0f * PI)) * 0.25f;
                return FVector2D(U, AdjustedV);
            }
            else
            {
                // 前面：U从0.5到0.75
                const float U = 0.5f + (Angle / (2.0f * PI)) * 0.25f;
                return FVector2D(U, AdjustedV);
            }
        }
    }
    else
    {
        // 倒角面：使用单UV系统（0-1范围）
        const float U = 0.25f + (Angle / (2.0f * PI)) * 0.5f;  // 0.25 到 0.75
        const float V = (Z + Frustum.GetHalfHeight()) / Frustum.Height;  // 0.0 到 1.0
        return FVector2D(U, V);
    }
}





void FFrustumBuilder::CalculateAngles()
{
    ArcAngleRadians = FMath::DegreesToRadians(Frustum.ArcAngle);
    StartAngle = -ArcAngleRadians / 2.0f;
    EndAngle = ArcAngleRadians / 2.0f;
}
