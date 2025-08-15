// Copyright (c) 2024. All rights reserved.

#include "PyramidBuilder.h"
#include "Pyramid.h"
#include "ModelGenMeshData.h"

FPyramidBuilder::FPyramidBuilder(const APyramid& InPyramid)
    : Pyramid(InPyramid)
{
    Clear();
    PrecomputeConstants();
    PrecomputeVertices();
    PrecomputeUVs();
}

bool FPyramidBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!ValidateParameters())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    GenerateBaseFace();
    GenerateBevelSection();
    GeneratePyramidSides();

    if (!ValidateGeneratedData())
    {
        return false;
    }

    OutMeshData = MeshData;
    return true;
}

bool FPyramidBuilder::ValidateParameters() const
{
    return Pyramid.IsValid();
}

int32 FPyramidBuilder::CalculateVertexCountEstimate() const
{
    return Pyramid.CalculateVertexCountEstimate();
}

int32 FPyramidBuilder::CalculateTriangleCountEstimate() const
{
    return Pyramid.CalculateTriangleCountEstimate();
}

void FPyramidBuilder::GenerateBaseFace()
{
    GenerateBasePolygon();
}

// 新增：生成底面多边形
void FPyramidBuilder::GenerateBasePolygon()
{
    // 使用预计算的底面顶点和UV，向下法线
    GeneratePolygonFaceOptimized(BaseVertices, FVector(0, 0, -1), BaseUVs, false);
}

void FPyramidBuilder::GenerateBevelSection()
{
    if (BevelRadius <= 0.0f)
    {
        return;
    }
    
    GenerateBevelSideStrip();
}

// 新增：生成倒角侧面条带
void FPyramidBuilder::GenerateBevelSideStrip()
{
    // 使用预计算的倒角顶点和UV数据
    GenerateSideStripOptimized(BevelBottomVertices, BevelTopVertices, BevelUVs, true);
}

void FPyramidBuilder::GeneratePyramidSides()
{
    GeneratePyramidSideTriangles();
}

// 新增：生成金字塔侧面三角形
void FPyramidBuilder::GeneratePyramidSideTriangles()
{
    // 为每个侧面生成三角形
    for (int32 i = 0; i < Sides; ++i)
    {
        const int32 NextI = (i + 1) % Sides;
        
        // 计算侧面法线（确保指向外部）
        FVector Edge1 = PyramidBaseVertices[NextI] - PyramidBaseVertices[i];
        FVector Edge2 = PyramidTopPoint - PyramidBaseVertices[i];
        FVector SideNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
        
        // 使用稳定UV映射系统：基于顶点位置生成UV
        FVector2D TopUV = GenerateStableUV(PyramidTopPoint, SideNormal);
        FVector2D BaseUV1 = GenerateStableUV(PyramidBaseVertices[i], SideNormal);
        FVector2D BaseUV2 = GenerateStableUV(PyramidBaseVertices[NextI], SideNormal);
        
        // 添加顶点
        int32 TopVertex = GetOrAddVertexWithDualUV(PyramidTopPoint, SideNormal);
        int32 V1 = GetOrAddVertexWithDualUV(PyramidBaseVertices[i], SideNormal);
        int32 V2 = GetOrAddVertexWithDualUV(PyramidBaseVertices[NextI], SideNormal);
        
        // 添加三角形
        AddTriangle(V2, V1, TopVertex);
    }
}

void FPyramidBuilder::GeneratePolygonFace(const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder, float UVOffsetZ)
{
    if (PolygonVerts.Num() < 3)
    {
        return;
    }
    
    TArray<int32> VertexIndices;
    for (int32 i = 0; i < PolygonVerts.Num(); ++i)
    {
        float Angle = 2.0f * PI * static_cast<float>(i) / PolygonVerts.Num();
        float U = 0.5f + 0.5f * FMath::Cos(Angle);
        float V = 0.5f + 0.5f * FMath::Sin(Angle);
        FVector2D UV(U, V + UVOffsetZ);
        int32 VertexIndex = GetOrAddVertex(PolygonVerts[i], Normal, UV);
        VertexIndices.Add(VertexIndex);
    }
    
    for (int32 i = 1; i < PolygonVerts.Num() - 1; ++i)
    {
        int32 V0 = VertexIndices[0];
        int32 V1 = VertexIndices[i];
        int32 V2 = VertexIndices[i + 1];
        
        if (bReverseOrder)
        {
            AddTriangle(V0, V2, V1);
        }
        else
        {
            AddTriangle(V0, V1, V2);
        }
    }
}

void FPyramidBuilder::GenerateSideStrip(const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal, float UVOffsetY, float UVScaleY)
{
    if (BottomVerts.Num() != TopVerts.Num())
    {
        return;
    }
    
    for (int32 i = 0; i < BottomVerts.Num(); ++i)
    {
        const int32 NextI = (i + 1) % BottomVerts.Num();
        
        FVector Edge1 = BottomVerts[NextI] - BottomVerts[i];
        FVector Edge2 = TopVerts[i] - BottomVerts[i];
        FVector SideNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
        
        if (bReverseNormal) SideNormal = -SideNormal;
        
        int32 V1 = GetOrAddVertex(BottomVerts[i], SideNormal, FVector2D(static_cast<float>(i) / BottomVerts.Num(), UVOffsetY));
        int32 V2 = GetOrAddVertex(BottomVerts[NextI], SideNormal, FVector2D(static_cast<float>(NextI) / BottomVerts.Num(), UVOffsetY));
        int32 V3 = GetOrAddVertex(TopVerts[NextI], SideNormal, FVector2D(static_cast<float>(NextI) / TopVerts.Num(), UVOffsetY + UVScaleY));
        int32 V4 = GetOrAddVertex(TopVerts[i], SideNormal, FVector2D(static_cast<float>(i) / TopVerts.Num(), UVOffsetY + UVScaleY));
        
        if (bReverseNormal)
        {
            AddTriangle(V1, V3, V2);
            AddTriangle(V1, V4, V3);
        }
        else
        {
            AddTriangle(V1, V2, V3);
            AddTriangle(V1, V3, V4);
        }
    }
}

TArray<FVector> FPyramidBuilder::GenerateCircleVertices(float Radius, float Z, int32 NumSides) const
{
    TArray<FVector> Vertices;
    Vertices.Reserve(NumSides);
    
    for (int32 i = 0; i < NumSides; ++i)
    {
        const float Angle = 2.0f * PI * static_cast<float>(i) / NumSides;
        const float X = Radius * FMath::Cos(Angle);
        const float Y = Radius * FMath::Sin(Angle);
        Vertices.Add(FVector(X, Y, Z));
    }
    
    return Vertices;
}

TArray<FVector> FPyramidBuilder::GenerateBevelVertices(float BottomRadius, float TopRadius, float Z, int32 NumSides) const
{
    // 这个方法可以用于生成更复杂的倒角形状
    // 目前使用简单的圆形顶点
    return GenerateCircleVertices(TopRadius, Z, NumSides);
}

void FPyramidBuilder::PrecomputeConstants()
{
    BaseRadius = Pyramid.BaseRadius;
    Height = Pyramid.Height;
    Sides = Pyramid.Sides;
    BevelRadius = Pyramid.BevelRadius;
    BevelTopRadius = Pyramid.GetBevelTopRadius();
}

void FPyramidBuilder::PrecomputeTrigonometricValues()
{
    AngleValues.SetNum(Sides);
    CosValues.SetNum(Sides);
    SinValues.SetNum(Sides);
    
    for (int32 i = 0; i < Sides; ++i)
    {
        const float Angle = 2.0f * PI * static_cast<float>(i) / Sides;
        AngleValues[i] = Angle;
        CosValues[i] = FMath::Cos(Angle);
        SinValues[i] = FMath::Sin(Angle);
    }
}

void FPyramidBuilder::PrecomputeUVScaleValues()
{
    UVScaleValues.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        UVScaleValues[i] = static_cast<float>(i) / Sides;
    }
}

void FPyramidBuilder::PrecomputeVertices()
{
    PrecomputeTrigonometricValues();
    InitializeBaseVertices();
    InitializeBevelVertices();
    InitializePyramidVertices();
}

void FPyramidBuilder::InitializeBaseVertices()
{
    BaseVertices.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        const float X = BevelTopRadius * CosValues[i];
        const float Y = BevelTopRadius * SinValues[i];
        BaseVertices[i] = FVector(X, Y, 0.0f);
    }
}

void FPyramidBuilder::InitializeBevelVertices()
{
    if (BevelRadius <= 0.0f)
    {
        BevelBottomVertices.Empty();
        BevelTopVertices.Empty();
        return;
    }
    
    BevelBottomVertices = BaseVertices;
    
    BevelTopVertices.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        const float X = BevelTopRadius * CosValues[i];
        const float Y = BevelTopRadius * SinValues[i];
        BevelTopVertices[i] = FVector(X, Y, BevelRadius);
    }
}

void FPyramidBuilder::InitializePyramidVertices()
{
    // 金字塔底部顶点（倒角顶部或底面）
    if (BevelRadius > 0.0f)
    {
        PyramidBaseVertices = BevelTopVertices;
    }
    else
    {
        PyramidBaseVertices = BaseVertices;
    }
    
    // 金字塔顶点
    PyramidTopPoint = FVector(0, 0, Height);
}

// 基于顶点位置的稳定UV映射 - 重写父类实现
FVector2D FPyramidBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    // 基于顶点位置生成稳定的UV坐标，确保模型变化时UV不变
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    // 根据法线方向选择UV映射策略（真正的2U系统：0-1范围）
    if (FMath::Abs(Normal.Z) > 0.9f)
    {
        // 水平面（底面或顶面）
        if (Z < 0.1f)
        {
            // 底面：使用圆形映射，范围 (0,0) 到 (1,1)
            float Radius = FMath::Sqrt(X * X + Y * Y);
            float Angle = FMath::Atan2(Y, X);
            if (Angle < 0) Angle += 2.0f * PI;
            
            float U = 0.5f + 0.5f * FMath::Cos(Angle);
            float V = 0.5f + 0.5f * FMath::Sin(Angle);
            return FVector2D(U, V);
        }
        else
        {
            // 顶面：使用投影映射，范围 (0,0) 到 (1,1)
            float U = (X / BaseRadius + 1.0f) * 0.5f;
            float V = (Y / BaseRadius + 1.0f) * 0.5f;
            return FVector2D(U, V);
        }
    }
    else
    {
        // 侧面：使用柱面映射，范围 (0,0) 到 (1,1) - 真正的2U系统
        float Angle = FMath::Atan2(Y, X);
        if (Angle < 0) Angle += 2.0f * PI;
        
        float U = Angle / (2.0f * PI);  // 0 到 1
        float V = (Z / Height);          // 0 到 1
        return FVector2D(U, V);
    }
}

// 新增：预计算UV
void FPyramidBuilder::PrecomputeUVs()
{
    // 先预计算UV缩放值
    PrecomputeUVScaleValues();
    
    // 生成各种UV坐标，确保不重叠
    BaseUVs = GenerateCircularUVs(Sides, 0.0f);
    SideUVs = GenerateSideStripUVs(Sides, 0.0f, 1.0f);
    
    // 倒角UV：占据UV空间的右上角区域，避免与侧面重叠
    if (BevelRadius > 0.0f)
    {
        BevelUVs.SetNum(Sides);
        for (int32 i = 0; i < Sides; ++i)
        {
            float U = 1.0f + (static_cast<float>(i) / Sides);  // 1.0 到 2.0
            float V = 0.0f + (static_cast<float>(i) / Sides);  // 0.0 到 1.0
            BevelUVs[i] = FVector2D(U, V);
        }
    }
}

// 新增：生成圆形UV坐标（2U系统）
TArray<FVector2D> FPyramidBuilder::GenerateCircularUVs(int32 NumSides, float UVOffsetZ) const
{
    TArray<FVector2D> UVs;
    UVs.SetNum(NumSides);
    
    for (int32 i = 0; i < NumSides; ++i)
    {
        // 使用2U系统：底面占据UV空间的左下角区域 (0,0) 到 (1,1)
        float U = 0.5f + 0.5f * FMath::Cos(AngleValues[i]);  // 0 到 1
        float V = 0.5f + 0.5f * FMath::Sin(AngleValues[i]);  // 0 到 1
        UVs[i] = FVector2D(U, V);
    }
    
    return UVs;
}

// 新增：生成侧面条带UV坐标（2U系统）
TArray<FVector2D> FPyramidBuilder::GenerateSideStripUVs(int32 NumSides, float UVOffsetY, float UVScaleY) const
{
    TArray<FVector2D> UVs;
    UVs.SetNum(NumSides);
    
    for (int32 i = 0; i < NumSides; ++i)
    {
        // 使用2U系统：侧面占据UV空间的右侧区域 (1,0) 到 (2,1)
        float U = 1.0f + (static_cast<float>(i) / NumSides);  // 1.0 到 2.0
        float V = UVOffsetY + UVScaleY;                       // 0.0 到 1.0
        UVs[i] = FVector2D(U, V);
    }
    
    return UVs;
}

// 新增：优化后的多边形面生成函数
void FPyramidBuilder::GeneratePolygonFaceOptimized(const TArray<FVector>& Vertices, 
                                                  const FVector& Normal, 
                                                  const TArray<FVector2D>& UVs,
                                                  bool bReverseOrder)
{
    if (Vertices.Num() < 3 || UVs.Num() != Vertices.Num())
    {
        return;
    }
    
    // 添加所有顶点，使用稳定UV映射
    TArray<int32> VertexIndices;
    VertexIndices.Reserve(Vertices.Num());
    
    for (int32 i = 0; i < Vertices.Num(); ++i)
    {
        // 使用稳定UV映射而不是预计算的UV
        FVector2D StableUV = GenerateStableUV(Vertices[i], Normal);
        int32 VertexIndex = GetOrAddVertexWithDualUV(Vertices[i], Normal);
        VertexIndices.Add(VertexIndex);
    }
    
    // 生成三角形（扇形三角剖分）
    for (int32 i = 1; i < Vertices.Num() - 1; ++i)
    {
        int32 V0 = VertexIndices[0];
        int32 V1 = VertexIndices[i];
        int32 V2 = VertexIndices[i + 1];
        
        if (bReverseOrder)
        {
            AddTriangle(V0, V2, V1);
        }
        else
        {
            AddTriangle(V0, V1, V2);
        }
    }
}

// 新增：优化后的侧面条带生成函数
void FPyramidBuilder::GenerateSideStripOptimized(const TArray<FVector>& BottomVerts, 
                                                const TArray<FVector>& TopVerts, 
                                                const TArray<FVector2D>& UVs,
                                                bool bReverseNormal)
{
    if (BottomVerts.Num() != TopVerts.Num() || UVs.Num() != BottomVerts.Num())
    {
        return;
    }
    
    for (int32 i = 0; i < BottomVerts.Num(); ++i)
    {
        const int32 NextI = (i + 1) % BottomVerts.Num();
        
        // 计算侧面法线（确保指向外部）
        FVector Edge1 = BottomVerts[NextI] - BottomVerts[i];
        FVector Edge2 = TopVerts[i] - BottomVerts[i];
        FVector SideNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
        
        if (bReverseNormal) SideNormal = -SideNormal;
        
        // 使用稳定UV映射系统
        FVector2D BottomUV1 = GenerateStableUV(BottomVerts[i], SideNormal);
        FVector2D BottomUV2 = GenerateStableUV(BottomVerts[NextI], SideNormal);
        FVector2D TopUV1 = GenerateStableUV(TopVerts[i], SideNormal);
        FVector2D TopUV2 = GenerateStableUV(TopVerts[NextI], SideNormal);
        
        // 添加四个顶点
        int32 V1 = GetOrAddVertexWithDualUV(BottomVerts[i], SideNormal);
        int32 V2 = GetOrAddVertexWithDualUV(BottomVerts[NextI], SideNormal);
        int32 V3 = GetOrAddVertexWithDualUV(TopVerts[NextI], SideNormal);
        int32 V4 = GetOrAddVertexWithDualUV(TopVerts[i], SideNormal);
        
        // 添加四边形（两个三角形）
        if (bReverseNormal)
        {
            AddTriangle(V1, V3, V2);
            AddTriangle(V1, V4, V3);
        }
        else
        {
            AddTriangle(V1, V2, V3);
            AddTriangle(V1, V3, V4);
        }
    }
}

// 新增：验证预计算数据
bool FPyramidBuilder::ValidatePrecomputedData() const
{
    bool bIsValid = true;
    
    // 验证常量
    if (BaseRadius <= 0.0f || Height <= 0.0f || Sides < 3)
    {
        return false;
    }
    
    // 验证顶点数据
    if (BaseVertices.Num() != Sides)
    {
        return false;
    }
    
    if (BevelRadius > 0.0f)
    {
        if (BevelBottomVertices.Num() != Sides || BevelTopVertices.Num() != Sides)
        {
            return false;
        }
    }
    
    if (PyramidBaseVertices.Num() != Sides)
    {
        return false;
    }
    
    // 验证UV数据
    if (BaseUVs.Num() != Sides || SideUVs.Num() != Sides)
    {
        return false;
    }
    
    // 验证三角函数值
    if (AngleValues.Num() != Sides || CosValues.Num() != Sides || SinValues.Num() != Sides)
    {
        return false;
    }
    
    return bIsValid;
}

FVector2D FPyramidBuilder::GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const
{
    // 第二UV通道：使用传统UV系统（0-1范围）
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    // 根据法线方向选择UV映射策略
    if (FMath::Abs(Normal.Z) > 0.9f)
    {
        // 水平面（底面或顶面）
        if (Z < 0.1f)
        {
            // 底面：使用传统UV系统
            float Radius = FMath::Sqrt(X * X + Y * Y);
            float Angle = FMath::Atan2(Y, X);
            if (Angle < 0) Angle += 2.0f * PI;
            
            float U = 0.5f + 0.5f * FMath::Cos(Angle);
            float V = 0.5f + 0.5f * FMath::Sin(Angle);
            return FVector2D(U, V);
        }
        else
        {
            // 顶面：使用传统UV系统
            float U = (X / BaseRadius + 1.0f) * 0.5f;
            float V = (Y / BaseRadius + 1.0f) * 0.5f;
            return FVector2D(U, V);
        }
    }
    else
    {
        // 侧面：使用传统UV系统
        float Angle = FMath::Atan2(Y, X);
        if (Angle < 0) Angle += 2.0f * PI;
        
        float U = Angle / (2.0f * PI);  // 0 到 1
        float V = (Z / Height);          // 0 到 1
        return FVector2D(U, V);
    }
}

int32 FPyramidBuilder::GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal)
{
    FVector2D MainUV = GenerateStableUVCustom(Pos, Normal);
    FVector2D SecondaryUV = GenerateSecondaryUV(Pos, Normal);
    
    return FModelGenMeshBuilder::GetOrAddVertexWithDualUV(Pos, Normal, MainUV, SecondaryUV);
}
