// Copyright (c) 2024. All rights reserved.

#include "FrustumParameters.h"

bool FFrustumParameters::IsValid() const
{
    // 检查基础几何参数
    if (TopRadius <= 0.0f || BottomRadius <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    // 检查细分参数
    if (Sides < 3 || HeightSegments < 1)
    {
        return false;
    }

    // 检查顶部和底部边数参数
    if (TopSides < 3 || BottomSides < 3)
    {
        return false;
    }

    // 检查倒角参数
    if (BevelRadius < 0.0f || BevelSections < 1)
    {
        return false;
    }

    // 检查弯曲参数
    if (MinBendRadius < 0.0f)
    {
        return false;
    }

    // 检查角度参数
    if (ArcAngle <= 0.0f || ArcAngle > 360.0f)
    {
        return false;
    }

    // 检查端面参数
    if (CapThickness < 0.0f)
    {
        return false;
    }

    return true;
}

int32 FFrustumParameters::CalculateVertexCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    // 基础顶点：顶部和底面的顶点
    int32 BaseVertices = TopSides + BottomSides;
    
    // 侧面顶点：高度分段数 * 最大边数（用于插值）
    int32 MaxSides = FMath::Max(TopSides, BottomSides);
    int32 SideVertices = HeightSegments * MaxSides;
    
    // 倒角顶点：如果有倒角，每个边缘需要额外的顶点
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        // 顶部和底面的倒角顶点
        BevelVertices = TopSides * BevelSections + BottomSides * BevelSections;
        // 侧面的倒角顶点
        BevelVertices += HeightSegments * MaxSides * BevelSections;
    }
    
    // 端面顶点：如果有端面厚度
    int32 CapVertices = 0;
    if (CapThickness > 0.0f)
    {
        CapVertices = TopSides + BottomSides; // 顶部和底面的端面中心点
    }

    return BaseVertices + SideVertices + BevelVertices + CapVertices;
}

int32 FFrustumParameters::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    // 基础三角形：顶部和底面的三角形
    int32 BaseTriangles = TopSides + BottomSides;
    
    // 侧面三角形：高度分段数 * 最大边数 * 2（每个四边形分解为两个三角形）
    int32 MaxSides = FMath::Max(TopSides, BottomSides);
    int32 SideTriangles = HeightSegments * MaxSides * 2;
    
    // 倒角三角形：如果有倒角，每个边缘需要额外的三角形
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        // 顶部和底面的倒角三角形
        BevelTriangles = TopSides * BevelSections * 2 + BottomSides * BevelSections * 2;
        // 侧面的倒角三角形
        BevelTriangles += HeightSegments * MaxSides * BevelSections * 2;
    }
    
    // 端面三角形：如果有端面厚度
    int32 CapTriangles = 0;
    if (CapThickness > 0.0f)
    {
        CapTriangles = TopSides + BottomSides; // 顶部和底面的端面三角形
    }

    return BaseTriangles + SideTriangles + BevelTriangles + CapTriangles;
}

void FFrustumParameters::PostEditChangeProperty(const FName& PropertyName)
{
    UE_LOG(LogTemp, Log, TEXT("FFrustumParameters::PostEditChangeProperty - Property changed: %s"), *PropertyName.ToString());
}

bool FFrustumParameters::operator==(const FFrustumParameters& Other) const
{
    return TopRadius == Other.TopRadius &&
           BottomRadius == Other.BottomRadius &&
           Height == Other.Height &&
           Sides == Other.Sides &&
           TopSides == Other.TopSides &&
           BottomSides == Other.BottomSides &&
           HeightSegments == Other.HeightSegments &&
           BevelRadius == Other.BevelRadius &&
           BevelSections == Other.BevelSections &&
           BendAmount == Other.BendAmount &&
           MinBendRadius == Other.MinBendRadius &&
           ArcAngle == Other.ArcAngle &&
           CapThickness == Other.CapThickness &&
           bFlipNormals == Other.bFlipNormals &&
           bDisableDebounce == Other.bDisableDebounce;
}

bool FFrustumParameters::operator!=(const FFrustumParameters& Other) const
{
    return !(*this == Other);
}
