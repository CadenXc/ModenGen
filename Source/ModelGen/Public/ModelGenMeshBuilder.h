// Copyright (c) 2024. All rights reserved.

/**
 * @file ModelGenMeshBuilder.h
 * @brief 通用网格构建器基类
 * 
 * 该文件定义了可被所有几何体生成器继承的通用构建器基类。
 * 提供了统一的mesh构建、顶点管理和验证功能。
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshData.h"

/**
 * 通用网格构建器基类
 * 提供所有几何体生成器的基础功能
 */
class MODELGEN_API FModelGenMeshBuilder
{
public:
    FModelGenMeshBuilder();
    virtual ~FModelGenMeshBuilder() = default;

    virtual bool Generate(FModelGenMeshData& OutMeshData) = 0;
    
    virtual bool ValidateParameters() const = 0;
    virtual int32 CalculateVertexCountEstimate() const = 0;
    virtual int32 CalculateTriangleCountEstimate() const = 0;

protected:
    FModelGenMeshData MeshData;

    TMap<FString, int32> UniqueVerticesMap;
    TMap<int32, FVector> IndexToPosMap;

    int32 GetOrAddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    
    FVector GetPosByIndex(int32 index) const;

    int32 AddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    void AddTriangle(int32 V0, int32 V1, int32 V2);
    void AddQuad(int32 V0, int32 V1, int32 V2, int32 V3);
    
    FVector CalculateTangent(const FVector& Normal) const;
    
    void Clear();
    
    bool ValidateGeneratedData() const;
    
    void ReserveMemory();
};
