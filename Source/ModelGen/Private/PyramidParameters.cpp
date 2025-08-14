// Copyright (c) 2024. All rights reserved.

#include "PyramidParameters.h"

bool FPyramidParameters::IsValid() const
{
    const bool bValidBaseRadius = BaseRadius > 0.0f;
    const bool bValidHeight = Height > 0.0f;
    const bool bValidSides = Sides >= 3 && Sides <= 100;
    const bool bValidBevelRadius = BevelRadius >= 0.0f && BevelRadius < Height;
    
    return bValidBaseRadius && bValidHeight && bValidSides && bValidBevelRadius;
}

float FPyramidParameters::GetBevelTopRadius() const
{
    if (BevelRadius <= 0.0f) 
    {
        return BaseRadius;
    }
    
    float ScaleFactor = 1.0f - (BevelRadius / Height);
    return FMath::Max(0.0f, BaseRadius * ScaleFactor);
}

int32 FPyramidParameters::CalculateVertexCountEstimate() const
{
    int32 BaseVertexCount = Sides;
    int32 BevelVertexCount = (BevelRadius > 0.0f) ? Sides * 2 : 0;
    int32 PyramidVertexCount = Sides + 1;
    
    return BaseVertexCount + BevelVertexCount + PyramidVertexCount;
}

int32 FPyramidParameters::CalculateTriangleCountEstimate() const
{
    int32 BaseTriangleCount = Sides - 2;
    int32 BevelTriangleCount = (BevelRadius > 0.0f) ? Sides * 2 : 0;
    int32 PyramidTriangleCount = Sides;
    
    return BaseTriangleCount + BevelTriangleCount + PyramidTriangleCount;
}
