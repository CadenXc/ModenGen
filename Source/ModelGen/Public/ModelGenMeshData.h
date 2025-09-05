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
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector> Vertices;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Triangles")
    TArray<int32> Triangles;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector> Normals;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector2D> UVs;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FVector2D> UV1s;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FLinearColor> VertexColors;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FProcMeshTangent> Tangents;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Materials")
    TArray<int32> MaterialIndices;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Collision")
    bool bHasCollision = false;

    //~ Begin Statistics Section
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 VertexCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 TriangleCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 MaterialCount = 0;

public:
    //~ Begin Public Methods
    void Clear();
    
    void Reserve(int32 InVertexCount, int32 InTriangleCount);
    
    bool IsValid() const;
    
    int32 GetVertexCount() const { return VertexCount; }
    
    int32 GetTriangleCount() const { return TriangleCount; }
    
    int32 GetMaterialCount() const { return MaterialCount; }
    
    int32 AddVertex(const FVector& Position, 
                   const FVector& Normal = FVector::ZeroVector, 
                   const FVector2D& UV = FVector2D::ZeroVector, 
                   const FLinearColor& Color = FLinearColor::White);
    
    int32 AddVertexWithDualUV(const FVector& Position, 
                              const FVector& Normal = FVector::ZeroVector, 
                              const FVector2D& UV = FVector2D::ZeroVector,
                              const FVector2D& UV1 = FVector2D::ZeroVector,
                              const FLinearColor& Color = FLinearColor::White);
    
    void AddTriangle(int32 V0, int32 V1, int32 V2, int32 MaterialIndex = 0);
    
    void AddQuad(int32 V0, int32 V1, int32 V2, int32 V3, int32 MaterialIndex = 0);
    
    void Merge(const FModelGenMeshData& Other);
    
    void ToProceduralMesh(UProceduralMeshComponent* MeshComponent, int32 SectionIndex = 0) const;

    void CalculateTangents();

    FVector CalculateTangent(const FVector& Normal) const;
private:
    // 用于三角形去重的键集合（基于规范化后的顶点索引）
    TSet<uint64> TriangleKeySet;
};
