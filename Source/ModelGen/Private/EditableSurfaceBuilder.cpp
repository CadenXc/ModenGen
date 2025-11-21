// Copyright (c) 2024. All rights reserved.

#include "EditableSurfaceBuilder.h"
#include "EditableSurface.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"
#include "Components/SplineComponent.h"

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

    SplineComponent = Surface.SplineComponent;
    PathSampleCount = Surface.PathSampleCount;
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
    // Alpha: 0.0 到 1.0，表示在样条线上的位置（沿距离参数化）
    
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2)
    {
        return FVector::ZeroVector;
    }
    
    // 使用样条线的距离参数化（0.0 到 1.0）
    const float SplineLength = SplineComponent->GetSplineLength();
    const float Distance = Alpha * SplineLength;
    
    // 获取样条线上该距离处的世界位置，然后转换为本地空间
    FVector WorldPos = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
    FVector LocalPos = SplineComponent->GetComponentTransform().InverseTransformPosition(WorldPos);
    
    return LocalPos;
}

FVector FEditableSurfaceBuilder::GetPathTangent(float Alpha) const
{
    // 从样条线获取切线方向，用于确定横截面的方向
    
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2)
    {
        return FVector::ForwardVector;
    }
    
    // 使用样条线的距离参数化
    const float SplineLength = SplineComponent->GetSplineLength();
    const float Distance = Alpha * SplineLength;
    
    // 获取样条线上该距离处的切线方向（世界空间），然后转换为本地空间
    FVector WorldTangent = SplineComponent->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
    FVector LocalTangent = SplineComponent->GetComponentTransform().InverseTransformVectorNoScale(WorldTangent);
    
    LocalTangent.Normalize();
    
    if (LocalTangent.IsNearlyZero())
    {
        LocalTangent = FVector::ForwardVector;
    }
    
    return LocalTangent;
}

float FEditableSurfaceBuilder::GetPathWidth(float Alpha) const
{
    // 当前使用统一的SurfaceWidth，后续可以根据样条线点或路点插值
    // TODO: 可以基于样条线点的自定义数据或路点数组进行宽度插值
    return SurfaceWidth;
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
    // 1. 沿样条线采样点
    // 2. 在每个点生成横截面
    // 3. 连接相邻截面形成网格
    // 4. 处理平滑度和护坡
    
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2)
    {
        return;
    }
    
    const int32 PathSegments = PathSampleCount - 1; // 使用PathSampleCount分段
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

