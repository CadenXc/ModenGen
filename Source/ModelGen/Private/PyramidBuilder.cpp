// Copyright (c) 2024. All rights reserved.

#include "PyramidBuilder.h"
#include "Pyramid.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"

FPyramidBuilder::FPyramidBuilder(const APyramid& InPyramid)
    : Pyramid(InPyramid)
{
    Clear();
}

void FPyramidBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    CosValues.Empty();
    SinValues.Empty();
}

bool FPyramidBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Pyramid.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    BaseRadius = Pyramid.BaseRadius;
    Height = Pyramid.Height;
    Sides = Pyramid.Sides;
    BevelRadius = Pyramid.BevelRadius;

    BevelTopRadius = Pyramid.GetBevelTopRadius();

    TopPoint = FVector(0, 0, Height);

    PrecomputeMath();

    GenerateBase();

    GenerateSlices();

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;
    return true;
}

int32 FPyramidBuilder::CalculateVertexCountEstimate() const
{
    return Pyramid.CalculateVertexCountEstimate();
}

int32 FPyramidBuilder::CalculateTriangleCountEstimate() const
{
    return Pyramid.CalculateTriangleCountEstimate();
}

void FPyramidBuilder::PrecomputeMath()
{
    const int32 Segments = FMath::Max(3, Sides);
    CosValues.SetNum(Segments + 1);
    SinValues.SetNum(Segments + 1);

    const float AngleStep = 2.0f * PI / Segments;

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Angle = i * AngleStep;
        FMath::SinCos(&SinValues[i], &CosValues[i], Angle);
    }
}

FVector FPyramidBuilder::GetRingPos(int32 Index, float Radius, float Z) const
{
    int32 SafeIndex = Index % (Sides + 1);
    return FVector(Radius * CosValues[SafeIndex], Radius * SinValues[SafeIndex], Z);
}

void FPyramidBuilder::GenerateBase()
{
    FVector Normal(0, 0, -1.0f);
    FVector CenterPos(0, 0, 0);
    FVector2D CenterUV(0.0f, 0.0f);

    int32 CenterIndex = GetOrAddVertex(CenterPos, Normal, CenterUV);

    TArray<int32> RimIndices;
    RimIndices.Reserve(Sides + 1);

    for (int32 i = 0; i <= Sides; ++i)
    {
        FVector Pos = GetRingPos(i, BevelTopRadius, 0.0f);
        FVector2D UV(Pos.X * ModelGenConstants::GLOBAL_UV_SCALE, Pos.Y * ModelGenConstants::GLOBAL_UV_SCALE);

        RimIndices.Add(GetOrAddVertex(Pos, Normal, UV));
    }

    for (int32 i = 0; i < Sides; ++i)
    {
        AddTriangle(CenterIndex, RimIndices[i], RimIndices[i + 1]);
    }
}

void FPyramidBuilder::GenerateSlices()
{
    const float Z_Bottom = 0.0f;
    const float Z_Mid = BevelRadius;
    const float Z_Top = Height;

    // 1. 准备圆锥法线参数
    float SideH = Height - BevelRadius;
    float SideSlopeLen = FMath::Sqrt(BevelTopRadius * BevelTopRadius + SideH * SideH);

    float ConeNormZ = BevelTopRadius / (SideSlopeLen > KINDA_SMALL_NUMBER ? SideSlopeLen : 1.0f);
    float ConeNormR = SideH / (SideSlopeLen > KINDA_SMALL_NUMBER ? SideSlopeLen : 1.0f);

    // 2. 准备圆柱法线参数
    float CylNormZ = 0.0f;
    float CylNormR = 1.0f;

    // 3. 准备倒角斜边长
    float BevelSlopeLen = BevelRadius;

    for (int32 i = 0; i < Sides; ++i)
    {
        // --- 位置计算 ---
        FVector P_Bot_L = GetRingPos(i, BevelTopRadius, Z_Bottom);
        FVector P_Bot_R = GetRingPos(i + 1, BevelTopRadius, Z_Bottom);
        FVector P_Mid_L = GetRingPos(i, BevelTopRadius, Z_Mid);
        FVector P_Mid_R = GetRingPos(i + 1, BevelTopRadius, Z_Mid);
        FVector P_Tip = TopPoint;

        // --- 法线计算 ---
        FVector N_Bevel_L, N_Bevel_R, N_Bevel_TL, N_Bevel_TR;
        FVector N_Side_L, N_Side_R, N_Side_Tip;

        if (Pyramid.bSmoothSides)
        {
            auto GetCylNormal = [&](int32 Idx) {
                return FVector(CosValues[Idx] * CylNormR, SinValues[Idx] * CylNormR, CylNormZ);
                };
            N_Bevel_L = GetCylNormal(i);
            N_Bevel_R = GetCylNormal(i + 1);
            N_Bevel_TL = N_Bevel_L;
            N_Bevel_TR = N_Bevel_R;

            auto GetConeNormal = [&](int32 Idx) {
                return FVector(CosValues[Idx] * ConeNormR, SinValues[Idx] * ConeNormR, ConeNormZ);
                };
            N_Side_L = GetConeNormal(i);
            N_Side_R = GetConeNormal(i + 1);

            N_Side_Tip = (N_Side_L + N_Side_R).GetSafeNormal();
        }
        else
        {
            FVector FaceN_Bevel = FVector::CrossProduct(P_Bot_R - P_Bot_L, P_Mid_L - P_Bot_L).GetSafeNormal();
            N_Bevel_L = N_Bevel_R = N_Bevel_TL = N_Bevel_TR = FaceN_Bevel;

            FVector FaceN_Side = FVector::CrossProduct(P_Mid_R - P_Mid_L, P_Tip - P_Mid_L).GetSafeNormal();
            N_Side_L = N_Side_R = N_Side_Tip = FaceN_Side;
        }

        // --- UV 计算 ---
        float W_Bot = FVector::Dist(P_Bot_L, P_Bot_R);

        FVector2D UV_Bot_L(0.0f, 0.0f);
        FVector2D UV_Bot_R(W_Bot * ModelGenConstants::GLOBAL_UV_SCALE, 0.0f);
        FVector2D UV_Mid_L(0.0f, BevelSlopeLen * ModelGenConstants::GLOBAL_UV_SCALE);
        FVector2D UV_Mid_R(W_Bot * ModelGenConstants::GLOBAL_UV_SCALE, BevelSlopeLen * ModelGenConstants::GLOBAL_UV_SCALE);

        float TotalV = (BevelSlopeLen + SideSlopeLen) * ModelGenConstants::GLOBAL_UV_SCALE;
        float HalfW = W_Bot * 0.5f * ModelGenConstants::GLOBAL_UV_SCALE;
        FVector2D UV_Tip(HalfW, TotalV);

        // --- 顶点生成与索引 ---

        // 【UV修正】：因为几何反转了（左变右），UV也要反转以匹配纹理方向
        // Bevel
        // V0 (BotL) 用 UV_Bot_R (逻辑上的右UV)
        // V1 (BotR) 用 UV_Bot_L (逻辑上的左UV)
        int32 V0 = GetOrAddVertex(P_Bot_L, N_Bevel_L, UV_Bot_R);
        int32 V1 = GetOrAddVertex(P_Bot_R, N_Bevel_R, UV_Bot_L);
        // V2 (MidR) 用 UV_Mid_L
        // V3 (MidL) 用 UV_Mid_R
        int32 V2 = GetOrAddVertex(P_Mid_R, N_Bevel_TR, UV_Mid_L);
        int32 V3 = GetOrAddVertex(P_Mid_L, N_Bevel_TL, UV_Mid_R);

        // Side
        int32 V_Side_L = GetOrAddVertex(P_Mid_L, N_Side_L, UV_Mid_R);
        int32 V_Side_R = GetOrAddVertex(P_Mid_R, N_Side_R, UV_Mid_L);
        int32 V_Side_Top = GetOrAddVertex(P_Tip, N_Side_Tip, UV_Tip);

        // --- 面生成 ---

        // 倒角面: V0 -> V3 -> V2 -> V1
        AddQuad(V0, V3, V2, V1);

        // 侧面: V_Side_R -> V_Side_L -> V_Side_Top
        AddTriangle(V_Side_R, V_Side_L, V_Side_Top);
    }
}