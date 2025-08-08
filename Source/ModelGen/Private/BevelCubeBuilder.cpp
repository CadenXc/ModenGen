// Copyright (c) 2024. All rights reserved.

#include "BevelCubeBuilder.h"
#include "BevelCubeParameters.h"
#include "ModelGenMeshData.h"

FBevelCubeBuilder::FBevelCubeBuilder(const FBevelCubeParameters& InParams)
    : Params(InParams)
{
    Clear();
}

bool FBevelCubeBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Starting generation"));
    
    // 验证生成参数
    if (!ValidateParameters())
    {
        UE_LOG(LogTemp, Error, TEXT("无效的圆角立方体生成参数"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Parameters validated successfully"));

    // 清除之前的几何数据
    Clear();
    ReserveMemory();

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Memory reserved"));

    // 计算立方体的核心点（倒角起始点）
    TArray<FVector> CorePoints = CalculateCorePoints();
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Core points calculated: %d points"), CorePoints.Num());

    // 按照以下顺序生成几何体：
    // 1. 生成主要面（6个面）
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Generating main faces"));
    GenerateMainFaces();
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Main faces generated, current vertices: %d"), MeshData.GetVertexCount());
    
    // 2. 生成边缘倒角（12条边）
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Generating edge bevels"));
    GenerateEdgeBevels(CorePoints);
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Edge bevels generated, current vertices: %d"), MeshData.GetVertexCount());
    
    // 3. 生成角落倒角（8个角）
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Generating corner bevels"));
    GenerateCornerBevels(CorePoints);
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Corner bevels generated, current vertices: %d"), MeshData.GetVertexCount());

    // 验证生成的网格数据
    if (!ValidateGeneratedData())
    {
        UE_LOG(LogTemp, Error, TEXT("生成的圆角立方体网格数据无效"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::Generate - Final mesh data: %d vertices, %d triangles"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());

    // 输出网格数据
    OutMeshData = MeshData;
    return true;
}

bool FBevelCubeBuilder::ValidateParameters() const
{
    return Params.IsValid();
}

int32 FBevelCubeBuilder::CalculateVertexCountEstimate() const
{
    return Params.CalculateVertexCountEstimate();
}

int32 FBevelCubeBuilder::CalculateTriangleCountEstimate() const
{
    return Params.CalculateTriangleCountEstimate();
}

TArray<FVector> FBevelCubeBuilder::CalculateCorePoints() const
{
    const float InnerOffset = Params.GetInnerOffset();
    
    TArray<FVector> CorePoints;
    CorePoints.Reserve(8);
    
    // 按照以下顺序添加8个角落的核心点：
    // 下面四个点（Z轴负方向）
    CorePoints.Add(FVector(-InnerOffset, -InnerOffset, -InnerOffset)); // 左后下
    CorePoints.Add(FVector(InnerOffset, -InnerOffset, -InnerOffset));  // 右后下
    CorePoints.Add(FVector(-InnerOffset, InnerOffset, -InnerOffset));  // 左前下
    CorePoints.Add(FVector(InnerOffset, InnerOffset, -InnerOffset));   // 右前下
    // 上面四个点（Z轴正方向）
    CorePoints.Add(FVector(-InnerOffset, -InnerOffset, InnerOffset));  // 左后上
    CorePoints.Add(FVector(InnerOffset, -InnerOffset, InnerOffset));   // 右后上
    CorePoints.Add(FVector(-InnerOffset, InnerOffset, InnerOffset));   // 左前上
    CorePoints.Add(FVector(InnerOffset, InnerOffset, InnerOffset));    // 右前上
    
    return CorePoints;
}

void FBevelCubeBuilder::GenerateMainFaces()
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - Starting main faces generation"));
    
    const float HalfSize = Params.GetHalfSize();
    const float InnerOffset = Params.GetInnerOffset();

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - HalfSize: %f, InnerOffset: %f"), HalfSize, InnerOffset);

    /**
     * 面数据结构，用于定义每个面的属性
     */
    struct FaceData
    {
        FVector Center;   // 面的中心点
        FVector SizeX;    // 面的X方向尺寸向量
        FVector SizeY;    // 面的Y方向尺寸向量
        FVector Normal;   // 面的法线向量
    };

    // 定义立方体的六个面
    TArray<FaceData> Faces = {
        // +X面（右面）：从+X方向看，Z轴向左(SizeX)，Y轴向上(SizeY)
        {
            FVector(HalfSize, 0, 0),                    // 中心点
            FVector(0, 0, -InnerOffset),                // X方向（实际是Z轴负方向）
            FVector(0, InnerOffset, 0),                 // Y方向
            FVector(1, 0, 0)                            // 法线
        },
        // -X面（左面）：从-X方向看，Z轴向右(SizeX)，Y轴向上(SizeY)
        {
            FVector(-HalfSize, 0, 0),                   // 中心点
            FVector(0, 0, InnerOffset),                 // X方向（实际是Z轴正方向）
            FVector(0, InnerOffset, 0),                 // Y方向
            FVector(-1, 0, 0)                           // 法线
        },
        // +Y面（前面）：从+Y方向看，X轴向左，Z轴向上
        {
            FVector(0, HalfSize, 0),                    // 中心点
            FVector(-InnerOffset, 0, 0),                // X方向
            FVector(0, 0, InnerOffset),                 // Y方向（实际是Z轴）
            FVector(0, 1, 0)                            // 法线
        },
        // -Y面（后面）：从-Y方向看，X轴向右，Z轴向上
        {
            FVector(0, -HalfSize, 0),                   // 中心点
            FVector(InnerOffset, 0, 0),                 // X方向
            FVector(0, 0, InnerOffset),                 // Y方向（实际是Z轴）
            FVector(0, -1, 0)                           // 法线
        },
        // +Z面（上面）：从+Z方向看，X轴向右，Y轴向上
        {
            FVector(0, 0, HalfSize),                    // 中心点
            FVector(InnerOffset, 0, 0),                 // X方向
            FVector(0, InnerOffset, 0),                 // Y方向
            FVector(0, 0, 1)                            // 法线
        },
        // -Z面（下面）：从-Z方向看，X轴向右，Y轴向下
        {
            FVector(0, 0, -HalfSize),                   // 中心点
            FVector(InnerOffset, 0, 0),                 // X方向
            FVector(0, -InnerOffset, 0),                // Y方向
            FVector(0, 0, -1)                           // 法线
        }
    };

    // 定义标准UV坐标
    TArray<FVector2D> UVs = {
        FVector2D(0, 0),  // 左下角
        FVector2D(0, 1),  // 左上角
        FVector2D(1, 1),  // 右上角
        FVector2D(1, 0)   // 右下角
    };

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - Generating %d faces"), Faces.Num());

    // 为每个面生成几何体
    for (int32 FaceIndex = 0; FaceIndex < Faces.Num(); ++FaceIndex)
    {
        const FaceData& Face = Faces[FaceIndex];
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - Generating face %d"), FaceIndex);
        
        // 生成面的四个顶点
        TArray<FVector> FaceVerts = GenerateRectangleVertices(Face.Center, Face.SizeX, Face.SizeY);
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - Face %d vertices: %d"), FaceIndex, FaceVerts.Num());
        
        // 使用这些顶点和UV创建面的几何体
        GenerateQuadSides(FaceVerts, Face.Normal, UVs);
        
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - Face %d completed, total vertices: %d"), 
               FaceIndex, MeshData.GetVertexCount());
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateMainFaces - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FBevelCubeBuilder::GenerateEdgeBevels(const TArray<FVector>& CorePoints)
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Starting edge bevels generation"));
    
    // 边缘倒角定义结构
    struct FEdgeBevelDef
    {
        int32 Core1Idx;
        int32 Core2Idx;
        FVector Normal1;
        FVector Normal2;
    };

    // 定义所有边缘的倒角数据
    TArray<FEdgeBevelDef> EdgeDefs = {
        // +X 方向的边缘
        { 0, 1, FVector(0,-1,0), FVector(0,0,-1) },
        { 2, 3, FVector(0,0,-1), FVector(0,1,0) },
        { 4, 5, FVector(0,0,1), FVector(0,-1,0) },
        { 6, 7, FVector(0,1,0), FVector(0,0,1) },

        // +Y 方向的边缘
        { 0, 2, FVector(0,0,-1), FVector(-1,0,0) },
        { 1, 3, FVector(1,0,0), FVector(0,0,-1) },
        { 4, 6, FVector(-1,0,0), FVector(0,0,1) },
        { 5, 7, FVector(0,0,1), FVector(1,0,0) },

        // +Z 方向的边缘
        { 0, 4, FVector(-1,0,0), FVector(0,-1,0) },
        { 1, 5, FVector(0,-1,0), FVector(1,0,0) },
        { 2, 6, FVector(0,1,0), FVector(-1,0,0) },
        { 3, 7, FVector(1,0,0), FVector(0,1,0) }
    };

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Generating %d edge bevels"), EdgeDefs.Num());

    // 为每条边生成倒角
    for (int32 EdgeIndex = 0; EdgeIndex < EdgeDefs.Num(); ++EdgeIndex)
    {
        const FEdgeBevelDef& EdgeDef = EdgeDefs[EdgeIndex];
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Generating edge %d (Core1=%d, Core2=%d)"), 
               EdgeIndex, EdgeDef.Core1Idx, EdgeDef.Core2Idx);
        
        GenerateEdgeStrip(CorePoints, EdgeDef.Core1Idx, EdgeDef.Core2Idx, EdgeDef.Normal1, EdgeDef.Normal2);
        
        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Edge %d completed, total vertices: %d"), 
               EdgeIndex, MeshData.GetVertexCount());
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeBevels - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FBevelCubeBuilder::GenerateCornerBevels(const TArray<FVector>& CorePoints)
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Starting corner bevels generation"));
    
    if (CorePoints.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid CorePoints array size. Expected 8 elements."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Generating 8 corner bevels"));

    // 为每个角落生成倒角
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const FVector& CurrentCorePoint = CorePoints[CornerIndex];
        bool bSpecialCornerRenderingOrder = (CornerIndex == 4 || CornerIndex == 7 || CornerIndex == 2 || CornerIndex == 1);

        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Generating corner %d at position (%f, %f, %f)"), 
               CornerIndex, CurrentCorePoint.X, CurrentCorePoint.Y, CurrentCorePoint.Z);

        // 计算角落的轴向
        const float SignX = FMath::Sign(CurrentCorePoint.X);
        const float SignY = FMath::Sign(CurrentCorePoint.Y);
        const float SignZ = FMath::Sign(CurrentCorePoint.Z);

        const FVector AxisX(SignX, 0.0f, 0.0f);
        const FVector AxisY(0.0f, SignY, 0.0f);
        const FVector AxisZ(0.0f, 0.0f, SignZ);

        // 创建顶点网格
        TArray<TArray<int32>> CornerVerticesGrid;
        CornerVerticesGrid.SetNum(Params.BevelSections + 1);

        for (int32 Lat = 0; Lat <= Params.BevelSections; ++Lat)
        {
            CornerVerticesGrid[Lat].SetNum(Params.BevelSections + 1 - Lat);
        }

        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Corner %d: Generating vertices grid"), CornerIndex);

        // 生成四分之一球体的顶点
        for (int32 Lat = 0; Lat <= Params.BevelSections; ++Lat)
        {
            for (int32 Lon = 0; Lon <= Params.BevelSections - Lat; ++Lon)
            {
                const float LonAlpha = static_cast<float>(Lon) / Params.BevelSections;
                const float LatAlpha = static_cast<float>(Lat) / Params.BevelSections;

                // 使用辅助函数生成顶点
                TArray<FVector> Vertices = GenerateCornerVertices(CurrentCorePoint, AxisX, AxisY, AxisZ, Lat, Lon);
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

        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Corner %d: Generating triangles"), CornerIndex);

        // 生成四分之一球体的三角形
        for (int32 Lat = 0; Lat < Params.BevelSections; ++Lat)
        {
            for (int32 Lon = 0; Lon < Params.BevelSections - Lat; ++Lon)
            {
                GenerateCornerTriangles(CornerVerticesGrid, Lat, Lon, bSpecialCornerRenderingOrder);
            }
        }

        UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Corner %d completed, total vertices: %d"), 
               CornerIndex, MeshData.GetVertexCount());
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateCornerBevels - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FBevelCubeBuilder::GenerateEdgeStrip(const TArray<FVector>& CorePoints, int32 Core1Idx, int32 Core2Idx, 
                                         const FVector& Normal1, const FVector& Normal2)
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateEdgeStrip - Starting edge strip generation (Core1=%d, Core2=%d)"), 
           Core1Idx, Core2Idx);
    
    TArray<int32> PrevStripStartIndices;
    TArray<int32> PrevStripEndIndices;

    for (int32 s = 0; s <= Params.BevelSections; ++s)
    {
        const float Alpha = static_cast<float>(s) / Params.BevelSections;
        FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();

        FVector PosStart = CorePoints[Core1Idx] + CurrentNormal * Params.BevelSize;
        FVector PosEnd = CorePoints[Core2Idx] + CurrentNormal * Params.BevelSize;

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
    Vertices.Add(Center - SizeX - SizeY); // 0: 左下
    Vertices.Add(Center - SizeX + SizeY); // 1: 左上  
    Vertices.Add(Center + SizeX + SizeY); // 2: 右上
    Vertices.Add(Center + SizeX - SizeY); // 3: 右下
    
    return Vertices;
}

void FBevelCubeBuilder::GenerateQuadSides(const TArray<FVector>& Verts, const FVector& Normal, const TArray<FVector2D>& UVs)
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateQuadSides - Starting quad generation"));
    
    if (Verts.Num() != 4 || UVs.Num() != 4)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateQuadSides: 需要4个顶点和4个UV坐标, Verts: %d, UVs: %d"), Verts.Num(), UVs.Num());
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateQuadSides - Adding 4 vertices"));
    
    // 添加四个顶点，每个顶点都使用相同的法线
    int32 V0 = GetOrAddVertex(Verts[0], Normal, UVs[0]);
    int32 V1 = GetOrAddVertex(Verts[1], Normal, UVs[1]);
    int32 V2 = GetOrAddVertex(Verts[2], Normal, UVs[2]);
    int32 V3 = GetOrAddVertex(Verts[3], Normal, UVs[3]);
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateQuadSides - Vertices added: V0=%d, V1=%d, V2=%d, V3=%d"), V0, V1, V2, V3);
    
    // 添加四边形（两个三角形）
    AddQuad(V0, V1, V2, V3);
    
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeBuilder::GenerateQuadSides - Quad added, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

TArray<FVector> FBevelCubeBuilder::GenerateEdgeVertices(const FVector& CorePoint1, const FVector& CorePoint2, 
                                                        const FVector& Normal1, const FVector& Normal2, float Alpha) const
{
    TArray<FVector> Vertices;
    Vertices.Reserve(2);
    
    // 计算插值法线
    FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();
    
    // 生成边缘的两个顶点
    FVector PosStart = CorePoint1 + CurrentNormal * Params.BevelSize;
    FVector PosEnd = CorePoint2 + CurrentNormal * Params.BevelSize;
    
    Vertices.Add(PosStart);
    Vertices.Add(PosEnd);
    
    return Vertices;
}

TArray<FVector> FBevelCubeBuilder::GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX, 
                                                          const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const
{
    TArray<FVector> Vertices;
    Vertices.Reserve(1);
    
    const float LatAlpha = static_cast<float>(Lat) / Params.BevelSections;
    const float LonAlpha = static_cast<float>(Lon) / Params.BevelSections;

    // 计算法线（三轴插值）
    FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
        AxisY * LatAlpha +
        AxisZ * LonAlpha);
    CurrentNormal.Normalize();

    // 计算顶点位置
    FVector CurrentPos = CorePoint + CurrentNormal * Params.BevelSize;
    Vertices.Add(CurrentPos);
    
    return Vertices;
}
