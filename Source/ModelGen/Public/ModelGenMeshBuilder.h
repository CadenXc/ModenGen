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
    //~ Begin Constructor
    FModelGenMeshBuilder();
    virtual ~FModelGenMeshBuilder() = default;

    //~ Begin Public Methods
    /** 生成网格数据 */
    virtual bool Generate(FModelGenMeshData& OutMeshData) = 0;
    
    /** 验证参数 */
    virtual bool ValidateParameters() const = 0;
    
    /** 计算预估的顶点数量 */
    virtual int32 CalculateVertexCountEstimate() const = 0;
    
    /** 计算预估的三角形数量 */
    virtual int32 CalculateTriangleCountEstimate() const = 0;

protected:
    //~ Begin Protected Members
    /** 当前构建的网格数据 */
    FModelGenMeshData MeshData;
    
    /** 用于顶点去重的映射表 */
    TMap<FVector, int32> UniqueVerticesMap;

    //~ Begin Protected Methods
    /** 获取或添加顶点，确保顶点不重复 */
    int32 GetOrAddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    
    /** 添加新顶点到网格数据 */
    int32 AddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    
    /** 添加三角形 */
    void AddTriangle(int32 V1, int32 V2, int32 V3);
    
    /** 添加四边形（两个三角形） */
    void AddQuad(int32 V1, int32 V2, int32 V3, int32 V4);
    
    /** 计算切线向量 */
    FVector CalculateTangent(const FVector& Normal) const;
    
    /** 清除所有数据 */
    void Clear();
    
    /** 验证生成的网格数据 */
    bool ValidateGeneratedData() const;
    
    /** 预分配内存 */
    void ReserveMemory();
};
