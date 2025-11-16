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
    // 底面也占据完整的 1x1 UV 区域
    FVector Normal(0, 0, -1);

    if (BottomVertices.Num() < 3)
    {
        return;
    }

    TArray<int32> VertexIndices;
    VertexIndices.Reserve(BottomVertices.Num());

    // 计算底面的边界框，用于映射到 1x1 UV 区域
    float MinX = BottomVertices[0].X;
    float MaxX = BottomVertices[0].X;
    float MinY = BottomVertices[0].Y;
    float MaxY = BottomVertices[0].Y;
    
    for (int32 i = 1; i < BottomVertices.Num(); ++i)
    {
        MinX = FMath::Min(MinX, BottomVertices[i].X);
        MaxX = FMath::Max(MaxX, BottomVertices[i].X);
        MinY = FMath::Min(MinY, BottomVertices[i].Y);
        MaxY = FMath::Max(MaxY, BottomVertices[i].Y);
    }
    
    const float RangeX = MaxX - MinX;
    const float RangeY = MaxY - MinY;
    
    // 将底面顶点映射到 1x1 UV 区域
    for (int32 i = 0; i < BottomVertices.Num(); ++i)
    {
        float U, V;
        if (RangeX > KINDA_SMALL_NUMBER)
        {
            U = (BottomVertices[i].X - MinX) / RangeX;
        }
        else
        {
            U = 0.5f; // 如果 X 范围太小，放在中心
        }
        
        if (RangeY > KINDA_SMALL_NUMBER)
        {
            V = (BottomVertices[i].Y - MinY) / RangeY;
        }
        else
        {
            V = 0.5f; // 如果 Y 范围太小，放在中心
        }
        
        FVector2D UV(U, V);
        int32 VertexIndex = GetOrAddVertex(BottomVertices[i], Normal, UV);
        VertexIndices.Add(VertexIndex);
    }

    // 生成三角形（扇形方式）
    for (int32 i = 1; i < BottomVertices.Num() - 1; ++i)
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

    // 计算倒角和侧面的高度比例，用于在 1x1 UV 区域中分配
    const float SideHeight = Height - BevelRadius;
    const float TotalHeight = Height;
    
    // 计算倒角在总高度中的比例（用于 UV V 值分配）
    const float BevelRatio = (TotalHeight > KINDA_SMALL_NUMBER) ? (BevelRadius / TotalHeight) : 0.0f;
    
    // 每个面占据完整的 1x1 UV 区域
    // 倒角部分：V 从 0 到 BevelRatio
    // 侧面部分：V 从 BevelRatio 到 1.0
    const float V_START = 0.0f;
    const float V_HEIGHT = BevelRatio;

    const bool bSmooth = Pyramid.bSmoothSides;

    // 预计算侧面的法线（用于倒角顶点的光滑）
    TArray<FVector> SideNormals;
    TMap<FVector, FVector> SmoothNormalsMap;
    if (bSmooth)
    {
        SideNormals.SetNum(Sides);
        for (int32 i = 0; i < Sides; ++i)
        {
            const int32 NextI = (i + 1) % Sides;
            FVector Edge1 = TopVertices[NextI] - TopVertices[i];
            FVector Edge2 = PyramidTopPoint - TopVertices[i];
            SideNormals[i] = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
        }

        // 为每个底边顶点计算平均法线（相邻面的法线平均值）
        for (int32 i = 0; i < Sides; ++i)
        {
            const int32 PrevI = (i - 1 + Sides) % Sides;
            
            // TopVertices 的光滑法线
            const FVector& TopPos = TopVertices[i];
            FVector TopSmoothNormal = (SideNormals[PrevI] + SideNormals[i]).GetSafeNormal();
            SmoothNormalsMap.Add(TopPos, TopSmoothNormal);
            
            // BottomVertices 的光滑法线（与 TopVertices 相同，因为都是侧面的底部）
            const FVector& BottomPos = BottomVertices[i];
            SmoothNormalsMap.Add(BottomPos, TopSmoothNormal);
        }
    }

    for (int32 i = 0; i < BottomVertices.Num(); ++i)
    {
        const int32 NextI = (i + 1) % BottomVertices.Num();
        const int32 PrevI = (i - 1 + BottomVertices.Num()) % BottomVertices.Num();

        // 计算倒角法线
        FVector BevelNormal_i = (BottomVertices[i] - FVector(0, 0, BottomVertices[i].Z)).GetSafeNormal();
        FVector BevelNormal_NextI = (BottomVertices[NextI] - FVector(0, 0, BottomVertices[NextI].Z)).GetSafeNormal();
        FVector BevelNormal_Top_i = (TopVertices[i] - FVector(0, 0, TopVertices[i].Z)).GetSafeNormal();
        FVector BevelNormal_Top_NextI = (TopVertices[NextI] - FVector(0, 0, TopVertices[NextI].Z)).GetSafeNormal();

        // 根据光滑选项调整法线
        FVector Normal_i = BevelNormal_i;
        FVector Normal_NextI = BevelNormal_NextI;
        FVector Normal_Top_i = BevelNormal_Top_i;
        FVector Normal_Top_NextI = BevelNormal_Top_NextI;

        if (bSmooth)
        {
            // 底部顶点：使用侧面的光滑法线（与侧面共享）
            Normal_i = SmoothNormalsMap.FindRef(BottomVertices[i]);
            Normal_NextI = SmoothNormalsMap.FindRef(BottomVertices[NextI]);
            
            // 顶部顶点：使用侧面的光滑法线（与侧面共享）
            Normal_Top_i = SmoothNormalsMap.FindRef(TopVertices[i]);
            Normal_Top_NextI = SmoothNormalsMap.FindRef(TopVertices[NextI]);
            
            // 如果找不到光滑法线，使用倒角法线
            if (Normal_i.IsNearlyZero())
            {
                Normal_i = BevelNormal_i;
            }
            if (Normal_NextI.IsNearlyZero())
            {
                Normal_NextI = BevelNormal_NextI;
            }
            if (Normal_Top_i.IsNearlyZero())
            {
                Normal_Top_i = BevelNormal_Top_i;
            }
            if (Normal_Top_NextI.IsNearlyZero())
            {
                Normal_Top_NextI = BevelNormal_Top_NextI;
            }
        }

        // 每个面占据完整的 1x1 UV 区域，U 从 0 到 1
        FVector2D UV_Bottom_i(0.0f, V_START);
        FVector2D UV_Bottom_NextI(1.0f, V_START);
        FVector2D UV_Top_i(0.0f, V_START + V_HEIGHT);
        FVector2D UV_Top_NextI(1.0f, V_START + V_HEIGHT);

        int32 V0 = GetOrAddVertex(BottomVertices[i], Normal_i, UV_Bottom_i);
        int32 V1 = GetOrAddVertex(BottomVertices[NextI], Normal_NextI, UV_Bottom_NextI);
        int32 V2 = GetOrAddVertex(TopVertices[NextI], Normal_Top_NextI, UV_Top_NextI);
        int32 V3 = GetOrAddVertex(TopVertices[i], Normal_Top_i, UV_Top_i);

        // 添加两个三角形组成四边形
        AddTriangle(V0, V3, V2);
        AddTriangle(V0, V2, V1);
    }
}

void FPyramidBuilder::GeneratePyramidSides()
{
    // 计算倒角和侧面的高度比例，用于在 1x1 UV 区域中分配
    const float SideHeight = Height - BevelRadius;
    const float TotalHeight = Height;
    
    // 计算倒角在总高度中的比例（用于 UV V 值分配）
    const float BevelRatio = (TotalHeight > KINDA_SMALL_NUMBER) ? (BevelRadius / TotalHeight) : 0.0f;
    
    // 每个面占据完整的 1x1 UV 区域
    // 倒角部分：V 从 0 到 BevelRatio
    // 侧面部分：V 从 BevelRatio 到 1.0
    const float V_START = BevelRatio;
    const float V_HEIGHT = 1.0f - BevelRatio;

    const bool bSmooth = Pyramid.bSmoothSides;

    // 预计算所有面的法线（用于光滑计算）
    TArray<FVector> SideNormals;
    SideNormals.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        const int32 NextI = (i + 1) % Sides;
        FVector Edge1 = TopVertices[NextI] - TopVertices[i];
        FVector Edge2 = PyramidTopPoint - TopVertices[i];
        SideNormals[i] = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
    }

    // 预计算每个顶点位置的平滑法线（光滑时）
    TMap<FVector, FVector> SmoothNormalsMap;
    if (bSmooth)
    {
        // 为每个底边顶点计算平均法线（相邻面的法线平均值）
        for (int32 i = 0; i < Sides; ++i)
        {
            const int32 PrevI = (i - 1 + Sides) % Sides;
            const FVector& Pos = TopVertices[i];
            
            // 计算相邻面的法线平均值（面 PrevI 和面 i）
            FVector SmoothNormal = (SideNormals[PrevI] + SideNormals[i]).GetSafeNormal();
            SmoothNormalsMap.Add(Pos, SmoothNormal);
        }
        
        // 为顶部顶点计算平均法线（所有面的法线平均值）
        FVector TopNormal = FVector::ZeroVector;
        for (int32 i = 0; i < Sides; ++i)
        {
            TopNormal += SideNormals[i];
        }
        SmoothNormalsMap.Add(PyramidTopPoint, TopNormal.GetSafeNormal());
    }

    for (int32 i = 0; i < Sides; ++i)
    {
        const int32 NextI = (i + 1) % Sides;

        FVector CurrentSideNormal = SideNormals[i];

        // 获取平滑法线（光滑时）
        FVector FinalTopNormal = bSmooth ? SmoothNormalsMap.FindRef(PyramidTopPoint) : CurrentSideNormal;
        FVector BaseNormal1 = bSmooth ? SmoothNormalsMap.FindRef(TopVertices[i]) : CurrentSideNormal;
        FVector BaseNormal2 = bSmooth ? SmoothNormalsMap.FindRef(TopVertices[NextI]) : CurrentSideNormal;

        // 如果平滑法线不存在，使用当前面的法线
        if (BaseNormal1.IsNearlyZero())
        {
            BaseNormal1 = CurrentSideNormal;
        }
        if (BaseNormal2.IsNearlyZero())
        {
            BaseNormal2 = CurrentSideNormal;
        }

        // 每个面占据完整的 1x1 UV 区域，U 从 0 到 1
        FVector2D UV_Top(0.5f, V_START + V_HEIGHT);
        FVector2D UV_Base1(0.0f, V_START);
        FVector2D UV_Base2(1.0f, V_START);

        int32 TopVertex = GetOrAddVertex(PyramidTopPoint, FinalTopNormal, UV_Top);
        int32 V1 = GetOrAddVertex(TopVertices[i], BaseNormal1, UV_Base1);
        int32 V2 = GetOrAddVertex(TopVertices[NextI], BaseNormal2, UV_Base2);

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
    InitializeVertices();
}

void FPyramidBuilder::InitializeVertices()
{
    // 底部顶点环（Z=0）
    BottomVertices.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        const float X = BevelTopRadius * CosValues[i];
        const float Y = BevelTopRadius * SinValues[i];
        BottomVertices[i] = FVector(X, Y, 0.0f);
    }

    // 顶部顶点环（如果有倒角则在Z=BevelRadius，否则等于底部）
    if (BevelRadius > 0.0f)
    {
        TopVertices.SetNum(Sides);
        for (int32 i = 0; i < Sides; ++i)
        {
            const float X = BevelTopRadius * CosValues[i];
            const float Y = BevelTopRadius * SinValues[i];
            TopVertices[i] = FVector(X, Y, BevelRadius);
        }
    }
    else
    {
        TopVertices = BottomVertices;
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

