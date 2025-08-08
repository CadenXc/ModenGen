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

    // 检查倒角参数
    if (ChamferRadius < 0.0f || ChamferSections < 1)
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
    int32 BaseVertices = Sides * 2;
    
    // 侧面顶点：高度分段数 * 侧面数
    int32 SideVertices = HeightSegments * Sides;
    
    // 倒角顶点：如果有倒角，每个边缘需要额外的顶点
    int32 ChamferVertices = 0;
    if (ChamferRadius > 0.0f)
    {
        // 顶部和底面的倒角顶点
        ChamferVertices = Sides * ChamferSections * 2;
        // 侧面的倒角顶点
        ChamferVertices += HeightSegments * Sides * ChamferSections;
    }
    
    // 端面顶点：如果有端面厚度
    int32 CapVertices = 0;
    if (CapThickness > 0.0f)
    {
        CapVertices = Sides * 2; // 顶部和底面的端面中心点
    }

    return BaseVertices + SideVertices + ChamferVertices + CapVertices;
}

int32 FFrustumParameters::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    // 基础三角形：顶部和底面的三角形
    int32 BaseTriangles = Sides * 2;
    
    // 侧面三角形：高度分段数 * 侧面数 * 2（每个四边形分解为两个三角形）
    int32 SideTriangles = HeightSegments * Sides * 2;
    
    // 倒角三角形：如果有倒角，每个边缘需要额外的三角形
    int32 ChamferTriangles = 0;
    if (ChamferRadius > 0.0f)
    {
        // 顶部和底面的倒角三角形
        ChamferTriangles = Sides * ChamferSections * 2;
        // 侧面的倒角三角形
        ChamferTriangles += HeightSegments * Sides * ChamferSections * 2;
    }
    
    // 端面三角形：如果有端面厚度
    int32 CapTriangles = 0;
    if (CapThickness > 0.0f)
    {
        CapTriangles = Sides * 2; // 顶部和底面的端面三角形
    }

    return BaseTriangles + SideTriangles + ChamferTriangles + CapTriangles;
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
           HeightSegments == Other.HeightSegments &&
           ChamferRadius == Other.ChamferRadius &&
           ChamferSections == Other.ChamferSections &&
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
