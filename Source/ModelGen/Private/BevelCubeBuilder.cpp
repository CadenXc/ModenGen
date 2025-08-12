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
    PrecomputeAlphaValues();           // 预计算Alpha值
    PrecomputeCornerGridSizes();      // 预计算角落网格尺寸
    
    // 添加调试信息
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Constructor - Initialized with: BevelSegments=%d, AlphaValues.Num()=%d"), 
           BevelSegments, AlphaValues.Num());
    
    // 显示CornerGridSizes的详细信息
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Constructor - CornerGridSizes.Num()=%d"), CornerGridSizes.Num());
    for (int32 i = 0; i < CornerGridSizes.Num(); ++i)
    {
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Constructor - CornerGridSizes[%d].Num()=%d"), i, CornerGridSizes[i].Num());
    }
    
    // 验证预计算数据
    if (!ValidatePrecomputedData())
    {
        UE_LOG(LogTemp, Error, TEXT("FBevelCubeBuilder::Constructor - Precomputed data validation failed!"));
    }
}

bool FBevelCubeBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Starting generation"));
    
    if (!ValidateParameters())
    {
        UE_LOG(LogTemp, Error, TEXT("无效的圆角立方体生成参数"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Parameters validated successfully"));

    Clear();
    ReserveMemory();

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Memory reserved"));

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Core points calculated: %d points"), CorePoints.Num());

    GenerateMainFaces();
    
    GenerateEdgeBevels();
    
    GenerateCornerBevels();

    if (!ValidateGeneratedData())
    {
        UE_LOG(LogTemp, Error, TEXT("生成的圆角立方体网格数据无效"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Final mesh data: %d vertices, %d triangles"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());

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

// 第一步：预计算常量
void FBevelCubeBuilder::PrecomputeConstants()
{
    HalfSize = Params.GetHalfSize();
    InnerOffset = Params.GetInnerOffset();
    BevelRadius = Params.BevelRadius;
    BevelSegments = Params.BevelSegments;
}

// 性能优化：预计算Alpha值
void FBevelCubeBuilder::PrecomputeAlphaValues()
{
    // 确保数组大小正确：从0到BevelSegments，总共BevelSegments + 1个元素
    const int32 ArraySize = BevelSegments + 1;
    AlphaValues.SetNum(ArraySize);
    
    for (int32 i = 0; i < ArraySize; ++i)
    {
        AlphaValues[i] = static_cast<float>(i) / BevelSegments;
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::PrecomputeAlphaValues - Created %d alpha values (0.0 to 1.0)"), ArraySize);
}

// 性能优化：预计算角落网格尺寸
void FBevelCubeBuilder::PrecomputeCornerGridSizes()
{
    // 确保数组大小正确：从0到BevelSegments，总共BevelSegments + 1个元素
    const int32 ArraySize = BevelSegments + 1;
    CornerGridSizes.SetNum(ArraySize);
    
    for (int32 Lat = 0; Lat < ArraySize; ++Lat)
    {
        const int32 LonCount = ArraySize - Lat;
        CornerGridSizes[Lat].SetNum(LonCount);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::PrecomputeCornerGridSizes - Created %d corner grid sizes"), ArraySize);
}

// 第二步：初始化面定义
void FBevelCubeBuilder::InitializeFaceDefinitions()
{
    FaceDefinitions = {
        // +X面（右面）：从+X方向看，Z轴向左(SizeX)，Y轴向上(SizeY)
        {
            FVector(HalfSize, 0, 0),                    // 中心点
            FVector(0, 0, -InnerOffset),                // X方向（实际是Z轴负方向）
            FVector(0, InnerOffset, 0),                 // Y方向
            FVector(1, 0, 0),                           // 法线
            TEXT("Right")                                // 面名称
        },
        // -X面（左面）：从-X方向看，Z轴向右(SizeX)，Y轴向上(SizeY)
        {
            FVector(-HalfSize, 0, 0),                   // 中心点
            FVector(0, 0, InnerOffset),                 // X方向（实际是Z轴正方向）
            FVector(0, InnerOffset, 0),                 // Y方向
            FVector(-1, 0, 0),                          // 法线
            TEXT("Left")                                 // 面名称
        },
        // +Y面（前面）：从+Y方向看，X轴向左，Z轴向上
        {
            FVector(0, HalfSize, 0),                    // 中心点
            FVector(-InnerOffset, 0, 0),                // X方向
            FVector(0, 0, InnerOffset),                 // Y方向（实际是Z轴）
            FVector(0, 1, 0),                           // 法线
            TEXT("Front")                                // 面名称
        },
        // -Y面（后面）：从-Y方向看，X轴向右，Z轴向上
        {
            FVector(0, -HalfSize, 0),                   // 中心点
            FVector(InnerOffset, 0, 0),                 // X方向
            FVector(0, 0, InnerOffset),                 // Y方向（实际是Z轴）
            FVector(0, -1, 0),                          // 法线
            TEXT("Back")                                 // 面名称
        },
        // +Z面（上面）：从+Z方向看，X轴向右，Y轴向上
        {
            FVector(0, 0, HalfSize),                    // 中心点
            FVector(InnerOffset, 0, 0),                 // X方向
            FVector(0, InnerOffset, 0),                 // Y方向
            FVector(0, 0, 1),                           // 法线
            TEXT("Top")                                  // 面名称
        },
        // -Z面（下面）：从-Z方向看，X轴向右，Y轴向下
        {
            FVector(0, 0, -HalfSize),                   // 中心点
            FVector(InnerOffset, 0, 0),                 // X方向
            FVector(0, -InnerOffset, 0),                // Y方向
            FVector(0, 0, -1),                          // 法线
            TEXT("Bottom")                               // 面名称
        }
    };
}

// 第三步：初始化边缘倒角定义
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
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - Generating %d faces"), FaceDefinitions.Num());

    // 定义标准UV坐标
    TArray<FVector2D> UVs = {
        FVector2D(0, 0),  // 左下角
        FVector2D(0, 1),  // 左上角
        FVector2D(1, 1),  // 右上角
        FVector2D(1, 0)   // 右下角
    };

    // 为每个面生成几何体
    for (const FFaceData& Face : FaceDefinitions)
    {
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - Generating face: %s"), *Face.Name);
        
        TArray<FVector> FaceVerts = GenerateRectangleVertices(Face.Center, Face.SizeX, Face.SizeY);
        
        GenerateQuadSides(FaceVerts, Face.Normal, UVs);
    }
}

void FBevelCubeBuilder::GenerateEdgeBevels()
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Generating %d edge bevels"), EdgeBevelDefs.Num());

    // 为每条边生成倒角
    for (const FEdgeBevelDef& EdgeDef : EdgeBevelDefs)
    {
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Generating edge %s (Core1=%d, Core2=%d)"), 
               *EdgeDef.Name, EdgeDef.Core1Idx, EdgeDef.Core2Idx);
        
        GenerateEdgeStrip(EdgeDef.Core1Idx, EdgeDef.Core2Idx, EdgeDef.Normal1, EdgeDef.Normal2);
        
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Edge %s completed, total vertices: %d"), 
               *EdgeDef.Name, MeshData.GetVertexCount());
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FBevelCubeBuilder::GenerateCornerBevels()
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Starting corner bevels generation"));
    
    if (CorePoints.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid CorePoints array size. Expected 8 elements."));
        return;
    }

    // 为每个角落生成倒角
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        GenerateCornerBevel(CornerIndex);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

// 新增：提取角落倒角生成逻辑
void FBevelCubeBuilder::GenerateCornerBevel(int32 CornerIndex)
{
    const FVector& CurrentCorePoint = CorePoints[CornerIndex];
    bool bSpecialCornerRenderingOrder = IsSpecialCorner(CornerIndex);

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevel - Generating corner %d at position (%f, %f, %f)"), 
           CornerIndex, CurrentCorePoint.X, CurrentCorePoint.Y, CurrentCorePoint.Z);

    // 计算角落的轴向
    const FVector AxisX(FMath::Sign(CurrentCorePoint.X), 0.0f, 0.0f);
    const FVector AxisY(0.0f, FMath::Sign(CurrentCorePoint.Y), 0.0f);
    const FVector AxisZ(0.0f, 0.0f, FMath::Sign(CurrentCorePoint.Z));

    // 使用预计算的网格尺寸，并添加安全检查
    if (CornerGridSizes.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("FBevelCubeBuilder::GenerateCornerBevel - CornerGridSizes is empty for corner %d"), CornerIndex);
        return;
    }
    
    TArray<TArray<int32>> CornerVerticesGrid = CornerGridSizes;
    
    // 验证CornerVerticesGrid的初始化
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevel - Corner %d: CornerVerticesGrid.Num()=%d"), 
           CornerIndex, CornerVerticesGrid.Num());
    for (int32 i = 0; i < CornerVerticesGrid.Num(); ++i)
    {
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevel - Corner %d: CornerVerticesGrid[%d].Num()=%d"), 
               CornerIndex, i, CornerVerticesGrid[i].Num());
    }

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevel - Corner %d: Generating vertices grid"), CornerIndex);

    // 生成四分之一球体的顶点
    GenerateCornerVerticesGrid(CornerIndex, CurrentCorePoint, AxisX, AxisY, AxisZ, CornerVerticesGrid);

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevel - Corner %d: Generating triangles"), CornerIndex);

    // 生成四分之一球体的三角形
    GenerateCornerTrianglesGrid(CornerVerticesGrid, bSpecialCornerRenderingOrder);

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevel - Corner %d completed, total vertices: %d"), 
           CornerIndex, MeshData.GetVertexCount());
}

void FBevelCubeBuilder::GenerateEdgeStrip(int32 Core1Idx, int32 Core2Idx, 
                                         const FVector& Normal1, const FVector& Normal2)
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeStrip - Starting edge strip generation (Core1=%d, Core2=%d)"), 
           Core1Idx, Core2Idx);
    
    TArray<int32> PrevStripStartIndices;
    TArray<int32> PrevStripEndIndices;

    // 使用预计算的Alpha值，避免重复计算
    for (int32 s = 0; s <= BevelSegments; ++s)
    {
        const float Alpha = GetAlphaValue(s);  // 使用安全的访问函数
        FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();

        FVector PosStart = CorePoints[Core1Idx] + CurrentNormal * BevelRadius;
        FVector PosEnd = CorePoints[Core2Idx] + CurrentNormal * BevelRadius;

        FVector2D UV1(Alpha, 0.0f);
        FVector2D UV2(Alpha, 1.0f);

        int32 VtxStart = GetOrAddVertex(PosStart, CurrentNormal, UV1);
        int32 VtxEnd = GetOrAddVertex(PosEnd, CurrentNormal, UV2);

        if (s > 0 && PrevStripStartIndices.Num() > 0 && PrevStripEndIndices.Num() > 0)
        {
            AddQuad(PrevStripStartIndices[0], PrevStripEndIndices[0], VtxEnd, VtxStart);
        }

        PrevStripStartIndices = { VtxStart };
        PrevStripEndIndices = { VtxEnd };
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeStrip - Edge strip completed"));
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
    if (Verts.Num() != 4 || UVs.Num() != 4)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateQuadSides: 需要4个顶点和4个UV坐标, Verts: %d, UVs: %d"), Verts.Num(), UVs.Num());
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
    
    const float LatAlpha = GetAlphaValue(Lat);  // 使用安全的访问函数
    const float LonAlpha = GetAlphaValue(Lon);  // 使用安全的访问函数

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

// 新增：生成角落顶点网格
void FBevelCubeBuilder::GenerateCornerVerticesGrid(int32 CornerIndex, const FVector& CorePoint, 
                                                   const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ,
                                                   TArray<TArray<int32>>& CornerVerticesGrid)
{
    // 生成四分之一球体的顶点
    for (int32 Lat = 0; Lat < CornerVerticesGrid.Num(); ++Lat)  // 使用数组大小而不是 <= BevelSegments
    {
        // 添加边界检查
        if (!IsValidCornerGridIndex(Lat, 0))
        {
            UE_LOG(LogTemp, Warning, TEXT("FBevelCubeBuilder::GenerateCornerVerticesGrid - Invalid Lat index %d for corner %d"), 
                   Lat, CornerIndex);
            continue;
        }
        
        for (int32 Lon = 0; Lon < CornerVerticesGrid[Lat].Num(); ++Lon)  // 使用子数组大小而不是 <= BevelSegments - Lat
        {
            // 添加边界检查
            if (!IsValidCornerGridIndex(Lat, Lon))
            {
                UE_LOG(LogTemp, Warning, TEXT("FBevelCubeBuilder::GenerateCornerVerticesGrid - Invalid Lon index %d for Lat %d, corner %d"), 
                       Lon, Lat, CornerIndex);
                continue;
            }
            
            const float LonAlpha = GetAlphaValue(Lon);  // 使用安全的访问函数
            const float LatAlpha = GetAlphaValue(Lat);  // 使用安全的访问函数

            // 使用辅助函数生成顶点
            TArray<FVector> Vertices = GenerateCornerVertices(CorePoint, AxisX, AxisY, AxisZ, Lat, Lon);
            if (Vertices.Num() > 0)
            {
                FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
                    AxisY * LatAlpha +
                    AxisZ * LonAlpha);
                CurrentNormal.Normalize();

                FVector2D UV(LonAlpha, LatAlpha);
                CornerVerticesGrid[Lat][Lon] = GetOrAddVertex(Vertices[0], CurrentNormal, UV);
            }
        }
    }
}

// 新增：生成角落三角形网格
void FBevelCubeBuilder::GenerateCornerTrianglesGrid(const TArray<TArray<int32>>& CornerVerticesGrid, bool bSpecialOrder)
{
    // 生成四分之一球体的三角形
    for (int32 Lat = 0; Lat < CornerVerticesGrid.Num() - 1; ++Lat)  // 使用数组大小-1，因为我们需要访问Lat+1
    {
        // 添加边界检查
        if (!IsValidCornerGridIndex(Lat, 0))
        {
            UE_LOG(LogTemp, Warning, TEXT("FBevelCubeBuilder::GenerateCornerTrianglesGrid - Invalid Lat index %d"), Lat);
            continue;
        }
        
        for (int32 Lon = 0; Lon < CornerVerticesGrid[Lat].Num() - 1; ++Lon)  // 使用子数组大小-1，因为我们需要访问Lon+1
        {
            // 添加边界检查
            if (!IsValidCornerGridIndex(Lat, Lon))
            {
                UE_LOG(LogTemp, Warning, TEXT("FBevelCubeBuilder::GenerateCornerTrianglesGrid - Invalid Lon index %d for Lat %d"), Lon, Lat);
                continue;
            }
            
            GenerateCornerTriangles(CornerVerticesGrid, Lat, Lon, bSpecialOrder);
        }
    }
}

// 新增：判断是否为特殊角落
bool FBevelCubeBuilder::IsSpecialCorner(int32 CornerIndex) const
{
    // 使用预定义的常量数组，避免硬编码
    static const TArray<int32> SpecialCornerIndices = { 4, 7, 2, 1 };
    return SpecialCornerIndices.Contains(CornerIndex);
}

// 新增：安全访问Alpha值
float FBevelCubeBuilder::GetAlphaValue(int32 Index) const
{
    if (IsValidAlphaIndex(Index))
    {
        return AlphaValues[Index];
    }
    
    UE_LOG(LogTemp, Warning, TEXT("FBevelCubeBuilder::GetAlphaValue - Invalid index %d, array size is %d. Using fallback value."), 
           Index, AlphaValues.Num());
    
    // 返回安全的默认值
    return FMath::Clamp(static_cast<float>(Index) / BevelSegments, 0.0f, 1.0f);
}

// 新增：检查Alpha索引是否有效
bool FBevelCubeBuilder::IsValidAlphaIndex(int32 Index) const
{
    return Index >= 0 && Index < AlphaValues.Num();
}

// 新增：检查角落网格索引是否有效
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

// 新增：获取指定Lat的网格大小
int32 FBevelCubeBuilder::GetCornerGridSize(int32 Lat) const
{
    if (Lat >= 0 && Lat < CornerGridSizes.Num())
    {
        return CornerGridSizes[Lat].Num();
    }
    
    UE_LOG(LogTemp, Warning, TEXT("FBevelCubeBuilder::GetCornerGridSize - Invalid Lat index %d, array size is %d"), 
           Lat, CornerGridSizes.Num());
    
    return 0;
}

// 新增：验证预计算数据
bool FBevelCubeBuilder::ValidatePrecomputedData() const
{
    bool bIsValid = true;
    
    // 验证AlphaValues
    if (AlphaValues.Num() != BevelSegments + 1)
    {
        UE_LOG(LogTemp, Error, TEXT("FBevelCubeBuilder::ValidatePrecomputedData - AlphaValues size mismatch: %d != %d"), 
               AlphaValues.Num(), BevelSegments + 1);
        bIsValid = false;
    }
    
    // 验证CornerGridSizes
    if (CornerGridSizes.Num() != BevelSegments + 1)
    {
        UE_LOG(LogTemp, Error, TEXT("FBevelCubeBuilder::ValidatePrecomputedData - CornerGridSizes size mismatch: %d != %d"), 
               CornerGridSizes.Num(), BevelSegments + 1);
        bIsValid = false;
    }
    
    // 验证每个子数组的大小
    for (int32 Lat = 0; Lat < CornerGridSizes.Num(); ++Lat)
    {
        int32 ExpectedSize = BevelSegments + 1 - Lat;
        if (CornerGridSizes[Lat].Num() != ExpectedSize)
        {
            UE_LOG(LogTemp, Error, TEXT("FBevelCubeBuilder::ValidatePrecomputedData - CornerGridSizes[%d] size mismatch: %d != %d"), 
                   Lat, CornerGridSizes[Lat].Num(), ExpectedSize);
            bIsValid = false;
        }
    }
    
    if (bIsValid)
    {
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::ValidatePrecomputedData - All precomputed data is valid"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FBevelCubeBuilder::ValidatePrecomputedData - Precomputed data validation failed"));
    }
    
    return bIsValid;
}
