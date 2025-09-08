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
    GeneratePolygonFaceOptimized(BaseVertices, FVector(0, 0, -1));
}

void FPyramidBuilder::GenerateBevelSection()
{
    if (BevelRadius <= 0.0f)
    {
        return;
    }

    GenerateSideStripOptimized(BevelBottomVertices, BevelTopVertices, BaseUVs, BevelUVs);
}

void FPyramidBuilder::GeneratePyramidSides()
{
    for (int32 i = 0; i < Sides; ++i)
    {
        const int32 NextI = (i + 1) % Sides;

        FVector Edge1 = PyramidBaseVertices[NextI] - PyramidBaseVertices[i];
        FVector Edge2 = PyramidTopPoint - PyramidBaseVertices[i];
        FVector SideNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        int32 TopVertex = GetOrAddVertexWithDualUV(PyramidTopPoint, SideNormal);
        int32 V1 = GetOrAddVertexWithDualUV(PyramidBaseVertices[i], SideNormal);
        int32 V2 = GetOrAddVertexWithDualUV(PyramidBaseVertices[NextI], SideNormal);

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

FVector2D FPyramidBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;

    if (FMath::Abs(Normal.Z) > 0.9f)
    {
        if (Z < 0.1f)
        {
            float U = 0.5f + X / (BevelTopRadius * 2.0f);
            float V = 0.5f + Y / (BevelTopRadius * 2.0f);
            return FVector2D(U, V);
        }
        else
        {
            float U = (X / BevelTopRadius + 1.0f) * 0.5f;
            float V = (Y / BevelTopRadius + 1.0f) * 0.5f;
            return FVector2D(U, V);
        }
    }
    else
    {
        float Angle = FMath::Atan2(Y, X);
        if (Angle < 0) Angle += 2.0f * PI;

        float U = Angle / (2.0f * PI);
        float V = (Z / Height);
        return FVector2D(U, V);
    }
}

void FPyramidBuilder::PrecomputeUVs()
{
    PrecomputeUVScaleValues();

    BaseUVs.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        // 底部UVs
        BaseUVs[i] = FVector2D(0.5f + 0.5f * CosValues[i], 0.5f + 0.5f * SinValues[i]);
    }

    PyramidSideUVs.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        PyramidSideUVs[i] = FVector2D(static_cast<float>(i) / Sides, 1.0f);
    }

    if (BevelRadius > 0.0f)
    {
        BevelUVs.SetNum(Sides);
        for (int32 i = 0; i < Sides; ++i)
        {
            float U = static_cast<float>(i) / Sides;
            float V = 0.0f; // Bevels bottom
            BevelUVs[i] = FVector2D(U, V);
        }
    }
}

void FPyramidBuilder::GeneratePolygonFaceOptimized(const TArray<FVector>& Vertices, const FVector& Normal)
{
    if (Vertices.Num() < 3)
    {
        return;
    }

    TArray<int32> VertexIndices;
    VertexIndices.Reserve(Vertices.Num());

    // 为底部面片使用 GetOrAddVertexWithDualUV
    for (int32 i = 0; i < Vertices.Num(); ++i)
    {
        int32 VertexIndex = GetOrAddVertexWithDualUV(Vertices[i], Normal);
        VertexIndices.Add(VertexIndex);
    }

    for (int32 i = 1; i < Vertices.Num() - 1; ++i)
    {
        int32 V0 = VertexIndices[0];
        int32 V1 = VertexIndices[i];
        int32 V2 = VertexIndices[i + 1];

        AddTriangle(V0, V1, V2);
    }
}

void FPyramidBuilder::GenerateSideStripOptimized(const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, const TArray<FVector2D>& BottomUVs, const TArray<FVector2D>& TopUVs)
{
    if (BottomVerts.Num() != TopVerts.Num() || BottomUVs.Num() != BottomVerts.Num() || TopUVs.Num() != TopVerts.Num())
    {
        return;
    }

    for (int32 i = 0; i < BottomVerts.Num(); ++i)
    {
        const int32 NextI = (i + 1) % BottomVerts.Num();

        // 为每个顶点计算独立的法线
        FVector Normal_i = (BottomVerts[i] - FVector(0, 0, BottomVerts[i].Z)).GetSafeNormal();
        FVector Normal_NextI = (BottomVerts[NextI] - FVector(0, 0, BottomVerts[NextI].Z)).GetSafeNormal();

        // 使用新的函数，传入 UV
        int32 V0 = GetOrAddVertexWithUV(BottomVerts[i], Normal_i, BottomUVs[i]);
        int32 V1 = GetOrAddVertexWithUV(BottomVerts[NextI], Normal_NextI, BottomUVs[NextI]);
        int32 V2 = GetOrAddVertexWithUV(TopVerts[NextI], Normal_NextI, TopUVs[NextI]);
        int32 V3 = GetOrAddVertexWithUV(TopVerts[i], Normal_i, TopUVs[i]);

        AddTriangle(V0, V3, V2);
        AddTriangle(V0, V2, V1);
    }
}

int32 FPyramidBuilder::GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal)
{
    FVector2D MainUV = GenerateStableUVCustom(Pos, Normal);
    FVector2D SecondaryUV(0.0f, 0.0f); // 暂时不生成次UV

    return FModelGenMeshBuilder::GetOrAddVertexWithDualUV(Pos, Normal, MainUV, SecondaryUV);
}

int32 FPyramidBuilder::GetOrAddVertexWithUV(const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    FVector2D SecondaryUV(0.0f, 0.0f); // 暂时不生成次UV
    return FModelGenMeshBuilder::GetOrAddVertexWithDualUV(Pos, Normal, UV, SecondaryUV);
}