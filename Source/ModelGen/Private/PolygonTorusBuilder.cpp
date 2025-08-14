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
    if (!ValidateParameters())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    ValidateAndClampParameters();

    GenerateVertices();
    GenerateTriangles();
    
    if (Params.TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps();
    }

    ApplySmoothing();

    if (!ValidateGeneratedData())
    {
        return false;
    }

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
    const float MajorRad = Params.MajorRadius;
    const float MinorRad = Params.MinorRadius;
    const int32 MajorSegs = Params.MajorSegments;
    const int32 MinorSegs = Params.MinorSegments;
    const float AngleRad = FMath::DegreesToRadians(Params.TorusAngle);
    
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
            
            // 计算UV坐标 - 基于顶点位置的稳定2U系统
            const FVector2D UV = GenerateStableUV(Position, Normal);
            
            // 添加顶点
            GetOrAddVertex(Position, Normal, UV);
        }
    }
}

void FPolygonTorusBuilder::GenerateTriangles()
{
    const int32 MajorSegs = Params.MajorSegments;
    const int32 MinorSegs = Params.MinorSegments;
    const bool bIsFullCircle = FMath::IsNearlyEqual(Params.TorusAngle, 360.0f);
    
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
}

void FPolygonTorusBuilder::GenerateEndCaps()
{
    // 直接使用高级端盖生成方式
    GenerateAdvancedEndCaps();
}

void FPolygonTorusBuilder::GenerateAdvancedEndCaps()
{
    // 如果是完整圆环，不需要端盖
    if (FMath::IsNearlyEqual(Params.TorusAngle, 360.0f))
    {
        return;
    }
    
    const float MajorRad = Params.MajorRadius;
    const float MinorRad = Params.MinorRadius;
    const int32 MinorSegs = Params.MinorSegments;
    const float AngleRad = FMath::DegreesToRadians(Params.TorusAngle);
    
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
    
    // 添加端面中心顶点 - 2U系统，避免重叠
    const int32 StartCenterIndex = GetOrAddVertex(StartCenter, StartNormal, FVector2D(0.5f, 1.0f));
    const int32 EndCenterIndex = GetOrAddVertex(EndCenter, EndNormal, FVector2D(0.5f, 2.0f));
    
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
}

void FPolygonTorusBuilder::ValidateAndClampParameters()
{
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
}

void FPolygonTorusBuilder::LogMeshStatistics()
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::LogMeshStatistics - Mesh statistics: %d vertices, %d triangles"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

void FPolygonTorusBuilder::ApplySmoothing()
{
    // 根据参数决定应用哪种平滑光照
    if (Params.bSmoothCrossSection)
    {
        ApplyHorizontalSmoothing();
    }
    
    if (Params.bSmoothVerticalSection)
    {
        ApplyVerticalSmoothing();
    }
}

void FPolygonTorusBuilder::ApplyHorizontalSmoothing()
{
    const int32 MajorSegs = Params.MajorSegments;
    const int32 MinorSegs = Params.MinorSegments;
    const int32 TotalVertices = MeshData.GetVertexCount();
    
    // 横面平滑光照：在径向方向（大圆环方向）上进行平滑
    // 这模拟了3ds Max中圆环在径向方向上的平滑光照效果
    
    // 对每个主分段（大圆环方向）的横面进行平滑
    for (int32 MajorIndex = 0; MajorIndex <= MajorSegs; ++MajorIndex)
    {
        const int32 SectionStartIndex = MajorIndex * MinorSegs;
        
        // 对当前横面的所有顶点进行平滑
        for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex)
        {
            const int32 CurrentVertexIndex = SectionStartIndex + MinorIndex;
            
            if (CurrentVertexIndex >= TotalVertices)
                continue;
            
            // 获取当前顶点位置和法线
            FVector CurrentPos = MeshData.Vertices[CurrentVertexIndex];
            FVector CurrentNormal = MeshData.Normals[CurrentVertexIndex];
            
            // 计算相邻顶点（在同一个横面上）
            FVector LeftPos, RightPos;
            int32 LeftIndex = SectionStartIndex + ((MinorIndex - 1 + MinorSegs) % MinorSegs);
            int32 RightIndex = SectionStartIndex + ((MinorIndex + 1) % MinorSegs);
            
            if (LeftIndex < TotalVertices && RightIndex < TotalVertices)
            {
                LeftPos = MeshData.Vertices[LeftIndex];
                RightPos = MeshData.Vertices[RightIndex];
                
                // 基于几何位置计算平滑法线
                // 在径向方向上计算切向量
                FVector RadialVector = (RightPos - LeftPos).GetSafeNormal();
                
                // 计算平滑后的法线（保持垂直于切向量的特性）
                FVector SmoothNormal = FVector::CrossProduct(RadialVector, FVector::UpVector).GetSafeNormal();
                
                // 确保方向与原始法线一致
                if (FVector::DotProduct(SmoothNormal, CurrentNormal) < 0.0f)
                {
                    SmoothNormal = -SmoothNormal;
                }
                
                // 应用平滑（混合原始法线和平滑法线）
                const float SmoothingStrength = 0.3f;  // 平滑强度
                FVector FinalNormal = FMath::Lerp(CurrentNormal, SmoothNormal, SmoothingStrength).GetSafeNormal();
                
                MeshData.Normals[CurrentVertexIndex] = FinalNormal;
            }
        }
    }
}

void FPolygonTorusBuilder::ApplyVerticalSmoothing()
{
    const int32 MajorSegs = Params.MajorSegments;
    const int32 MinorSegs = Params.MinorSegments;
    const int32 TotalVertices = MeshData.GetVertexCount();
    
    // 竖面平滑光照：在轴向方向（小圆环方向）上进行平滑
    // 这模拟了3ds Max中圆环在轴向方向上的平滑光照效果
    
    // 对每个次分段（小圆环方向）的竖面进行平滑
    for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex)
    {
        // 对当前竖面的所有顶点进行平滑
        for (int32 MajorIndex = 0; MajorIndex <= MajorSegs; ++MajorIndex)
        {
            const int32 CurrentVertexIndex = MajorIndex * MinorSegs + MinorIndex;
            
            if (CurrentVertexIndex >= TotalVertices)
                continue;
            
            // 获取当前顶点位置和法线
            FVector CurrentPos = MeshData.Vertices[CurrentVertexIndex];
            FVector CurrentNormal = MeshData.Normals[CurrentVertexIndex];
            
            // 计算相邻顶点（在同一个竖面上）
            FVector PrevPos, NextPos;
            int32 PrevIndex = ((MajorIndex - 1 + (MajorSegs + 1)) % (MajorSegs + 1)) * MinorSegs + MinorIndex;
            int32 NextIndex = ((MajorIndex + 1) % (MajorSegs + 1)) * MinorSegs + MinorIndex;
            
            if (PrevIndex < TotalVertices && NextIndex < TotalVertices)
            {
                PrevPos = MeshData.Vertices[PrevIndex];
                NextPos = MeshData.Vertices[NextIndex];
                
                // 基于几何位置计算平滑法线
                // 在轴向方向上计算切向量
                FVector AxialVector = (NextPos - PrevPos).GetSafeNormal();
                
                // 计算平滑后的法线（保持垂直于切向量的特性）
                FVector SmoothNormal = FVector::CrossProduct(AxialVector, FVector::RightVector).GetSafeNormal();
                
                // 确保方向与原始法线一致
                if (FVector::DotProduct(SmoothNormal, CurrentNormal) < 0.0f)
                {
                    SmoothNormal = -SmoothNormal;
                }
                
                // 应用平滑（混合原始法线和平滑法线）
                const float SmoothingStrength = 0.3f;  // 平滑强度
                FVector FinalNormal = FMath::Lerp(CurrentNormal, SmoothNormal, SmoothingStrength).GetSafeNormal();
                
                MeshData.Normals[CurrentVertexIndex] = FinalNormal;
            }
        }
    }
}

// 基于顶点位置的稳定UV映射 - 重写父类实现
FVector2D FPolygonTorusBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    // 基于顶点位置生成稳定的UV坐标，确保模型变化时UV不变
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    // 计算角度（基于位置）
    float MajorAngle = FMath::Atan2(Y, X);
    if (MajorAngle < 0) MajorAngle += 2.0f * PI;
    
    // 计算到中心的距离
    float DistanceFromCenter = FMath::Sqrt(X * X + Y * Y);
    float MinorRadius = DistanceFromCenter - Params.MajorRadius;
    
    // 计算次角度（基于Z坐标和次半径）
    float MinorAngle = FMath::Asin(Z / Params.MinorRadius);
    if (MinorRadius < 0) MinorAngle = PI - MinorAngle;
    
    // 2U系统：主圆环占据右侧区域 (1.0,0) 到 (2.0,1)
    float U = 1.0f + (MajorAngle / (2.0f * PI));  // 1.0 到 2.0
    float V = (MinorAngle + PI) / (2.0f * PI);     // 0.0 到 1.0
    
    return FVector2D(U, V);
}
