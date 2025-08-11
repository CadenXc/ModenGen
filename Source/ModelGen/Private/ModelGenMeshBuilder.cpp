// Copyright (c) 2024. All rights reserved.

#include "ModelGenMeshBuilder.h"

FModelGenMeshBuilder::FModelGenMeshBuilder()
{
    Clear();
}

int32 FModelGenMeshBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    // 创建一个复合键，包含位置、法线和UV
    // 使用字符串作为键，确保所有属性都匹配
    FString VertexKey = FString::Printf(TEXT("%.6f,%.6f,%.6f|%.6f,%.6f,%.6f|%.6f,%.6f"),
        Pos.X, Pos.Y, Pos.Z,
        Normal.X, Normal.Y, Normal.Z,
        UV.X, UV.Y);

    // 尝试查找已存在的顶点
    if (int32* FoundIndex = UniqueVerticesMap.Find(VertexKey))
    {
        return *FoundIndex;
    }

    // 添加新顶点并记录其索引
    const int32 NewIndex = AddVertex(Pos, Normal, UV);
    UniqueVerticesMap.Add(VertexKey, NewIndex);

    IndexToPosMap.Add(NewIndex, Pos);
    return NewIndex;
}

FVector FModelGenMeshBuilder::GetPosByIndex(int32 Index)
{
    // 直接从映射中获取位置
    if (const FVector* FoundPos = IndexToPosMap.Find(Index))
    {
        return *FoundPos;
    }

    return FVector::ZeroVector;
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
