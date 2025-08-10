// Copyright (c) 2024. All rights reserved.

/**
 * @file ModelGenMeshData.h
 * @brief 通用网格数据结构
 * 
 * 该文件定义了可被所有几何体生成器复用的通用网格数据结构。
 * 提供了统一的mesh数据存储、验证和操作方法。
 */

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "ModelGenMeshData.generated.h"

/**
 * 通用网格数据结构
 * 可以被所有几何体生成器复用
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FModelGenMeshData
{
    GENERATED_BODY()

public:
    //~ Begin Mesh Data Section
    /** 顶点位置数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector> Vertices;
    
    /** 三角形索引数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Triangles")
    TArray<int32> Triangles;
    
    /** 顶点法线数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector> Normals;
    
    /** 顶点UV坐标数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector2D> UVs;
    
    /** 顶点颜色数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FLinearColor> VertexColors;
    
    /** 顶点切线数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FProcMeshTangent> Tangents;
    
    /** 三角形材质索引数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Materials")
    TArray<int32> MaterialIndices;

    //~ Begin Statistics Section
    /** 顶点总数 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 VertexCount = 0;
    
    /** 三角形总数 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 TriangleCount = 0;
    
    /** 材质总数 */
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 MaterialCount = 0;

public:
    //~ Begin Public Methods
    /** 清除所有网格数据 */
    void Clear();
    
    /** 预分配内存以提高性能 */
    void Reserve(int32 InVertexCount, int32 InTriangleCount);
    
    /** 验证网格数据的完整性和有效性 */
    bool IsValid() const;
    
    /** 获取顶点总数 */
    int32 GetVertexCount() const { return VertexCount; }
    
    /** 获取三角形总数 */
    int32 GetTriangleCount() const { return TriangleCount; }
    
    /** 获取材质总数 */
    int32 GetMaterialCount() const { return MaterialCount; }
    
    /** 添加顶点 */
    int32 AddVertex(const FVector& Position, 
                   const FVector& Normal = FVector::ZeroVector, 
                   const FVector2D& UV = FVector2D::ZeroVector, 
                   const FLinearColor& Color = FLinearColor::White);
    
    /** 添加三角形 */
    void AddTriangle(int32 V0, int32 V1, int32 V2, int32 MaterialIndex = 0);
    
    /** 添加四边形（两个三角形） */
    void AddQuad(int32 V0, int32 V1, int32 V2, int32 V3, int32 MaterialIndex = 0);
    
    /** 合并另一个网格数据 */
    void Merge(const FModelGenMeshData& Other);
    
    /** 转换为UProceduralMeshComponent可用的格式 */
    void ToProceduralMesh(UProceduralMeshComponent* MeshComponent, int32 SectionIndex = 0) const;

    /** 使用引擎工具计算切线（MikkTSpace）以匹配DCC显示 */
    void CalculateTangents();

private:
    /** 计算切线向量 */
    FVector CalculateTangent(const FVector& Normal) const;
};
