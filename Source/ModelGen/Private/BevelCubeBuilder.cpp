// Copyright (c) 2024. All rights reserved.

#include "BevelCubeBuilder.h"
#include "BevelCube.h"
#include "ModelGenMeshData.h"

FBevelCubeBuilder::FBevelCubeBuilder(const ABevelCube& InBevelCube)
    : BevelCube(InBevelCube)
{
    Clear();

    // 内联 PrecomputeConstants
    HalfSize = BevelCube.GetHalfSize();
    InnerOffset = BevelCube.GetInnerOffset();
    BevelRadius = BevelCube.BevelRadius;
    BevelSegments = BevelCube.BevelSegments;

    InitializeFaceDefinitions();
    InitializeEdgeBevelDefs();
    CalculateCorePoints();

    // 内联 PrecomputeAlphaValues
    const int32 ArraySize = BevelSegments + 1;
    AlphaValues.SetNum(ArraySize);
    for (int32 i = 0; i < ArraySize; ++i)
    {
        AlphaValues[i] = static_cast<float>(i) / BevelSegments;
    }

    // 内联 PrecomputeCornerGridSizes
    CornerGridSizes.SetNum(ArraySize);
    for (int32 Lat = 0; Lat < ArraySize; ++Lat)
    {
        const int32 LonCount = ArraySize - Lat;
        CornerGridSizes[Lat].SetNum(LonCount);
    }

    FaceUVSize = 0.25f;
    BevelUVWidth = (BevelRadius / HalfSize) * FaceUVSize;
    InnerUVSize = FaceUVSize - 2 * BevelUVWidth;

    // UV Offsets for a standard cross layout
    //      [Top]
    // [Left][Front][Right][Back]
    //      [Bottom]
    FaceUVOffsets.SetNum(6);
    FaceUVOffsets[0] = FVector2D(0.5f, 0.25f);   // Right (+X)
    FaceUVOffsets[1] = FVector2D(0.0f, 0.25f);   // Left (-X)
    FaceUVOffsets[2] = FVector2D(0.25f, 0.25f);  // Front (+Y)
    FaceUVOffsets[3] = FVector2D(0.75f, 0.25f);  // Back (-Y)
    FaceUVOffsets[4] = FVector2D(0.25f, 0.5f);   // Top (+Z)
    FaceUVOffsets[5] = FVector2D(0.25f, 0.0f);   // Bottom (-Z)
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

    // 计算正确的切线（用于法线贴图等）
    MeshData.CalculateTangents();

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

void FBevelCubeBuilder::InitializeFaceDefinitions()
{
    FaceDefinitions = {
        // 0 +X面（右面）
        { FVector(HalfSize, 0, 0), FVector(0, 0, -InnerOffset), FVector(0, InnerOffset, 0), FVector(1, 0, 0), TEXT("Right") },
        // 1 -X面（左面）
        { FVector(-HalfSize, 0, 0), FVector(0, 0, InnerOffset), FVector(0, InnerOffset, 0), FVector(-1, 0, 0), TEXT("Left") },
        // 2 +Y面（前面）
        { FVector(0, HalfSize, 0), FVector(-InnerOffset, 0, 0), FVector(0, 0, InnerOffset), FVector(0, 1, 0), TEXT("Front") },
        // 3 -Y面（后面）
        { FVector(0, -HalfSize, 0), FVector(InnerOffset, 0, 0), FVector(0, 0, InnerOffset), FVector(0, -1, 0), TEXT("Back") },
        // 4 +Z面（上面）
        { FVector(0, 0, HalfSize), FVector(InnerOffset, 0, 0), FVector(0, InnerOffset, 0), FVector(0, 0, 1), TEXT("Top") },
        // 5 -Z面（下面）
        { FVector(0, 0, -HalfSize), FVector(InnerOffset, 0, 0), FVector(0, -InnerOffset, 0), FVector(0, 0, -1), TEXT("Bottom") }
    };
}

void FBevelCubeBuilder::InitializeEdgeBevelDefs()
{
    EdgeBevelDefs = {
        { 0, 1, FVector(0,-1,0), FVector(0,0,-1), TEXT("Edge+X1") }, // 0: Core(-i,-i,-i) to (+i,-i,-i) -> Connects Back and Bottom
        { 2, 3, FVector(0,0,-1), FVector(0,1,0), TEXT("Edge+X2") }, // 1: Core(-i,+i,-i) to (+i,+i,-i) -> Connects Bottom and Front
        { 4, 5, FVector(0,0,1), FVector(0,-1,0), TEXT("Edge+X3") }, // 2: Core(-i,-i,+i) to (+i,-i,+i) -> Connects Top and Back
        { 6, 7, FVector(0,1,0), FVector(0,0,1), TEXT("Edge+X4") }, // 3: Core(-i,+i,+i) to (+i,+i,+i) -> Connects Front and Top
        { 0, 2, FVector(0,0,-1), FVector(-1,0,0), TEXT("Edge+Y1") }, // 4: Core(-i,-i,-i) to (-i,+i,-i) -> Connects Bottom and Left
        { 1, 3, FVector(1,0,0), FVector(0,0,-1), TEXT("Edge+Y2") }, // 5: Core(+i,-i,-i) to (+i,+i,-i) -> Connects Right and Bottom
        { 4, 6, FVector(-1,0,0), FVector(0,0,1), TEXT("Edge+Y3") }, // 6: Core(-i,-i,+i) to (-i,+i,+i) -> Connects Left and Top
        { 5, 7, FVector(0,0,1), FVector(1,0,0), TEXT("Edge+Y4") }, // 7: Core(+i,-i,+i) to (+i,+i,+i) -> Connects Top and Right
        { 0, 4, FVector(-1,0,0), FVector(0,-1,0), TEXT("Edge+Z1") }, // 8: Core(-i,-i,-i) to (-i,-i,+i) -> Connects Left and Back
        { 1, 5, FVector(0,-1,0), FVector(1,0,0), TEXT("Edge+Z2") }, // 9: Core(+i,-i,-i) to (+i,-i,+i) -> Connects Back and Right
        { 2, 6, FVector(0,1,0), FVector(-1,0,0), TEXT("Edge+Z3") }, // 10: Core(-i,+i,-i) to (-i,+i,+i) -> Connects Front and Left
        { 3, 7, FVector(1,0,0), FVector(0,1,0), TEXT("Edge+Z4") }  // 11: Core(+i,+i,-i) to (+i,+i,+i) -> Connects Right and Front
    };
}

int32 FBevelCubeBuilder::GetFaceIndex(const FVector& Normal) const
{
    for (int32 Index = 0; Index < FaceDefinitions.Num(); ++Index)
    {
        if (FaceDefinitions[Index].Normal.Equals(Normal, 0.001f))
        {
            return Index;
        }
    }
    return -1;
}

void FBevelCubeBuilder::CalculateCorePoints()
{
    CorePoints.Reserve(8);
    CorePoints.Add(FVector(-InnerOffset, -InnerOffset, -InnerOffset)); // 0: -x -y -z
    CorePoints.Add(FVector(InnerOffset, -InnerOffset, -InnerOffset)); // 1: +x -y -z
    CorePoints.Add(FVector(-InnerOffset, InnerOffset, -InnerOffset)); // 2: -x +y -z
    CorePoints.Add(FVector(InnerOffset, InnerOffset, -InnerOffset)); // 3: +x +y -z
    CorePoints.Add(FVector(-InnerOffset, -InnerOffset, InnerOffset)); // 4: -x -y +z
    CorePoints.Add(FVector(InnerOffset, -InnerOffset, InnerOffset)); // 5: +x -y +z
    CorePoints.Add(FVector(-InnerOffset, InnerOffset, InnerOffset)); // 6: -x +y +z
    CorePoints.Add(FVector(InnerOffset, InnerOffset, InnerOffset)); // 7: +x +y +z
}

void FBevelCubeBuilder::GenerateMainFaces()
{
    for (int32 FaceIndex = 0; FaceIndex < FaceDefinitions.Num(); ++FaceIndex)
    {
        const FFaceData& Face = FaceDefinitions[FaceIndex];
        TArray<FVector> FaceVerts = GenerateRectangleVertices(Face.Center, Face.SizeX, Face.SizeY);

        FVector2D UVOffset = FaceUVOffsets[FaceIndex] + FVector2D(BevelUVWidth, BevelUVWidth);

        TArray<FVector2D> FaceUVs;
        FaceUVs.Add(UVOffset + FVector2D(0, 0));
        FaceUVs.Add(UVOffset + FVector2D(0, InnerUVSize));
        FaceUVs.Add(UVOffset + FVector2D(InnerUVSize, InnerUVSize));
        FaceUVs.Add(UVOffset + FVector2D(InnerUVSize, 0));

        GenerateQuadSides(FaceVerts, Face.Normal, FaceUVs);
    }
}

void FBevelCubeBuilder::GenerateEdgeStrip(int32 Core1Idx, int32 Core2Idx,
    const FVector& Normal1, const FVector& Normal2,
    const FVector2D& UVOffset, const FVector2D& UVScale, bool ArcAlongU, bool reverse)
{
    TArray<int32> PrevStripStartIndices;
    TArray<int32> PrevStripEndIndices;

    for (int32 s = 0; s <= BevelSegments; ++s)
    {
        float Alpha = GetAlphaValue(s);
        if (reverse) Alpha = 1.0f - Alpha;
        FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();

        FVector PosStart = CorePoints[Core1Idx] + CurrentNormal * BevelRadius;
        FVector PosEnd = CorePoints[Core2Idx] + CurrentNormal * BevelRadius;

        float U_Alpha = FMath::Sin(Alpha * PI / 2.0f);

        FVector2D UV_Start;
        FVector2D UV_End;
        if (ArcAlongU)
        {
            UV_Start = UVOffset + FVector2D(U_Alpha * UVScale.X, 0.0f);
            UV_End = UVOffset + FVector2D(U_Alpha * UVScale.X, UVScale.Y);
        }
        else
        {
            UV_Start = UVOffset + FVector2D(0.0f, U_Alpha * UVScale.Y);
            UV_End = UVOffset + FVector2D(UVScale.X, U_Alpha * UVScale.Y);
        }

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

void FBevelCubeBuilder::GenerateEdgeBevels()
{
    // REFACTORED: Generate each edge bevel twice, once for each adjacent face,
    // with correctly calculated UVs to place them in the margins of the face's UV block.
    for (int32 EdgeIndex = 0; EdgeIndex < EdgeBevelDefs.Num(); ++EdgeIndex)
    {
        const FEdgeBevelDef& EdgeDef = EdgeBevelDefs[EdgeIndex];

        FVector2D UVOffset1, UVScale1, UVOffset2, UVScale2;
        bool ArcAlongU1 = false, Reverse1 = false, Swap1 = false;
        bool ArcAlongU2 = false, Reverse2 = false, Swap2 = false;

        switch (EdgeIndex)
        {
        case 0: // Edge+X1: back (3), bottom (5)
            // Back face's bottom edge
            UVOffset1 = FaceUVOffsets[3] + FVector2D(BevelUVWidth, 0);
            UVScale1 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse1 = false;
            // Bottom face's back edge
            UVOffset2 = FaceUVOffsets[5] + FVector2D(BevelUVWidth, 0);
            UVScale2 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse2 = false;
            break;

        case 1: // Edge+X2: bottom (5), front (2)
            // Bottom face's front edge
            UVOffset1 = FaceUVOffsets[5] + FVector2D(BevelUVWidth, BevelUVWidth + InnerUVSize);
            UVScale1 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse1 = false;
            // Front face's bottom edge
            UVOffset2 = FaceUVOffsets[2] + FVector2D(BevelUVWidth, 0);
            UVScale2 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse2 = false;
            break;

        case 2: // Edge+X3: top (4), back (3)
            // Top face's back edge
            UVOffset1 = FaceUVOffsets[4] + FVector2D(BevelUVWidth, 0);
            UVScale1 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse1 = false;
            // Back face's top edge
            UVOffset2 = FaceUVOffsets[3] + FVector2D(BevelUVWidth, BevelUVWidth + InnerUVSize);
            UVScale2 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse2 = false;
            break;

        case 3: // Edge+X4: front (2), top (4)
            // Front face's top edge
            UVOffset1 = FaceUVOffsets[2] + FVector2D(BevelUVWidth, BevelUVWidth + InnerUVSize);
            UVScale1 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse1 = false;
            // Top face's front edge
            UVOffset2 = FaceUVOffsets[4] + FVector2D(BevelUVWidth, BevelUVWidth + InnerUVSize);
            UVScale2 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse2 = false;
            break;

        case 4: // Edge+Y1: bottom (5), left (1)
            // Bottom face's left edge
            UVOffset1 = FaceUVOffsets[5] + FVector2D(0, BevelUVWidth);
            UVScale1 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU1 = true; Reverse1 = false;
            // Left face's bottom edge
            UVOffset2 = FaceUVOffsets[1] + FVector2D(BevelUVWidth, 0);
            UVScale2 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse2 = false;
            break;

        case 5: // Edge+Y2: right (0), bottom (5)
            // Right face's bottom edge
            UVOffset1 = FaceUVOffsets[0] + FVector2D(BevelUVWidth, 0);
            UVScale1 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse1 = false;
            // Bottom face's right edge
            UVOffset2 = FaceUVOffsets[5] + FVector2D(BevelUVWidth + InnerUVSize, BevelUVWidth);
            UVScale2 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU2 = true; Reverse2 = false;
            break;

        case 6: // Edge+Y3: left (1), top (4)
            // Left face's top edge
            UVOffset1 = FaceUVOffsets[1] + FVector2D(BevelUVWidth, BevelUVWidth + InnerUVSize);
            UVScale1 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse1 = false;
            // Top face's left edge
            UVOffset2 = FaceUVOffsets[4] + FVector2D(0, BevelUVWidth);
            UVScale2 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU2 = true; Reverse2 = false;
            break;

        case 7: // Edge+Y4: top (4), right (0)
            // Top face's right edge
            UVOffset1 = FaceUVOffsets[4] + FVector2D(BevelUVWidth + InnerUVSize, BevelUVWidth);
            UVScale1 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU1 = true; Reverse1 = false;
            // Right face's top edge
            UVOffset2 = FaceUVOffsets[0] + FVector2D(BevelUVWidth, BevelUVWidth + InnerUVSize);
            UVScale2 = FVector2D(InnerUVSize, BevelUVWidth);
            Reverse2 = false;
            break;

        case 8: // Edge+Z1: left (1), back (3)
            // Left face's back edge
            UVOffset1 = FaceUVOffsets[1] + FVector2D(0, BevelUVWidth);
            UVScale1 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU1 = true; Reverse1 = false;
            // Back face's left edge
            UVOffset2 = FaceUVOffsets[3] + FVector2D(0, BevelUVWidth);
            UVScale2 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU2 = true; Reverse2 = false;
            break;

        case 9: // Edge+Z2: back (3), right (0)
            // Back face's right edge
            UVOffset1 = FaceUVOffsets[3] + FVector2D(BevelUVWidth + InnerUVSize, BevelUVWidth);
            UVScale1 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU1 = true; Reverse1 = false;
            // Right face's back edge
            UVOffset2 = FaceUVOffsets[0] + FVector2D(0, BevelUVWidth);
            UVScale2 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU2 = true; Reverse2 = false;
            break;

        case 10: // Edge+Z3: front (2), left (1)
            // Front face's left edge
            UVOffset1 = FaceUVOffsets[2] + FVector2D(0, BevelUVWidth);
            UVScale1 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU1 = true; Reverse1 = false;
            // Left face's front edge
            UVOffset2 = FaceUVOffsets[1] + FVector2D(BevelUVWidth + InnerUVSize, BevelUVWidth);
            UVScale2 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU2 = true; Reverse2 = false;
            break;

        case 11: // Edge+Z4: right (0), front (2)
            // Right face's front edge
            UVOffset1 = FaceUVOffsets[0] + FVector2D(BevelUVWidth + InnerUVSize, BevelUVWidth);
            UVScale1 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU1 = true; Reverse1 = false;
            // Front face's right edge
            UVOffset2 = FaceUVOffsets[2] + FVector2D(BevelUVWidth + InnerUVSize, BevelUVWidth);
            UVScale2 = FVector2D(BevelUVWidth, InnerUVSize);
            ArcAlongU2 = true; Reverse2 = false;
            break;
        }

        int32 Core1Idx = EdgeDef.Core1Idx, Core2Idx = EdgeDef.Core2Idx;
        FVector Normal1 = EdgeDef.Normal1, Normal2 = EdgeDef.Normal2;

        if (Swap1) { Swap(Core1Idx, Core2Idx); }
        GenerateEdgeStrip(Core1Idx, Core2Idx, Normal1, Normal2, UVOffset1, UVScale1, ArcAlongU1, Reverse1);

        Core1Idx = EdgeDef.Core1Idx; Core2Idx = EdgeDef.Core2Idx;
        if (Swap2) { Swap(Core1Idx, Core2Idx); }
        GenerateEdgeStrip(Core1Idx, Core2Idx, Normal2, Normal1, UVOffset2, UVScale2, ArcAlongU2, Reverse2);
    }
}


void FBevelCubeBuilder::GenerateCornerBevels()
{
    // REFACTORED: Generate each corner fan three times, once for each adjacent face,
    // with correctly calculated UVs to place them in the corners of the face's UV block.
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        FVector2D UVOffsetX, UVOffsetY, UVOffsetZ;
        bool FlipUX = false, FlipUY = false, FlipUZ = false;
        bool FlipVX = false, FlipVY = false, FlipVZ = false;
        bool bSpecialOrder = IsSpecialCorner(CornerIndex);

        float B = BevelUVWidth;
        float I = InnerUVSize;

        switch (CornerIndex)
        {
        case 0: // -x -y -z: left (1), back (3), bottom (5)
            UVOffsetX = FaceUVOffsets[1]; // Left face, bottom-back corner -> UV bottom-left
            UVOffsetY = FaceUVOffsets[3]; // Back face, bottom-left corner -> UV bottom-left
            UVOffsetZ = FaceUVOffsets[5]; // Bottom face, back-left corner -> UV bottom-left
            break;

        case 1: // +x -y -z: right (0), back (3), bottom (5)
            UVOffsetX = FaceUVOffsets[0] + FVector2D(B + I, 0); // Right face, bottom-back corner -> UV bottom-right
            UVOffsetY = FaceUVOffsets[3] + FVector2D(B + I, 0); // Back face, bottom-right corner -> UV bottom-right
            UVOffsetZ = FaceUVOffsets[5] + FVector2D(B + I, 0); // Bottom face, back-right corner -> UV bottom-right
            FlipUX = FlipUY = FlipUZ = true;
            break;

        case 2: // -x +y -z: left (1), front (2), bottom (5)
            UVOffsetX = FaceUVOffsets[1] + FVector2D(0, B + I); // Left face, bottom-front corner -> UV top-left
            UVOffsetY = FaceUVOffsets[2];                       // Front face, bottom-left corner -> UV bottom-left
            UVOffsetZ = FaceUVOffsets[5] + FVector2D(0, B + I);   // Bottom face, front-left corner -> UV top-left
            FlipVX = FlipVZ = true;
            break;

        case 3: // +x +y -z: right (0), front (2), bottom (5)
            UVOffsetX = FaceUVOffsets[0] + FVector2D(B + I, B + I); // Right face, bottom-front corner -> UV top-right
            UVOffsetY = FaceUVOffsets[2] + FVector2D(B + I, 0);   // Front face, bottom-right corner -> UV bottom-right
            UVOffsetZ = FaceUVOffsets[5] + FVector2D(B + I, B + I); // Bottom face, front-right corner -> UV top-right
            FlipUX = FlipUY = FlipUZ = true;
            FlipVX = FlipVZ = true;
            break;

        case 4: // -x -y +z: left (1), back (3), top (4)
            UVOffsetX = FaceUVOffsets[1] + FVector2D(B + I, 0);   // Left face, top-back corner -> UV bottom-right
            UVOffsetY = FaceUVOffsets[3] + FVector2D(0, B + I);   // Back face, top-left corner -> UV top-left
            UVOffsetZ = FaceUVOffsets[4];                       // Top face, back-left corner -> UV bottom-left
            FlipUX = true;
            FlipVY = true;
            break;

        case 5: // +x -y +z: right (0), back (3), top (4)
            UVOffsetX = FaceUVOffsets[0];                       // Right face, top-back corner -> UV bottom-left
            UVOffsetY = FaceUVOffsets[3] + FVector2D(B + I, B + I); // Back face, top-right corner -> UV top-right
            UVOffsetZ = FaceUVOffsets[4] + FVector2D(B + I, 0);   // Top face, back-right corner -> UV bottom-right
            FlipUY = FlipUZ = true;
            FlipVY = true;
            break;

        case 6: // -x +y +z: left (1), front (2), top (4)
            UVOffsetX = FaceUVOffsets[1] + FVector2D(B + I, B + I); // Left face, top-front corner -> UV top-right
            UVOffsetY = FaceUVOffsets[2] + FVector2D(0, B + I);   // Front face, top-left corner -> UV top-left
            UVOffsetZ = FaceUVOffsets[4] + FVector2D(0, B + I);   // Top face, front-left corner -> UV top-left
            FlipUX = true;
            FlipVX = FlipVY = FlipVZ = true;
            break;

        case 7: // +x +y +z: right (0), front (2), top (4)
            UVOffsetX = FaceUVOffsets[0] + FVector2D(0, B + I);   // Right face, top-front corner -> UV top-left
            UVOffsetY = FaceUVOffsets[2] + FVector2D(B + I, B + I); // Front face, top-right corner -> UV top-right
            UVOffsetZ = FaceUVOffsets[4] + FVector2D(B + I, B + I); // Top face, front-right corner -> UV top-right
            FlipUY = FlipUZ = true;
            FlipVX = FlipVY = FlipVZ = true;
            break;
        }

        FVector2D UVScale = FVector2D(BevelUVWidth, BevelUVWidth);
        GenerateCornerBevelWithFlip(CornerIndex, UVOffsetX, UVScale, FlipUX, FlipVX, bSpecialOrder);
        GenerateCornerBevelWithFlip(CornerIndex, UVOffsetY, UVScale, FlipUY, FlipVY, bSpecialOrder);
        GenerateCornerBevelWithFlip(CornerIndex, UVOffsetZ, UVScale, FlipUZ, FlipVZ, bSpecialOrder);
    }
}


void FBevelCubeBuilder::GenerateCornerBevelWithFlip(int32 CornerIndex, const FVector2D& UVOffset, const FVector2D& UVScale, bool FlipU, bool FlipV, bool bSpecialOrder)
{
    const FVector& CurrentCorePoint = CorePoints[CornerIndex];

    const FVector AxisX = FVector(FMath::Sign(CurrentCorePoint.X), 0.0f, 0.0f);
    const FVector AxisY = FVector(0.0f, FMath::Sign(CurrentCorePoint.Y), 0.0f);
    const FVector AxisZ = FVector(0.0f, 0.0f, FMath::Sign(CurrentCorePoint.Z));

    TArray<TArray<int32>> CornerVerticesGrid = CornerGridSizes;

    GenerateCornerVerticesGrid(CornerIndex, CurrentCorePoint, AxisX, AxisY, AxisZ, CornerVerticesGrid, UVOffset, UVScale, FlipU, FlipV);

    GenerateCornerTrianglesGrid(CornerVerticesGrid, bSpecialOrder);
}

void FBevelCubeBuilder::GenerateCornerVerticesGrid(int32 CornerIndex, const FVector& CorePoint,
    const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ,
    TArray<TArray<int32>>& CornerVerticesGrid,
    const FVector2D& UVOffset, const FVector2D& UVScale, bool FlipU, bool FlipV)
{
    for (int32 Lat = 0; Lat < CornerVerticesGrid.Num(); ++Lat)
    {
        if (!IsValidCornerGridIndex(Lat, 0)) continue;

        float LatAlpha = GetAlphaValue(Lat);
        for (int32 Lon = 0; Lon < CornerVerticesGrid[Lat].Num(); ++Lon)
        {
            if (!IsValidCornerGridIndex(Lat, Lon)) continue;

            float LonAlpha = GetAlphaValue(Lon);

            float u = LonAlpha;
            float v = LatAlpha;

            if (FlipU) u = 1.0f - u;
            if (FlipV) v = 1.0f - v;

            FVector2D UV = UVOffset + FVector2D(u * UVScale.X, v * UVScale.Y);

            TArray<FVector> Vertices = GenerateCornerVertices(CorePoint, AxisX, AxisY, AxisZ, Lat, Lon);
            if (Vertices.Num() > 0)
            {
                FVector CurrentNormal = (Vertices[0] - CorePoint).GetSafeNormal();

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