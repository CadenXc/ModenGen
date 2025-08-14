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

struct FFaceData
{
    FVector Center;
    FVector SizeX;
    FVector SizeY;
    FVector Normal;
    FString Name;
};

struct FEdgeBevelDef
{
    int32 Core1Idx;
    int32 Core2Idx;
    FVector Normal1;
    FVector Normal2;
    FString Name;
};

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

    TArray<FVector> CorePoints;
    float HalfSize;
    float InnerOffset;
    float BevelRadius;
    int32 BevelSegments;

    TArray<FFaceData> FaceDefinitions;
    TArray<FEdgeBevelDef> EdgeBevelDefs;

    TArray<float> AlphaValues;
    TArray<TArray<int32>> CornerGridSizes;

    void PrecomputeConstants();
    void InitializeFaceDefinitions();
    void InitializeEdgeBevelDefs();
    void CalculateCorePoints();
    void PrecomputeAlphaValues();
    void PrecomputeCornerGridSizes();

    void GenerateMainFaces();
    void GenerateEdgeBevels();
    void GenerateCornerBevels();
    void GenerateEdgeStrip(int32 Core1Idx, int32 Core2Idx, 
                          const FVector& Normal1, const FVector& Normal2,
                          float UVOffsetX, float UVWidth);
    void GenerateCornerTriangles(const TArray<TArray<int32>>& CornerVerticesGrid, 
                                int32 Lat, int32 Lon, bool bSpecialOrder);
    
    TArray<FVector> GenerateRectangleVertices(const FVector& Center, const FVector& SizeX, const FVector& SizeY) const;
    void GenerateQuadSides(const TArray<FVector>& Verts, const FVector& Normal, const TArray<FVector2D>& UVs);
    TArray<FVector> GenerateEdgeVertices(const FVector& CorePoint1, const FVector& CorePoint2, 
                                        const FVector& Normal1, const FVector& Normal2, float Alpha) const;
    TArray<FVector> GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX, 
                                          const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const;
    
    void GenerateCornerBevel(int32 CornerIndex, float UVOffsetX, float UVWidth);
    void GenerateCornerVerticesGrid(int32 CornerIndex, const FVector& CorePoint, 
                                   const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ,
                                   TArray<TArray<int32>>& CornerVerticesGrid,
                                   float UVOffsetX, float UVWidth);
    void GenerateCornerTrianglesGrid(const TArray<TArray<int32>>& CornerVerticesGrid, bool bSpecialOrder);
    bool IsSpecialCorner(int32 CornerIndex) const;
    
    float GetAlphaValue(int32 Index) const;
    bool IsValidAlphaIndex(int32 Index) const;
    bool IsValidCornerGridIndex(int32 Lat, int32 Lon) const;
    int32 GetCornerGridSize(int32 Lat) const;
    bool ValidatePrecomputedData() const;
    
    /** 基于顶点位置的稳定UV映射 - 重写父类实现 */
    virtual FVector2D GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const override;
    
};
