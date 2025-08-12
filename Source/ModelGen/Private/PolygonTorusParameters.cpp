// Copyright (c) 2024. All rights reserved.

#include "PolygonTorusParameters.h"

bool FPolygonTorusParameters::IsValid() const
{
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusParameters::IsValid - Checking parameters: MajorRadius=%f, MinorRadius=%f, MajorSegments=%d, MinorSegments=%d, TorusAngle=%f"), 
           MajorRadius, MinorRadius, MajorSegments, MinorSegments, TorusAngle);
    
    // 检查基础尺寸是否有效
    const bool bValidMajorRadius = MajorRadius > 0.0f;
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusParameters::IsValid - MajorRadius valid: %s"), bValidMajorRadius ? TEXT("true") : TEXT("false"));
    
    const bool bValidMinorRadius = MinorRadius > 0.0f && MinorRadius <= MajorRadius * 0.9f;
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusParameters::IsValid - MinorRadius valid: %s"), bValidMinorRadius ? TEXT("true") : TEXT("false"));
    
    // 检查分段数是否在有效范围内
    const bool bValidMajorSegments = MajorSegments >= 3 && MajorSegments <= 256;
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusParameters::IsValid - MajorSegments valid: %s"), bValidMajorSegments ? TEXT("true") : TEXT("false"));
    
    const bool bValidMinorSegments = MinorSegments >= 3 && MinorSegments <= 256;
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusParameters::IsValid - MinorSegments valid: %s"), bValidMinorSegments ? TEXT("true") : TEXT("false"));
    
    // 检查角度是否有效
    const bool bValidTorusAngle = TorusAngle >= 1.0f && TorusAngle <= 360.0f;
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusParameters::IsValid - TorusAngle valid: %s"), bValidTorusAngle ? TEXT("true") : TEXT("false"));
    
    const bool bResult = bValidMajorRadius && bValidMinorRadius && bValidMajorSegments && 
                        bValidMinorSegments && bValidTorusAngle;
    UE_LOG(LogTemp, Log, TEXT("FPolygonTorusParameters::IsValid - Final result: %s"), bResult ? TEXT("true") : TEXT("false"));
    
    return bResult;
}

int32 FPolygonTorusParameters::CalculateVertexCountEstimate() const
{
    // 基础圆环顶点数
    int32 BaseVertexCount = MajorSegments * MinorSegments;
    
    // 端盖顶点数（当TorusAngle < 360时自动生成）
    int32 CapVertexCount = 0;
    if (TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        // 端盖使用与次分段数相同的分段数
        CapVertexCount = MinorSegments * 2; // 起始和结束端盖
    }
    
    return BaseVertexCount + CapVertexCount;
}

int32 FPolygonTorusParameters::CalculateTriangleCountEstimate() const
{
    // 基础圆环三角形数（每个四边形两个三角形）
    int32 BaseTriangleCount = MajorSegments * MinorSegments * 2;
    
    // 端盖三角形数（当TorusAngle < 360时自动生成）
    int32 CapTriangleCount = 0;
    if (TorusAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        // 端盖使用与次分段数相同的分段数
        CapTriangleCount = MinorSegments * 2; // 起始和结束端盖
    }
    
    return BaseTriangleCount + CapTriangleCount;
}
