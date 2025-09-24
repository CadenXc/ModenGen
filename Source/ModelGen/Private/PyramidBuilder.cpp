// Copyright (c) 2024. All rights reserved.

#include "PyramidBuilder.h"
#include "Pyramid.h"
#include "ModelGenMeshData.h"

FPyramidBuilder::FPyramidBuilder(const APyramid& InPyramid)
    : Pyramid(InPyramid)
{
    Clear();
    BaseRadius = Pyramid.BaseRadius;
    Height = Pyramid.Height;
    Sides = Pyramid.Sides;
    BevelRadius = Pyramid.BevelRadius;
    BevelTopRadius = Pyramid.GetBevelTopRadius();

    PrecomputeVertices();
    PrecomputeUVs();
}

bool FPyramidBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Pyramid.IsValid())
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

    // 计算正确的切线（用于法线贴图等）
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

void FPyramidBuilder::GenerateBaseFace()
{
    // 底面使用独立的UV区域，类似BevelCube的主面处理
    FVector Normal(0, 0, -1);

    if (BaseVertices.Num() < 3)
    {
        return;
    }

    TArray<int32> VertexIndices;
    VertexIndices.Reserve(BaseVertices.Num());

    // 计算底面UV区域，使正多边形边长和为1
    // 正n边形周长 = 2n × 半径 × sin(π/n)
    const float PolygonPerimeter = 2.0f * Sides * BevelTopRadius * FMath::Sin(PI / Sides);
    // 使周长归一化为1，计算UV半径
    const float UVRadius = 1.0f / (2.0f * Sides * FMath::Sin(PI / Sides));
    const FVector2D BaseCenterUV(0.5f, 0.5f - UVRadius); // 底面中心位置

    // 底面UV映射，使用动态半径
    for (int32 i = 0; i < BaseVertices.Num(); ++i)
    {
        // 将底面顶点映射到UV空间，使用动态半径
        float U = BaseCenterUV.X + (BaseVertices[i].X / BevelTopRadius) * UVRadius;
        float V = BaseCenterUV.Y + (BaseVertices[i].Y / BevelTopRadius) * UVRadius;
        FVector2D UV(U, V);

        int32 VertexIndex = GetOrAddVertex(BaseVertices[i], Normal, UV);
        VertexIndices.Add(VertexIndex);
    }

    // 生成三角形（扇形方式）
    for (int32 i = 1; i < BaseVertices.Num() - 1; ++i)
    {
        int32 V0 = VertexIndices[0];
        int32 V1 = VertexIndices[i];
        int32 V2 = VertexIndices[i + 1];

        AddTriangle(V0, V1, V2);
    }
}

void FPyramidBuilder::GenerateBevelSection()
{
    if (BevelRadius <= 0.0f)
    {
        return;
    }

    // 倒角部分UV映射，与侧面连续分布
    // 倒角V值根据倒角高度与底边边长的比例计算，确保UV映射比例正确
    const float U_WIDTH_PER_SIDE = 1.0f / Sides;
    
    // 计算底面周长：2π × BevelTopRadius
    const float BaseCircumference = 2.0f * PI * BevelTopRadius;
    
    // 计算倒角V值范围：倒角高度 / 底面周长
    const float BevelVHeight = BevelRadius / BaseCircumference;
    
    // 计算侧面V值范围：侧面高度 / 侧面周长
    const float SideHeight = Height - BevelRadius;
    const float SideCircumference = 2.0f * PI * BevelTopRadius;
    const float SideVHeight = SideHeight / SideCircumference;
    
    // 使用原始几何比例，不进行归一化
    const float V_START = 0.0f;  // 倒角从V=0开始
    const float V_HEIGHT = BevelVHeight;  // 使用原始倒角V高度

    for (int32 i = 0; i < BevelBottomVertices.Num(); ++i)
    {
        const int32 NextI = (i + 1) % BevelBottomVertices.Num();

        // 为每个顶点计算独立的法线
        FVector Normal_i = (BevelBottomVertices[i] - FVector(0, 0, BevelBottomVertices[i].Z)).GetSafeNormal();
        FVector Normal_NextI = (BevelBottomVertices[NextI] - FVector(0, 0, BevelBottomVertices[NextI].Z)).GetSafeNormal();

        // 计算UV坐标，与侧面共享UV空间，使用原始U值
        float U_START = static_cast<float>(i) * U_WIDTH_PER_SIDE;
        float U_END = U_START + U_WIDTH_PER_SIDE;

        FVector2D UV_Bottom_i(U_START, V_START);
        FVector2D UV_Bottom_NextI(U_END, V_START);
        FVector2D UV_Top_i(U_START, V_START + V_HEIGHT);
        FVector2D UV_Top_NextI(U_END, V_START + V_HEIGHT);

        // 创建顶点
        int32 V0 = GetOrAddVertex(BevelBottomVertices[i], Normal_i, UV_Bottom_i);
        int32 V1 = GetOrAddVertex(BevelBottomVertices[NextI], Normal_NextI, UV_Bottom_NextI);
        int32 V2 = GetOrAddVertex(BevelTopVertices[NextI], Normal_NextI, UV_Top_NextI);
        int32 V3 = GetOrAddVertex(BevelTopVertices[i], Normal_i, UV_Top_i);

        // 添加两个三角形组成四边形
        AddTriangle(V0, V3, V2);
        AddTriangle(V0, V2, V1);
    }
}

void FPyramidBuilder::GeneratePyramidSides()
{
    // 金字塔侧面UV映射，根据周长比例动态分布
    // 计算底面周长与高度的比例，用于UV坐标调整
    
    const float BaseCircumference = 2.0f * PI * BaseRadius;
    const float SideHeight = Height - BevelRadius;
    const float CircumferenceRatio = BaseCircumference / SideHeight;
    
    const float U_WIDTH_PER_SIDE = 1.0f / Sides;
    
    // 计算倒角V值范围
    const float BevelTopCircumference = 2.0f * PI * BevelTopRadius;
    const float BevelVHeight = BevelRadius / BevelTopCircumference;
    
    // 计算侧面周长：2π × BevelTopRadius
    const float SideCircumference = 2.0f * PI * BevelTopRadius;
    
    // 计算侧面V值范围：侧面高度 / 侧面周长
    const float SideVHeight = SideHeight / SideCircumference;
    
    // 使用原始几何比例，不进行归一化
    // 侧面在倒角上方，使用原始V值
    const float V_START = BevelVHeight;  // 侧面在倒角上方，从倒角结束位置开始
    const float V_HEIGHT = SideVHeight;  // 使用原始侧面V高度

    for (int32 i = 0; i < Sides; ++i)
    {
        const int32 NextI = (i + 1) % Sides;

        // 计算侧面法线
        FVector Edge1 = PyramidBaseVertices[NextI] - PyramidBaseVertices[i];
        FVector Edge2 = PyramidTopPoint - PyramidBaseVertices[i];
        FVector SideNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        // 计算UV坐标，根据周长比例调整，使用原始U值
        float U_START = static_cast<float>(i) * U_WIDTH_PER_SIDE;
        float U_END = U_START + U_WIDTH_PER_SIDE;

        // 顶点UV坐标 - 顶点在中心
        FVector2D UV_Top((U_START + U_END) * 0.5f, V_START + V_HEIGHT);
        // 底边顶点UV坐标，根据周长比例调整V坐标
        FVector2D UV_Base1(U_START, V_START);
        FVector2D UV_Base2(U_END, V_START);

        // 创建顶点
        int32 TopVertex = GetOrAddVertex(PyramidTopPoint, SideNormal, UV_Top);
        int32 V1 = GetOrAddVertex(PyramidBaseVertices[i], SideNormal, UV_Base1);
        int32 V2 = GetOrAddVertex(PyramidBaseVertices[NextI], SideNormal, UV_Base2);

        // 添加三角形
        AddTriangle(V2, V1, TopVertex);
    }
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

void FPyramidBuilder::PrecomputeUVs()
{
    PrecomputeUVScaleValues();

    // 底部UV - 使用动态半径计算，参考PolygonTorus端面做法
    BaseUVs.SetNum(Sides);
    
    // 计算底面UV区域，使正多边形边长和为1
    // 使周长归一化为1，计算UV半径
    const float UVRadius = 1.0f / (2.0f * Sides * FMath::Sin(PI / Sides));
    const FVector2D BaseCenterUV(0.5f, 0.5f - UVRadius); // 底面中心位置
    
    for (int32 i = 0; i < Sides; ++i)
    {
        float U = BaseCenterUV.X + CosValues[i] * UVRadius;
        float V = BaseCenterUV.Y + SinValues[i] * UVRadius;
        BaseUVs[i] = FVector2D(U, V);
    }

    // 金字塔侧面UV - 使用归一化的V值
    PyramidSideUVs.SetNum(Sides);
    
    // 计算侧面高度和侧面周长
    const float SideHeight = Height - BevelRadius;
    const float SideCircumference = 2.0f * PI * BevelTopRadius;
    const float SideVHeight = SideHeight / SideCircumference;
    
    // 计算底面周长用于倒角V值计算
    const float BaseCircumference = 2.0f * PI * BevelTopRadius;
    
    // 使用原始几何比例，不进行归一化
    for (int32 i = 0; i < Sides; ++i)
    {
        float U = static_cast<float>(i) / Sides;
        float V = SideVHeight;
        PyramidSideUVs[i] = FVector2D(U, V);
    }

    // 倒角UV - 使用倒角高度与底边边长的比例计算V值
    if (BevelRadius > 0.0f)
    {
        BevelUVs.SetNum(Sides);
        
        // 计算底面周长和倒角V值范围
        const float BaseCircumferenceLocal = 2.0f * PI * BevelTopRadius;
        const float BevelVHeightLocal = BevelRadius / BaseCircumferenceLocal;
        
        // 使用原始几何比例，不进行归一化
        const float V_START = 0.0f;  // 倒角从V=0开始
        
        for (int32 i = 0; i < Sides; ++i)
        {
            float U = static_cast<float>(i) / Sides;
            float V = V_START; // 倒角底部V值，使用原始比例
            BevelUVs[i] = FVector2D(U, V);
        }
    }
}

