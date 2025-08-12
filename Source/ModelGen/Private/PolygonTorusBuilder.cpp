// Copyright (c) 2024. All rights reserved.

#include "PolygonTorusBuilder.h"
#include "PolygonTorusParameters.h"
#include "ModelGenMeshData.h"

FPolygonTorusBuilder::FPolygonTorusBuilder(const FPolygonTorusParameters& InParams)
    : Params(InParams)
{
    Clear();
}

bool FPolygonTorusBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Starting generation"));
    
    // 验证生成参数
    if (!ValidateParameters())
    {
        UE_LOG(LogTemp, Error, TEXT("无效的圆环生成参数"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Parameters validated successfully"));

    // 清除之前的几何数据
    Clear();
    ReserveMemory();

    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Memory reserved"));

    // 验证和修正参数
    ValidateAndClampParameters();

    // 按照以下顺序生成几何体：
    // 1. 生成顶点
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Generating vertices"));
    GenerateVertices();
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Vertices generated, current vertices: %d"), MeshData.GetVertexCount());
    
    // 2. 生成三角形
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Generating triangles"));
    GenerateTriangles();
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Triangles generated, current triangles: %d"), MeshData.GetTriangleCount());
    
    // 3. 生成端盖（当TorusAngle < 360时自动生成）
    if (Params.TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Generating end caps (TorusAngle < 360)"));
        GenerateEndCaps();
        UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - End caps generated, current vertices: %d"), MeshData.GetVertexCount());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Skipping end caps (TorusAngle = 360)"));
    }

    // 验证生成的网格数据
    if (!ValidateGeneratedData())
    {
        UE_LOG(LogTemp, Error, TEXT("生成的圆环网格数据无效"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::Generate - Final mesh data: %d vertices, %d triangles"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());

    // 输出网格数据
    OutMeshData = MeshData;
    return true;
}

bool FPolygonTorusBuilder::ValidateParameters() const
{
    return Params.IsValid();
}

int32 FPolygonTorusBuilder::CalculateVertexCountEstimate() const
{
    return Params.CalculateVertexCountEstimate();
}

int32 FPolygonTorusBuilder::CalculateTriangleCountEstimate() const
{
    return Params.CalculateTriangleCountEstimate();
}

void FPolygonTorusBuilder::GenerateVertices()
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateVertices - Starting vertex generation"));
    
    const float MajorRad = Params.MajorRadius;
    const float MinorRad = Params.MinorRadius;
    const int32 MajorSegs = Params.MajorSegments;
    const int32 MinorSegs = Params.MinorSegments;
    const float AngleRad = FMath::DegreesToRadians(Params.TorusAngle);
    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateOptimizedVertices - Parameters: MajorRad=%f, MinorRad=%f, MajorSegs=%d, MinorSegs=%d, AngleRad=%f"), 
           MajorRad, MinorRad, MajorSegs, MinorSegs, AngleRad);
    
    // 计算角度步长
    const float MajorAngleStep = AngleRad / MajorSegs;
    const float MinorAngleStep = 2.0f * PI / MinorSegs;
    
    // 生成顶点
    for (int32 MajorIndex = 0; MajorIndex <= MajorSegs; ++MajorIndex)
    {
        const float MajorAngle = MajorIndex * MajorAngleStep;
        const float MajorCos = FMath::Cos(MajorAngle);
        const float MajorSin = FMath::Sin(MajorAngle);
        
        for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex)
        {
            const float MinorAngle = MinorIndex * MinorAngleStep;
            const float MinorCos = FMath::Cos(MinorAngle);
            const float MinorSin = FMath::Sin(MinorAngle);
            
            // 计算顶点位置
            const float X = (MajorRad + MinorRad * MinorCos) * MajorCos;
            const float Y = (MajorRad + MinorRad * MinorCos) * MajorSin;
            const float Z = MinorRad * MinorSin;
            
            const FVector Position(X, Y, Z);
            
            // 计算法线
            const FVector Normal = FVector(MinorCos * MajorCos, MinorCos * MajorSin, MinorSin).GetSafeNormal();
            
            // 计算UV坐标
            const float U = static_cast<float>(MajorIndex) / MajorSegs;
            const float V = static_cast<float>(MinorIndex) / MinorSegs;
            const FVector2D UV(U, V);
            
            // 添加顶点
            GetOrAddVertex(Position, Normal, UV);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateVertices - Completed, generated %d vertices"), MeshData.GetVertexCount());
}

void FPolygonTorusBuilder::GenerateTriangles()
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateTriangles - Starting triangle generation"));
    
    const int32 MajorSegs = Params.MajorSegments;
    const int32 MinorSegs = Params.MinorSegments;
    const bool bIsFullCircle = FMath::IsNearlyEqual(Params.TorusAngle, 360.0f);
    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateOptimizedTriangles - Parameters: MajorSegs=%d, MinorSegs=%d, bIsFullCircle=%s"), 
           MajorSegs, MinorSegs, bIsFullCircle ? TEXT("true") : TEXT("false"));
    
    // 生成三角形
    for (int32 MajorIndex = 0; MajorIndex < MajorSegs; ++MajorIndex)
    {
        for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex)
        {
            const int32 CurrentMajor = MajorIndex;
            const int32 NextMajor = (MajorIndex + 1) % (MajorSegs + 1);
            const int32 CurrentMinor = MinorIndex;
            const int32 NextMinor = (MinorIndex + 1) % MinorSegs;
            
            // 计算四个顶点的索引
            const int32 V0 = CurrentMajor * MinorSegs + CurrentMinor;
            const int32 V1 = CurrentMajor * MinorSegs + NextMinor;
            const int32 V2 = NextMajor * MinorSegs + NextMinor;
            const int32 V3 = NextMajor * MinorSegs + CurrentMinor;
            
            // 添加两个三角形组成一个四边形
            AddTriangle(V0, V1, V2);
            AddTriangle(V0, V2, V3);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateTriangles - Completed, generated %d triangles"), MeshData.GetTriangleCount());
}

void FPolygonTorusBuilder::GenerateEndCaps()
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateEndCaps - Starting end caps generation"));
    
    // 直接使用高级端盖生成方式
    GenerateAdvancedEndCaps();
    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateEndCaps - Completed"));
}

void FPolygonTorusBuilder::GenerateAdvancedEndCaps()
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateAdvancedEndCaps - Starting advanced end caps generation"));
    
    // 如果是完整圆环，不需要端盖
    if (FMath::IsNearlyEqual(Params.TorusAngle, 360.0f))
    {
        UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateAdvancedEndCaps - Full circle torus, skipping end caps"));
        return;
    }
    
    const float MajorRad = Params.MajorRadius;
    const float MinorRad = Params.MinorRadius;
    const int32 MinorSegs = Params.MinorSegments;
    const float AngleRad = FMath::DegreesToRadians(Params.TorusAngle);
    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateAdvancedEndCaps - Parameters: MajorRad=%f, MinorRad=%f, MinorSegs=%d, AngleRad=%f"), 
           MajorRad, MinorRad, MinorSegs, AngleRad);
    
    // 计算起始和结束角度
    const float StartAngle = 0.0f;
    const float EndAngle = AngleRad;
    
    // 计算端面中心点
    const FVector StartCenter(
        FMath::Cos(StartAngle) * MajorRad,
        FMath::Sin(StartAngle) * MajorRad,
        0.0f
    );
    
    const FVector EndCenter(
        FMath::Cos(EndAngle) * MajorRad,
        FMath::Sin(EndAngle) * MajorRad,
        0.0f
    );
    
    // 计算端面法线（指向圆环内部）
    const FVector StartNormal = FVector(-FMath::Sin(StartAngle), FMath::Cos(StartAngle), 0.0f);
    const FVector EndNormal = FVector(-FMath::Sin(EndAngle), FMath::Cos(EndAngle), 0.0f);
    
    // 添加端面中心顶点
    const int32 StartCenterIndex = GetOrAddVertex(StartCenter, StartNormal, FVector2D(0.5f, 0.5f));
    const int32 EndCenterIndex = GetOrAddVertex(EndCenter, EndNormal, FVector2D(0.5f, 0.5f));
    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateAdvancedEndCaps - Added center vertices: start=%d, end=%d"), 
           StartCenterIndex, EndCenterIndex);
    
    // 生成端盖三角形 - 直接使用现有的边缘顶点
    // 起始端盖：使用第一个截面的所有顶点
    for (int32 i = 0; i < MinorSegs; ++i)
    {
        const int32 NextI = (i + 1) % MinorSegs;
        
        const int32 V0 = StartCenterIndex;
        const int32 V1 = NextI;
        const int32 V2 = i;
        AddTriangle(V0, V1, V2);
    }
    
    // 结束端盖：使用最后一个截面的所有顶点
    const int32 LastSectionStart = Params.MajorSegments * MinorSegs;
    for (int32 i = 0; i < MinorSegs; ++i)
    {
        const int32 NextI = (i + 1) % MinorSegs;
        
        const int32 V0 = EndCenterIndex;
        const int32 V1 = LastSectionStart + i;
        const int32 V2 = LastSectionStart + NextI;
        AddTriangle(V0, V1, V2);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::GenerateAdvancedEndCaps - Completed, total vertices: %d, triangles: %d"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FPolygonTorusBuilder::ValidateAndClampParameters()
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::ValidateAndClampParameters - Starting parameter validation"));
    
    // 约束分段数
    const int32 OldMajorSegments = Params.MajorSegments;
    const int32 OldMinorSegments = Params.MinorSegments;
    Params.MajorSegments = FMath::Clamp(Params.MajorSegments, 3, 256);
    Params.MinorSegments = FMath::Clamp(Params.MinorSegments, 3, 256);
    
    if (OldMajorSegments != Params.MajorSegments)
    {
        UE_LOG(LogTemp, Warning, TEXT("主分段数已从 %d 调整为 %d"), OldMajorSegments, Params.MajorSegments);
    }
    
    if (OldMinorSegments != Params.MinorSegments)
    {
        UE_LOG(LogTemp, Warning, TEXT("次分段数已从 %d 调整为 %d"), OldMinorSegments, Params.MinorSegments);
    }
    
    // 约束半径
    const float OldMajorRadius = Params.MajorRadius;
    const float OldMinorRadius = Params.MinorRadius;
    Params.MajorRadius = FMath::Max(Params.MajorRadius, 1.0f);
    Params.MinorRadius = FMath::Clamp(Params.MinorRadius, 1.0f, Params.MajorRadius * 0.9f);
    
    if (OldMajorRadius != Params.MajorRadius)
    {
        UE_LOG(LogTemp, Warning, TEXT("主半径已从 %f 调整为 %f"), OldMajorRadius, Params.MajorRadius);
    }
    
    if (OldMinorRadius != Params.MinorRadius)
    {
        UE_LOG(LogTemp, Warning, TEXT("次半径已从 %f 调整为 %f"), OldMinorRadius, Params.MinorRadius);
    }
    
    // 约束角度
    const float OldTorusAngle = Params.TorusAngle;
    Params.TorusAngle = FMath::Clamp(Params.TorusAngle, 1.0f, 360.0f);
    
    if (OldTorusAngle != Params.TorusAngle)
    {
        UE_LOG(LogTemp, Warning, TEXT("圆环角度已从 %f 调整为 %f"), OldTorusAngle, Params.TorusAngle);
    }
    

    
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::ValidateAndClampParameters - Completed"));
}

void FPolygonTorusBuilder::LogMeshStatistics()
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::LogMeshStatistics - Mesh statistics: %d vertices, %d triangles"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}
