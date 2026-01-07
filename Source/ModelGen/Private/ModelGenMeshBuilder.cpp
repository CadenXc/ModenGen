#include "ModelGenMeshBuilder.h"

FModelGenMeshBuilder::FModelGenMeshBuilder()
{
    Clear();
}

int32 FModelGenMeshBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    FString VertexKey = FString::Printf(TEXT("%.6f,%.6f,%.6f|%.6f,%.6f,%.6f|%.6f,%.6f"),
        Pos.X, Pos.Y, Pos.Z,
        Normal.X, Normal.Y, Normal.Z,
        UV.X, UV.Y);

    if (int32* FoundIndex = UniqueVerticesMap.Find(VertexKey))
    {
        return *FoundIndex;
    }

    const int32 NewIndex = AddVertex(Pos, Normal, UV);
    UniqueVerticesMap.Add(VertexKey, NewIndex);

    return NewIndex;
}

FVector FModelGenMeshBuilder::GetPosByIndex(int32 Index) const
{
    if (Index >= 0 && Index < MeshData.Vertices.Num())
    {
        return MeshData.Vertices[Index];
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