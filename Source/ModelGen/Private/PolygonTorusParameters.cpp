// Copyright (c) 2024. All rights reserved.

#include "PolygonTorusParameters.h"

bool FPolygonTorusParameters::IsValid() const
{
    const bool bValidMajorRadius = MajorRadius > 0.0f;
    const bool bValidMinorRadius = MinorRadius > 0.0f && MinorRadius <= MajorRadius * 0.9f;
    const bool bValidMajorSegments = MajorSegments >= 3 && MajorSegments <= 256;
    const bool bValidMinorSegments = MinorSegments >= 3 && MinorSegments <= 256;
    const bool bValidTorusAngle = TorusAngle >= 1.0f && TorusAngle <= 360.0f;
    
    return bValidMajorRadius && bValidMinorRadius && bValidMajorSegments && 
           bValidMinorSegments && bValidTorusAngle;
}

int32 FPolygonTorusParameters::CalculateVertexCountEstimate() const
{
    int32 BaseVertexCount = MajorSegments * MinorSegments;
    
    int32 CapVertexCount = 0;
    if (TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CapVertexCount = MinorSegments * 2;
    }
    
    return BaseVertexCount + CapVertexCount;
}

int32 FPolygonTorusParameters::CalculateTriangleCountEstimate() const
{
    int32 BaseTriangleCount = MajorSegments * MinorSegments * 2;
    
    int32 CapTriangleCount = 0;
    if (TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        CapTriangleCount = MinorSegments * 2;
    }
    
    return BaseTriangleCount + CapTriangleCount;
}
