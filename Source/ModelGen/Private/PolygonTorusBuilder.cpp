// Copyright (c) 2024. All rights reserved.

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
    // 如果全是硬边，顶点数是面数 * 4
    return PolygonTorus.MajorSegments * PolygonTorus.MinorSegments * 4;
}

int32 FPolygonTorusBuilder::CalculateTriangleCountEstimate() const
{
    return PolygonTorus.CalculateTriangleCountEstimate();
}

void FPolygonTorusBuilder::PrecomputeMath()
{
    // 1. Major Angles (路径)
    const float TorusAngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);
    const int32 MajorSegs = PolygonTorus.MajorSegments;

    const float StartAngle = -TorusAngleRad / 2.0f;
    const float MajorStep = TorusAngleRad / MajorSegs;

    // 缓存 N+1 个角度以便插值
    MajorAngleCache.SetNum(MajorSegs + 1);
    for (int32 i = 0; i <= MajorSegs; ++i)
    {
        float Angle = StartAngle + i * MajorStep;
        FMath::SinCos(&MajorAngleCache[i].Sin, &MajorAngleCache[i].Cos, Angle);
    }

    // 2. Minor Angles (截面)
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const float MinorStep = 2.0f * PI / MinorSegs;

    MinorAngleCache.SetNum(MinorSegs + 1);
    for (int32 i = 0; i <= MinorSegs; ++i)
    {
        // 恢复原逻辑的相位偏移，保证多边形朝向符合预期
        float Angle = (i * MinorStep) - HALF_PI - (MinorStep * 0.5f);
        FMath::SinCos(&MinorAngleCache[i].Sin, &MinorAngleCache[i].Cos, Angle);
    }
}

// 【核心修复】：翻转圆环表面的渲染方向
// 之前：V0(i,j) -> V1(i+1,j) -> V2(i+1,j+1) -> V3(i,j+1) (导致法线向内)
// 修正：V0(i,j) -> V3(i,j+1) -> V2(i+1,j+1) -> V1(i+1,j) (法线向外)
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

    for (int32 i = 0; i < MajorSegs; ++i)
    {
        float NextU = CurrentU + MajorArcStep;
        float CurrentV = 0.0f;

        const FCachedTrig& Maj0 = MajorAngleCache[i];
        const FCachedTrig& Maj1 = MajorAngleCache[i + 1];

        // 硬边 Major 法线准备
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
            float NextV = CurrentV + MinorArcStep;

            // 硬边 Minor 法线准备
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

            // 定义 4 个角的数据源
            // 0: (i, j), 1: (i+1, j), 2: (i+1, j+1), 3: (i, j+1)
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

                // 位置
                float RadialOffset = MinP.Cos * MinorRad;
                float ZOffset = MinP.Sin * MinorRad;
                float FinalZ = ZOffset + MinorRad;

                FVector Pos(
                    (MajorRad + RadialOffset) * MajP.Cos,
                    (MajorRad + RadialOffset) * MajP.Sin,
                    FinalZ
                );

                // 法线
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

                // UV
                FVector2D UV(Corners[k].U * ModelGenConstants::GLOBAL_UV_SCALE, Corners[k].V * ModelGenConstants::GLOBAL_UV_SCALE);

                Indices[k] = GetOrAddVertex(Pos, Normal, UV);
            }

            // 【修改处】：反转连接顺序以修正渲染方向
            // 原顺序 0,1,2,3 导致法线向内
            // 新顺序 0,3,2,1 产生向外的法线
            AddQuad(Indices[0], Indices[3], Indices[2], Indices[1]);

            // 收集端盖索引
            // Start Cap: i=0. 
            // 我们需要收集这一圈的左侧边。
            // 原本取 0 (i,j) 和 3 (i,j+1)。
            // 只需要存 Indices[0] 即可代表 V(0, j)
            if (i == 0)
            {
                StartCapRingIndices.Add(Indices[0]);
            }
            // End Cap: i=End.
            // 需要收集右侧边。
            // 原本取 1 (i+1, j) 和 2 (i+1, j+1)。
            // 存 Indices[1] 代表 V(End, j)
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
    // Start Cap
    CreateCap(StartCapRingIndices, true);

    // End Cap
    CreateCap(EndCapRingIndices, false);
}

void FPolygonTorusBuilder::CreateCap(const TArray<int32>& RingIndices, bool bIsStart)
{
    if (RingIndices.Num() < 3) return;

    // 1. 计算端盖中心和法线
    const float TorusAngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);
    const float Angle = bIsStart ? (-TorusAngleRad / 2.0f) : (TorusAngleRad / 2.0f);

    float CosA, SinA;
    FMath::SinCos(&SinA, &CosA, Angle);

    // 中心位置：Z轴也需要偏移 (MinorRad)
    FVector CenterPos(
        PolygonTorus.MajorRadius * CosA,
        PolygonTorus.MajorRadius * SinA,
        PolygonTorus.MinorRadius
    );

    // 法线: Start(-Tangent), End(+Tangent)
    FVector Normal = bIsStart ?
        FVector(SinA, -CosA, 0.0f) :
        FVector(-SinA, CosA, 0.0f);

    // 2. 创建新的 Cap 顶点 (硬边)
    TArray<int32> CapVertices;
    CapVertices.Reserve(RingIndices.Num());

    // 沿用之前的 UV 计算逻辑，这里从 RingIndices 获取位置计算 Planar UV
    for (int32 Idx : RingIndices)
    {
        FVector Pos = GetPosByIndex(Idx);

        // Planar UV: Local X (Horizontal dist from major ring), Local Y (Z height)
        // H_Dist: Distance from CenterPos in XY plane? No, dist from Major Radius arc.
        // Simple approx: Len(XY) - MajorRadius.
        float R_Current = FVector2D(Pos.X, Pos.Y).Size();
        float LocalX = R_Current - PolygonTorus.MajorRadius;
        float LocalY = Pos.Z - PolygonTorus.MinorRadius;

        FVector2D UV(LocalX * ModelGenConstants::GLOBAL_UV_SCALE, LocalY * ModelGenConstants::GLOBAL_UV_SCALE);

        CapVertices.Add(AddVertex(Pos, Normal, UV));
    }

    // 添加中心点 UV=(0,0)
    int32 CenterIdx = AddVertex(CenterPos, Normal, FVector2D(0, 0));

    // 3. 生成三角扇
    // RingIndices 来自 Minor 循环 (0..Segs-1)，是闭合圆环
    const int32 NumVerts = CapVertices.Num();
    for (int32 i = 0; i < NumVerts; ++i)
    {
        int32 V_Curr = CapVertices[i];
        int32 V_Next = CapVertices[(i + 1) % NumVerts]; // 闭合

        if (bIsStart)
        {
            // Start Cap (Look Back): Center -> Next -> Curr (Check winding)
            // Minor loop goes CCW in (R,Z) plane?
            // Let's try standard: Center, Next, Curr
            AddTriangle(CenterIdx, V_Next, V_Curr);
        }
        else
        {
            // End Cap (Look Forward): Center -> Curr -> Next
            AddTriangle(CenterIdx, V_Curr, V_Next);
        }
    }
}

void FPolygonTorusBuilder::ValidateAndClampParameters()
{
    // No-op
}