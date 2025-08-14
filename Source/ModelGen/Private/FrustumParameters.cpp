// Copyright (c) 2024. All rights reserved.

#include "FrustumParameters.h"

bool FFrustumParameters::IsValid() const
{
    if (TopRadius <= 0.0f || BottomRadius <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    if (TopSides < 3 || BottomSides < 3 || HeightSegments < 1)
    {
        return false;
    }

    if (BevelRadius < 0.0f || BevelSegments < 1)
    {
        return false;
    }

    if (MinBendRadius < 0.0f)
    {
        return false;
    }

    if (ArcAngle <= 0.0f || ArcAngle > 360.0f)
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

    int32 BaseVertices = TopSides + BottomSides;
    int32 MaxSides = FMath::Max(TopSides, BottomSides);
    int32 SideVertices = HeightSegments * MaxSides;
    
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        BevelVertices = TopSides * BevelSegments + BottomSides * BevelSegments;
        BevelVertices += HeightSegments * MaxSides * BevelSegments;
    }

    return BaseVertices + SideVertices + BevelVertices;
}

int32 FFrustumParameters::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    int32 BaseTriangles = TopSides + BottomSides;
    int32 MaxSides = FMath::Max(TopSides, BottomSides);
    int32 SideTriangles = HeightSegments * MaxSides * 2;
    
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangles = TopSides * BevelSegments * 2 + BottomSides * BevelSegments * 2;
        BevelTriangles += HeightSegments * MaxSides * BevelSegments * 2;
    }

    return BaseTriangles + SideTriangles + BevelTriangles;
}

void FFrustumParameters::PostEditChangeProperty(const FName& PropertyName)
{
    // 可以在这里添加属性变更处理逻辑
}

bool FFrustumParameters::operator==(const FFrustumParameters& Other) const
{
    return TopRadius == Other.TopRadius &&
           BottomRadius == Other.BottomRadius &&
           Height == Other.Height &&
           TopSides == Other.TopSides &&
           BottomSides == Other.BottomSides &&
           HeightSegments == Other.HeightSegments &&
           BevelRadius == Other.BevelRadius &&
           BevelSegments == Other.BevelSegments &&
           BendAmount == Other.BendAmount &&
           MinBendRadius == Other.MinBendRadius &&
           ArcAngle == Other.ArcAngle &&
           bFlipNormals == Other.bFlipNormals;
}

bool FFrustumParameters::operator!=(const FFrustumParameters& Other) const
{
    return !(*this == Other);
}
