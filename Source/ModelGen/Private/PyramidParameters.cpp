// Copyright (c) 2024. All rights reserved.

#include "PyramidParameters.h"

bool FPyramidParameters::IsValid() const
{
    UE_LOG(LogTemp, Log, TEXT("FPyramidParameters::IsValid - Checking parameters: BaseRadius=%f, Height=%f, Sides=%d, BevelRadius=%f"), 
           BaseRadius, Height, Sides, BevelRadius);
    
    // 检查基础尺寸是否有效
    const bool bValidBaseRadius = BaseRadius > 0.0f;
    UE_LOG(LogTemp, Log, TEXT("FPyramidParameters::IsValid - BaseRadius valid: %s"), bValidBaseRadius ? TEXT("true") : TEXT("false"));
    
    const bool bValidHeight = Height > 0.0f;
    UE_LOG(LogTemp, Log, TEXT("FPyramidParameters::IsValid - Height valid: %s"), bValidHeight ? TEXT("true") : TEXT("false"));
    
    // 检查边数是否在有效范围内
    const bool bValidSides = Sides >= 3 && Sides <= 100;
    UE_LOG(LogTemp, Log, TEXT("FPyramidParameters::IsValid - Sides valid: %s"), bValidSides ? TEXT("true") : TEXT("false"));
    
    // 检查倒角半径是否有效
    const bool bValidBevelRadius = BevelRadius >= 0.0f && BevelRadius < Height;
    UE_LOG(LogTemp, Log, TEXT("FPyramidParameters::IsValid - BevelRadius valid: %s"), bValidBevelRadius ? TEXT("true") : TEXT("false"));
    
    const bool bResult = bValidBaseRadius && bValidHeight && bValidSides && bValidBevelRadius;
    UE_LOG(LogTemp, Log, TEXT("FPyramidParameters::IsValid - Final result: %s"), bResult ? TEXT("true") : TEXT("false"));
    
    return bResult;
}

float FPyramidParameters::GetBevelTopRadius() const
{
    if (BevelRadius <= 0.0f) 
    {
        return BaseRadius;
    }
    // 计算倒角顶部的半径（线性缩放）
    // 倒角高度越高，顶部半径越小
    float ScaleFactor = 1.0f - (BevelRadius / Height);
    return FMath::Max(0.0f, BaseRadius * ScaleFactor);
}



int32 FPyramidParameters::CalculateVertexCountEstimate() const
{
    // 底面顶点数
    int32 BaseVertexCount = Sides;
    
    // 倒角部分顶点数（如果有倒角）
    int32 BevelVertexCount = 0;
    if (BevelRadius > 0.0f)
    {
        BevelVertexCount = Sides * 2; // 倒角顶部和底部
    }
    
    // 金字塔侧面顶点数
    int32 PyramidVertexCount = Sides + 1; // 底面 + 顶点
    
    return BaseVertexCount + BevelVertexCount + PyramidVertexCount;
}

int32 FPyramidParameters::CalculateTriangleCountEstimate() const
{
    // 底面三角形数
    int32 BaseTriangleCount = Sides - 2; // 多边形三角剖分
    
    // 倒角部分三角形数（如果有倒角）
    int32 BevelTriangleCount = 0;
    if (BevelRadius > 0.0f)
    {
        BevelTriangleCount = Sides * 2; // 倒角侧面
    }
    
    // 金字塔侧面三角形数
    int32 PyramidTriangleCount = Sides; // 每个侧面一个三角形
    
    return BaseTriangleCount + BevelTriangleCount + PyramidTriangleCount;
}
