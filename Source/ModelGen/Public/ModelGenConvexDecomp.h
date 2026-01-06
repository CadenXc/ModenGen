// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Plane.h"
#include "Math/Box.h"

class UProceduralMeshComponent;
class UBodySetup;

struct FMeshData
{
    TArray<FVector> Vertices;
    TArray<int32> Indices;
};

struct FDecompParams
{
    int32 TargetHullCount = 8;
    int32 MaxHullVertices = 16;
    int32 MaxDepth = 10;
    float MinVolumeRatio = 0.001f;
};

struct FFace
{
    int32 V0, V1, V2;
    FFace* Neighbor[3];
    TArray<int32> OutsidePoints;
    bool bVisited;

    FFace()
        : V0(-1), V1(-1), V2(-1), bVisited(false)
    {
        Neighbor[0] = Neighbor[1] = Neighbor[2] = nullptr;
    }

    FFace(int32 InV0, int32 InV1, int32 InV2)
        : V0(InV0), V1(InV1), V2(InV2), bVisited(false)
    {
        Neighbor[0] = Neighbor[1] = Neighbor[2] = nullptr;
    }

    void UpdatePlane(const TArray<FVector>& Points, FVector& OutNormal, float& OutDistance) const
    {
        FVector Edge1 = Points[V1] - Points[V0];
        FVector Edge2 = Points[V2] - Points[V0];
        OutNormal = FVector::CrossProduct(Edge1, Edge2);
        float NormalLen = OutNormal.Size();
        if (NormalLen > KINDA_SMALL_NUMBER)
        {
            OutNormal /= NormalLen;
            OutDistance = -FVector::DotProduct(OutNormal, Points[V0]);
        }
        else
        {
            OutNormal = FVector::ZeroVector;
            OutDistance = 0.0f;
        }
    }

    float GetDistance(const TArray<FVector>& Points, const FVector& Point) const
    {
        FVector Normal;
        float Distance;
        UpdatePlane(Points, Normal, Distance);
        return FVector::DotProduct(Normal, Point) + Distance;
    }
};

struct FBoundaryEdge
{
    int32 V0, V1;
    FFace* NeighborFace;

    FBoundaryEdge()
        : V0(-1), V1(-1), NeighborFace(nullptr)
    {
    }

    FBoundaryEdge(int32 InV0, int32 InV1, FFace* InNeighborFace)
        : V0(InV0), V1(InV1), NeighborFace(InNeighborFace)
    {
    }
};

class FModelGenConvexDecomp
{
public:
    static bool GenerateConvexHulls(
        UProceduralMeshComponent* ProceduralMeshComponent,
        UBodySetup* BodySetup,
        int32 HullCount,
        int32 MaxHullVerts,
        uint32 HullPrecision);

private:
    static bool ExtractMeshData(UProceduralMeshComponent* ProceduralMeshComponent, FMeshData& OutMeshData);

    static void RecursiveDecompose(
        const FMeshData& MeshData,
        const TArray<int32>& TriangleIndices,
        const FDecompParams& Params,
        int32 CurrentDepth,
        TArray<FKConvexElem>& OutConvexElems);

    static bool GenerateConvexHull(const TArray<FVector>& Points, FKConvexElem& OutConvexElem);

    static bool IsCollinear(const FVector& P0, const FVector& P1, const FVector& P2, float Epsilon);

    static bool IsCoplanar(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float Epsilon);

    static bool BuildInitialTetrahedron(
        const TArray<FVector>& Points,
        TArray<FFace*>& OutFaces,
        TArray<int32>& OutUnassignedPoints);

    static int32 FindFurthestPoint(const TArray<FVector>& Points, FFace* Face);

    static void FindVisibleFaces(
        const TArray<FVector>& Points,
        int32 PointIndex,
        FFace* StartFace,
        TArray<FFace*>& OutVisibleFaces,
        TMap<int32, FBoundaryEdge>& OutBoundaryEdges);

    static void ConstructNewFaces(
        const TArray<FVector>& Points,
        int32 PointIndex,
        TArray<FFace*>& OutNewFaces,
        TMap<int32, FBoundaryEdge>& BoundaryEdges,
        const TArray<FFace*>& VisibleFaces);

    static void PartitionOutsideSet(
        TArray<FFace*>& PendingFaces,
        TArray<FFace*>& NewFaces,
        const TArray<FVector>& Points,
        TArray<int32>& UnassignedPoints,
        FFace*& HeadFace);

    static FBox CalculateTriangleBounds(const FMeshData& MeshData, const TArray<int32>& TriangleIndices);

    static void SplitMeshByPlane(
        const FMeshData& MeshData,
        const TArray<int32>& TriangleIndices,
        const FPlane& SplitPlane,
        TArray<int32>& OutLeftTriangles,
        TArray<int32>& OutRightTriangles);

    static int32 GetLongestAxis(const FBox& Bounds);
};
