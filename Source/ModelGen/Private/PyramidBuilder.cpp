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

    // 缓存参数
    BaseRadius = Pyramid.BaseRadius;
    Height = Pyramid.Height;
    Sides = Pyramid.Sides;
    BevelRadius = Pyramid.BevelRadius;

    // 恢复原始逻辑：
    // 在原始实现中，底面(Z=0)和倒角顶面(Z=BevelRadius)都使用了 BevelTopRadius。
    // 这使得倒角区域变成了一个垂直的"棱柱"底座。
    BevelTopRadius = Pyramid.GetBevelTopRadius();

    TopPoint = FVector(0, 0, Height);

    PrecomputeMath();

    GenerateBase();

    if (BevelRadius > KINDA_SMALL_NUMBER)
    {
        GenerateBevel();
    }

    GenerateSides();

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
    // 底面法线向下
    FVector Normal(0, 0, -1.0f);
    FVector CenterPos(0, 0, 0);
    FVector2D CenterUV(0.0f, 0.0f);

    // 1. 创建中心点
    int32 CenterIndex = AddVertex(CenterPos, Normal, CenterUV);

    // 2. 创建边缘点
    // 【棱柱逻辑修复】：为了匹配倒角棱柱的底部，这里必须使用 BevelTopRadius
    TArray<int32> RimIndices;
    RimIndices.Reserve(Sides + 1);

    for (int32 i = 0; i <= Sides; ++i)
    {
        FVector Pos = GetRingPos(i, BevelTopRadius, 0.0f);
        FVector2D UV(Pos.X * ModelGenConstants::GLOBAL_UV_SCALE, Pos.Y * ModelGenConstants::GLOBAL_UV_SCALE);

        RimIndices.Add(AddVertex(Pos, Normal, UV));
    }

    // 3. 生成三角扇
    // 【方向修复】：改为 Center -> Curr -> Next (逆时针)，修正底面渲染反向问题
    for (int32 i = 0; i < Sides; ++i)
    {
        AddTriangle(CenterIndex, RimIndices[i], RimIndices[i + 1]);
    }
}

void FPyramidBuilder::GenerateBevel()
{
    // 倒角/棱柱部分
    const float Z_Bottom = 0.0f;
    const float Z_Top = BevelRadius;
    const bool bSmooth = Pyramid.bSmoothSides;

    // 【棱柱逻辑修复】：因为是垂直棱柱，上下半径相同，均为 BevelTopRadius
    float dR = 0.0f; // 半径差为0
    float dZ = BevelRadius;
    float SlopeLen = dZ; // 斜边长度等于高度

    // 垂直表面的法线 (XY平面上的径向向量)
    float NormZ_Comp = 0.0f;
    float NormR_Comp = 1.0f;

    for (int32 i = 0; i < Sides; ++i)
    {
        // 几何位置：上下半径一致
        FVector P0 = GetRingPos(i, BevelTopRadius, Z_Bottom);      // Bottom Left
        FVector P1 = GetRingPos(i + 1, BevelTopRadius, Z_Bottom);    // Bottom Right
        FVector P2 = GetRingPos(i + 1, BevelTopRadius, Z_Top);       // Top Right
        FVector P3 = GetRingPos(i, BevelTopRadius, Z_Top);         // Top Left

        // 法线
        FVector N0, N1, N2, N3;

        if (bSmooth)
        {
            auto GetSmoothNorm = [&](int32 Idx) {
                return FVector(CosValues[Idx] * NormR_Comp, SinValues[Idx] * NormR_Comp, NormZ_Comp);
                };
            N0 = GetSmoothNorm(i);
            N1 = GetSmoothNorm(i + 1);
            N2 = N1;
            N3 = N0;
        }
        else
        {
            // 硬边：面法线 (垂直板)
            FVector FaceNormal = FVector::CrossProduct(P1 - P0, P3 - P0).GetSafeNormal();
            N0 = N1 = N2 = N3 = FaceNormal;
        }

        // UV (矩形展开)
        float W = FVector::Dist(P0, P1);
        float H = SlopeLen; // 等于 BevelHeight

        FVector2D UV0(0.0f, 0.0f);
        FVector2D UV1(W * ModelGenConstants::GLOBAL_UV_SCALE, 0.0f);
        FVector2D UV2(W * ModelGenConstants::GLOBAL_UV_SCALE, H * ModelGenConstants::GLOBAL_UV_SCALE);
        FVector2D UV3(0.0f, H * ModelGenConstants::GLOBAL_UV_SCALE);

        int32 V0 = AddVertex(P0, N0, UV0);
        int32 V1 = AddVertex(P1, N1, UV1);
        int32 V2 = AddVertex(P2, N2, UV2);
        int32 V3 = AddVertex(P3, N3, UV3);

        // 连接顺序：V0 -> V3 -> V2 -> V1 (保持之前修正过的侧面方向)
        AddQuad(V0, V3, V2, V1);
    }
}

void FPyramidBuilder::GenerateSides()
{
    // 金字塔主体
    const float Z_Base = BevelRadius;
    const float R_Base = BevelTopRadius;

    const bool bSmooth = Pyramid.bSmoothSides;

    // 计算金字塔侧面的解析法线参数
    float dR = R_Base;
    float dZ = Height - BevelRadius;
    float SlantHeight = FMath::Sqrt(dR * dR + dZ * dZ);

    float NormZ_Comp = dR / (SlantHeight > KINDA_SMALL_NUMBER ? SlantHeight : 1.0f);
    float NormR_Comp = dZ / (SlantHeight > KINDA_SMALL_NUMBER ? SlantHeight : 1.0f);

    for (int32 i = 0; i < Sides; ++i)
    {
        // 位置
        FVector P0 = GetRingPos(i, R_Base, Z_Base);     // Base Left (Curr)
        FVector P1 = GetRingPos(i + 1, R_Base, Z_Base);   // Base Right (Next)
        FVector P_Top = TopPoint;                       // Tip

        // 法线
        FVector N0, N1, N_Top;

        if (bSmooth)
        {
            auto GetConeNormal = [&](int32 Idx) {
                return FVector(CosValues[Idx] * NormR_Comp, SinValues[Idx] * NormR_Comp, NormZ_Comp);
                };
            N0 = GetConeNormal(i);
            N1 = GetConeNormal(i + 1);
            N_Top = (N0 + N1).GetSafeNormal();
        }
        else
        {
            // 硬边法线
            FVector FaceNormal = FVector::CrossProduct(P1 - P0, P_Top - P0).GetSafeNormal();
            N0 = N1 = N_Top = FaceNormal;
        }

        // UV
        float EdgeWidth = FVector::Dist(P0, P1);
        FVector2D UV0(0.0f, 0.0f);
        FVector2D UV1(EdgeWidth * ModelGenConstants::GLOBAL_UV_SCALE, 0.0f);
        FVector2D UV_Tip(EdgeWidth * 0.5f * ModelGenConstants::GLOBAL_UV_SCALE, SlantHeight * ModelGenConstants::GLOBAL_UV_SCALE);

        int32 V0 = AddVertex(P0, N0, UV0);
        int32 V1 = AddVertex(P1, N1, UV1);
        int32 V_Tip = AddVertex(P_Top, N_Top, UV_Tip);

        // 三角形连接顺序：V1 -> V0 -> V_Tip
        AddTriangle(V1, V0, V_Tip);
    }
}