// Copyright (c) 2024. All rights reserved.

#include "ModelGenMeshData.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"

void FModelGenMeshData::Clear()
{
    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UVs.Reset();
    VertexColors.Reset();
    Tangents.Reset();
    MaterialIndices.Reset();
    
    VertexCount = 0;
    TriangleCount = 0;
    MaterialCount = 0;
}

void FModelGenMeshData::Reserve(int32 InVertexCount, int32 InTriangleCount)
{
    Vertices.Reserve(InVertexCount);
    Normals.Reserve(InVertexCount);
    UVs.Reserve(InVertexCount);
    VertexColors.Reserve(InVertexCount);
    Tangents.Reserve(InVertexCount);
    
    Triangles.Reserve(InTriangleCount * 3);
    MaterialIndices.Reserve(InTriangleCount);
}

bool FModelGenMeshData::IsValid() const
{
    const bool bHasBasicGeometry = Vertices.Num() > 0 && Triangles.Num() > 0;
    const bool bValidTriangleCount = Triangles.Num() % 3 == 0;
    const bool bMatchingArraySizes = Normals.Num() == Vertices.Num() &&
                                   UVs.Num() == Vertices.Num() &&
                                   Tangents.Num() == Vertices.Num();
    
    return bHasBasicGeometry && bValidTriangleCount && bMatchingArraySizes;
}

int32 FModelGenMeshData::AddVertex(const FVector& Position, const FVector& Normal, 
                                  const FVector2D& UV, const FLinearColor& Color)
{
    const int32 Index = Vertices.Num();
    
    Vertices.Add(Position);
    Normals.Add(Normal);
    UVs.Add(UV);
    VertexColors.Add(Color);
    Tangents.Add(FProcMeshTangent(CalculateTangent(Normal), false));
    
    VertexCount = Vertices.Num();
    return Index;
}

void FModelGenMeshData::AddTriangle(int32 V1, int32 V2, int32 V3, int32 MaterialIndex)
{
    Triangles.Add(V1);
    Triangles.Add(V2);
    Triangles.Add(V3);
    MaterialIndices.Add(MaterialIndex);
    
    TriangleCount = Triangles.Num() / 3;
    MaterialCount = FMath::Max(MaterialCount, MaterialIndex + 1);
}

void FModelGenMeshData::AddQuad(int32 V0, int32 V1, int32 V2, int32 V3, int32 MaterialIndex)
{
    AddTriangle(V0, V1, V2, MaterialIndex);
    AddTriangle(V0, V2, V3, MaterialIndex);
}

void FModelGenMeshData::Merge(const FModelGenMeshData& Other)
{
    const int32 VertexOffset = Vertices.Num();
    const int32 TriangleOffset = Triangles.Num();
    
    // 合并顶点数据
    Vertices.Append(Other.Vertices);
    Normals.Append(Other.Normals);
    UVs.Append(Other.UVs);
    VertexColors.Append(Other.VertexColors);
    Tangents.Append(Other.Tangents);
    
    // 合并三角形数据（需要调整索引）
    for (int32 i = 0; i < Other.Triangles.Num(); ++i)
    {
        Triangles.Add(Other.Triangles[i] + VertexOffset);
    }
    
    // 合并材质索引
    MaterialIndices.Append(Other.MaterialIndices);
    
    // 更新统计信息
    VertexCount = Vertices.Num();
    TriangleCount = Triangles.Num() / 3;
    MaterialCount = FMath::Max(MaterialCount, Other.MaterialCount);
}

void FModelGenMeshData::ToProceduralMesh(UProceduralMeshComponent* MeshComponent, int32 SectionIndex) const
{
    UE_LOG(LogTemp, Log, TEXT("FModelGenMeshData::ToProceduralMesh - Starting conversion"));
    
    if (!MeshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("FModelGenMeshData::ToProceduralMesh - MeshComponent is null"));
        return;
    }
    
    if (!IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("FModelGenMeshData::ToProceduralMesh - Mesh data is not valid"));
        UE_LOG(LogTemp, Error, TEXT("FModelGenMeshData::ToProceduralMesh - Vertices: %d, Triangles: %d, Normals: %d, UVs: %d, Tangents: %d"), 
               Vertices.Num(), Triangles.Num(), Normals.Num(), UVs.Num(), Tangents.Num());
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("FModelGenMeshData::ToProceduralMesh - Creating mesh section with %d vertices, %d triangles"), 
           Vertices.Num(), Triangles.Num());
    
    MeshComponent->CreateMeshSection_LinearColor(
        SectionIndex,
        Vertices,
        Triangles,
        Normals,
        UVs,
        VertexColors,
        Tangents,
        true
    );
    
    UE_LOG(LogTemp, Log, TEXT("FModelGenMeshData::ToProceduralMesh - Mesh section created successfully"));
}

void FModelGenMeshData::CalculateTangents()
{
    if (Vertices.Num() == 0 || Triangles.Num() == 0 || UVs.Num() != Vertices.Num())
    {
        return;
    }

    // 使用引擎提供的切线计算，遵循 MikkTSpace
    TArray<FVector> OutNormals = Normals; // 可由函数覆盖
    TArray<FProcMeshTangent> OutTangents = Tangents;
    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, OutNormals, OutTangents);
    Normals = MoveTemp(OutNormals);
    Tangents = MoveTemp(OutTangents);
}

FVector FModelGenMeshData::CalculateTangent(const FVector& Normal) const
{
    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    
    TangentDirection.Normalize();
    return TangentDirection;
}
