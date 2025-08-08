// Copyright (c) 2024. All rights reserved.

#include "ModelGenMeshBuilder.h"

FModelGenMeshBuilder::FModelGenMeshBuilder()
{
    Clear();
}

int32 FModelGenMeshBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    // 尝试查找已存在的顶点
    if (int32* FoundIndex = UniqueVerticesMap.Find(Pos))
    {
        return *FoundIndex;
    }

    // 添加新顶点并记录其索引
    const int32 NewIndex = AddVertex(Pos, Normal, UV);
    UniqueVerticesMap.Add(Pos, NewIndex);
    return NewIndex;
}

int32 FModelGenMeshBuilder::AddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    return MeshData.AddVertex(Pos, Normal, UV);
}

void FModelGenMeshBuilder::AddTriangle(int32 V1, int32 V2, int32 V3)
{
    MeshData.AddTriangle(V1, V2, V3);
}

void FModelGenMeshBuilder::AddQuad(int32 V1, int32 V2, int32 V3, int32 V4)
{
    MeshData.AddQuad(V1, V2, V3, V4);
}

FVector FModelGenMeshBuilder::CalculateTangent(const FVector& Normal) const
{
    // 首先尝试使用上向量计算切线
    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    
    // 如果结果接近零向量，改用右向量计算
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    
    TangentDirection.Normalize();
    return TangentDirection;
}

void FModelGenMeshBuilder::Clear()
{
    MeshData.Clear();
    UniqueVerticesMap.Empty();
}

bool FModelGenMeshBuilder::ValidateGeneratedData() const
{
    return MeshData.IsValid();
}

void FModelGenMeshBuilder::ReserveMemory()
{
    const int32 EstimatedVertexCount = CalculateVertexCountEstimate();
    const int32 EstimatedTriangleCount = CalculateTriangleCountEstimate();
    
    MeshData.Reserve(EstimatedVertexCount, EstimatedTriangleCount);
}
