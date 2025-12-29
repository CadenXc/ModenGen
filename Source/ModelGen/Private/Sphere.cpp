// Copyright (c) 2024. All rights reserved.

#include "Sphere.h"
#include "SphereBuilder.h"
#include "ModelGenMeshData.h"

ASphere::ASphere()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ASphere::GenerateMesh()
{
    TryGenerateMeshInternal();
}

bool ASphere::TryGenerateMeshInternal()
{
    if (!IsValid())
    {
        return false;
    }

    FSphereBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData))
    {
        if (GetProceduralMesh())
        {
            GetProceduralMesh()->ClearAllMeshSections();
        }
        return false;
    }

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("ASphere::TryGenerateMeshInternal - 网格生成结果为空（可能是参数导致几何体消失）"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

bool ASphere::IsValid() const
{
    return Radius > KINDA_SMALL_NUMBER &&
        Sides >= 4 && Sides <= 64 &&
        HorizontalCut >= 0.0f && HorizontalCut < 1.0f && // 不能等于1
        VerticalCut > KINDA_SMALL_NUMBER && VerticalCut <= 1.0f; // 不能等于0
}

int32 ASphere::CalculateVertexCountEstimate() const
{
    const int32 NumRings = FMath::Max(2, Sides / 2);
    const int32 GridVerts = (NumRings + 1) * (Sides + 1);
    const int32 CapVerts = Sides * 4; // 粗略估算顶部和侧面切口的顶点
    return GridVerts + CapVerts;
}

int32 ASphere::CalculateTriangleCountEstimate() const
{
    const int32 NumRings = FMath::Max(2, Sides / 2);
    const int32 GridTris = NumRings * Sides * 2;
    const int32 CapTris = Sides * 4;
    return GridTris + CapTris;
}

void ASphere::SetSides(int32 NewSides)
{
    // 钳制范围
    if (NewSides < 4) NewSides = 4;
    if (NewSides > 64) NewSides = 64;

    if (NewSides != Sides)
    {
        int32 OldSides = Sides;
        Sides = NewSides;

        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                Sides = OldSides;
                UE_LOG(LogTemp, Warning, TEXT("SetSides: 网格生成失败，参数已恢复为 %d"), OldSides);
            }
        }
    }
}

void ASphere::SetHorizontalCut(float NewHorizontalCut)
{
    NewHorizontalCut = FMath::RoundToFloat(NewHorizontalCut * 100.0f) / 100.0f;

    if (NewHorizontalCut >= 1.0f - KINDA_SMALL_NUMBER)
    {
        NewHorizontalCut = 1.0f - KINDA_SMALL_NUMBER;
    }

    if (NewHorizontalCut >= 0.0f && !FMath::IsNearlyEqual(NewHorizontalCut, HorizontalCut))
    {
        float OldHorizontalCut = HorizontalCut;
        HorizontalCut = NewHorizontalCut;

        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                UE_LOG(LogTemp, Warning, TEXT("SetHorizontalCut: 该参数下无法生成有效网格"));
            }
        }
    }
}

void ASphere::SetVerticalCut(float NewVerticalCut)
{
    NewVerticalCut = FMath::RoundToFloat(NewVerticalCut * 100.0f) / 100.0f;

    if (NewVerticalCut <= KINDA_SMALL_NUMBER)
    {
        NewVerticalCut = 0.0f;
    }

    if (NewVerticalCut >= 0.0f && NewVerticalCut <= 1.0f &&
        !FMath::IsNearlyEqual(NewVerticalCut, VerticalCut))
    {
        float OldVerticalCut = VerticalCut;
        VerticalCut = NewVerticalCut;

        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                UE_LOG(LogTemp, Warning, TEXT("SetVerticalCut: 该参数下无法生成有效网格"));
            }
        }
    }
}

void ASphere::SetRadius(float NewRadius)
{
    if (NewRadius > 0.0f && !FMath::IsNearlyEqual(NewRadius, Radius))
    {
        float OldRadius = Radius;
        Radius = NewRadius;

        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                Radius = OldRadius;
                UE_LOG(LogTemp, Warning, TEXT("SetRadius: 网格生成失败，参数已恢复为 %f"), OldRadius);
            }
        }
    }
}