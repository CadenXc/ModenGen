// Copyright (c) 2024. All rights reserved.

/**
 * @file ModelGenMeshBuilder.h
 * @brief 通用网格构建器基类
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshData.h"

// 通用网格构建器基类
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
    
    int32 GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal, 
                                   const FVector2D& UV, const FVector2D& UV1);
    
    FVector GetPosByIndex(int32 index) const;

    int32 AddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    void AddTriangle(int32 V0, int32 V1, int32 V2);
    void AddQuad(int32 V0, int32 V1, int32 V2, int32 V3);
    
    FVector CalculateTangent(const FVector& Normal) const;
    
    void Clear();
    
    bool ValidateGeneratedData() const;
    
    void ReserveMemory();
    
    FVector2D GenerateStableUV(const FVector& Position, const FVector& Normal) const;
    virtual FVector2D GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const;
    virtual FVector2D GenerateSecondaryUVCustom(const FVector& Position, const FVector& Normal) const;
};
