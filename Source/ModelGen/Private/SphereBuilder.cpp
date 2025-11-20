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
    // Theta: 水平角度 (0 到 2*PI)
    // Phi: 垂直角度 (0 到 PI，从顶部到底部)
    float X = Radius * FMath::Sin(Phi) * FMath::Cos(Theta);
    float Y = Radius * FMath::Sin(Phi) * FMath::Sin(Theta);
    float Z = Radius * FMath::Cos(Phi);
    return FVector(X, Y, Z);
}

FVector FSphereBuilder::GetSphereNormal(float Theta, float Phi) const
{
    // 球面法线就是归一化的位置向量
    FVector Pos = GetSpherePoint(Theta, Phi);
    return Pos.GetSafeNormal();
}

FVector2D FSphereBuilder::GetSphereUV(float Theta, float Phi) const
{
    // U: 基于Theta (0 到 2*PI -> 0 到 1)
    // V: 基于Phi (0 到 PI -> 0 到 1)
    float U = Theta / (2.0f * PI);
    float V = Phi / PI;
    return FVector2D(U * ModelGenConstants::GLOBAL_UV_SCALE, V * ModelGenConstants::GLOBAL_UV_SCALE);
}

void FSphereBuilder::GenerateSphereMesh()
{
    // TODO: 实现球体网格生成逻辑
    // 1. 根据 HorizontalCut 和 VerticalCut 计算实际的角度范围
    // 2. 生成顶点网格
    // 3. 连接三角形
    // 4. 计算法线和UV
    
    // 横截断：从底部截断（HorizontalCut = 0 表示不截断，1 表示完全截断到底部）
    const float StartPhi = 0.0f;  // 从顶部开始
    const float EndPhi = PI * (1.0f - HorizontalCut);  // 从底部截断
    const float PhiRange = EndPhi - StartPhi;
    
    const float StartTheta = 0.0f;
    const float EndTheta = (VerticalCut > 0.0f && VerticalCut < 360.0f) 
        ? FMath::DegreesToRadians(VerticalCut) : (2.0f * PI);
    const float ThetaRange = EndTheta - StartTheta;
    
    const int32 VerticalSegments = Sides;
    const int32 HorizontalSegments = Sides;
    
    // 生成顶点
    TArray<TArray<int32>> VertexGrid;
    VertexGrid.Reserve(VerticalSegments + 1);
    
    for (int32 v = 0; v <= VerticalSegments; ++v)
    {
        const float Phi = StartPhi + (static_cast<float>(v) / VerticalSegments) * PhiRange;
        TArray<int32> Row;
        Row.Reserve(HorizontalSegments + 1);
        
        for (int32 h = 0; h <= HorizontalSegments; ++h)
        {
            const float Theta = StartTheta + (static_cast<float>(h) / HorizontalSegments) * ThetaRange;
            
            FVector Pos = GetSpherePoint(Theta, Phi);
            FVector Normal = GetSphereNormal(Theta, Phi);
            FVector2D UV = GetSphereUV(Theta, Phi);
            
            Row.Add(GetOrAddVertex(Pos, Normal, UV));
        }
        VertexGrid.Add(Row);
    }
    
    // 生成三角形（确保从外部看是逆时针顺序）
    for (int32 v = 0; v < VerticalSegments; ++v)
    {
        for (int32 h = 0; h < HorizontalSegments; ++h)
        {
            int32 V0 = VertexGrid[v][h];
            int32 V1 = VertexGrid[v][h + 1];
            int32 V2 = VertexGrid[v + 1][h];
            int32 V3 = VertexGrid[v + 1][h + 1];
            
            // 第一个三角形：V0 -> V1 -> V2 (逆时针，从外部看)
            AddTriangle(V0, V1, V2);
            // 第二个三角形：V1 -> V3 -> V2 (逆时针，从外部看)
            AddTriangle(V1, V3, V2);
        }
    }
}

void FSphereBuilder::GenerateCaps()
{
    // 计算角度范围
    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float StartTheta = 0.0f;
    const float EndTheta = (VerticalCut > 0.0f && VerticalCut < 360.0f) 
        ? FMath::DegreesToRadians(VerticalCut) : (2.0f * PI);
    
    // 生成横截断端盖（底部）
    if (HorizontalCut > KINDA_SMALL_NUMBER)
    {
        GenerateHorizontalCap(EndPhi, true); // 底部端盖
    }
    
    // 生成竖截断端盖
    if (VerticalCut > 0.0f && VerticalCut < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateVerticalCap(StartTheta, true);  // 起始端盖
        GenerateVerticalCap(EndTheta, false);    // 结束端盖
    }
}

void FSphereBuilder::GenerateHorizontalCap(float Phi, bool bIsBottom)
{
    // 生成横截断端盖（圆形端盖或扇形端盖，取决于是否有竖截断）
    // Phi: 截断位置的Phi角度
    // bIsBottom: 是否为底部端盖
    
    const float StartTheta = 0.0f;
    const float EndTheta = (VerticalCut > 0.0f && VerticalCut < 360.0f) 
        ? FMath::DegreesToRadians(VerticalCut) : (2.0f * PI);
    const float ThetaRange = EndTheta - StartTheta;
    const bool bHasVerticalCut = (VerticalCut > 0.0f && VerticalCut < 360.0f);
    
    const int32 CapSegments = Sides;
    
    // 计算端盖中心点：横截断平面的圆的中心
    // 在Phi角度处，圆的中心在Z轴上，距离球心的距离是 Radius * Cos(Phi)
    float CenterZ = Radius * FMath::Cos(Phi);
    FVector CenterPos(0.0f, 0.0f, CenterZ);
    
    // 计算法线方向：垂直于截断平面，指向外部
    // 对于底部端盖，法线向下；对于顶部端盖，法线向上
    FVector CenterNormal = GetSphereNormal(0.0f, Phi);
    
    // 如果端盖在底部，法线应该向下
    if (bIsBottom)
    {
        CenterNormal = -CenterNormal;
    }
    
    FVector2D CenterUV(CenterPos.X * ModelGenConstants::GLOBAL_UV_SCALE, 
                       CenterPos.Y * ModelGenConstants::GLOBAL_UV_SCALE);
    int32 CenterIndex = AddVertex(CenterPos, CenterNormal, CenterUV);
    
    // 生成边界顶点
    TArray<int32> BoundaryVertices;
    BoundaryVertices.Reserve(CapSegments + 1);
    
    for (int32 i = 0; i <= CapSegments; ++i)
    {
        float Theta = StartTheta + (static_cast<float>(i) / CapSegments) * ThetaRange;
        FVector Pos = GetSpherePoint(Theta, Phi);
        FVector Normal = CenterNormal; // 端盖法线统一
        FVector2D UV(Pos.X * ModelGenConstants::GLOBAL_UV_SCALE, 
                      Pos.Y * ModelGenConstants::GLOBAL_UV_SCALE);
        
        BoundaryVertices.Add(GetOrAddVertex(Pos, Normal, UV));
    }
    
    // 生成三角形（从中心到边界）
    for (int32 i = 0; i < CapSegments; ++i)
    {
        if (bIsBottom)
        {
            // 底部端盖：从外部看是逆时针（Center -> V[i] -> V[i+1]）
            AddTriangle(CenterIndex, BoundaryVertices[i], BoundaryVertices[i + 1]);
        }
        else
        {
            // 顶部端盖：从外部看是逆时针（Center -> V[i+1] -> V[i]）
            AddTriangle(CenterIndex, BoundaryVertices[i + 1], BoundaryVertices[i]);
        }
    }
}

void FSphereBuilder::GenerateVerticalCap(float Theta, bool bIsStart)
{
    // 生成竖截断端盖（扇形端盖）
    // Theta: 截断位置的Theta角度
    // bIsStart: 是否为起始端盖
    
    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;
    const float MidPhi = (StartPhi + EndPhi) * 0.5f;
    const bool bHasHorizontalCut = (HorizontalCut > KINDA_SMALL_NUMBER);
    
    const int32 CapSegments = Sides;
    
    // 计算端盖中心点
    // 如果有横截断，使用横截断平面的中心点（与横截断端盖使用相同的中心点）
    // 如果没有横截断，使用球心
    FVector CenterPos;
    if (bHasHorizontalCut)
    {
        // 有横截断：使用横截断平面的中心点（Z轴上的点）
        // 与横截断端盖使用相同的中心点：(0, 0, Radius * Cos(EndPhi))
        float CenterZ = Radius * FMath::Cos(EndPhi);
        CenterPos = FVector(0.0f, 0.0f, CenterZ);
    }
    else
    {
        // 无横截断：使用球心
        CenterPos = FVector(0.0f, 0.0f, 0.0f);
    }
    
    // 计算端盖法线（垂直于截断面，指向外部）
    // 法线方向：在XY平面内，垂直于Theta方向
    FVector NormalDir(-FMath::Sin(Theta), FMath::Cos(Theta), 0.0f);
    
    if (bIsStart)
    {
        // 起始端盖：法线指向Theta增加的方向（指向球体外部）
        NormalDir = -NormalDir;
    }
    // 结束端盖：法线指向Theta减少的方向（指向球体外部，已经是正确的）
    
    FVector2D CenterUV(CenterPos.X * ModelGenConstants::GLOBAL_UV_SCALE, 
                       CenterPos.Y * ModelGenConstants::GLOBAL_UV_SCALE);
    int32 CenterIndex = AddVertex(CenterPos, NormalDir, CenterUV);
    
    // 生成边界顶点（沿Phi方向，从顶部到底部）
    TArray<int32> BoundaryVertices;
    BoundaryVertices.Reserve(CapSegments + 1);
    
    for (int32 i = 0; i <= CapSegments; ++i)
    {
        float Phi = StartPhi + (static_cast<float>(i) / CapSegments) * PhiRange;
        FVector Pos = GetSpherePoint(Theta, Phi);
        FVector Normal = NormalDir; // 端盖法线统一
        FVector2D UV(Pos.X * ModelGenConstants::GLOBAL_UV_SCALE, 
                      Pos.Y * ModelGenConstants::GLOBAL_UV_SCALE);
        
        BoundaryVertices.Add(GetOrAddVertex(Pos, Normal, UV));
    }
    
    // 生成三角形（从中心到边界，确保从外部看是逆时针）
    for (int32 i = 0; i < CapSegments; ++i)
    {
        if (bIsStart)
        {
            // 起始端盖：从外部看是逆时针（Center -> V[i] -> V[i+1]）
            // 边界顶点从顶部（i=0）到底部（i=CapSegments）
            AddTriangle(CenterIndex, BoundaryVertices[i], BoundaryVertices[i + 1]);
        }
        else
        {
            // 结束端盖：从外部看是逆时针（Center -> V[i+1] -> V[i]）
            // 需要反转顺序以确保逆时针
            AddTriangle(CenterIndex, BoundaryVertices[i + 1], BoundaryVertices[i]);
        }
    }
}

