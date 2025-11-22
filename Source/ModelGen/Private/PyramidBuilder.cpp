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

    // 1. 法线参数
    float SideH = Height - BevelRadius;
    float SideSlopeLen = FMath::Sqrt(BevelTopRadius * BevelTopRadius + SideH * SideH);

    float ConeNormZ = BevelTopRadius / (SideSlopeLen > KINDA_SMALL_NUMBER ? SideSlopeLen : 1.0f);
    float ConeNormR = SideH / (SideSlopeLen > KINDA_SMALL_NUMBER ? SideSlopeLen : 1.0f);

    float CylNormZ = 0.0f;
    float CylNormR = 1.0f;

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

            N_Side_Tip = FVector(0.0f, 0.0f, 1.0f);
        }
        else
        {
            FVector FaceN_Bevel = FVector::CrossProduct(P_Bot_R - P_Bot_L, P_Mid_L - P_Bot_L).GetSafeNormal();
            N_Bevel_L = N_Bevel_R = N_Bevel_TL = N_Bevel_TR = FaceN_Bevel;

            FVector FaceN_Side = FVector::CrossProduct(P_Mid_R - P_Mid_L, P_Tip - P_Mid_L).GetSafeNormal();
            N_Side_L = N_Side_R = N_Side_Tip = FaceN_Side;
        }

        // --- UV 计算 (反转V轴以修复贴图倒置) ---
        float W_Bot = FVector::Dist(P_Bot_L, P_Bot_R);
        float HalfW = W_Bot * 0.5f * ModelGenConstants::GLOBAL_UV_SCALE;

        // 计算各段的高度 (UV单位)
        float V_Bevel = BevelSlopeLen * ModelGenConstants::GLOBAL_UV_SCALE;
        float V_Side = SideSlopeLen * ModelGenConstants::GLOBAL_UV_SCALE;
        float V_Total = V_Bevel + V_Side;

        // 底部 (Z=0) 对应 V=V_Total (纹理底部)
        // 中间 (Z=Bevel) 对应 V=V_Side
        // 顶部 (Z=Height) 对应 V=0 (纹理顶部)

        FVector2D UV_Bot_L(0.0f, V_Total);
        FVector2D UV_Bot_R(W_Bot * ModelGenConstants::GLOBAL_UV_SCALE, V_Total);

        FVector2D UV_Mid_L(0.0f, V_Side);
        FVector2D UV_Mid_R(W_Bot * ModelGenConstants::GLOBAL_UV_SCALE, V_Side);

        FVector2D UV_Tip(HalfW, 0.0f);

        // --- 顶点生成与索引 ---

        // Bevel (注意UV左右反转以匹配之前修正的渲染方向)
        int32 V0 = GetOrAddVertex(P_Bot_L, N_Bevel_L, UV_Bot_R);
        int32 V1 = GetOrAddVertex(P_Bot_R, N_Bevel_R, UV_Bot_L);
        int32 V2 = GetOrAddVertex(P_Mid_R, N_Bevel_TR, UV_Mid_L);
        int32 V3 = GetOrAddVertex(P_Mid_L, N_Bevel_TL, UV_Mid_R);

        // Side
        int32 V_Side_L = GetOrAddVertex(P_Mid_L, N_Side_L, UV_Mid_R);
        int32 V_Side_R = GetOrAddVertex(P_Mid_R, N_Side_R, UV_Mid_L);
        int32 V_Side_Top = GetOrAddVertex(P_Tip, N_Side_Tip, UV_Tip);

        // --- 面生成 ---
        AddQuad(V0, V3, V2, V1);
        AddTriangle(V_Side_R, V_Side_L, V_Side_Top);
    }
}