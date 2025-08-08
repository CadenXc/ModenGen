// Copyright (c) 2024. All rights reserved.

#include "PyramidBuilder.h"
#include "PyramidParameters.h"
#include "ModelGenMeshData.h"

FPyramidBuilder::FPyramidBuilder(const FPyramidParameters& InParams)
    : Params(InParams)
{
    Clear();
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

    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Memory reserved"));

    // 按照以下顺序生成几何体：
    // 1. 生成底面
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Generating base face"));
    GenerateBaseFace();
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Base face generated, current vertices: %d"), MeshData.GetVertexCount());
    
    // 2. 生成倒角部分（如果有）
    if (Params.BevelRadius > 0.0f)
    {
        UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Generating bevel section (BevelRadius=%f)"), Params.BevelRadius);
        UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - BevelTopRadius=%f, BevelHeight=%f"), Params.GetBevelTopRadius(), Params.BevelRadius);
        GenerateBevelSection();
        UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Bevel section generated, current vertices: %d"), MeshData.GetVertexCount());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Skipping bevel section (BevelRadius=%f)"), Params.BevelRadius);
    }
    
    // 3. 生成金字塔侧面
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Generating pyramid sides"));
    GeneratePyramidSides();
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::Generate - Pyramid sides generated, current vertices: %d"), MeshData.GetVertexCount());

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
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBaseFace - Starting base face generation"));
    
    // 生成底面顶点
    TArray<FVector> BaseVerts = GenerateCircleVertices(Params.BaseRadius, 0.0f, Params.Sides);
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBaseFace - Generated %d base vertices"), BaseVerts.Num());
    
    // 生成底面（向下法线，确保正确的面方向）
    GeneratePolygonFace(BaseVerts, FVector(0, 0, -1), false, 0.0f);
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBaseFace - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FPyramidBuilder::GenerateBevelSection()
{
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBevelSection - Starting bevel section generation"));
    
    const float BevelHeight = Params.BevelRadius;
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBevelSection - BevelHeight: %f"), BevelHeight);
    
    // 生成棱柱底部和顶部顶点（上下底半径相同）
    TArray<FVector> BevelBottomVerts = GenerateCircleVertices(Params.BaseRadius, 0.0f, Params.Sides);
    TArray<FVector> BevelTopVerts = GenerateCircleVertices(Params.BaseRadius, BevelHeight, Params.Sides);
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBevelSection - Generated prism vertices: bottom=%d, top=%d"), 
           BevelBottomVerts.Num(), BevelTopVerts.Num());
    
    // 生成棱柱侧面
    GenerateSideStrip(BevelBottomVerts, BevelTopVerts, false, 0.0f, 1.0f);
    
    // 生成棱柱顶部面
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBevelSection - Generating prism top face"));
    GeneratePolygonFace(BevelTopVerts, FVector(0, 0, 1), false, 0.5f);
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GenerateBevelSection - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FPyramidBuilder::GeneratePyramidSides()
{
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePyramidSides - Starting pyramid sides generation"));
    
    const float PyramidBaseRadius = Params.BaseRadius;  // 棱柱顶部半径就是BaseRadius
    const float PyramidBaseHeight = Params.BevelRadius;
    const float PyramidHeight = Params.Height;
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePyramidSides - BaseRadius: %f, BaseHeight: %f, Height: %f"), 
           PyramidBaseRadius, PyramidBaseHeight, PyramidHeight);
    
    // 生成金字塔底面顶点（棱柱顶部或原始底面）
    TArray<FVector> PyramidBaseVerts = GenerateCircleVertices(PyramidBaseRadius, PyramidBaseHeight, Params.Sides);
    
    // 生成金字塔顶点
    FVector PyramidTop = FVector(0, 0, PyramidHeight);
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePyramidSides - Generated pyramid vertices: base=%d, top=1"), 
           PyramidBaseVerts.Num());
    
    // 为每个侧面生成三角形（金字塔主体）
    for (int32 i = 0; i < Params.Sides; ++i)
    {
        const int32 NextI = (i + 1) % Params.Sides;
        
        // 计算侧面法线（确保指向外部）
        FVector Edge1 = PyramidBaseVerts[NextI] - PyramidBaseVerts[i];
        FVector Edge2 = PyramidTop - PyramidBaseVerts[i];
        FVector SideNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
        
        // 确保法线指向外部（从金字塔中心向外）
        FVector CenterToVertex = (PyramidBaseVerts[i] + PyramidBaseVerts[NextI]) * 0.5f;
        if (FVector::DotProduct(SideNormal, CenterToVertex) < 0)
        {
            SideNormal = -SideNormal;
        }
        
        // 添加金字塔顶点
        int32 TopVertex = GetOrAddVertex(PyramidTop, SideNormal, FVector2D(0.5f, 1.0f));
        
        // 添加底面顶点
        int32 V1 = GetOrAddVertex(PyramidBaseVerts[i], SideNormal, FVector2D(static_cast<float>(i) / Params.Sides, 0.0f));
        int32 V2 = GetOrAddVertex(PyramidBaseVerts[NextI], SideNormal, FVector2D(static_cast<float>(NextI) / Params.Sides, 0.0f));
        
        // 添加三角形（确保正确的顶点顺序）
        AddTriangle(V1, V2, TopVertex);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePyramidSides - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FPyramidBuilder::GeneratePolygonFace(const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder, float UVOffsetZ)
{
    UE_LOG(LogTemp, Log, TEXT("FPyramidBuilder::GeneratePolygonFace - Generating polygon face with %d vertices"), PolygonVerts.Num());
    
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
            AddTriangle(V0, V1, V2);
        }
        else
        {
            AddTriangle(V0, V2, V1);
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
        
        // 确保法线指向外部
        FVector CenterToEdge = (BottomVerts[i] + BottomVerts[NextI] + TopVerts[i] + TopVerts[NextI]) * 0.25f;
        if (FVector::DotProduct(SideNormal, CenterToEdge) < 0)
        {
            SideNormal = -SideNormal;
        }
        
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
