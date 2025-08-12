// Copyright (c) 2024. All rights reserved.

#include "PyramidBuilder.h"
#include "PyramidParameters.h"
#include "ModelGenMeshData.h"

FPyramidBuilder::FPyramidBuilder(const FPyramidParameters& InParams)
    : Params(InParams)
{
    Clear();
    
    // 新增：预计算所有常量和数据
    PrecomputeConstants();
    PrecomputeVertices();
    PrecomputeUVs();
    
    // 添加调试信息
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Constructor - Initialized with: BaseRadius=%f, Height=%f, Sides=%d, BevelRadius=%f"), 
           BaseRadius, Height, Sides, BevelRadius);
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Constructor - Precomputed %d vertices and UVs"), BaseVertices.Num());
    
    // 验证预计算数据
    if (!ValidatePrecomputedData())
    {
        UE_LOG(LogTemp, Error, TEXT("FPyramidBuilder::Constructor - Precomputed data validation failed!"));
    }
}

bool FPyramidBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Starting generation"));
    
    // 验证生成参数
    if (!ValidateParameters())
    {
        UE_LOG(LogTemp, Error, TEXT("无效的金字塔生成参数"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Parameters validated successfully"));

    // 清除之前的几何数据
    Clear();
    ReserveMemory();


    // 按照以下顺序生成几何体：
    // 1. 生成底面
    GenerateBaseFace();
    
    // 2. 生成倒角部分（如果有）
    if (Params.BevelRadius > 0.0f)
    {
        GenerateBevelSection();
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Skipping bevel section (BevelRadius=%f)"), Params.BevelRadius);
    }
    
    // 3. 生成金字塔侧面
    GeneratePyramidSides();
    // 验证生成的网格数据
    if (!ValidateGeneratedData())
    {
        UE_LOG(LogTemp, Error, TEXT("生成的金字塔网格数据无效"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Final mesh data: %d vertices, %d triangles"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());

    // 输出网格数据
    OutMeshData = MeshData;
    return true;
}

bool FPyramidBuilder::ValidateParameters() const
{
    return Params.IsValid();
}

int32 FPyramidBuilder::CalculateVertexCountEstimate() const
{
    return Params.CalculateVertexCountEstimate();
}

int32 FPyramidBuilder::CalculateTriangleCountEstimate() const
{
    return Params.CalculateTriangleCountEstimate();
}

void FPyramidBuilder::GenerateBaseFace()
{
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBaseFace - Generating base face with %d vertices"), BaseVertices.Num());
    
    // 使用预计算的顶点和UV数据
    GenerateBasePolygon();
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBaseFace - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
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
        UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBevelSection - Skipping bevel section (BevelRadius=%f)"), BevelRadius);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBevelSection - Generating bevel section with %d vertices"), BevelBottomVertices.Num());
    
    // 使用预计算的顶点和UV数据
    GenerateBevelSideStrip();
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBevelSection - Completed"));
}

// 新增：生成倒角侧面条带
void FPyramidBuilder::GenerateBevelSideStrip()
{
    // 使用预计算的倒角顶点和UV数据
    GenerateSideStripOptimized(BevelBottomVertices, BevelTopVertices, BevelUVs, true);
}

void FPyramidBuilder::GeneratePyramidSides()
{
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePyramidSides - Generating pyramid sides with %d base vertices"), PyramidBaseVertices.Num());
    
    // 使用预计算的数据生成金字塔侧面
    GeneratePyramidSideTriangles();
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePyramidSides - Completed"));
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
        
        // 使用预计算的UV数据
        FVector2D TopUV = FVector2D(1.0f, 2.0f);  // 2U系统：顶点UV
        FVector2D BaseUV1 = SideUVs[i];
        FVector2D BaseUV2 = SideUVs[NextI];
        
        // 添加顶点
        int32 TopVertex = GetOrAddVertex(PyramidTopPoint, SideNormal, TopUV);
        int32 V1 = GetOrAddVertex(PyramidBaseVertices[i], SideNormal, BaseUV1);
        int32 V2 = GetOrAddVertex(PyramidBaseVertices[NextI], SideNormal, BaseUV2);
        
        // 添加三角形
        AddTriangle(V2, V1, TopVertex);
    }
}

void FPyramidBuilder::GeneratePolygonFace(const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder, float UVOffsetZ)
{
    if (PolygonVerts.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("GeneratePolygonFace: 需要至少3个顶点"));
        return;
    }
    
    // 添加所有顶点
    TArray<int32> VertexIndices;
    for (int32 i = 0; i < PolygonVerts.Num(); ++i)
    {
        // 计算圆形UV坐标
        float Angle = 2.0f * PI * static_cast<float>(i) / PolygonVerts.Num();
        float U = 0.5f + 0.5f * FMath::Cos(Angle);
        float V = 0.5f + 0.5f * FMath::Sin(Angle);
        FVector2D UV(U, V + UVOffsetZ);
        int32 VertexIndex = GetOrAddVertex(PolygonVerts[i], Normal, UV);
        VertexIndices.Add(VertexIndex);
    }
    
    // 生成三角形（扇形三角剖分）
    for (int32 i = 1; i < PolygonVerts.Num() - 1; ++i)
    {
        int32 V0 = VertexIndices[0];
        int32 V1 = VertexIndices[i];
        int32 V2 = VertexIndices[i + 1];
        
        if (bReverseOrder)
        {
            AddTriangle(V0, V2, V1);
            //AddTriangle(V0, V1, V2);
        }
        else
        {
            AddTriangle(V0, V1, V2);
            //AddTriangle(V0, V2, V1);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePolygonFace - Polygon face completed"));
}

void FPyramidBuilder::GenerateSideStrip(const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal, float UVOffsetY, float UVScaleY)
{
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateSideStrip - Generating side strip with %d bottom and %d top vertices"), 
           BottomVerts.Num(), TopVerts.Num());
    
    if (BottomVerts.Num() != TopVerts.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateSideStrip: 底部和顶部顶点数量不匹配"));
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
        
        // 添加四个顶点
        int32 V1 = GetOrAddVertex(BottomVerts[i], SideNormal, FVector2D(static_cast<float>(i) / BottomVerts.Num(), UVOffsetY));
        int32 V2 = GetOrAddVertex(BottomVerts[NextI], SideNormal, FVector2D(static_cast<float>(NextI) / BottomVerts.Num(), UVOffsetY));
        int32 V3 = GetOrAddVertex(TopVerts[NextI], SideNormal, FVector2D(static_cast<float>(NextI) / TopVerts.Num(), UVOffsetY + UVScaleY));
        int32 V4 = GetOrAddVertex(TopVerts[i], SideNormal, FVector2D(static_cast<float>(i) / TopVerts.Num(), UVOffsetY + UVScaleY));
        
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
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateSideStrip - Side strip completed"));
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

// 新增：预计算常量
void FPyramidBuilder::PrecomputeConstants()
{
    BaseRadius = Params.BaseRadius;
    Height = Params.Height;
    Sides = Params.Sides;
    BevelRadius = Params.BevelRadius;
    BevelTopRadius = Params.GetBevelTopRadius();
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::PrecomputeConstants - Constants: BaseRadius=%f, Height=%f, Sides=%d, BevelRadius=%f, BevelTopRadius=%f"), 
           BaseRadius, Height, Sides, BevelRadius, BevelTopRadius);
}

// 新增：预计算三角函数值
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
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::PrecomputeTrigonometricValues - Created %d trigonometric values"), Sides);
}

// 新增：预计算UV缩放值
void FPyramidBuilder::PrecomputeUVScaleValues()
{
    UVScaleValues.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        UVScaleValues[i] = static_cast<float>(i) / Sides;
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::PrecomputeUVScaleValues - Created %d UV scale values"), Sides);
}

// 新增：预计算顶点
void FPyramidBuilder::PrecomputeVertices()
{
    // 先预计算三角函数值
    PrecomputeTrigonometricValues();
    
    // 初始化各种顶点
    InitializeBaseVertices();
    InitializeBevelVertices();
    InitializePyramidVertices();
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::PrecomputeVertices - All vertices precomputed"));
}

// 新增：初始化底面顶点
void FPyramidBuilder::InitializeBaseVertices()
{
    BaseVertices.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        const float X = BevelTopRadius * CosValues[i];
        const float Y = BevelTopRadius * SinValues[i];
        BaseVertices[i] = FVector(X, Y, 0.0f);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::InitializeBaseVertices - Created %d base vertices"), BaseVertices.Num());
}

// 新增：初始化倒角顶点
void FPyramidBuilder::InitializeBevelVertices()
{
    if (BevelRadius <= 0.0f)
    {
        BevelBottomVertices.Empty();
        BevelTopVertices.Empty();
        return;
    }
    
    // 倒角底部顶点（与底面相同）
    BevelBottomVertices = BaseVertices;
    
    // 倒角顶部顶点
    BevelTopVertices.SetNum(Sides);
    for (int32 i = 0; i < Sides; ++i)
    {
        const float X = BevelTopRadius * CosValues[i];
        const float Y = BevelTopRadius * SinValues[i];
        BevelTopVertices[i] = FVector(X, Y, BevelRadius);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::InitializeBevelVertices - Created %d bevel vertices"), BevelTopVertices.Num());
}

// 新增：初始化金字塔顶点
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
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::InitializePyramidVertices - Created pyramid base with %d vertices, top at %f"), 
           PyramidBaseVertices.Num(), Height);
}

// 新增：预计算UV
void FPyramidBuilder::PrecomputeUVs()
{
    // 先预计算UV缩放值
    PrecomputeUVScaleValues();
    
    // 生成各种UV坐标
    BaseUVs = GenerateCircularUVs(Sides, 0.0f);
    SideUVs = GenerateSideStripUVs(Sides, 0.0f, 1.0f);
    BevelUVs = GenerateSideStripUVs(Sides, 0.0f, 1.0f);
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::PrecomputeUVs - All UVs precomputed"));
}

// 新增：生成圆形UV坐标（2U系统）
TArray<FVector2D> FPyramidBuilder::GenerateCircularUVs(int32 NumSides, float UVOffsetZ) const
{
    TArray<FVector2D> UVs;
    UVs.SetNum(NumSides);
    
    for (int32 i = 0; i < NumSides; ++i)
    {
        // 使用2U系统：UV范围从0到2
        float U = 1.0f + FMath::Cos(AngleValues[i]);  // 0 到 2
        float V = 1.0f + FMath::Sin(AngleValues[i]) + UVOffsetZ;  // 0 到 2
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
        // 使用2U系统：UV范围从0到2
        float U = UVScaleValues[i] * 2.0f;  // 0 到 2
        float V = UVOffsetY + UVScaleY;     // 0 到 2
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
        UE_LOG(LogTemp, Warning, TEXT("GeneratePolygonFaceOptimized: 需要至少3个顶点，且顶点和UV数量必须匹配"));
        return;
    }
    
    // 添加所有顶点
    TArray<int32> VertexIndices;
    VertexIndices.Reserve(Vertices.Num());
    
    for (int32 i = 0; i < Vertices.Num(); ++i)
    {
        int32 VertexIndex = GetOrAddVertex(Vertices[i], Normal, UVs[i]);
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
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePolygonFaceOptimized - Polygon face completed with %d triangles"), Vertices.Num() - 2);
}

// 新增：优化后的侧面条带生成函数
void FPyramidBuilder::GenerateSideStripOptimized(const TArray<FVector>& BottomVerts, 
                                                const TArray<FVector>& TopVerts, 
                                                const TArray<FVector2D>& UVs,
                                                bool bReverseNormal)
{
    if (BottomVerts.Num() != TopVerts.Num() || UVs.Num() != BottomVerts.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateSideStripOptimized: 底部、顶部顶点和UV数量必须匹配"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateSideStripOptimized - Generating side strip with %d vertices"), BottomVerts.Num());
    
    for (int32 i = 0; i < BottomVerts.Num(); ++i)
    {
        const int32 NextI = (i + 1) % BottomVerts.Num();
        
        // 计算侧面法线（确保指向外部）
        FVector Edge1 = BottomVerts[NextI] - BottomVerts[i];
        FVector Edge2 = TopVerts[i] - BottomVerts[i];
        FVector SideNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
        
        if (bReverseNormal) SideNormal = -SideNormal;
        
        // 使用预计算的UV数据
        FVector2D BottomUV1 = UVs[i];
        FVector2D BottomUV2 = UVs[NextI];
        FVector2D TopUV1 = UVs[i];
        FVector2D TopUV2 = UVs[NextI];
        
        // 添加四个顶点
        int32 V1 = GetOrAddVertex(BottomVerts[i], SideNormal, BottomUV1);
        int32 V2 = GetOrAddVertex(BottomVerts[NextI], SideNormal, BottomUV2);
        int32 V3 = GetOrAddVertex(TopVerts[NextI], SideNormal, TopUV2);
        int32 V4 = GetOrAddVertex(TopVerts[i], SideNormal, TopUV1);
        
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
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateSideStripOptimized - Side strip completed"));
}

// 新增：验证预计算数据
bool FPyramidBuilder::ValidatePrecomputedData() const
{
    bool bIsValid = true;
    
    // 验证常量
    if (BaseRadius <= 0.0f || Height <= 0.0f || Sides < 3)
    {
        UE_LOG(LogTemp, Error, TEXT("FPyramidBuilder::ValidatePrecomputedData - Invalid constants: BaseRadius=%f, Height=%f, Sides=%d"), 
               BaseRadius, Height, Sides);
        bIsValid = false;
    }
    
    // 验证顶点数据
    if (BaseVertices.Num() != Sides)
    {
        UE_LOG(LogTemp, Error, TEXT("FPyramidBuilder::ValidatePrecomputedData - BaseVertices count mismatch: %d != %d"), 
               BaseVertices.Num(), Sides);
        bIsValid = false;
    }
    
    if (BevelRadius > 0.0f)
    {
        if (BevelBottomVertices.Num() != Sides || BevelTopVertices.Num() != Sides)
        {
            UE_LOG(LogTemp, Error, TEXT("FPyramidBuilder::ValidatePrecomputedData - Bevel vertices count mismatch: Bottom=%d, Top=%d, Expected=%d"), 
                   BevelBottomVertices.Num(), BevelTopVertices.Num(), Sides);
            bIsValid = false;
        }
    }
    
    if (PyramidBaseVertices.Num() != Sides)
    {
        UE_LOG(LogTemp, Error, TEXT("FPyramidBuilder::ValidatePrecomputedData - PyramidBaseVertices count mismatch: %d != %d"), 
               PyramidBaseVertices.Num(), Sides);
        bIsValid = false;
    }
    
    // 验证UV数据
    if (BaseUVs.Num() != Sides || SideUVs.Num() != Sides)
    {
        UE_LOG(LogTemp, Error, TEXT("FPyramidBuilder::ValidatePrecomputedData - UV count mismatch: BaseUVs=%d, SideUVs=%d, Expected=%d"), 
               BaseUVs.Num(), SideUVs.Num(), Sides);
        bIsValid = false;
    }
    
    // 验证三角函数值
    if (AngleValues.Num() != Sides || CosValues.Num() != Sides || SinValues.Num() != Sides)
    {
        UE_LOG(LogTemp, Error, TEXT("FPyramidBuilder::ValidatePrecomputedData - Trigonometric values count mismatch: %d != %d"), 
               AngleValues.Num(), Sides);
        bIsValid = false;
    }
    
    if (bIsValid)
    {
        UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::ValidatePrecomputedData - All data validated successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FPyramidBuilder::ValidatePrecomputedData - Data validation failed!"));
    }
    
    return bIsValid;
}
