// Copyright (c) 2024. All rights reserved.

#include "BevelCubeBuilder.h"
#include "BevelCubeParameters.h"
#include "ModelGenMeshData.h"

FBevelCubeBuilder::FBevelCubeBuilder(const FBevelCubeParameters& InParams)
    : Params(InParams)
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
    if (!ValidateParameters())
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

bool FBevelCubeBuilder::ValidateParameters() const
{
    return Params.IsValid();
}

int32 FBevelCubeBuilder::CalculateVertexCountEstimate() const
{
    return Params.GetVertexCount();
}

int32 FBevelCubeBuilder::CalculateTriangleCountEstimate() const
{
    return Params.GetTriangleCount();
}

void FBevelCubeBuilder::PrecomputeConstants()
{
    HalfSize = Params.GetHalfSize();
    InnerOffset = Params.GetInnerOffset();
    BevelRadius = Params.BevelRadius;
    BevelSegments = Params.BevelSegments;
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
    // 为每个面分配唯一的UV区域，避免重叠
    for (int32 FaceIndex = 0; FaceIndex < FaceDefinitions.Num(); ++FaceIndex)
    {
        const FFaceData& Face = FaceDefinitions[FaceIndex];
        
        // 计算UV偏移，每个面占据UV空间的1/6
        // 使用稳定UV映射系统，基于顶点位置生成UV
        TArray<FVector> FaceVerts = GenerateRectangleVertices(Face.Center, Face.SizeX, Face.SizeY);
        
        GenerateQuadSides(FaceVerts, Face.Normal, TArray<FVector2D>());
    }
}

void FBevelCubeBuilder::GenerateEdgeBevels()
{
    // 为每条边生成倒角，分配唯一的UV区域
    for (int32 EdgeIndex = 0; EdgeIndex < EdgeBevelDefs.Num(); ++EdgeIndex)
    {
        const FEdgeBevelDef& EdgeDef = EdgeBevelDefs[EdgeIndex];
        
        // 使用稳定UV映射系统，基于顶点位置生成UV
        GenerateEdgeStrip(EdgeDef.Core1Idx, EdgeDef.Core2Idx, EdgeDef.Normal1, EdgeDef.Normal2, 0.0f, 0.0f);
    }
}

void FBevelCubeBuilder::GenerateCornerBevels()
{
    if (CorePoints.Num() < 8)
    {
        return;
    }

    // 为每个角落生成倒角，分配唯一的UV区域
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        // 使用稳定UV映射系统，基于顶点位置生成UV
        GenerateCornerBevel(CornerIndex, 0.0f, 0.0f);
    }
}

// 新增：提取角落倒角生成逻辑
void FBevelCubeBuilder::GenerateCornerBevel(int32 CornerIndex, float UVOffsetX, float UVWidth)
{
    const FVector& CurrentCorePoint = CorePoints[CornerIndex];
    bool bSpecialCornerRenderingOrder = IsSpecialCorner(CornerIndex);

    // 计算角落的轴向
    const FVector AxisX(FMath::Sign(CurrentCorePoint.X), 0.0f, 0.0f);
    const FVector AxisY(0.0f, FMath::Sign(CurrentCorePoint.Y), 0.0f);
    const FVector AxisZ(0.0f, 0.0f, FMath::Sign(CurrentCorePoint.Z));

    // 使用预计算的网格尺寸，并添加安全检查
    if (CornerGridSizes.Num() == 0)
    {
        return;
    }
    
    TArray<TArray<int32>> CornerVerticesGrid = CornerGridSizes;
    
    // 生成四分之一球体的顶点
    GenerateCornerVerticesGrid(CornerIndex, CurrentCorePoint, AxisX, AxisY, AxisZ, CornerVerticesGrid, UVOffsetX, UVWidth);

    // 生成四分之一球体的三角形
    GenerateCornerTrianglesGrid(CornerVerticesGrid, bSpecialCornerRenderingOrder);
}

void FBevelCubeBuilder::GenerateEdgeStrip(int32 Core1Idx, int32 Core2Idx, 
                                         const FVector& Normal1, const FVector& Normal2,
                                         float UVOffsetX, float UVWidth)
{
    TArray<int32> PrevStripStartIndices;
    TArray<int32> PrevStripEndIndices;

    // 使用预计算的Alpha值，避免重复计算
    for (int32 s = 0; s <= BevelSegments; ++s)
    {
        const float Alpha = GetAlphaValue(s);
        FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();

        FVector PosStart = CorePoints[Core1Idx] + CurrentNormal * BevelRadius;
        FVector PosEnd = CorePoints[Core2Idx] + CurrentNormal * BevelRadius;

        int32 VtxStart = GetOrAddVertexWithDualUV(PosStart, CurrentNormal);
        int32 VtxEnd = GetOrAddVertexWithDualUV(PosEnd, CurrentNormal);

        if (s > 0 && PrevStripStartIndices.Num() > 0 && PrevStripEndIndices.Num() > 0)
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
    
    // 生成矩形的四个顶点（按照右手法则：从法线方向看，顶点按逆时针排列）
    Vertices.Add(Center - SizeX - SizeY);
    Vertices.Add(Center - SizeX + SizeY);
    Vertices.Add(Center + SizeX + SizeY);
    Vertices.Add(Center + SizeX - SizeY);
    
    return Vertices;
}

void FBevelCubeBuilder::GenerateQuadSides(const TArray<FVector>& Verts, const FVector& Normal, const TArray<FVector2D>& UVs)
{
    if (Verts.Num() != 4)
    {
        return;
    }
    
    int32 V0 = GetOrAddVertexWithDualUV(Verts[0], Normal);
    int32 V1 = GetOrAddVertexWithDualUV(Verts[1], Normal);
    int32 V2 = GetOrAddVertexWithDualUV(Verts[2], Normal);
    int32 V3 = GetOrAddVertexWithDualUV(Verts[3], Normal);
    
    AddQuad(V0, V1, V2, V3);
}

TArray<FVector> FBevelCubeBuilder::GenerateEdgeVertices(const FVector& CorePoint1, const FVector& CorePoint2, 
                                                        const FVector& Normal1, const FVector& Normal2, float Alpha) const
{
    TArray<FVector> Vertices;
    Vertices.Reserve(2);
    
    // 计算插值法线
    FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();
    
    // 生成边缘的两个顶点
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

    // 计算法线（三轴插值）
    FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
        AxisY * LatAlpha +
        AxisZ * LonAlpha);
    CurrentNormal.Normalize();

    // 计算顶点位置
    FVector CurrentPos = CorePoint + CurrentNormal * BevelRadius;
    Vertices.Add(CurrentPos);
    
    return Vertices;
}

void FBevelCubeBuilder::GenerateCornerVerticesGrid(int32 CornerIndex, const FVector& CorePoint, 
                                                   const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ,
                                                   TArray<TArray<int32>>& CornerVerticesGrid,
                                                   float UVOffsetX, float UVWidth)
{
    // 生成四分之一球体的顶点
    for (int32 Lat = 0; Lat < CornerVerticesGrid.Num(); ++Lat)
    {
        // 添加边界检查
        if (!IsValidCornerGridIndex(Lat, 0))
        {
            continue;
        }
        
        for (int32 Lon = 0; Lon < CornerVerticesGrid[Lat].Num(); ++Lon)
        {
            // 添加边界检查
            if (!IsValidCornerGridIndex(Lat, Lon))
            {
                continue;
            }
            
            const float LonAlpha = GetAlphaValue(Lon);
            const float LatAlpha = GetAlphaValue(Lat);

            // 使用辅助函数生成顶点
            TArray<FVector> Vertices = GenerateCornerVertices(CorePoint, AxisX, AxisY, AxisZ, Lat, Lon);
            if (Vertices.Num() > 0)
            {
                FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
                    AxisY * LatAlpha +
                    AxisZ * LonAlpha);
                CurrentNormal.Normalize();

                CornerVerticesGrid[Lat][Lon] = GetOrAddVertexWithDualUV(Vertices[0], CurrentNormal);
            }
        }
    }
}

void FBevelCubeBuilder::GenerateCornerTrianglesGrid(const TArray<TArray<int32>>& CornerVerticesGrid, bool bSpecialOrder)
{
    // 生成四分之一球体的三角形
    for (int32 Lat = 0; Lat < CornerVerticesGrid.Num() - 1; ++Lat)
    {
        // 添加边界检查
        if (!IsValidCornerGridIndex(Lat, 0))
        {
            continue;
        }
        
        for (int32 Lon = 0; Lon < CornerVerticesGrid[Lat].Num() - 1; ++Lon)
        {
            // 添加边界检查
            if (!IsValidCornerGridIndex(Lat, Lon))
            {
                continue;
            }
            
            GenerateCornerTriangles(CornerVerticesGrid, Lat, Lon, bSpecialOrder);
        }
    }
}

bool FBevelCubeBuilder::IsSpecialCorner(int32 CornerIndex) const
{
    // 使用预定义的常量数组，避免硬编码
    static const TArray<int32> SpecialCornerIndices = { 4, 7, 2, 1 };
    return SpecialCornerIndices.Contains(CornerIndex);
}

float FBevelCubeBuilder::GetAlphaValue(int32 Index) const
{
    if (IsValidAlphaIndex(Index))
    {
        return AlphaValues[Index];
    }
    
    // 返回安全的默认值
    return FMath::Clamp(static_cast<float>(Index) / BevelSegments, 0.0f, 1.0f);
}

bool FBevelCubeBuilder::IsValidAlphaIndex(int32 Index) const
{
    return Index >= 0 && Index < AlphaValues.Num();
}

bool FBevelCubeBuilder::IsValidCornerGridIndex(int32 Lat, int32 Lon) const
{
    if (Lat < 0 || Lat >= CornerGridSizes.Num())
    {
        return false;
    }
    
    if (Lon < 0 || Lon >= CornerGridSizes[Lat].Num())
    {
        return false;
    }
    
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
    
    // 验证AlphaValues
    if (AlphaValues.Num() != BevelSegments + 1)
    {
        bIsValid = false;
    }
    
    // 验证CornerGridSizes
    if (CornerGridSizes.Num() != BevelSegments + 1)
    {
        bIsValid = false;
    }
    
    // 验证每个子数组的大小
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

// 基于顶点位置的稳定UV映射 - 重写父类实现
FVector2D FBevelCubeBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    // 基于顶点位置生成稳定的UV坐标，确保模型变化时UV不变
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    // 根据法线方向选择UV映射策略（真正的2U系统：0-1范围）
    if (FMath::Abs(Normal.X) > 0.9f)
    {
        // X面（左右面）
        if (Normal.X > 0)
        {
            // 右面：U从0.0到1.0，V从0.0到1.0
            float U = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Z / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
        else
        {
            // 左面：U从0.0到1.0，V从0.0到1.0
            float U = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Z / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
    }
    else if (FMath::Abs(Normal.Y) > 0.9f)
    {
        // Y面（前后面）
        if (Normal.Y > 0)
        {
            // 前面：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Z / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
        else
        {
            // 后面：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Z / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
    }
    else if (FMath::Abs(Normal.Z) > 0.9f)
    {
        // Z面（上下面）
        if (Normal.Z > 0)
        {
            // 上面：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
        else
        {
            // 下面：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
    }
    else
    {
        // 倒角面：使用位置投影到UV空间
        // 边缘倒角：U从0.0到1.0，V从0.0到1.0
        // 角落倒角：U从0.0到1.0，V从0.0到1.0
        
        // 根据位置判断是边缘还是角落倒角
        float MaxComponent = FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z));
        float MinComponent = FMath::Min3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z));
        
        if (MaxComponent - MinComponent < BevelRadius * 0.5f)
        {
            // 角落倒角：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
        else
        {
            // 边缘倒角：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
    }
}

FVector2D FBevelCubeBuilder::GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const
{
    // 第二UV通道：使用传统UV系统（0-1范围）
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    // 根据法线方向选择UV映射策略
    if (FMath::Abs(Normal.X) > 0.9f)
    {
        // X面（左右面）
        if (Normal.X > 0)
        {
            // 右面：U从0.0到1.0，V从0.0到1.0
            float U = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Z / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
        else
        {
            // 左面：U从0.0到1.0，V从0.0到1.0
            float U = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Z / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
    }
    else if (FMath::Abs(Normal.Y) > 0.9f)
    {
        // Y面（前后面）
        if (Normal.Y > 0)
        {
            // 前面：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Z / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
        else
        {
            // 后面：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Z / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
    }
    else if (FMath::Abs(Normal.Z) > 0.9f)
    {
        // Z面（上下面）
        if (Normal.Z > 0)
        {
            // 上面：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
        else
        {
            // 下面：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
    }
    else
    {
        // 倒角面：使用位置投影到UV空间
        // 边缘倒角：U从0.0到1.0，V从0.0到1.0
        // 角落倒角：U从0.0到1.0，V从0.0到1.0
        
        // 根据位置判断是边缘还是角落倒角
        float MaxComponent = FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z));
        float MinComponent = FMath::Min3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z));
        
        if (MaxComponent - MinComponent < BevelRadius * 0.5f)
        {
            // 角落倒角：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
        else
        {
            // 边缘倒角：U从0.0到1.0，V从0.0到1.0
            float U = (X / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            float V = (Y / HalfSize + 1.0f) * 0.5f;  // 0.0 到 1.0
            return FVector2D(U, V);
        }
    }
}

int32 FBevelCubeBuilder::GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal)
{
    FVector2D MainUV = GenerateStableUVCustom(Pos, Normal);
    FVector2D SecondaryUV = GenerateSecondaryUV(Pos, Normal);
    
    return FModelGenMeshBuilder::GetOrAddVertexWithDualUV(Pos, Normal, MainUV, SecondaryUV);
}
