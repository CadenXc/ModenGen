// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

#include "ModelGenMeshData.generated.h"

USTRUCT(BlueprintType)
struct FModelGenMeshData
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
    TArray<FLinearColor> VertexColors;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Vertices")
    TArray<FProcMeshTangent> Tangents;
    

    //~ Begin Statistics Section
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 VertexCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "Mesh Data|Statistics")
    int32 TriangleCount = 0;
    

public:
    //~ Begin Public Methods
    void Clear();
    
    void Reserve(int32 InVertexCount, int32 InTriangleCount);
    
    bool IsValid() const;
    
    int32 GetVertexCount() const { return VertexCount; }
    
    int32 GetTriangleCount() const { return TriangleCount; }
    
    
    int32 AddVertex(const FVector& Position, 
                   const FVector& Normal = FVector::ZeroVector, 
                   const FVector2D& UV = FVector2D::ZeroVector, 
                   const FLinearColor& Color = FLinearColor::White);
    
    void AddTriangle(int32 V0, int32 V1, int32 V2);
    
    void AddQuad(int32 V0, int32 V1, int32 V2, int32 V3);
    
    void Merge(const FModelGenMeshData& Other);
    
    void ToProceduralMesh(UProceduralMeshComponent* MeshComponent, int32 SectionIndex = 0) const;

    void CalculateTangents();

    FVector CalculateTangent(const FVector& Normal) const;
private:
    // 用于三角形去重的键集合（基于规范化后的顶点索引）
    TSet<uint64> TriangleKeySet;
};
