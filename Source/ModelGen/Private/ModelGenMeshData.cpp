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
    TriangleKeySet.Reset();

    VertexCount = 0;
    TriangleCount = 0;
}

void FModelGenMeshData::Reserve(int32 InVertexCount, int32 InTriangleCount)
{
    Vertices.Reserve(InVertexCount);
    Normals.Reserve(InVertexCount);
    UVs.Reserve(InVertexCount);
    VertexColors.Reserve(InVertexCount);
    Tangents.Reserve(InVertexCount);

    Triangles.Reserve(InTriangleCount * 3);
}

bool FModelGenMeshData::IsValid() const
{
    const bool bHasBasicGeometry = Vertices.Num() > 0 && Triangles.Num() > 0;
    const bool bValidTriangleCount = Triangles.Num() % 3 == 0;
    const bool bMatchingArraySizes = Normals.Num() == Vertices.Num() &&
        UVs.Num() == Vertices.Num() &&
        Tangents.Num() == Vertices.Num();

    // 添加额外的安全检查
    const bool bValidIndices = [this]() -> bool
        {
            for (int32 Index : Triangles)
            {
                if (Index < 0 || Index >= Vertices.Num())
                {
                    return false;
                }
            }
            return true;
        }();

    return bHasBasicGeometry && bValidTriangleCount && bMatchingArraySizes && bValidIndices;
}

int32 FModelGenMeshData::AddVertex(const FVector& Position, const FVector& Normal,
    const FVector2D& UV, const FLinearColor& Color)
{
    const int32 Index = Vertices.Num();

    Vertices.Add(Position);
    Normals.Add(Normal);
    UVs.Add(UV);
    VertexColors.Add(Color);
    Tangents.Add(FProcMeshTangent(FVector::ZeroVector, false));

    VertexCount = Vertices.Num();
    return Index;
}

void FModelGenMeshData::AddTriangle(int32 V1, int32 V2, int32 V3)
{
    if (V1 == V2 || V2 == V3 || V1 == V3)
    {
        return;
    }

    int32 A = V1, B = V2, C = V3;
    if (A > B) Swap(A, B);
    if (B > C) Swap(B, C);
    if (A > B) Swap(A, B);
    const uint64 Key = (static_cast<uint64>(A) << 42) | (static_cast<uint64>(B) << 21) | static_cast<uint64>(C);

    if (TriangleKeySet.Contains(Key))
    {
        return;
    }

    TriangleKeySet.Add(Key);

    Triangles.Add(V1);
    Triangles.Add(V2);
    Triangles.Add(V3);
    TriangleCount = Triangles.Num() / 3;
}

void FModelGenMeshData::AddQuad(int32 V0, int32 V1, int32 V2, int32 V3)
{
    AddTriangle(V0, V1, V2);
    AddTriangle(V0, V2, V3);
}

void FModelGenMeshData::Merge(const FModelGenMeshData& Other)
{
    const int32 VertexOffset = Vertices.Num();
    const int32 TriangleOffset = Triangles.Num();

    Vertices.Append(Other.Vertices);
    Normals.Append(Other.Normals);
    UVs.Append(Other.UVs);
    VertexColors.Append(Other.VertexColors);
    Tangents.Append(Other.Tangents);

    for (int32 i = 0; i < Other.Triangles.Num(); ++i)
    {
        Triangles.Add(Other.Triangles[i] + VertexOffset);
    }


    VertexCount = Vertices.Num();
    TriangleCount = Triangles.Num() / 3;
}

void FModelGenMeshData::ToProceduralMesh(UProceduralMeshComponent* MeshComponent, int32 SectionIndex) const
{
    if (!MeshComponent)
    {
        return;
    }

    if (!IsValid())
    {
        return;
    }

    bool bCreateCollision = MeshComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
    MeshComponent->CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, bCreateCollision);
}

void FModelGenMeshData::CalculateTangents()
{
    if (Vertices.Num() == 0 || Triangles.Num() == 0 || UVs.Num() != Vertices.Num())
    {
        return;
    }

    TArray<FVector> OriginalNormals = Normals;
    
    TArray<FVector> OutNormals;
    TArray<FProcMeshTangent> OutTangents;
    
    OutNormals.SetNum(Vertices.Num());
    OutTangents.SetNum(Vertices.Num());
    
    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
        Vertices, 
        Triangles, 
        UVs, 
        OutNormals, 
        OutTangents
    );
    
    Normals = MoveTemp(OriginalNormals);
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