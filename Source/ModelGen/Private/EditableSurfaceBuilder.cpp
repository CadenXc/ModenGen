// Copyright (c) 2024. All rights reserved.

#include "EditableSurfaceBuilder.h"
#include "EditableSurface.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"

FEditableSurfaceBuilder::FEditableSurfaceBuilder(const AEditableSurface& InSurface)
    : Surface(InSurface)
{
    Clear();
}

void FEditableSurfaceBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    FrontCrossSections.Empty();
    FrontVertexStartIndex = 0;
    FrontVertexCount = 0;
}

bool FEditableSurfaceBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Surface.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    Waypoints = Surface.Waypoints;
    SurfaceWidth = Surface.SurfaceWidth;
    bEnableThickness = Surface.bEnableThickness;
    ThicknessValue = Surface.ThicknessValue;
    SideSmoothness = Surface.SideSmoothness;
    RightSlopeLength = Surface.RightSlopeLength;
    RightSlopeGradient = Surface.RightSlopeGradient;
    LeftSlopeLength = Surface.LeftSlopeLength;
    LeftSlopeGradient = Surface.LeftSlopeGradient;

    GenerateSurfaceMesh();
    
    if (bEnableThickness)
    {
        GenerateThickness();
    }

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;
    return true;
}

int32 FEditableSurfaceBuilder::CalculateVertexCountEstimate() const
{
    return Surface.CalculateVertexCountEstimate();
}

int32 FEditableSurfaceBuilder::CalculateTriangleCountEstimate() const
{
    return Surface.CalculateTriangleCountEstimate();
}

FVector FEditableSurfaceBuilder::InterpolatePathPoint(float Alpha) const
{
    // TODO: 实现Catmull-Rom样条插值
    // Alpha: 0.0 到 1.0，表示在路径上的位置
    
    if (Waypoints.Num() < 2)
    {
        return FVector::ZeroVector;
    }
    
    if (Waypoints.Num() == 2)
    {
        return FMath::Lerp(Waypoints[0].Position, Waypoints[1].Position, Alpha);
    }
    
    // 简单的线性插值（后续可以改为Catmull-Rom）
    const float ScaledAlpha = Alpha * (Waypoints.Num() - 1);
    const int32 Index = FMath::FloorToInt(ScaledAlpha);
    const float LocalAlpha = ScaledAlpha - Index;
    
    if (Index >= Waypoints.Num() - 1)
    {
        return Waypoints.Last().Position;
    }
    
    return FMath::Lerp(Waypoints[Index].Position, Waypoints[Index + 1].Position, LocalAlpha);
}

FVector FEditableSurfaceBuilder::GetPathTangent(float Alpha) const
{
    // 计算路径切线方向，用于确定横截面的方向
    
    if (Waypoints.Num() < 2)
    {
        return FVector::ForwardVector;
    }
    
    // 使用更小的Epsilon来获得更精确的切线
    const float Epsilon = 0.001f;
    float Alpha0 = FMath::Max(0.0f, Alpha - Epsilon);
    float Alpha1 = FMath::Min(1.0f, Alpha + Epsilon);
    
    // 如果Alpha在端点，使用单侧差分
    if (Alpha <= Epsilon)
    {
        Alpha0 = 0.0f;
        Alpha1 = FMath::Min(Epsilon * 2.0f, 1.0f);
    }
    else if (Alpha >= 1.0f - Epsilon)
    {
        Alpha0 = FMath::Max(1.0f - Epsilon * 2.0f, 0.0f);
        Alpha1 = 1.0f;
    }
    
    FVector P0 = InterpolatePathPoint(Alpha0);
    FVector P1 = InterpolatePathPoint(Alpha1);
    
    FVector Tangent = (P1 - P0).GetSafeNormal();
    if (Tangent.IsNearlyZero())
    {
        // 如果切线为零，尝试使用相邻路点的方向
        if (Waypoints.Num() >= 2)
        {
            const float ScaledAlpha = Alpha * (Waypoints.Num() - 1);
            const int32 Index = FMath::Clamp(FMath::FloorToInt(ScaledAlpha), 0, Waypoints.Num() - 2);
            Tangent = (Waypoints[Index + 1].Position - Waypoints[Index].Position).GetSafeNormal();
        }
        
        if (Tangent.IsNearlyZero())
        {
            Tangent = FVector::ForwardVector;
        }
    }
    
    return Tangent;
}

float FEditableSurfaceBuilder::GetPathWidth(float Alpha) const
{
    // 根据Alpha插值计算宽度
    if (Waypoints.Num() < 2)
    {
        return SurfaceWidth;
    }
    
    const float ScaledAlpha = Alpha * (Waypoints.Num() - 1);
    const int32 Index = FMath::FloorToInt(ScaledAlpha);
    const float LocalAlpha = ScaledAlpha - Index;
    
    if (Index >= Waypoints.Num() - 1)
    {
        return Waypoints.Last().Width;
    }
    
    return FMath::Lerp(Waypoints[Index].Width, Waypoints[Index + 1].Width, LocalAlpha);
}

TArray<int32> FEditableSurfaceBuilder::GenerateCrossSection(const FVector& Center, const FVector& Forward, 
                                                             const FVector& Up, float Width, int32 Segments)
{
    // 生成横截面顶点
    // Center: 截面中心点
    // Forward: 路径前进方向
    // Up: 上方向
    // Width: 截面宽度
    // Segments: 分段数（考虑平滑度）
    
    TArray<int32> CrossSectionIndices;
    CrossSectionIndices.Reserve(Segments + 1);
    
    // 计算右方向（Forward × Up）
    FVector Right = FVector::CrossProduct(Forward, Up).GetSafeNormal();
    
    // 如果叉积结果为零，使用默认右方向
    if (Right.IsNearlyZero())
    {
        // 如果Forward和Up平行，使用世界空间的右方向
        if (FMath::Abs(Forward.Z) > 0.9f)
        {
            Right = FVector::RightVector;
        }
        else
        {
            Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
            if (Right.IsNearlyZero())
            {
                Right = FVector::RightVector;
            }
        }
    }
    
    // 生成截面顶点
    for (int32 i = 0; i <= Segments; ++i)
    {
        float Alpha = static_cast<float>(i) / Segments;
        float Offset = (Alpha - 0.5f) * Width; // -Width/2 到 +Width/2
        
        FVector Pos = Center + Right * Offset;
        FVector Normal = Up; // 默认法线向上
        FVector2D UV(Alpha * ModelGenConstants::GLOBAL_UV_SCALE, 0.0f);
        
        CrossSectionIndices.Add(GetOrAddVertex(Pos, Normal, UV));
    }
    
    return CrossSectionIndices;
}

void FEditableSurfaceBuilder::GenerateSurfaceMesh()
{
    // 实现曲面网格生成逻辑
    // 1. 沿路径采样点
    // 2. 在每个点生成横截面
    // 3. 连接相邻截面形成网格
    // 4. 处理平滑度和护坡
    
    if (Waypoints.Num() < 2)
    {
        return;
    }
    
    const int32 PathSegments = FMath::Max(1, Waypoints.Num() - 1) * 4; // 每个路点之间4个分段
    const int32 WidthSegments = FMath::Max(2, SideSmoothness + 1);
    
    // 记录前面顶点起始索引
    FrontVertexStartIndex = MeshData.Vertices.Num();
    
    // 保存横截面数据用于厚度生成
    FrontCrossSections.Empty();
    FrontCrossSections.Reserve(PathSegments + 1);
    
    // 生成所有横截面
    for (int32 p = 0; p <= PathSegments; ++p)
    {
        float Alpha = static_cast<float>(p) / PathSegments;
        FVector Center = InterpolatePathPoint(Alpha);
        FVector Forward = GetPathTangent(Alpha);
        FVector Up = FVector::UpVector; // 默认向上
        float Width = GetPathWidth(Alpha);
        
        TArray<int32> CrossSection = GenerateCrossSection(Center, Forward, Up, Width, WidthSegments);
        FrontCrossSections.Add(CrossSection);
    }
    
    // 连接相邻截面（确保从外部看是逆时针顺序）
    for (int32 p = 0; p < PathSegments; ++p)
    {
        const TArray<int32>& SectionA = FrontCrossSections[p];
        const TArray<int32>& SectionB = FrontCrossSections[p + 1];
        
        for (int32 w = 0; w < WidthSegments; ++w)
        {
            int32 V0 = SectionA[w];
            int32 V1 = SectionA[w + 1];
            int32 V2 = SectionB[w];
            int32 V3 = SectionB[w + 1];
            
            // 第一个三角形：V0 -> V2 -> V1 (逆时针，从上面看，法线向上)
            AddTriangle(V0, V2, V1);
            // 第二个三角形：V1 -> V2 -> V3 (逆时针，从上面看，法线向上)
            AddTriangle(V1, V2, V3);
        }
    }
    
    // 记录前面顶点数量
    FrontVertexCount = MeshData.Vertices.Num() - FrontVertexStartIndex;
}

void FEditableSurfaceBuilder::GenerateThickness()
{
    // 实现厚度生成逻辑
    // 1. 复制前面顶点并沿法线方向偏移生成背面
    // 2. 生成侧面连接前面和背面
    // 3. 生成两个端面（起始端和结束端）
    
    if (FrontCrossSections.Num() < 2)
    {
        return;
    }
    
    const int32 NumSections = FrontCrossSections.Num();
    const int32 WidthSegments = FrontCrossSections[0].Num() - 1;
    
    // 1. 生成背面顶点（沿法线方向向下偏移）
    TArray<TArray<int32>> BackCrossSections;
    BackCrossSections.Reserve(NumSections);
    
    for (int32 s = 0; s < NumSections; ++s)
    {
        TArray<int32> BackSection;
        BackSection.Reserve(FrontCrossSections[s].Num());
        
        for (int32 v = 0; v < FrontCrossSections[s].Num(); ++v)
        {
            int32 FrontVertexIdx = FrontCrossSections[s][v];
            FVector FrontPos = GetPosByIndex(FrontVertexIdx);
            FVector FrontNormal = MeshData.Normals[FrontVertexIdx];
            
            // 沿法线方向向下偏移（法线向上，所以向下是 -Normal）
            FVector BackPos = FrontPos - FrontNormal * ThicknessValue;
            FVector BackNormal = -FrontNormal; // 背面法线相反
            
            // 使用相同的UV
            FVector2D UV = MeshData.UVs[FrontVertexIdx];
            
            BackSection.Add(AddVertex(BackPos, BackNormal, UV));
        }
        
        BackCrossSections.Add(BackSection);
    }
    
    // 2. 生成侧面（连接前面和背面）
    for (int32 s = 0; s < NumSections - 1; ++s)
    {
        const TArray<int32>& FrontSectionA = FrontCrossSections[s];
        const TArray<int32>& FrontSectionB = FrontCrossSections[s + 1];
        const TArray<int32>& BackSectionA = BackCrossSections[s];
        const TArray<int32>& BackSectionB = BackCrossSections[s + 1];
        
        // 连接每个宽度分段
        for (int32 w = 0; w < WidthSegments; ++w)
        {
            int32 F0 = FrontSectionA[w];
            int32 F1 = FrontSectionA[w + 1];
            int32 F2 = FrontSectionB[w];
            int32 F3 = FrontSectionB[w + 1];
            
            int32 B0 = BackSectionA[w];
            int32 B1 = BackSectionA[w + 1];
            int32 B2 = BackSectionB[w];
            int32 B3 = BackSectionB[w + 1];
            
            // 上边缘（前面）：F0 -> F2 -> F1, F1 -> F2 -> F3 (已在前面的GenerateSurfaceMesh中生成)
            
            // 下边缘（背面）：B0 -> B1 -> B2, B1 -> B3 -> B2 (逆时针，从背面看)
            AddTriangle(B0, B1, B2);
            AddTriangle(B1, B3, B2);
            
            // 左侧面：F0 -> B0 -> F2, F2 -> B0 -> B2
            AddTriangle(F0, B0, F2);
            AddTriangle(F2, B0, B2);
            
            // 右侧面：F1 -> F3 -> B1, F3 -> B3 -> B1
            AddTriangle(F1, F3, B1);
            AddTriangle(F3, B3, B1);
        }
    }
    
    // 3. 生成起始端面
    if (NumSections > 0)
    {
        const TArray<int32>& FrontStart = FrontCrossSections[0];
        const TArray<int32>& BackStart = BackCrossSections[0];
        
        for (int32 w = 0; w < WidthSegments; ++w)
        {
            int32 F0 = FrontStart[w];
            int32 F1 = FrontStart[w + 1];
            int32 B0 = BackStart[w];
            int32 B1 = BackStart[w + 1];
            
            // 起始端面：F0 -> B1 -> B0, F0 -> F1 -> B1 (逆时针，从起始端看)
            AddTriangle(F0, B1, B0);
            AddTriangle(F0, F1, B1);
        }
    }
    
    // 4. 生成结束端面
    if (NumSections > 0)
    {
        const TArray<int32>& FrontEnd = FrontCrossSections[NumSections - 1];
        const TArray<int32>& BackEnd = BackCrossSections[NumSections - 1];
        
        for (int32 w = 0; w < WidthSegments; ++w)
        {
            int32 F0 = FrontEnd[w];
            int32 F1 = FrontEnd[w + 1];
            int32 B0 = BackEnd[w];
            int32 B1 = BackEnd[w + 1];
            
            // 结束端面：F1 -> F0 -> B0, F1 -> B0 -> B1 (逆时针，从结束端看)
            AddTriangle(F1, F0, B0);
            AddTriangle(F1, B0, B1);
        }
    }
}

