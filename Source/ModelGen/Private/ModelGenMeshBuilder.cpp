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

FVector FModelGenMeshBuilder::GetPosByIndex(int32 Index) const
{
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

void FModelGenMeshBuilder::AddTriangle(int32 V0, int32 V1, int32 V2)
{
    MeshData.AddTriangle(V0, V1, V2);
}

void FModelGenMeshBuilder::AddQuad(int32 V0, int32 V1, int32 V2, int32 V3)
{
    MeshData.AddQuad(V0, V1, V2, V3);
}

FVector FModelGenMeshBuilder::CalculateTangent(const FVector& Normal) const
{
    return MeshData.CalculateTangent(Normal);
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
