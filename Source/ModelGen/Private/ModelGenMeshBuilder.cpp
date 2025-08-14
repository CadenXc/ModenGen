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

// 通用稳定UV映射实现
FVector2D FModelGenMeshBuilder::GenerateStableUV(const FVector& Position, const FVector& Normal) const
{
    // 首先尝试调用子类的自定义实现
    FVector2D CustomUV = GenerateStableUVCustom(Position, Normal);
    
    // 如果子类没有重写，返回默认实现
    if (CustomUV.X == 0.0f && CustomUV.Y == 0.0f)
    {
        // 默认实现：基于位置和法线的简单映射
        float X = Position.X;
        float Y = Position.Y;
        float Z = Position.Z;
        
        // 根据法线方向选择UV映射策略
        if (FMath::Abs(Normal.Z) > 0.9f)
        {
            // 水平面：使用X和Y坐标
            float U = (X + 1.0f) * 0.5f;  // 0 到 1
            float V = (Y + 1.0f) * 0.5f;  // 0 到 1
            return FVector2D(U, V);
        }
        else if (FMath::Abs(Normal.X) > 0.9f)
        {
            // 垂直X面：使用Y和Z坐标
            float U = (Y + 1.0f) * 0.5f;  // 0 到 1
            float V = (Z + 1.0f) * 0.5f;  // 0 到 1
            return FVector2D(U, V);
        }
        else
        {
            // 垂直Y面：使用X和Z坐标
            float U = (X + 1.0f) * 0.5f;  // 0 到 1
            float V = (Z + 1.0f) * 0.5f;  // 0 到 1
            return FVector2D(U, V);
        }
    }
    
    return CustomUV;
}

// 默认自定义实现
FVector2D FModelGenMeshBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    // 默认返回零向量，表示使用通用实现
    return FVector2D(0.0f, 0.0f);
}
