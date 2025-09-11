// Copyright (c) 2024. All rights reserved.

#include "BevelCubeBuilder.h"
#include "BevelCube.h"
#include "ModelGenMeshData.h"

FBevelCubeBuilder::FBevelCubeBuilder(const ABevelCube& InBevelCube)
    : BevelCube(InBevelCube)
{
    Clear();
    PrecomputeConstants();
    InitializeFaceDefinitions();
    InitializeEdgeBevelDefs();
    CalculateCorePoints();
    PrecomputeAlphaValues();
    PrecomputeCornerGridSizes();
}

bool FBevelCubeBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!BevelCube.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    GenerateMainFaces();
    GenerateEdgeBevels();
    GenerateCornerBevels();

    if (!ValidateGeneratedData())
    {
        return false;
    }

    OutMeshData = MeshData;
    return true;
}

int32 FBevelCubeBuilder::CalculateVertexCountEstimate() const
{
    return BevelCube.GetVertexCount();
}

int32 FBevelCubeBuilder::CalculateTriangleCountEstimate() const
{
    return BevelCube.GetTriangleCount();
}

void FBevelCubeBuilder::PrecomputeConstants()
{
    HalfSize = BevelCube.GetHalfSize();
    InnerOffset = BevelCube.GetInnerOffset();
    BevelRadius = BevelCube.BevelRadius;
    BevelSegments = BevelCube.BevelSegments;
}

void FBevelCubeBuilder::PrecomputeAlphaValues()
{
    const int32 ArraySize = BevelSegments + 1;
    AlphaValues.SetNum(ArraySize);

    for (int32 i = 0; i < ArraySize; ++i)
    {
        AlphaValues[i] = static_cast<float>(i) / BevelSegments;
    }
}

void FBevelCubeBuilder::PrecomputeCornerGridSizes()
{
    const int32 ArraySize = BevelSegments + 1;
    CornerGridSizes.SetNum(ArraySize);

    for (int32 Lat = 0; Lat < ArraySize; ++Lat)
    {
        const int32 LonCount = ArraySize - Lat;
        CornerGridSizes[Lat].SetNum(LonCount);
    }
}

void FBevelCubeBuilder::InitializeFaceDefinitions()
{
    FaceDefinitions = {
        // +X面（右面）
        {
            FVector(HalfSize, 0, 0),
            FVector(0, 0, -InnerOffset),
            FVector(0, InnerOffset, 0),
            FVector(1, 0, 0),
            TEXT("Right")
        },
        // -X面（左面）
        {
            FVector(-HalfSize, 0, 0),
            FVector(0, 0, InnerOffset),
            FVector(0, InnerOffset, 0),
            FVector(-1, 0, 0),
            TEXT("Left")
        },
        // +Y面（前面）
        {
            FVector(0, HalfSize, 0),
            FVector(-InnerOffset, 0, 0),
            FVector(0, 0, InnerOffset),
            FVector(0, 1, 0),
            TEXT("Front")
        },
        // -Y面（后面）
        {
            FVector(0, -HalfSize, 0),
            FVector(InnerOffset, 0, 0),
            FVector(0, 0, InnerOffset),
            FVector(0, -1, 0),
            TEXT("Back")
        },
        // +Z面（上面）
        {
            FVector(0, 0, HalfSize),
            FVector(InnerOffset, 0, 0),
            FVector(0, InnerOffset, 0),
            FVector(0, 0, 1),
            TEXT("Top")
        },
        // -Z面（下面）
        {
            FVector(0, 0, -HalfSize),
            FVector(InnerOffset, 0, 0),
            FVector(0, -InnerOffset, 0),
            FVector(0, 0, -1),
            TEXT("Bottom")
        }
    };
}

void FBevelCubeBuilder::InitializeEdgeBevelDefs()
{
    EdgeBevelDefs = {
        // +X 方向的边缘
        { 0, 1, FVector(0,-1,0), FVector(0,0,-1), TEXT("Edge+X1") },
        { 2, 3, FVector(0,0,-1), FVector(0,1,0), TEXT("Edge+X2") },
        { 4, 5, FVector(0,0,1), FVector(0,-1,0), TEXT("Edge+X3") },
        { 6, 7, FVector(0,1,0), FVector(0,0,1), TEXT("Edge+X4") },

        // +Y 方向的边缘
        { 0, 2, FVector(0,0,-1), FVector(-1,0,0), TEXT("Edge+Y1") },
        { 1, 3, FVector(1,0,0), FVector(0,0,-1), TEXT("Edge+Y2") },
        { 4, 6, FVector(-1,0,0), FVector(0,0,1), TEXT("Edge+Y3") },
        { 5, 7, FVector(0,0,1), FVector(1,0,0), TEXT("Edge+Y4") },

        // +Z 方向的边缘
        { 0, 4, FVector(-1,0,0), FVector(0,-1,0), TEXT("Edge+Z1") },
        { 1, 5, FVector(0,-1,0), FVector(1,0,0), TEXT("Edge+Z2") },
        { 2, 6, FVector(0,1,0), FVector(-1,0,0), TEXT("Edge+Z3") },
        { 3, 7, FVector(1,0,0), FVector(0,1,0), TEXT("Edge+Z4") }
    };
}

// 计算核心点
void FBevelCubeBuilder::CalculateCorePoints()
{
    CorePoints.Reserve(8);

    // 8个角落的核心点：
    CorePoints.Add(FVector(-InnerOffset, -InnerOffset, -InnerOffset));
    CorePoints.Add(FVector(InnerOffset, -InnerOffset, -InnerOffset));
    CorePoints.Add(FVector(-InnerOffset, InnerOffset, -InnerOffset));
    CorePoints.Add(FVector(InnerOffset, InnerOffset, -InnerOffset));
    CorePoints.Add(FVector(-InnerOffset, -InnerOffset, InnerOffset));
    CorePoints.Add(FVector(InnerOffset, -InnerOffset, InnerOffset));
    CorePoints.Add(FVector(-InnerOffset, InnerOffset, InnerOffset));
    CorePoints.Add(FVector(InnerOffset, InnerOffset, InnerOffset));
}

void FBevelCubeBuilder::GenerateMainFaces()
{
    const float U_WIDTH = 1.0f / 4.0f;
    const float V_HEIGHT = 1.0f / 6.0f;

    for (int32 FaceIndex = 0; FaceIndex < FaceDefinitions.Num(); ++FaceIndex)
    {
        const FFaceData& Face = FaceDefinitions[FaceIndex];
        TArray<FVector> FaceVerts = GenerateRectangleVertices(Face.Center, Face.SizeX, Face.SizeY);

        const float U0 = 0.0f;
        const float V0 = static_cast<float>(FaceIndex) * V_HEIGHT;

        TArray<FVector2D> FaceUVs;
        FaceUVs.Add(FVector2D(U0, V0));
        FaceUVs.Add(FVector2D(U0, V0 + V_HEIGHT));
        FaceUVs.Add(FVector2D(U0 + U_WIDTH, V0 + V_HEIGHT));
        FaceUVs.Add(FVector2D(U0 + U_WIDTH, V0));

        GenerateQuadSides(FaceVerts, Face.Normal, FaceUVs);
    }
}

void FBevelCubeBuilder::GenerateEdgeBevels()
{
    const float U_WIDTH = 1.0f / 4.0f;
    const float V_HEIGHT = 1.0f / 12.0f;
    const float U_OFFSET = U_WIDTH;

    for (int32 EdgeIndex = 0; EdgeIndex < EdgeBevelDefs.Num(); ++EdgeIndex)
    {
        const FEdgeBevelDef& EdgeDef = EdgeBevelDefs[EdgeIndex];
        const float U0 = U_OFFSET;
        const float V0 = static_cast<float>(EdgeIndex) * V_HEIGHT;

        GenerateEdgeStrip(EdgeDef.Core1Idx, EdgeDef.Core2Idx, EdgeDef.Normal1, EdgeDef.Normal2,
            FVector2D(U0, V0), FVector2D(U_WIDTH, V_HEIGHT));
    }
}

void FBevelCubeBuilder::GenerateCornerBevels()
{
    if (CorePoints.Num() < 8)
    {
        return;
    }

    const float U_WIDTH = 1.0f / 4.0f;
    const float V_HEIGHT = 1.0f / 8.0f;
    const float U_OFFSET = 2.0f * U_WIDTH;

    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const float U0 = U_OFFSET;
        const float V0 = static_cast<float>(CornerIndex) * V_HEIGHT;
        GenerateCornerBevel(CornerIndex, FVector2D(U0, V0), FVector2D(U_WIDTH, V_HEIGHT));
    }
}

void FBevelCubeBuilder::GenerateCornerBevel(int32 CornerIndex, const FVector2D& UVOffset, const FVector2D& UVScale)
{
    const FVector& CurrentCorePoint = CorePoints[CornerIndex];
    bool bSpecialCornerRenderingOrder = IsSpecialCorner(CornerIndex);

    const FVector AxisX(FMath::Sign(CurrentCorePoint.X), 0.0f, 0.0f);
    const FVector AxisY(0.0f, FMath::Sign(CurrentCorePoint.Y), 0.0f);
    const FVector AxisZ(0.0f, 0.0f, FMath::Sign(CurrentCorePoint.Z));

    if (CornerGridSizes.Num() == 0)
    {
        return;
    }

    TArray<TArray<int32>> CornerVerticesGrid = CornerGridSizes;

    GenerateCornerVerticesGrid(CornerIndex, CurrentCorePoint, AxisX, AxisY, AxisZ, CornerVerticesGrid, UVOffset, UVScale);
    GenerateCornerTrianglesGrid(CornerVerticesGrid, bSpecialCornerRenderingOrder);
}

void FBevelCubeBuilder::GenerateEdgeStrip(int32 Core1Idx, int32 Core2Idx,
    const FVector& Normal1, const FVector& Normal2,
    const FVector2D& UVOffset, const FVector2D& UVScale)
{
    TArray<int32> PrevStripStartIndices;
    TArray<int32> PrevStripEndIndices;

    for (int32 s = 0; s <= BevelSegments; ++s)
    {
        const float Alpha = GetAlphaValue(s);
        FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();

        FVector PosStart = CorePoints[Core1Idx] + CurrentNormal * BevelRadius;
        FVector PosEnd = CorePoints[Core2Idx] + CurrentNormal * BevelRadius;

        const float U_Alpha = static_cast<float>(s) / BevelSegments;
        FVector2D UV_Start = UVOffset + FVector2D(U_Alpha * UVScale.X, 0.0f);
        FVector2D UV_End = UVOffset + FVector2D(U_Alpha * UVScale.X, 1.0f * UVScale.Y);

        int32 VtxStart = GetOrAddVertex(PosStart, CurrentNormal, UV_Start);
        int32 VtxEnd = GetOrAddVertex(PosEnd, CurrentNormal, UV_End);

        if (s > 0)
        {
            AddQuad(PrevStripStartIndices[0], PrevStripEndIndices[0], VtxEnd, VtxStart);
        }

        PrevStripStartIndices = { VtxStart };
        PrevStripEndIndices = { VtxEnd };
    }
}

void FBevelCubeBuilder::GenerateCornerTriangles(const TArray<TArray<int32>>& CornerVerticesGrid,
    int32 Lat, int32 Lon, bool bSpecialOrder)
{
    const int32 V00 = CornerVerticesGrid[Lat][Lon];
    const int32 V10 = CornerVerticesGrid[Lat + 1][Lon];
    const int32 V01 = CornerVerticesGrid[Lat][Lon + 1];

    if (bSpecialOrder)
    {
        AddTriangle(V00, V01, V10);
    }
    else
    {
        AddTriangle(V00, V10, V01);
    }

    if (Lon + 1 < CornerVerticesGrid[Lat + 1].Num())
    {
        const int32 V11 = CornerVerticesGrid[Lat + 1][Lon + 1];

        if (bSpecialOrder)
        {
            AddTriangle(V10, V01, V11);
        }
        else
        {
            AddTriangle(V10, V11, V01);
        }
    }
}

TArray<FVector> FBevelCubeBuilder::GenerateRectangleVertices(const FVector& Center, const FVector& SizeX, const FVector& SizeY) const
{
    TArray<FVector> Vertices;
    Vertices.Reserve(4);

    Vertices.Add(Center - SizeX - SizeY);
    Vertices.Add(Center - SizeX + SizeY);
    Vertices.Add(Center + SizeX + SizeY);
    Vertices.Add(Center + SizeX - SizeY);

    return Vertices;
}

void FBevelCubeBuilder::GenerateQuadSides(const TArray<FVector>& Verts, const FVector& Normal, const TArray<FVector2D>& UVs)
{
    if (Verts.Num() != 4 || UVs.Num() != 4)
    {
        return;
    }

    int32 V0 = GetOrAddVertex(Verts[0], Normal, UVs[0]);
    int32 V1 = GetOrAddVertex(Verts[1], Normal, UVs[1]);
    int32 V2 = GetOrAddVertex(Verts[2], Normal, UVs[2]);
    int32 V3 = GetOrAddVertex(Verts[3], Normal, UVs[3]);

    AddQuad(V0, V1, V2, V3);
}

TArray<FVector> FBevelCubeBuilder::GenerateEdgeVertices(const FVector& CorePoint1, const FVector& CorePoint2,
    const FVector& Normal1, const FVector& Normal2, float Alpha) const
{
    TArray<FVector> Vertices;
    Vertices.Reserve(2);

    FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();

    FVector PosStart = CorePoint1 + CurrentNormal * BevelRadius;
    FVector PosEnd = CorePoint2 + CurrentNormal * BevelRadius;

    Vertices.Add(PosStart);
    Vertices.Add(PosEnd);

    return Vertices;
}

TArray<FVector> FBevelCubeBuilder::GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX,
    const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const
{
    TArray<FVector> Vertices;
    Vertices.Reserve(1);

    const float LatAlpha = GetAlphaValue(Lat);
    const float LonAlpha = GetAlphaValue(Lon);

    FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
        AxisY * LatAlpha +
        AxisZ * LonAlpha);
    CurrentNormal.Normalize();

    FVector CurrentPos = CorePoint + CurrentNormal * BevelRadius;
    Vertices.Add(CurrentPos);

    return Vertices;
}

void FBevelCubeBuilder::GenerateCornerVerticesGrid(int32 CornerIndex, const FVector& CorePoint,
    const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ,
    TArray<TArray<int32>>& CornerVerticesGrid,
    const FVector2D& UVOffset, const FVector2D& UVScale)
{
    for (int32 Lat = 0; Lat < CornerVerticesGrid.Num(); ++Lat)
    {
        if (!IsValidCornerGridIndex(Lat, 0)) continue;

        for (int32 Lon = 0; Lon < CornerVerticesGrid[Lat].Num(); ++Lon)
        {
            if (!IsValidCornerGridIndex(Lat, Lon)) continue;

            const float LonAlpha = GetAlphaValue(Lon);
            const float LatAlpha = GetAlphaValue(Lat);

            TArray<FVector> Vertices = GenerateCornerVertices(CorePoint, AxisX, AxisY, AxisZ, Lat, Lon);
            if (Vertices.Num() > 0)
            {
                FVector CurrentNormal = (Vertices[0] - CorePoint).GetSafeNormal();

                FVector2D UV = UVOffset + FVector2D(LonAlpha * UVScale.X, LatAlpha * UVScale.Y);

                CornerVerticesGrid[Lat][Lon] = GetOrAddVertex(Vertices[0], CurrentNormal, UV);
            }
        }
    }
}

void FBevelCubeBuilder::GenerateCornerTrianglesGrid(const TArray<TArray<int32>>& CornerVerticesGrid, bool bSpecialOrder)
{
    for (int32 Lat = 0; Lat < CornerVerticesGrid.Num() - 1; ++Lat)
    {
        if (!IsValidCornerGridIndex(Lat, 0)) continue;

        for (int32 Lon = 0; Lon < CornerVerticesGrid[Lat].Num() - 1; ++Lon)
        {
            if (!IsValidCornerGridIndex(Lat, Lon)) continue;

            GenerateCornerTriangles(CornerVerticesGrid, Lat, Lon, bSpecialOrder);
        }
    }
}

bool FBevelCubeBuilder::IsSpecialCorner(int32 CornerIndex) const
{
    static const TArray<int32> SpecialCornerIndices = { 4, 7, 2, 1 };
    return SpecialCornerIndices.Contains(CornerIndex);
}

float FBevelCubeBuilder::GetAlphaValue(int32 Index) const
{
    if (IsValidAlphaIndex(Index))
    {
        return AlphaValues[Index];
    }

    return FMath::Clamp(static_cast<float>(Index) / BevelSegments, 0.0f, 1.0f);
}

bool FBevelCubeBuilder::IsValidAlphaIndex(int32 Index) const
{
    return Index >= 0 && Index < AlphaValues.Num();
}

bool FBevelCubeBuilder::IsValidCornerGridIndex(int32 Lat, int32 Lon) const
{
    if (Lat < 0 || Lat >= CornerGridSizes.Num()) return false;
    if (Lon < 0 || Lon >= CornerGridSizes[Lat].Num()) return false;
    return true;
}

int32 FBevelCubeBuilder::GetCornerGridSize(int32 Lat) const
{
    if (Lat >= 0 && Lat < CornerGridSizes.Num())
    {
        return CornerGridSizes[Lat].Num();
    }
    return 0;
}

bool FBevelCubeBuilder::ValidatePrecomputedData() const
{
    bool bIsValid = true;

    if (AlphaValues.Num() != BevelSegments + 1) bIsValid = false;
    if (CornerGridSizes.Num() != BevelSegments + 1) bIsValid = false;

    for (int32 Lat = 0; Lat < CornerGridSizes.Num(); ++Lat)
    {
        int32 ExpectedSize = BevelSegments + 1 - Lat;
        if (CornerGridSizes[Lat].Num() != ExpectedSize)
        {
            bIsValid = false;
        }
    }

    return bIsValid;
}

int32 FBevelCubeBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    return FModelGenMeshBuilder::GetOrAddVertex(Pos, Normal, UV);
}