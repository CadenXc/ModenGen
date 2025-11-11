// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Plane.h"
#include "Math/Box.h"

// 前向声明
class UProceduralMeshComponent;
class UBodySetup;

/**
 * 网格数据结构（用于凸包分解）
 */
struct FMeshData
{
    TArray<FVector> Vertices;
    TArray<int32> Indices;
};

/**
 * 分解参数
 */
struct FDecompParams
{
    int32 TargetHullCount = 8;
    int32 MaxHullVertices = 16;
    int32 MaxDepth = 10;
    float MinVolumeRatio = 0.001f;
};

/**
 * QuickHull算法中的面结构
 */
struct FFace
{
    int32 V0, V1, V2; // 三个顶点索引
    FFace* Neighbor[3]; // 三个邻居面
    TArray<int32> OutsidePoints; // 在这个面外部的点索引
    bool bVisited; // 访问标志

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

    // 计算面的法线和距离
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

    // 计算点到面的距离（带符号）
    float GetDistance(const TArray<FVector>& Points, const FVector& Point) const
    {
        FVector Normal;
        float Distance;
        UpdatePlane(Points, Normal, Distance);
        return FVector::DotProduct(Normal, Point) + Distance;
    }
};

/**
 * QuickHull算法中的边界边结构
 */
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

/**
 * 凸包分解类（使用QuickHull算法）
 * 全平台支持，包括移动端（Android/iOS）
 */
class FModelGenConvexDecomp
{
public:
    /**
     * 生成多个凸包
     */
    static bool GenerateConvexHulls(
        UProceduralMeshComponent* ProceduralMeshComponent,
        UBodySetup* BodySetup,
        int32 HullCount,
        int32 MaxHullVerts,
        uint32 HullPrecision);

private:
    /**
     * 提取网格数据
     */
    static bool ExtractMeshData(UProceduralMeshComponent* ProceduralMeshComponent, FMeshData& OutMeshData);

    /**
     * 递归分解网格
     */
    static void RecursiveDecompose(
        const FMeshData& MeshData,
        const TArray<int32>& TriangleIndices,
        const FDecompParams& Params,
        int32 CurrentDepth,
        TArray<FKConvexElem>& OutConvexElems);

    /**
     * 生成单个凸包
     */
    static bool GenerateConvexHull(const TArray<FVector>& Points, FKConvexElem& OutConvexElem);

    /**
     * 检查三点是否共线
     */
    static bool IsCollinear(const FVector& P0, const FVector& P1, const FVector& P2, float Epsilon);

    /**
     * 检查四点是否共面
     */
    static bool IsCoplanar(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float Epsilon);

    /**
     * 构建初始四面体
     */
    static bool BuildInitialTetrahedron(
        const TArray<FVector>& Points,
        TArray<FFace*>& OutFaces,
        TArray<int32>& OutUnassignedPoints);

    /**
     * 找到最远的点
     */
    static int32 FindFurthestPoint(const TArray<FVector>& Points, FFace* Face);

    /**
     * 找到所有可见面
     */
    static void FindVisibleFaces(
        const TArray<FVector>& Points,
        int32 PointIndex,
        FFace* StartFace,
        TArray<FFace*>& OutVisibleFaces,
        TMap<int32, FBoundaryEdge>& OutBoundaryEdges);

    /**
     * 构建新面
     */
    static void ConstructNewFaces(
        const TArray<FVector>& Points,
        int32 PointIndex,
        TArray<FFace*>& OutNewFaces,
        TMap<int32, FBoundaryEdge>& BoundaryEdges,
        const TArray<FFace*>& VisibleFaces);

    /**
     * 分配外部点集
     */
    static void PartitionOutsideSet(
        TArray<FFace*>& PendingFaces,
        TArray<FFace*>& NewFaces,
        const TArray<FVector>& Points,
        TArray<int32>& UnassignedPoints,
        FFace*& HeadFace);

    /**
     * 计算三角形集合的包围盒
     */
    static FBox CalculateTriangleBounds(const FMeshData& MeshData, const TArray<int32>& TriangleIndices);

    /**
     * 按平面分割网格
     */
    static void SplitMeshByPlane(
        const FMeshData& MeshData,
        const TArray<int32>& TriangleIndices,
        const FPlane& SplitPlane,
        TArray<int32>& OutLeftTriangles,
        TArray<int32>& OutRightTriangles);

    /**
     * 获取包围盒的最长轴
     */
    static int32 GetLongestAxis(const FBox& Bounds);
};
