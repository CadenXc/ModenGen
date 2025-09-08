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
    GeneratePolygonFaceOptimized(BaseVertices, FVector(0, 0, -1), BaseUVs);
}

void FPyramidBuilder::GenerateBevelSection()
{
    if (BevelRadius <= 0.0f)
    {
        return;
    }
    
    GenerateSideStripOptimized(BevelBottomVertices, BevelTopVertices, BevelUVs);
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
    return GenerateCircleVertices(TopRadius, Z, NumSides);
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
            float Radius = FMath::Sqrt(X * X + Y * Y);
            float Angle = FMath::Atan2(Y, X);
            if (Angle < 0) Angle += 2.0f * PI;
            
            float U = 0.5f + 0.5f * FMath::Cos(Angle);
            float V = 0.5f + 0.5f * FMath::Sin(Angle);
            return FVector2D(U, V);
        }
        else
        {
            float U = (X / BaseRadius + 1.0f) * 0.5f;
            float V = (Y / BaseRadius + 1.0f) * 0.5f;
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
    
    BaseUVs = GenerateCircularUVs(Sides, 0.0f);
    SideUVs = GenerateSideStripUVs(Sides, 0.0f, 1.0f);
    
    if (BevelRadius > 0.0f)
    {
        BevelUVs.SetNum(Sides);
        for (int32 i = 0; i < Sides; ++i)
        {
            float U = 1.0f + (static_cast<float>(i) / Sides);
            float V = 0.0f + (static_cast<float>(i) / Sides);
            BevelUVs[i] = FVector2D(U, V);
        }
    }
}

TArray<FVector2D> FPyramidBuilder::GenerateCircularUVs(int32 NumSides, float UVOffsetZ) const
{
    TArray<FVector2D> UVs;
    UVs.SetNum(NumSides);
    
    for (int32 i = 0; i < NumSides; ++i)
    {
        float U = 0.5f + 0.5f * FMath::Cos(AngleValues[i]);
        float V = 0.5f + 0.5f * FMath::Sin(AngleValues[i]);
        UVs[i] = FVector2D(U, V);
    }
    
    return UVs;
}

TArray<FVector2D> FPyramidBuilder::GenerateSideStripUVs(int32 NumSides, float UVOffsetY, float UVScaleY) const
{
    TArray<FVector2D> UVs;
    UVs.SetNum(NumSides);
    
    for (int32 i = 0; i < NumSides; ++i)
    {
        float U = 1.0f + (static_cast<float>(i) / NumSides);
        float V = UVOffsetY + UVScaleY;
        UVs[i] = FVector2D(U, V);
    }
    
    return UVs;
}

void FPyramidBuilder::GeneratePolygonFaceOptimized(const TArray<FVector>& Vertices, const FVector& Normal, const TArray<FVector2D>& UVs)
{
    if (Vertices.Num() < 3 || UVs.Num() != Vertices.Num())
    {
        return;
    }
    
    TArray<int32> VertexIndices;
    VertexIndices.Reserve(Vertices.Num());
    
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

void FPyramidBuilder::GenerateSideStripOptimized(const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, const TArray<FVector2D>& UVs)
{
    if (BottomVerts.Num() != TopVerts.Num() || UVs.Num() != BottomVerts.Num())
    {
        return;
    }
    
    for (int32 i = 0; i < BottomVerts.Num(); ++i)
    {
        const int32 NextI = (i + 1) % BottomVerts.Num();
        
        FVector Edge1 = BottomVerts[NextI] - BottomVerts[i];
        FVector Edge2 = TopVerts[i] - BottomVerts[i];
        FVector SideNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

        // 暂时使用
		SideNormal = -SideNormal;
        
        int32 V0 = GetOrAddVertexWithDualUV(BottomVerts[i], SideNormal);
        int32 V1 = GetOrAddVertexWithDualUV(BottomVerts[NextI], SideNormal);
        int32 V2 = GetOrAddVertexWithDualUV(TopVerts[NextI], SideNormal);
        int32 V3 = GetOrAddVertexWithDualUV(TopVerts[i], SideNormal);
        
		AddTriangle(V0, V1, V2);
		AddTriangle(V0, V2, V3);
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



bool FPyramidBuilder::ValidatePrecomputedData() const
{
    if (BaseRadius <= 0.0f || Height <= 0.0f || Sides < 3)
    {
        return false;
    }
    
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
    
    if (BaseUVs.Num() != Sides || SideUVs.Num() != Sides)
    {
        return false;
    }
    
    if (AngleValues.Num() != Sides || CosValues.Num() != Sides || SinValues.Num() != Sides)
    {
        return false;
    }
    
    return true;
}

FVector2D FPyramidBuilder::GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const
{
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    if (FMath::Abs(Normal.Z) > 0.9f)
    {
        if (Z < 0.1f)
        {
            float Radius = FMath::Sqrt(X * X + Y * Y);
            float Angle = FMath::Atan2(Y, X);
            if (Angle < 0) Angle += 2.0f * PI;
            
            float U = 0.5f + 0.5f * FMath::Cos(Angle);
            float V = 0.5f + 0.5f * FMath::Sin(Angle);
            return FVector2D(U, V);
        }
        else
        {
            float U = (X / BaseRadius + 1.0f) * 0.5f;
            float V = (Y / BaseRadius + 1.0f) * 0.5f;
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

int32 FPyramidBuilder::GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal)
{
    FVector2D MainUV = GenerateStableUVCustom(Pos, Normal);
    FVector2D SecondaryUV = GenerateSecondaryUV(Pos, Normal);
    
    return FModelGenMeshBuilder::GetOrAddVertexWithDualUV(Pos, Normal, MainUV, SecondaryUV);
}
