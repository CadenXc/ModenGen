// Copyright (c) 2024. All rights reserved.

#include "BevelCubeParameters.h"

bool FBevelCubeParameters::IsValid() const
{
    const bool bValidSize = Size > 0.0f;
    const bool bValidBevelSize = BevelRadius >= 0.0f && BevelRadius < GetHalfSize();
    const bool bValidSections = BevelSegments >= 1 && BevelSegments <= 10;
    
    return bValidSize && bValidBevelSize && bValidSections;
}

int32 FBevelCubeParameters::GetVertexCount() const
{
    const int32 BaseVertexCount = 24;
    const int32 EdgeBevelVertexCount = 12 * (BevelSegments + 1) * 2;
    const int32 CornerBevelVertexCount = 8 * (BevelSegments + 1) * (BevelSegments + 1) / 2;
    
    return BaseVertexCount + EdgeBevelVertexCount + CornerBevelVertexCount;
}

int32 FBevelCubeParameters::GetTriangleCount() const
{
    const int32 BaseTriangleCount = 12;
    const int32 EdgeBevelTriangleCount = 12 * BevelSegments * 2;
    const int32 CornerBevelTriangleCount = 8 * BevelSegments * BevelSegments * 2;
    
    return BaseTriangleCount + EdgeBevelTriangleCount + CornerBevelTriangleCount;
}
