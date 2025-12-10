// Copyright (c) 2024. All rights reserved.

#include "SphereBuilder.h"
#include "Sphere.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"

FSphereBuilder::FSphereBuilder(const ASphere& InSphere)
    : Sphere(InSphere)
{
    Clear();
}

void FSphereBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
}

bool FSphereBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Sphere.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    Radius = Sphere.Radius;
    Sides = Sphere.Sides;
    HorizontalCut = Sphere.HorizontalCut;
    VerticalCut = Sphere.VerticalCut;

    // 【修复1】：防止横截断 = 1.0 时崩溃
    // 当 HorizontalCut = 1.0 时，EndPhi = 0，会导致所有顶点都在极点，生成无效网格
    // 限制最大值为接近但不等于 1.0
    if (HorizontalCut >= 1.0f - KINDA_SMALL_NUMBER)
    {
        HorizontalCut = 1.0f - KINDA_SMALL_NUMBER;
    }

    // 【核心修复】：计算Z偏移量，使横截面（或最低点）位于Z=0
    // 如果有横截断，中心点在横截面上；如果没有横截断，中心点在最低点
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float CutPlaneZ = Radius * FMath::Cos(EndPhi);
    ZOffset = -CutPlaneZ;  // 偏移量使截断面Z=0

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
    // 【核心修复】：调整Z坐标，使横截面（或最低点）位于Z=0
    // 如果有横截断，中心点在横截面上；如果没有横截断，中心点在最低点
    float Z = Radius * CosPhi + ZOffset;

    return FVector(X, Y, Z);
}

FVector FSphereBuilder::GetSphereNormal(float Theta, float Phi) const
{
    return GetSpherePoint(Theta, Phi).GetSafeNormal();
}

FVector2D FSphereBuilder::GetSphereUV(float Theta, float Phi) const
{
    float U = Theta * Radius;
    float V = Phi * Radius;
    return FVector2D(U * ModelGenConstants::GLOBAL_UV_SCALE, V * ModelGenConstants::GLOBAL_UV_SCALE);
}

void FSphereBuilder::GenerateSphereMesh()
{
    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;

    // 【修复1】：防止横截断 = 1.0 时生成无效网格
    if (PhiRange <= KINDA_SMALL_NUMBER)
    {
        // 如果 PhiRange 太小，只生成一个端盖平面
        return;
    }

    const float StartTheta = 0.0f;
    // 【修复2】：区分竖截断 0 和 360 的效果
    float EndTheta;
    if (FMath::IsNearlyZero(VerticalCut))
    {
        EndTheta = 0.0f;  // 0度，没有球体
    }
    else if (FMath::IsNearlyEqual(VerticalCut, 360.0f))
    {
        EndTheta = 2.0f * PI;  // 360度，完整球体
    }
    else
    {
        EndTheta = FMath::DegreesToRadians(VerticalCut);
    }
    const float ThetaRange = EndTheta - StartTheta;

    // 【修复2】：如果 ThetaRange 为 0，不需要生成球体
    if (ThetaRange <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const int32 VerticalSegments = Sides;
    const int32 HorizontalSegments = Sides;

    TArray<TArray<int32>> GridIndices;
    GridIndices.SetNum(VerticalSegments + 1);

    for (int32 v = 0; v <= VerticalSegments; ++v)
    {
        const float VRatio = static_cast<float>(v) / VerticalSegments;
        const float Phi = StartPhi + VRatio * PhiRange;

        GridIndices[v].Reserve(HorizontalSegments + 1);

        for (int32 h = 0; h <= HorizontalSegments; ++h)
        {
            const float HRatio = static_cast<float>(h) / HorizontalSegments;
            const float Theta = StartTheta + HRatio * ThetaRange;

            FVector Pos = GetSpherePoint(Theta, Phi);
            FVector Normal = GetSphereNormal(Theta, Phi);
            FVector2D UV = GetSphereUV(Theta, Phi);

            GridIndices[v].Add(GetOrAddVertex(Pos, Normal, UV));
        }
    }

    for (int32 v = 0; v < VerticalSegments; ++v)
    {
        for (int32 h = 0; h < HorizontalSegments; ++h)
        {
            int32 V0 = GridIndices[v][h];
            int32 V1 = GridIndices[v][h + 1];
            int32 V2 = GridIndices[v + 1][h];
            int32 V3 = GridIndices[v + 1][h + 1];

            if (v == 0)
            {
                AddTriangle(V3, V2, V0);
            }
            else if (v == VerticalSegments - 1 && FMath::IsNearlyEqual(EndPhi, PI))
            {
                AddTriangle(V1, V2, V0);
            }
            else
            {
                AddQuad(V1, V3, V2, V0);
            }
        }
    }
}

void FSphereBuilder::GenerateCaps()
{
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float StartTheta = 0.0f;
    // 【修复2】：使用与 GenerateSphereMesh 相同的逻辑
    float EndTheta;
    if (FMath::IsNearlyZero(VerticalCut))
    {
        EndTheta = 0.0f;
    }
    else if (FMath::IsNearlyEqual(VerticalCut, 360.0f))
    {
        EndTheta = 2.0f * PI;
    }
    else
    {
        EndTheta = FMath::DegreesToRadians(VerticalCut);
    }

    if (HorizontalCut > KINDA_SMALL_NUMBER && EndPhi < PI - KINDA_SMALL_NUMBER)
    {
        GenerateHorizontalCap(EndPhi, true);
    }

    // 【修复2】：只有当竖截断不是完整球体（360度）时才生成竖截断端盖
    // VerticalCut = 0 时不需要生成端盖（因为没有球体）
    if (!FMath::IsNearlyZero(VerticalCut) && !FMath::IsNearlyEqual(VerticalCut, 360.0f))
    {
        GenerateVerticalCap(StartTheta, true);
        GenerateVerticalCap(EndTheta, false);
    }
}

void FSphereBuilder::GenerateHorizontalCap(float Phi, bool bIsBottom)
{
    const float StartTheta = 0.0f;
    // 【修复2】：使用与 GenerateSphereMesh 相同的逻辑
    float EndTheta;
    if (FMath::IsNearlyZero(VerticalCut))
    {
        EndTheta = 0.0f;
    }
    else if (FMath::IsNearlyEqual(VerticalCut, 360.0f))
    {
        EndTheta = 2.0f * PI;
    }
    else
    {
        EndTheta = FMath::DegreesToRadians(VerticalCut);
    }
    const float ThetaRange = EndTheta - StartTheta;

    // 【修复2】：如果 ThetaRange 为 0，不需要生成端盖
    if (ThetaRange <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // 【核心修复】：调整Z坐标，使横截面位于Z=0
    float CenterZ = Radius * FMath::Cos(Phi) + ZOffset;
    FVector CenterPos(0.0f, 0.0f, CenterZ);

    FVector Normal(0.0f, 0.0f, bIsBottom ? -1.0f : 1.0f);

    FVector2D CenterUV(CenterPos.X * ModelGenConstants::GLOBAL_UV_SCALE, CenterPos.Y * ModelGenConstants::GLOBAL_UV_SCALE);
    int32 CenterIndex = AddVertex(CenterPos, Normal, CenterUV);

    TArray<int32> RimIndices;
    RimIndices.Reserve(Sides + 1);

    for (int32 i = 0; i <= Sides; ++i)
    {
        float Theta = StartTheta + (static_cast<float>(i) / Sides) * ThetaRange;

        FVector Pos = GetSpherePoint(Theta, Phi);
        FVector2D UV(Pos.X * ModelGenConstants::GLOBAL_UV_SCALE, Pos.Y * ModelGenConstants::GLOBAL_UV_SCALE);

        RimIndices.Add(AddVertex(Pos, Normal, UV));
    }

    for (int32 i = 0; i < Sides; ++i)
    {
        if (bIsBottom)
        {
            // Bottom Cap: 反转渲染方向
            AddTriangle(RimIndices[i], RimIndices[i + 1], CenterIndex);
        }
        else
        {
            // Top Cap: 反转渲染方向
            AddTriangle(RimIndices[i + 1], RimIndices[i], CenterIndex);
        }
    }
}

void FSphereBuilder::GenerateVerticalCap(float Theta, bool bIsStart)
{
    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;

    FVector Tangent(-FMath::Sin(Theta), FMath::Cos(Theta), 0.0f);
    FVector Normal = bIsStart ? -Tangent : Tangent;

    TArray<int32> ProfileIndices;
    ProfileIndices.Reserve(Sides + 1);

    TArray<int32> AxisIndices;
    AxisIndices.Reserve(Sides + 1);

    // 【核心修复】：计算最大Z值，用于UV坐标计算
    // 最高点Z = Radius + ZOffset，最低点Z = -Radius + ZOffset
    const float MaxZ = Radius + ZOffset;

    // Generate vertices
    for (int32 i = 0; i <= Sides; ++i)
    {
        float Phi = StartPhi + (static_cast<float>(i) / Sides) * PhiRange;

        // 1. Profile Vertex
        FVector Pos = GetSpherePoint(Theta, Phi);
        float R_Proj = Radius * FMath::Sin(Phi);
        float Z_Proj = Pos.Z;
        float U = bIsStart ? -R_Proj : R_Proj;
        float V = MaxZ - Z_Proj;

        FVector2D UV(U * ModelGenConstants::GLOBAL_UV_SCALE, V * ModelGenConstants::GLOBAL_UV_SCALE);
        ProfileIndices.Add(AddVertex(Pos, Normal, UV));

        // 2. Axis Vertex
        // 【核心修复】：调整Z坐标，使横截面位于Z=0
        float Z_Axis = Radius * FMath::Cos(Phi) + ZOffset;
        FVector AxisPos(0, 0, Z_Axis);
        float V_Axis = MaxZ - Z_Axis;
        FVector2D UV_Axis(0.0f, V_Axis * ModelGenConstants::GLOBAL_UV_SCALE);
        AxisIndices.Add(AddVertex(AxisPos, Normal, UV_Axis));
    }

    // Stitch
    for (int32 i = 0; i < Sides; ++i)
    {
        int32 V_Axis_Curr = AxisIndices[i];
        int32 V_Prof_Curr = ProfileIndices[i];
        int32 V_Prof_Next = ProfileIndices[i + 1];
        int32 V_Axis_Next = AxisIndices[i + 1];

        // 【核心修复】：处理极点退化，防止生成重叠的无效三角形

        // Top Pole Check: i=0 and StartPhi=0
        bool bTopDegenerate = (i == 0) && FMath::IsNearlyZero(StartPhi);

        // Bottom Pole Check: i=Last and EndPhi=PI (Full bottom)
        bool bBottomDegenerate = (i == Sides - 1) && FMath::IsNearlyEqual(EndPhi, PI);

        if (bTopDegenerate)
        {
            // Top: Axis_Curr and Prof_Curr are the same vertex (Pole)
            // Use triangle: Axis_Curr, Prof_Next, Axis_Next
        if (bIsStart)
                AddTriangle(V_Axis_Curr, V_Prof_Next, V_Axis_Next);
            else
                AddTriangle(V_Axis_Curr, V_Axis_Next, V_Prof_Next);
        }
        else if (bBottomDegenerate)
        {
            // Bottom: Axis_Next and Prof_Next are the same vertex (Pole)
            // Use triangle: Axis_Curr, Prof_Curr, Axis_Next
            if (bIsStart)
                AddTriangle(V_Axis_Curr, V_Prof_Curr, V_Axis_Next);
            else
                AddTriangle(V_Axis_Curr, V_Axis_Next, V_Prof_Curr);
        }
        else
        {
            // Normal Quad
            if (bIsStart)
                AddQuad(V_Axis_Curr, V_Prof_Curr, V_Prof_Next, V_Axis_Next);
            else
            AddQuad(V_Axis_Curr, V_Axis_Next, V_Prof_Next, V_Prof_Curr);
        }
    }
}