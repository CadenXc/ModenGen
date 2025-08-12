// Copyright (c) 2024. All rights reserved.

#include "HollowPrismParameters.h"

bool FHollowPrismParameters::IsValid() const
{
    // 检查基础几何参数
    if (InnerRadius <= 0.0f || OuterRadius <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    // 检查内外半径关系
    if (InnerRadius >= OuterRadius)
    {
        return false;
    }

    // 检查边数
    if (Sides < 3 || InnerSides < 3 || OuterSides < 3)
    {
        return false;
    }

    // 检查弧角
    if (ArcAngle <= 0.0f || ArcAngle > 360.0f)
    {
        return false;
    }

    // 检查倒角参数
    if (BevelRadius < 0.0f || BevelSegments < 1)
    {
        return false;
    }

    return true;
}

float FHollowPrismParameters::GetWallThickness() const
{
    return OuterRadius - InnerRadius;
}

bool FHollowPrismParameters::IsFullCircle() const
{
    return FMath::IsNearlyEqual(ArcAngle, 360.0f, 0.1f);
}

int32 FHollowPrismParameters::CalculateVertexCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    // 基础顶点：内外环的顶点
    int32 BaseVertices = InnerSides + OuterSides;
    
    // 倒角顶点：如果有倒角，每个边缘需要额外的顶点
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        // 顶部和底面的倒角顶点
        BevelVertices = (InnerSides + OuterSides) * BevelSegments * 2; // 内外环各2个
    }
    
    // 端面顶点：如果不是完整圆环
    int32 EndCapVertices = 0;
    if (!IsFullCircle())
    {
        EndCapVertices = InnerSides + OuterSides; // 内外环的端面
    }

    return BaseVertices + BevelVertices + EndCapVertices;
}

int32 FHollowPrismParameters::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    // 基础三角形：侧面的四边形（每个四边形分解为两个三角形）
    int32 BaseTriangles = InnerSides + OuterSides;
    
    // 倒角三角形：如果有倒角，每个边缘需要额外的三角形
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        // 顶部和底面的倒角三角形
        BevelTriangles = (InnerSides + OuterSides) * BevelSegments * 2; // 内外环各2个
    }
    
    // 端面三角形：如果不是完整圆环
    int32 EndCapTriangles = 0;
    if (!IsFullCircle())
    {
        EndCapTriangles = InnerSides + OuterSides; // 内外环的端面
    }

    return BaseTriangles + BevelTriangles + EndCapTriangles;
}

void FHollowPrismParameters::PostEditChangeProperty(const FName& PropertyName)
{
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismParameters::PostEditChangeProperty - Property changed: %s"), *PropertyName.ToString());
}

bool FHollowPrismParameters::operator==(const FHollowPrismParameters& Other) const
{
    return InnerRadius == Other.InnerRadius &&
           OuterRadius == Other.OuterRadius &&
           Height == Other.Height &&
           Sides == Other.Sides &&
           InnerSides == Other.InnerSides &&
           OuterSides == Other.OuterSides &&
           ArcAngle == Other.ArcAngle &&
           BevelRadius == Other.BevelRadius &&
           BevelSegments == Other.BevelSegments &&
           bUseTriangleMethod == Other.bUseTriangleMethod &&
           bFlipNormals == Other.bFlipNormals &&
           bDisableDebounce == Other.bDisableDebounce;
}

bool FHollowPrismParameters::operator!=(const FHollowPrismParameters& Other) const
{
    return !(*this == Other);
}
