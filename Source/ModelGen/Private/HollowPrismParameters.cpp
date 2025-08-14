// Copyright (c) 2024. All rights reserved.

#include "HollowPrismParameters.h"

bool FHollowPrismParameters::IsValid() const
{
    if (InnerRadius <= 0.0f || OuterRadius <= 0.0f || Height <= 0.0f)
    {
        return false;
    }

    if (InnerRadius >= OuterRadius)
    {
        return false;
    }

    if (Sides < 3 || InnerSides < 3 || OuterSides < 3)
    {
        return false;
    }

    if (ArcAngle <= 0.0f || ArcAngle > 360.0f)
    {
        return false;
    }

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

    int32 BaseVertices = InnerSides + OuterSides;
    
    int32 BevelVertices = 0;
    if (BevelRadius > 0.0f)
    {
        BevelVertices = (InnerSides + OuterSides) * BevelSegments * 2;
    }
    
    int32 EndCapVertices = 0;
    if (!IsFullCircle())
    {
        EndCapVertices = InnerSides + OuterSides;
    }

    return BaseVertices + BevelVertices + EndCapVertices;
}

int32 FHollowPrismParameters::CalculateTriangleCountEstimate() const
{
    if (!IsValid())
    {
        return 0;
    }

    int32 BaseTriangles = InnerSides + OuterSides;
    
    int32 BevelTriangles = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangles = (InnerSides + OuterSides) * BevelSegments * 2;
    }
    
    int32 EndCapTriangles = 0;
    if (!IsFullCircle())
    {
        EndCapTriangles = InnerSides + OuterSides;
    }

    return BaseTriangles + BevelTriangles + EndCapTriangles;
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
