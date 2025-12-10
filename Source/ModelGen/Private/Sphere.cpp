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
        return false;
    }

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ASphere::TryGenerateMeshInternal - 生成的网格数据无效"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

bool ASphere::IsValid() const
{
    return Radius > 0.0f && 
           Sides >= 4 && Sides <= 64 &&
           HorizontalCut >= 0.0f && HorizontalCut <= 1.0f &&
           VerticalCut >= 0.0f && VerticalCut <= 360.0f;
}

int32 ASphere::CalculateVertexCountEstimate() const
{
    if (!IsValid()) return 0;

    // 估算顶点数：根据边数和截断参数
    const int32 VerticalSegments = Sides;
    const int32 HorizontalSegments = Sides;
    
    // 考虑横截断和竖截断的影响
    const float EffectiveVerticalRatio = 1.0f - HorizontalCut;
    const float EffectiveHorizontalRatio = (VerticalCut > 0.0f && VerticalCut < 360.0f) 
        ? (VerticalCut / 360.0f) : 1.0f;
    
    const int32 EstimatedVertices = FMath::CeilToInt(
        (VerticalSegments + 1) * (HorizontalSegments + 1) * EffectiveVerticalRatio * EffectiveHorizontalRatio
    );
    
    return EstimatedVertices;
}

int32 ASphere::CalculateTriangleCountEstimate() const
{
    if (!IsValid()) return 0;

    const int32 VerticalSegments = Sides;
    const int32 HorizontalSegments = Sides;
    
    const float EffectiveVerticalRatio = 1.0f - HorizontalCut;
    const float EffectiveHorizontalRatio = (VerticalCut > 0.0f && VerticalCut < 360.0f) 
        ? (VerticalCut / 360.0f) : 1.0f;
    
    const int32 EstimatedTriangles = FMath::CeilToInt(
        VerticalSegments * HorizontalSegments * 2 * EffectiveVerticalRatio * EffectiveHorizontalRatio
    );
    
    return EstimatedTriangles;
}

void ASphere::SetSides(int32 NewSides)
{
    if (NewSides >= 4 && NewSides <= 64 && NewSides != Sides)
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
    // 保留到0.01精度
    NewHorizontalCut = FMath::RoundToFloat(NewHorizontalCut * 100.0f) / 100.0f;
    
    // 【修复1】：防止横截断 = 1.0 时崩溃，限制最大值为接近但不等于 1.0
    if (NewHorizontalCut >= 1.0f - KINDA_SMALL_NUMBER)
    {
        NewHorizontalCut = 1.0f - KINDA_SMALL_NUMBER;
        UE_LOG(LogTemp, Warning, TEXT("SetHorizontalCut: 横截断值被限制为 %f（接近但不等于1.0，防止崩溃）"), NewHorizontalCut);
    }
    
    if (NewHorizontalCut >= 0.0f && NewHorizontalCut <= 1.0f && 
        !FMath::IsNearlyEqual(NewHorizontalCut, HorizontalCut))
    {
        float OldHorizontalCut = HorizontalCut;
        HorizontalCut = NewHorizontalCut;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                HorizontalCut = OldHorizontalCut;
                UE_LOG(LogTemp, Warning, TEXT("SetHorizontalCut: 网格生成失败，参数已恢复为 %f"), OldHorizontalCut);
            }
        }
    }
}

void ASphere::SetVerticalCut(float NewVerticalCut)
{
    // 保留到0.01精度
    NewVerticalCut = FMath::RoundToFloat(NewVerticalCut * 100.0f) / 100.0f;
    
    if (NewVerticalCut >= 0.0f && NewVerticalCut <= 360.0f && 
        !FMath::IsNearlyEqual(NewVerticalCut, VerticalCut))
    {
        float OldVerticalCut = VerticalCut;
        VerticalCut = NewVerticalCut;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                VerticalCut = OldVerticalCut;
                UE_LOG(LogTemp, Warning, TEXT("SetVerticalCut: 网格生成失败，参数已恢复为 %f"), OldVerticalCut);
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

