// Copyright (c) 2024. All rights reserved.

#include "SphereBuilder.h"
#include "Sphere.h"
#include "ModelGenMeshData.h"

FSphereBuilder::FSphereBuilder(const ASphere& InSphere)
    : Sphere(InSphere)
{
    Clear();
}

void FSphereBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    Radius = 0.0f;
    Sides = 0;
    HorizontalCut = 0.0f;
    VerticalCut = 0.0f;
    ZOffset = 0.0f;
}

static bool IsTriangleDegenerate(int32 V0, int32 V1, int32 V2)
{
    return (V0 == V1) || (V1 == V2) || (V2 == V0);
}


bool FSphereBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Sphere.IsValid())
    {
        return false;
    }

    Clear();

    // Cache settings
    Radius = Sphere.Radius;
    Sides = Sphere.Sides;
    HorizontalCut = Sphere.HorizontalCut;
    VerticalCut = Sphere.VerticalCut;

    // --- 【防崩溃措施 1】：严格的参数钳制 ---

    // 限制 HorizontalCut 最大值，保留微小余量防止除以零或几何消失
    if (HorizontalCut >= 1.0f - KINDA_SMALL_NUMBER)
    {
        HorizontalCut = 1.0f - KINDA_SMALL_NUMBER;
    }

    // 限制 VerticalCut 最小值，防止生成极细的切片导致物理引擎报错
    // 如果想要完全闭合，VerticalCut 必须 > 0
    if (VerticalCut <= KINDA_SMALL_NUMBER)
    {
        // 切片太薄，直接不生成，视为失败但不崩溃
        return false;
    }

    // --- 【防崩溃措施 2】：几何尺寸检查 ---
    // 如果 球体半径 * 角度比例 太小，说明网格会缩成一团，容易导致精度问题
    const float PhiRange = PI * (1.0f - HorizontalCut);
    const float ThetaRange = VerticalCut * 2.0f * PI;

    // 检查弧长精度（可选，根据项目需求调整阈值）
    const float MinArcLength = 0.01f; // 0.01cm
    if (Radius * PhiRange < MinArcLength || Radius * ThetaRange < MinArcLength)
    {
        // 几何体过小，放弃生成
        return false;
    }

    ReserveMemory();

    // 计算 Z 偏移量
    const float EndPhi = PhiRange; // StartPhi is 0
    const float CutPlaneZ = Radius * FMath::Cos(EndPhi);
    ZOffset = -CutPlaneZ;

    GenerateSphereMesh();
    GenerateCaps();

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;

    return true;
}

int32 FSphereBuilder::CalculateVertexCountEstimate() const
{
    return Sphere.CalculateVertexCountEstimate();
}

int32 FSphereBuilder::CalculateTriangleCountEstimate() const
{
    return Sphere.CalculateTriangleCountEstimate();
}

FVector FSphereBuilder::GetSpherePoint(float Theta, float Phi) const
{
    float SinPhi = FMath::Sin(Phi);
    float CosPhi = FMath::Cos(Phi);
    float SinTheta = FMath::Sin(Theta);
    float CosTheta = FMath::Cos(Theta);

    float X = Radius * SinPhi * CosTheta;
    float Y = Radius * SinPhi * SinTheta;
    
    float Z = Radius * CosPhi + ZOffset;

    return FVector(X, Y, Z);
}

FVector FSphereBuilder::GetSphereNormal(float Theta, float Phi) const
{
    return FVector(
        FMath::Sin(Phi) * FMath::Cos(Theta),
        FMath::Sin(Phi) * FMath::Sin(Theta),
        FMath::Cos(Phi)
    );
}

void FSphereBuilder::SafeAddTriangle(int32 V0, int32 V1, int32 V2)
{
    if (!IsTriangleDegenerate(V0, V1, V2))
    {
        AddTriangle(V0, V1, V2);
    }
}

void FSphereBuilder::SafeAddQuad(int32 V0, int32 V1, int32 V2, int32 V3)
{
    // 四边形拆分为两个三角形处理
    SafeAddTriangle(V0, V1, V3);
    SafeAddTriangle(V1, V2, V3);
}

void FSphereBuilder::GenerateSphereMesh()
{
    const int32 NumRings = FMath::Max(2, Sides / 2);
    const int32 NumSegments = Sides;

    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;

    if (PhiRange <= KINDA_SMALL_NUMBER) return;

    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;
    const float ThetaRange = EndTheta - StartTheta;

    TArray<TArray<int32>> GridIndices;
    GridIndices.SetNum(NumRings + 1);

    // 1. 生成顶点
    for (int32 v = 0; v <= NumRings; ++v)
    {
        const float VRatio = static_cast<float>(v) / NumRings;
        const float Phi = StartPhi + VRatio * PhiRange;

        GridIndices[v].Reserve(NumSegments + 1);

        for (int32 h = 0; h <= NumSegments; ++h)
        {
            const float HRatio = static_cast<float>(h) / NumSegments;
            const float Theta = StartTheta + HRatio * ThetaRange;

            FVector Pos = GetSpherePoint(Theta, Phi);
            FVector Normal = GetSphereNormal(Theta, Phi);
            FVector2D UV(HRatio, VRatio);

            GridIndices[v].Add(AddVertex(Pos, Normal, UV));
        }
    }

    // 2. 生成面
    for (int32 v = 0; v < NumRings; ++v)
    {
        for (int32 h = 0; h < NumSegments; ++h)
        {
            int32 V0 = GridIndices[v][h];       // Top-Left
            int32 V1 = GridIndices[v][h + 1];   // Top-Right
            int32 V2 = GridIndices[v + 1][h];   // Bottom-Left
            int32 V3 = GridIndices[v + 1][h + 1]; // Bottom-Right

            // 使用 SafeAdd 替代直接 Add，并在极点处逻辑明确化

            // 顶部极点 (v=0)
            if (v == 0)
            {
                // Top Triangle: V0和V1在几何上重合，但在索引上可能是不同的
                // 如果它们索引不同但位置相同，SafeAddTriangle 不会过滤，这没问题
                // 但如果索引相同，SafeAddTriangle 会过滤
                SafeAddTriangle(V0, V3, V2);
            }
            // 底部极点 (如果完全闭合)
            else if (v == NumRings - 1 && FMath::IsNearlyEqual(EndPhi, PI))
            {
                SafeAddTriangle(V0, V1, V2);
            }
            else
            {
                // 标准四边形
                SafeAddQuad(V0, V1, V3, V2);
            }
        }
    }
}

void FSphereBuilder::GenerateCaps()
{
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;

    // 只有当有实际开口时才生成盖子
    if (EndPhi < PI - KINDA_SMALL_NUMBER)
    {
        GenerateHorizontalCap(EndPhi, true);
    }

    // 只有当不是完整圆周时才生成竖切面
    if (VerticalCut < 1.0f - KINDA_SMALL_NUMBER)
    {
        GenerateVerticalCap(StartTheta, true);
        GenerateVerticalCap(EndTheta, false);
    }
}

void FSphereBuilder::GenerateHorizontalCap(float Phi, bool bIsBottom)
{
    const int32 Segments = Sides;
    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;
    const float ThetaRange = EndTheta - StartTheta;

    if (ThetaRange <= KINDA_SMALL_NUMBER) return;

    float CenterZ = Radius * FMath::Cos(Phi) + ZOffset;
    FVector CenterPos(0.0f, 0.0f, CenterZ);
    FVector Normal(0.0f, 0.0f, -1.0f);
    FVector2D CenterUV(0.5f, 0.5f);

    int32 CenterIndex = AddVertex(CenterPos, Normal, CenterUV);

    TArray<int32> RimIndices;
    RimIndices.Reserve(Segments + 1);

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Ratio = static_cast<float>(i) / Segments;
        float Theta = StartTheta + Ratio * ThetaRange;

        FVector Pos = GetSpherePoint(Theta, Phi);
        float U = (Pos.X / Radius) * 0.5f + 0.5f;
        float V = (Pos.Y / Radius) * 0.5f + 0.5f;

        RimIndices.Add(AddVertex(Pos, Normal, FVector2D(U, V)));
    }

    for (int32 i = 0; i < Segments; ++i)
    {
        SafeAddTriangle(CenterIndex, RimIndices[i], RimIndices[i + 1]);
    }
}

void FSphereBuilder::GenerateVerticalCap(float Theta, bool bIsStart)
{
    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;

    if (PhiRange <= KINDA_SMALL_NUMBER) return;

    const int32 Segments = FMath::Max(2, Sides / 2);

    FVector Tangent(-FMath::Sin(Theta), FMath::Cos(Theta), 0.0f);
    FVector Normal = bIsStart ? -Tangent : Tangent;

    TArray<int32> ProfileIndices;
    TArray<int32> AxisIndices;

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Ratio = static_cast<float>(i) / Segments;
        float Phi = StartPhi + Ratio * PhiRange;

        FVector Pos = GetSpherePoint(Theta, Phi);
        FVector2D UV_Prof(1.0f, Ratio);
        ProfileIndices.Add(AddVertex(Pos, Normal, UV_Prof));

        FVector AxisPos(0.0f, 0.0f, Pos.Z);
        FVector2D UV_Axis(0.0f, Ratio);
        AxisIndices.Add(AddVertex(AxisPos, Normal, UV_Axis));
    }

    for (int32 i = 0; i < Segments; ++i)
    {
        int32 AxisCurr = AxisIndices[i];
        int32 AxisNext = AxisIndices[i + 1];
        int32 ProfCurr = ProfileIndices[i];
        int32 ProfNext = ProfileIndices[i + 1];

        bool bTopDegenerate = (i == 0);
        bool bBottomDegenerate = (i == Segments - 1) && FMath::IsNearlyEqual(EndPhi, PI);

        if (bTopDegenerate)
        {
            if (bIsStart) SafeAddTriangle(AxisCurr, ProfNext, AxisNext);
            else          SafeAddTriangle(AxisCurr, AxisNext, ProfNext);
        }
        else if (bBottomDegenerate)
        {
            if (bIsStart) SafeAddTriangle(AxisCurr, ProfCurr, AxisNext);
            else          SafeAddTriangle(AxisCurr, AxisNext, ProfCurr);
        }
        else
        {
            if (bIsStart) SafeAddQuad(AxisCurr, ProfCurr, ProfNext, AxisNext);
            else          SafeAddQuad(AxisCurr, AxisNext, ProfNext, ProfCurr);
        }
    }
}