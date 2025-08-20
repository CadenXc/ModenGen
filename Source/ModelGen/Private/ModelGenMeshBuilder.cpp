// Copyright (c) 2024. All rights reserved.

#include "ModelGenMeshBuilder.h"

FModelGenMeshBuilder::FModelGenMeshBuilder()
{
    Clear();
}

int32 FModelGenMeshBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    // 使用更高效的复合键生成
    FString VertexKey = FString::Printf(TEXT("%.4f,%.4f,%.4f|%.3f,%.3f,%.3f|%.4f,%.4f"),
        Pos.X, Pos.Y, Pos.Z,
        Normal.X, Normal.Y, Normal.Z,
        UV.X, UV.Y);

    if (int32* FoundIndex = UniqueVerticesMap.Find(VertexKey))
    {
        return *FoundIndex;
    }

    const int32 NewIndex = AddVertex(Pos, Normal, UV);
    UniqueVerticesMap.Add(VertexKey, NewIndex);
    IndexToPosMap.Add(NewIndex, Pos);
    return NewIndex;
}

int32 FModelGenMeshBuilder::GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal, 
                                                     const FVector2D& UV, const FVector2D& UV1)
{
    // 使用更高效的复合键生成
    FString VertexKey = FString::Printf(TEXT("%.4f,%.4f,%.4f|%.3f,%.3f,%.3f|%.4f,%.4f|%.4f,%.4f"),
        Pos.X, Pos.Y, Pos.Z,
        Normal.X, Normal.Y, Normal.Z,
        UV.X, UV.Y, UV1.X, UV1.Y);

    if (int32* FoundIndex = UniqueVerticesMap.Find(VertexKey))
    {
        return *FoundIndex;
    }

    const int32 NewIndex = MeshData.AddVertexWithDualUV(Pos, Normal, UV, UV1);
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

FVector2D FModelGenMeshBuilder::GenerateStableUV(const FVector& Position, const FVector& Normal) const
{
    // 首先尝试调用子类的自定义实现
    FVector2D CustomUV = GenerateStableUVCustom(Position, Normal);
    
    // 如果子类没有重写，返回默认实现
    if (CustomUV.X == 0.0f && CustomUV.Y == 0.0f)
    {
        // 简化的默认UV映射：基于法线方向选择坐标
        const float Threshold = 0.9f;
        if (FMath::Abs(Normal.Z) > Threshold)
        {
            return FVector2D((Position.X + 1.0f) * 0.5f, (Position.Y + 1.0f) * 0.5f);
        }
        else if (FMath::Abs(Normal.X) > Threshold)
        {
            return FVector2D((Position.Y + 1.0f) * 0.5f, (Position.Z + 1.0f) * 0.5f);
        }
        else
        {
            return FVector2D((Position.X + 1.0f) * 0.5f, (Position.Z + 1.0f) * 0.5f);
        }
    }
    
    return CustomUV;
}

FVector2D FModelGenMeshBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    return FVector2D(0.0f, 0.0f);
}

FVector2D FModelGenMeshBuilder::GenerateSecondaryUVCustom(const FVector& Position, const FVector& Normal) const
{
    return FVector2D(0.0f, 0.0f);
}
