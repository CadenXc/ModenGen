// Copyright (c) 2024. All rights reserved.

#include "BevelCubeParameters.h"

bool FBevelCubeParameters::IsValid() const
{
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeParameters::IsValid - Checking parameters: Size=%f, BevelSize=%f, BevelSections=%d"), 
           Size, BevelSize, BevelSections);
    
    // 检查基础尺寸是否有效
    const bool bValidSize = Size > 0.0f;
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeParameters::IsValid - Size valid: %s"), bValidSize ? TEXT("true") : TEXT("false"));
    
    // 检查倒角尺寸是否在有效范围内
    const bool bValidBevelSize = BevelSize >= 0.0f && BevelSize < GetHalfSize();
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeParameters::IsValid - BevelSize valid: %s (BevelSize=%f, HalfSize=%f)"), 
           bValidBevelSize ? TEXT("true") : TEXT("false"), BevelSize, GetHalfSize());
    
    // 检查分段数是否在有效范围内
    const bool bValidSections = BevelSections >= 1 && BevelSections <= 10;
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeParameters::IsValid - Sections valid: %s"), bValidSections ? TEXT("true") : TEXT("false"));
    
    const bool bResult = bValidSize && bValidBevelSize && bValidSections;
    UE_LOG(LogTemp, Log, TEXT("FBevelCubeParameters::IsValid - Final result: %s"), bResult ? TEXT("true") : TEXT("false"));
    
    return bResult;
}

int32 FBevelCubeParameters::CalculateVertexCountEstimate() const
{
    // 基础顶点数：6个面 * 4个顶点 = 24个顶点
    int32 BaseVertexCount = 24;
    
    // 边缘倒角顶点：12条边 * (BevelSections + 1) * 2个顶点
    int32 EdgeBevelVertexCount = 12 * (BevelSections + 1) * 2;
    
    // 角落倒角顶点：8个角 * (BevelSections + 1) * (BevelSections + 1) / 2
    int32 CornerBevelVertexCount = 8 * (BevelSections + 1) * (BevelSections + 1) / 2;
    
    return BaseVertexCount + EdgeBevelVertexCount + CornerBevelVertexCount;
}

int32 FBevelCubeParameters::CalculateTriangleCountEstimate() const
{
    // 基础三角形数：6个面 * 2个三角形 = 12个三角形
    int32 BaseTriangleCount = 12;
    
    // 边缘倒角三角形：12条边 * BevelSections * 2个三角形
    int32 EdgeBevelTriangleCount = 12 * BevelSections * 2;
    
    // 角落倒角三角形：8个角 * BevelSections * BevelSections * 2个三角形
    int32 CornerBevelTriangleCount = 8 * BevelSections * BevelSections * 2;
    
    return BaseTriangleCount + EdgeBevelTriangleCount + CornerBevelTriangleCount;
}
