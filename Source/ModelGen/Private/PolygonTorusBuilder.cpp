#include "PolygonTorusBuilder.h"
#include "PolygonTorus.h"
#include "ModelGenMeshData.h"
#include "Math/UnrealMathUtility.h"
#include "ModelGenConstants.h"

FPolygonTorusBuilder::FPolygonTorusBuilder(const APolygonTorus& InPolygonTorus)
    : PolygonTorus(InPolygonTorus)
{
    Clear();
}

void FPolygonTorusBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    MajorAngleCache.Empty();
    MinorAngleCache.Empty();
    StartCapRingIndices.Empty();
    EndCapRingIndices.Empty();
}

bool FPolygonTorusBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (PolygonTorus.MajorSegments < 3 || PolygonTorus.MinorSegments < 3)
    {
        return false;
    }

    Clear();
    ReserveMemory();

    PrecomputeMath();

    GenerateTorusSurface();

    if (FMath::Abs(PolygonTorus.TorusAngle) < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps();
    }

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;
    return true;
}

int32 FPolygonTorusBuilder::CalculateVertexCountEstimate() const
{
    return PolygonTorus.MajorSegments * PolygonTorus.MinorSegments * 4;
}

int32 FPolygonTorusBuilder::CalculateTriangleCountEstimate() const
{
    return PolygonTorus.CalculateTriangleCountEstimate();
}

void FPolygonTorusBuilder::PrecomputeMath()
{
    const float TorusAngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);
    const int32 MajorSegs = PolygonTorus.MajorSegments;

    const float StartAngle = -TorusAngleRad / 2.0f;
    const float MajorStep = TorusAngleRad / MajorSegs;

    MajorAngleCache.SetNum(MajorSegs + 1);
    for (int32 i = 0; i <= MajorSegs; ++i)
    {
        float Angle = StartAngle + i * MajorStep;
        FMath::SinCos(&MajorAngleCache[i].Sin, &MajorAngleCache[i].Cos, Angle);
    }

    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const float MinorStep = 2.0f * PI / MinorSegs;

    MinorAngleCache.SetNum(MinorSegs + 1);
    for (int32 i = 0; i <= MinorSegs; ++i)
    {
        float Angle = (i * MinorStep) - HALF_PI - (MinorStep * 0.5f);
        FMath::SinCos(&MinorAngleCache[i].Sin, &MinorAngleCache[i].Cos, Angle);
    }
}

void FPolygonTorusBuilder::GenerateTorusSurface()
{
    const int32 MajorSegs = PolygonTorus.MajorSegments;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const float MajorRad = PolygonTorus.MajorRadius;
    const float MinorRad = PolygonTorus.MinorRadius;

    const bool bSmoothVert = PolygonTorus.bSmoothVerticalSection;
    const bool bSmoothCross = PolygonTorus.bSmoothCrossSection;

    StartCapRingIndices.Reserve(MinorSegs);
    EndCapRingIndices.Reserve(MinorSegs);

    float CurrentU = 0.0f;
    const float MajorArcStep = (FMath::DegreesToRadians(PolygonTorus.TorusAngle) / MajorSegs) * MajorRad;

    const float TotalMinorCircumference = (2.0f * PI) * MinorRad;

    for (int32 i = 0; i < MajorSegs; ++i)
    {
        float NextU = CurrentU + MajorArcStep;
        float CurrentV = TotalMinorCircumference;

        const FCachedTrig& Maj0 = MajorAngleCache[i];
        const FCachedTrig& Maj1 = MajorAngleCache[i + 1];

        float MajCos_Normal = 0.f, MajSin_Normal = 0.f;
        if (!bSmoothVert)
        {
            float MidCos = (Maj0.Cos + Maj1.Cos) * 0.5f;
            float MidSin = (Maj0.Sin + Maj1.Sin) * 0.5f;
            float Len = FMath::Sqrt(MidCos * MidCos + MidSin * MidSin);
            MajCos_Normal = MidCos / Len;
            MajSin_Normal = MidSin / Len;
        }

        for (int32 j = 0; j < MinorSegs; ++j)
        {
            const FCachedTrig& Min0 = MinorAngleCache[j];
            const FCachedTrig& Min1 = MinorAngleCache[j + 1];

            float MinorArcStep = (2.0f * PI / MinorSegs) * MinorRad;
            float NextV = CurrentV - MinorArcStep;

            float MinCos_Normal = 0.f, MinSin_Normal = 0.f;
            if (!bSmoothCross)
            {
                float MidCos = (Min0.Cos + Min1.Cos) * 0.5f;
                float MidSin = (Min0.Sin + Min1.Sin) * 0.5f;
                float Len = FMath::Sqrt(MidCos * MidCos + MidSin * MidSin);
                MinCos_Normal = MidCos / Len;
                MinSin_Normal = MidSin / Len;
            }

            int32 Indices[4];

            struct FCornerInfo { const FCachedTrig* Maj; const FCachedTrig* Min; float U; float V; };
            FCornerInfo Corners[4] = {
                { &Maj0, &Min0, CurrentU, CurrentV },
                { &Maj1, &Min0, NextU,    CurrentV },
                { &Maj1, &Min1, NextU,    NextV },
                { &Maj0, &Min1, CurrentU, NextV }
            };

            for (int k = 0; k < 4; ++k)
            {
                const FCachedTrig& MajP = *Corners[k].Maj;
                const FCachedTrig& MinP = *Corners[k].Min;

                float RadialOffset = MinP.Cos * MinorRad;
                float ZOffset = MinP.Sin * MinorRad;
                float FinalZ = ZOffset + MinorRad;

                FVector Pos(
                    (MajorRad + RadialOffset) * MajP.Cos,
                    (MajorRad + RadialOffset) * MajP.Sin,
                    FinalZ
                );

                float UseMajCos = bSmoothVert ? MajP.Cos : MajCos_Normal;
                float UseMajSin = bSmoothVert ? MajP.Sin : MajSin_Normal;
                float UseMinCos = bSmoothCross ? MinP.Cos : MinCos_Normal;
                float UseMinSin = bSmoothCross ? MinP.Sin : MinSin_Normal;

                FVector Normal(
                    UseMinCos * UseMajCos,
                    UseMinCos * UseMajSin,
                    UseMinSin
                );
                Normal.Normalize();

                FVector2D UV(Corners[k].U * ModelGenConstants::GLOBAL_UV_SCALE, Corners[k].V * ModelGenConstants::GLOBAL_UV_SCALE);

                Indices[k] = GetOrAddVertex(Pos, Normal, UV);
            }

            AddQuad(Indices[0], Indices[3], Indices[2], Indices[1]);

            if (i == 0)
            {
                StartCapRingIndices.Add(Indices[0]);
            }
            if (i == MajorSegs - 1)
            {
                EndCapRingIndices.Add(Indices[1]);
            }

            CurrentV = NextV;
        }
        CurrentU = NextU;
    }
}

void FPolygonTorusBuilder::GenerateEndCaps()
{
    CreateCap(StartCapRingIndices, true);
    CreateCap(EndCapRingIndices, false);
}

void FPolygonTorusBuilder::CreateCap(const TArray<int32>& RingIndices, bool bIsStart)
{
    if (RingIndices.Num() < 3) return;

    const float TorusAngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);
    const float Angle = bIsStart ? (-TorusAngleRad / 2.0f) : (TorusAngleRad / 2.0f);

    float CosA, SinA;
    FMath::SinCos(&SinA, &CosA, Angle);

    FVector CenterPos(
        PolygonTorus.MajorRadius * CosA,
        PolygonTorus.MajorRadius * SinA,
        PolygonTorus.MinorRadius
    );

    FVector Normal = bIsStart ?
        FVector(SinA, -CosA, 0.0f) :
        FVector(-SinA, CosA, 0.0f);

    auto GetCapUV = [&](const FVector& P)
        {
            float R_Current = FVector2D(P.X, P.Y).Size();

            float LocalX = R_Current - PolygonTorus.MajorRadius;
            
            if (bIsStart)
            {
                LocalX = -LocalX; 
            }

            float LocalY = -P.Z; 

            return FVector2D(LocalX * ModelGenConstants::GLOBAL_UV_SCALE, LocalY * ModelGenConstants::GLOBAL_UV_SCALE);
        };

    TArray<int32> CapVertices;
    CapVertices.Reserve(RingIndices.Num());

    for (int32 Idx : RingIndices)
    {
        FVector Pos = GetPosByIndex(Idx);
        FVector2D UV = GetCapUV(Pos);
        CapVertices.Add(AddVertex(Pos, Normal, UV));
    }

    FVector2D CenterUV = GetCapUV(CenterPos);
    int32 CenterIdx = AddVertex(CenterPos, Normal, CenterUV);

    const int32 NumVerts = CapVertices.Num();
    for (int32 i = 0; i < NumVerts; ++i)
    {
        int32 V_Curr = CapVertices[i];
        int32 V_Next = CapVertices[(i + 1) % NumVerts];

        if (bIsStart)
        {
            AddTriangle(CenterIdx, V_Next, V_Curr);
        }
        else
        {
            AddTriangle(CenterIdx, V_Curr, V_Next);
        }
    }
}

void FPolygonTorusBuilder::ValidateAndClampParameters()
{
}