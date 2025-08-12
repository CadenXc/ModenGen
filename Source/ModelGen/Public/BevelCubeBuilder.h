// Copyright (c) 2024. All rights reserved.

/**
 * @file BevelCubeBuilder.h
 * @brief 圆角立方体网格构建器
 * 
 * 该类负责生成圆角立方体的几何数据，继承自FModelGenMeshBuilder。
 * 提供了完整的圆角立方体生成功能，包括主要面、边缘倒角和角落倒角。
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "BevelCubeParameters.h"

/**
 * 面数据结构，用于定义每个面的属性
 */
struct FFaceData
{
    FVector Center;   // 面的中心点
    FVector SizeX;    // 面的X方向尺寸向量
    FVector SizeY;    // 面的Y方向尺寸向量
    FVector Normal;   // 面的法线向量
    FString Name;     // 面的名称
};

/**
 * 边缘倒角定义结构
 */
struct FEdgeBevelDef
{
    int32 Core1Idx;   // 第一个核心点索引
    int32 Core2Idx;   // 第二个核心点索引
    FVector Normal1;  // 第一个法线
    FVector Normal2;  // 第二个法线
    FString Name;     // 边缘名称
};

/**
 * 圆角立方体网格构建器
 */
class MODELGEN_API FBevelCubeBuilder : public FModelGenMeshBuilder
{
public:
    explicit FBevelCubeBuilder(const FBevelCubeParameters& InParams);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual bool ValidateParameters() const override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    FBevelCubeParameters Params;

    // 新增成员变量 - 第一步：CorePoints和预计算常量
    TArray<FVector> CorePoints;           // 核心点数组
    float HalfSize;                       // 半尺寸
    float InnerOffset;                    // 内偏移
    float BevelRadius;                    // 倒角半径
    int32 BevelSegments;                  // 倒角段数

    // 新增成员变量 - 第二步：FaceDefinitions数组
    TArray<FFaceData> FaceDefinitions;    // 面定义数组

    // 新增成员变量 - 第三步：EdgeBevelDefs数组
    TArray<FEdgeBevelDef> EdgeBevelDefs;  // 边缘倒角定义数组

    // 性能优化：预计算常用值
    TArray<float> AlphaValues;            // 预计算的Alpha值 (0.0 到 1.0)
    TArray<TArray<int32>> CornerGridSizes; // 预计算的角落网格尺寸

    // 初始化函数
    void PrecomputeConstants();
    void InitializeFaceDefinitions();
    void InitializeEdgeBevelDefs();
    void CalculateCorePoints();
    void PrecomputeAlphaValues();         // 预计算Alpha值
    void PrecomputeCornerGridSizes();    // 预计算角落网格尺寸

    // 原有的私有函数
    void GenerateMainFaces();
    
    void GenerateEdgeBevels();
    
    void GenerateCornerBevels();
    
    void GenerateEdgeStrip(int32 Core1Idx, int32 Core2Idx, 
                          const FVector& Normal1, const FVector& Normal2);
    
    void GenerateCornerTriangles(const TArray<TArray<int32>>& CornerVerticesGrid, 
                                int32 Lat, int32 Lon, bool bSpecialOrder);
    
    TArray<FVector> GenerateRectangleVertices(const FVector& Center, const FVector& SizeX, const FVector& SizeY) const;
    
    void GenerateQuadSides(const TArray<FVector>& Verts, const FVector& Normal, const TArray<FVector2D>& UVs);
    
    TArray<FVector> GenerateEdgeVertices(const FVector& CorePoint1, const FVector& CorePoint2, 
                                        const FVector& Normal1, const FVector& Normal2, float Alpha) const;
    
    TArray<FVector> GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX, 
                                          const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const;
    
    // 新增：代码结构优化函数
    void GenerateCornerBevel(int32 CornerIndex);
    void GenerateCornerVerticesGrid(int32 CornerIndex, const FVector& CorePoint, 
                                   const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ,
                                   TArray<TArray<int32>>& CornerVerticesGrid);
    void GenerateCornerTrianglesGrid(const TArray<TArray<int32>>& CornerVerticesGrid, bool bSpecialOrder);
    bool IsSpecialCorner(int32 CornerIndex) const;
    
    // 新增：安全访问函数
    float GetAlphaValue(int32 Index) const;
    bool IsValidAlphaIndex(int32 Index) const;
    
    // 新增：CornerGridSizes安全访问函数
    bool IsValidCornerGridIndex(int32 Lat, int32 Lon) const;
    int32 GetCornerGridSize(int32 Lat) const;
    
    // 新增：验证预计算数据
    bool ValidatePrecomputedData() const;
};
