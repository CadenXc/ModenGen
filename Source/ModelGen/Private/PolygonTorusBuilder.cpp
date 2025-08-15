// Copyright (c) 2024. All rights reserved.

#include "PolygonTorusBuilder.h"
#include "PolygonTorus.h"
#include "ModelGenMeshData.h"

FPolygonTorusBuilder::FPolygonTorusBuilder(const APolygonTorus& InPolygonTorus)
    : PolygonTorus(InPolygonTorus)
{
    Clear();
    ValidateAndClampParameters();
}

bool FPolygonTorusBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!ValidateParameters())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    GenerateVertices();
    GenerateTriangles();

    if (PolygonTorus.TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
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
    return PolygonTorus.IsValid();
}

int32 FPolygonTorusBuilder::CalculateVertexCountEstimate() const
{
    return PolygonTorus.CalculateVertexCountEstimate();
}

int32 FPolygonTorusBuilder::CalculateTriangleCountEstimate() const
{
    return PolygonTorus.CalculateTriangleCountEstimate();
}

void FPolygonTorusBuilder::GenerateVertices()
{
    const float MajorRad = PolygonTorus.MajorRadius;
    const float MinorRad = PolygonTorus.MinorRadius;
    const int32 MajorSegs = PolygonTorus.MajorSegments;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const float AngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);
    
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
            
            // 添加顶点 - 使用双UV通道
            GetOrAddVertexWithDualUV(Position, Normal);
        }
    }
}

void FPolygonTorusBuilder::GenerateTriangles()
{
    const int32 MajorSegs = PolygonTorus.MajorSegments;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const bool bIsFullCircle = FMath::IsNearlyEqual(PolygonTorus.TorusAngle, 360.0f);
    
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
    if (FMath::IsNearlyEqual(PolygonTorus.TorusAngle, 360.0f))
    {
        return;
    }
    
    const float MajorRad = PolygonTorus.MajorRadius;
    const float MinorRad = PolygonTorus.MinorRadius;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const float AngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);
    
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
            const int32 StartCenterIndex = GetOrAddVertexWithDualUV(StartCenter, StartNormal);
        const int32 EndCenterIndex = GetOrAddVertexWithDualUV(EndCenter, EndNormal);
    
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
    const int32 LastSectionStart = PolygonTorus.MajorSegments * MinorSegs;
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
    // 验证参数但不修改，只记录警告
    if (PolygonTorus.MajorSegments < 3 || PolygonTorus.MajorSegments > 256)
    {
        UE_LOG(LogTemp, Warning, TEXT("主分段数 %d 超出有效范围 [3, 256]，建议调整"), PolygonTorus.MajorSegments);
    }
    
    if (PolygonTorus.MinorSegments < 3 || PolygonTorus.MinorSegments > 256)
    {
        UE_LOG(LogTemp, Warning, TEXT("次分段数 %d 超出有效范围 [3, 256]，建议调整"), PolygonTorus.MinorSegments);
    }
    
    if (PolygonTorus.MajorRadius < 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("主半径 %f 小于最小值 1.0，建议调整"), PolygonTorus.MajorRadius);
    }
    
    if (PolygonTorus.MinorRadius < 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("次半径 %f 小于最小值 1.0，建议调整"), PolygonTorus.MinorRadius);
    }
    
    if (PolygonTorus.MinorRadius > PolygonTorus.MajorRadius * 0.9f)
    {
        UE_LOG(LogTemp, Warning, TEXT("次半径 %f 超过主半径的 90%% (%f)，建议调整"), 
               PolygonTorus.MinorRadius, PolygonTorus.MajorRadius * 0.9f);
    }
    
    if (PolygonTorus.TorusAngle < 1.0f || PolygonTorus.TorusAngle > 360.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("圆环角度 %f 超出有效范围 [1.0, 360.0]，建议调整"), PolygonTorus.TorusAngle);
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
    if (PolygonTorus.bSmoothCrossSection)
    {
        ApplyHorizontalSmoothing();
    }
    
    if (PolygonTorus.bSmoothVerticalSection)
    {
        ApplyVerticalSmoothing();
    }
}

void FPolygonTorusBuilder::ApplyHorizontalSmoothing()
{
    const int32 MajorSegs = PolygonTorus.MajorSegments;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
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
    const int32 MajorSegs = PolygonTorus.MajorSegments;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
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
    float MinorRadius = DistanceFromCenter - PolygonTorus.MajorRadius;
    
    // 计算次角度（基于Z坐标和次半径）
    float MinorAngle = FMath::Asin(Z / PolygonTorus.MinorRadius);
    if (MinorRadius < 0) MinorAngle = PI - MinorAngle;
    
    // 真正的2U系统：主圆环占据整个UV空间 (0.0,0) 到 (1.0,1)
    float U = MajorAngle / (2.0f * PI);            // 0.0 到 1.0
    float V = (MinorAngle + PI) / (2.0f * PI);     // 0.0 到 1.0
    
    return FVector2D(U, V);
}

FVector2D FPolygonTorusBuilder::GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const
{
    // 第二UV通道：使用传统UV系统（0-1范围）
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    // 计算角度（基于位置）
    float MajorAngle = FMath::Atan2(Y, X);
    if (MajorAngle < 0) MajorAngle += 2.0f * PI;
    
    // 计算到中心的距离
    float DistanceFromCenter = FMath::Sqrt(X * X + Y * Y);
    float MinorRadius = DistanceFromCenter - PolygonTorus.MajorRadius;
    
    // 计算次角度（基于Z坐标和次半径）
    float MinorAngle = FMath::Asin(Z / PolygonTorus.MinorRadius);
    if (MinorRadius < 0) MinorAngle = PI - MinorAngle;
    
    // 传统UV系统：使用不同的映射策略避免重叠
    float U = MajorAngle / (2.0f * PI);                    // 0.0 到 1.0
    float V = 0.2f + (MinorAngle + PI) / (2.0f * PI) * 0.6f;  // 0.2 到 0.8
    
    return FVector2D(U, V);
}

int32 FPolygonTorusBuilder::GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal)
{
    FVector2D MainUV = GenerateStableUVCustom(Pos, Normal);
    FVector2D SecondaryUV = GenerateSecondaryUV(Pos, Normal);
    
    return FModelGenMeshBuilder::GetOrAddVertexWithDualUV(Pos, Normal, MainUV, SecondaryUV);
}
