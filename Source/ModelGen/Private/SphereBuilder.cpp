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

bool FSphereBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Sphere.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    // Cache settings
    Radius = Sphere.Radius;
    Sides = Sphere.Sides;
    HorizontalCut = Sphere.HorizontalCut;
    VerticalCut = Sphere.VerticalCut;

    // 【修复1】：防止横截断 = 1.0 时崩溃
    // 限制最大值为接近但不等于 1.0，否则球体消失
    if (HorizontalCut >= 1.0f - KINDA_SMALL_NUMBER)
    {
        HorizontalCut = 1.0f - KINDA_SMALL_NUMBER;
    }

    // 【核心修复】：计算 Z 偏移量，使模型的“脚底”位于 (0,0,0)
    // 新逻辑下，球体是从顶部(Phi=0)生成到底部(Phi=EndPhi)
    // 所以最低点就是 EndPhi 所在的高度
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float CutPlaneZ = Radius * FMath::Cos(EndPhi);
    
    // ZOffset 将把最低点移动到 Z=0
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
    
    // 【核心修复】：应用 ZOffset
    float Z = Radius * CosPhi + ZOffset;

    return FVector(X, Y, Z);
}

FVector FSphereBuilder::GetSphereNormal(float Theta, float Phi) const
{
    // 法线方向不受 ZOffset 影响，依旧是从球心指向表面
    // 注意：这里的球心在世界坐标系下实际上是 (0, 0, ZOffset)
    return FVector(
        FMath::Sin(Phi) * FMath::Cos(Theta),
        FMath::Sin(Phi) * FMath::Sin(Theta),
        FMath::Cos(Phi)
    );
}

void FSphereBuilder::GenerateSphereMesh()
{
    // 【保留多边形优化】：纵向分段为横向的一半，保证侧视图形状正确
    const int32 NumRings = FMath::Max(2, Sides / 2);
    const int32 NumSegments = Sides;

    // 【方向反转】：从 0 (Top) 到 EndPhi (Cut Plane)
    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;

    if (PhiRange <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // 处理竖截断角度
    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;
    const float ThetaRange = EndTheta - StartTheta;

    // 临时存储索引网格
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
            
            // 使用标准 0-1 UV 映射
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

            // 【反转渲染方向】：反转所有面的顶点顺序
            // 检测极点情况 (Top Pole)
            // 当 Phi=0 时，V0 和 V1 是同一个点（北极点）
            if (v == 0)
            {
                // Top Triangle: 反转顺序
                AddTriangle(V0, V3, V2);
            }
            // 底部虽然可能是切面，但在 GenerateSphereMesh 里我们假设是生成"皮"
            // 只有当 EndPhi == PI 时，底部才是极点
            // 如果 EndPhi < PI (有截断)，底部是开环的，应该生成 Quad
            else if (v == NumRings - 1 && FMath::IsNearlyEqual(EndPhi, PI))
            {
                // Bottom Triangle (Pole): 反转顺序
                AddTriangle(V0, V1, V2);
            }
            else
            {
                // Standard Quad: 反转顺序
                AddQuad(V0, V1, V3, V2);
            }
        }
    }
}

void FSphereBuilder::GenerateCaps()
{
    const float EndPhi = PI * (1.0f - HorizontalCut);
    
    // 竖截断参数
    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;

    // 【生成底部横截盖子】
    // 只有当 EndPhi < PI (即没到底) 时，底部才有一个洞，需要封口
    // StartPhi 固定为 0 (北极)，所以顶部不需要盖子（本身就是闭合的极点）
    if (EndPhi < PI - KINDA_SMALL_NUMBER)
    {
        GenerateHorizontalCap(EndPhi, true); // true = Bottom Cap
    }

    // 【生成竖截面盖子】
    // 当 VerticalCut < 1.0 时生成
    if (VerticalCut < 1.0f - KINDA_SMALL_NUMBER)
    {
        GenerateVerticalCap(StartTheta, true);
        GenerateVerticalCap(EndTheta, false);
    }
}

void FSphereBuilder::GenerateHorizontalCap(float Phi, bool bIsBottom)
{
    // 横向盖子依然使用 Sides 分段，保持圆滑度
    const int32 Segments = Sides;
    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;
    const float ThetaRange = EndTheta - StartTheta;

    if (ThetaRange <= KINDA_SMALL_NUMBER) return;

    // 中心点 (ZOffset 已在 GetSpherePoint 中包含，这里手动计算中心 Z)
    float CenterZ = Radius * FMath::Cos(Phi) + ZOffset;
    FVector CenterPos(0.0f, 0.0f, CenterZ);

    // Normal: Bottom Cap 指向 -Z
    FVector Normal(0.0f, 0.0f, -1.0f); 

    // UV: 平面投影 (0.5, 0.5) 为中心
    FVector2D CenterUV(0.5f, 0.5f);
    int32 CenterIndex = AddVertex(CenterPos, Normal, CenterUV);

    TArray<int32> RimIndices;
    RimIndices.Reserve(Segments + 1);

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Ratio = static_cast<float>(i) / Segments;
        float Theta = StartTheta + Ratio * ThetaRange;

        FVector Pos = GetSpherePoint(Theta, Phi); // GetSpherePoint 已包含 ZOffset
        
        // Planar UV
        float U = (Pos.X / Radius) * 0.5f + 0.5f;
        float V = (Pos.Y / Radius) * 0.5f + 0.5f;

        RimIndices.Add(AddVertex(Pos, Normal, FVector2D(U, V)));
    }

    // 生成三角扇
    for (int32 i = 0; i < Segments; ++i)
        {
        // Bottom Cap: Center -> Rim[i] -> Rim[i+1] (CW looking from top = CCW looking from bottom)
        // 实际上：Center -> Next -> Curr 还是 Center -> Curr -> Next?
        // Bottom Cap Normal is Down. We look from Down.
        // Theta goes CCW around Z+. Looking from bottom, Theta goes CW.
        // To get CCW winding relative to Bottom Normal:
        AddTriangle(CenterIndex, RimIndices[i], RimIndices[i + 1]);
    }
}

void FSphereBuilder::GenerateVerticalCap(float Theta, bool bIsStart)
{
    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;

    // 纵向分段必须匹配 GenerateSphereMesh (Sides/2)
    const int32 Segments = FMath::Max(2, Sides / 2);

    // 计算切面法线
    FVector Tangent(-FMath::Sin(Theta), FMath::Cos(Theta), 0.0f);
    // bIsStart (Theta=0) -> Normal points along -Tangent (Clockwise)
    FVector Normal = bIsStart ? -Tangent : Tangent;

    TArray<int32> ProfileIndices;
    TArray<int32> AxisIndices;

    // 生成顶点条带
    for (int32 i = 0; i <= Segments; ++i)
    {
        float Ratio = static_cast<float>(i) / Segments;
        float Phi = StartPhi + Ratio * PhiRange;

        // 1. 表面点
        FVector Pos = GetSpherePoint(Theta, Phi);
        
        // UV: 简单映射
        // U: 0 (Axis) -> 1 (Surface)
        // V: 0 (Top) -> 1 (Bottom)
        FVector2D UV_Prof(1.0f, Ratio);
        ProfileIndices.Add(AddVertex(Pos, Normal, UV_Prof));

        // 2. 轴线点 (投影到 Z轴)
        FVector AxisPos(0.0f, 0.0f, Pos.Z); // Pos.Z 已经包含了 ZOffset
        FVector2D UV_Axis(0.0f, Ratio);
        AxisIndices.Add(AddVertex(AxisPos, Normal, UV_Axis));
    }

    // 缝合
    for (int32 i = 0; i < Segments; ++i)
    {
        int32 AxisCurr = AxisIndices[i];
        int32 AxisNext = AxisIndices[i + 1];
        int32 ProfCurr = ProfileIndices[i];
        int32 ProfNext = ProfileIndices[i + 1];

        // 处理极点退化 (Top is always a pole at Phi=0)
        bool bTopDegenerate = (i == 0); 
        // Bottom only degenerate if full sphere (EndPhi == PI)
        bool bBottomDegenerate = (i == Segments - 1) && FMath::IsNearlyEqual(EndPhi, PI);

        if (bTopDegenerate)
        {
            // Top: AxisCurr == ProfCurr (Pole)
            // Triangle: AxisCurr -> ProfNext -> AxisNext
        if (bIsStart)
                AddTriangle(AxisCurr, ProfNext, AxisNext);
            else
                AddTriangle(AxisCurr, AxisNext, ProfNext);
        }
        else if (bBottomDegenerate)
        {
            // Bottom: AxisNext == ProfNext (Pole)
            // Triangle: AxisCurr -> ProfCurr -> AxisNext
            if (bIsStart)
                AddTriangle(AxisCurr, ProfCurr, AxisNext);
            else
                AddTriangle(AxisCurr, AxisNext, ProfCurr);
        }
        else
        {
            // Quad
            if (bIsStart)
                AddQuad(AxisCurr, ProfCurr, ProfNext, AxisNext);
            else
                AddQuad(AxisCurr, AxisNext, ProfNext, ProfCurr);
        }
    }
}