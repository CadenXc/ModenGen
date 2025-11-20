// Copyright (c) 2024. All rights reserved.

#include "EditableSurface.h"
#include "EditableSurfaceBuilder.h"
#include "ModelGenMeshData.h"

AEditableSurface::AEditableSurface()
{
    PrimaryActorTick.bCanEverTick = false;
    InitializeDefaultWaypoints();
}

void AEditableSurface::InitializeDefaultWaypoints()
{
    Waypoints.Empty();
    Waypoints.Reserve(WaypointCount);
    
    // 初始化默认路点：沿X轴分布（更容易看到）
    for (int32 i = 0; i < WaypointCount; ++i)
    {
        float Alpha = static_cast<float>(i) / FMath::Max(1, WaypointCount - 1);
        // 沿X轴分布，Y和Z为0，这样更容易看到
        FVector Position(Alpha * 200.0f, 0.0f, 0.0f);
        Waypoints.Add(FSurfaceWaypoint(Position, SurfaceWidth));
    }
}

void AEditableSurface::GenerateMesh()
{
    TryGenerateMeshInternal();
}

bool AEditableSurface::TryGenerateMeshInternal()
{
    if (!IsValid())
    {
        return false;
    }

    FEditableSurfaceBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData))
    {
        return false;
    }

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("AEditableSurface::TryGenerateMeshInternal - 生成的网格数据无效"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

bool AEditableSurface::IsValid() const
{
    return WaypointCount >= 2 && WaypointCount <= 20 &&
           SurfaceWidth > 0.0f &&
           (!bEnableThickness || (ThicknessValue > 0.0f)) &&
           SideSmoothness >= 0 && SideSmoothness <= 5 &&
           (SideSmoothness == 0 || (RightSlopeLength > 0.0f && LeftSlopeLength > 0.0f)) &&
           Waypoints.Num() >= 2;
}

int32 AEditableSurface::CalculateVertexCountEstimate() const
{
    if (!IsValid()) return 0;

    // 估算顶点数：基于路点数量和宽度分段
    const int32 WidthSegments = FMath::Max(2, SideSmoothness + 1);
    const int32 BaseVertices = WaypointCount * WidthSegments;
    
    // 如果启用厚度，需要双倍顶点
    const int32 ThicknessMultiplier = bEnableThickness ? 2 : 1;
    
    return BaseVertices * ThicknessMultiplier;
}

int32 AEditableSurface::CalculateTriangleCountEstimate() const
{
    if (!IsValid()) return 0;

    const int32 WidthSegments = FMath::Max(2, SideSmoothness + 1);
    const int32 BaseTriangles = (WaypointCount - 1) * (WidthSegments - 1) * 2;
    
    // 如果启用厚度，需要添加侧面和端面
    int32 ThicknessTriangles = 0;
    if (bEnableThickness)
    {
        ThicknessTriangles = WaypointCount * (WidthSegments - 1) * 2; // 侧面
        ThicknessTriangles += (WidthSegments - 1) * 2; // 两个端面
    }
    
    return BaseTriangles + ThicknessTriangles;
}

void AEditableSurface::SetWaypointCount(int32 NewWaypointCount)
{
    if (NewWaypointCount >= 2 && NewWaypointCount <= 20 && NewWaypointCount != WaypointCount)
    {
        int32 OldWaypointCount = WaypointCount;
        WaypointCount = NewWaypointCount;
        
        // 调整路点数组
        if (Waypoints.Num() != WaypointCount)
        {
            InitializeDefaultWaypoints();
        }
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                WaypointCount = OldWaypointCount;
                InitializeDefaultWaypoints();
                UE_LOG(LogTemp, Warning, TEXT("SetWaypointCount: 网格生成失败，参数已恢复为 %d"), OldWaypointCount);
            }
        }
    }
}

void AEditableSurface::SetSurfaceWidth(float NewSurfaceWidth)
{
    if (NewSurfaceWidth > 0.0f && !FMath::IsNearlyEqual(NewSurfaceWidth, SurfaceWidth))
    {
        float OldSurfaceWidth = SurfaceWidth;
        SurfaceWidth = NewSurfaceWidth;
        
        // 更新所有路点的宽度
        for (FSurfaceWaypoint& Waypoint : Waypoints)
        {
            Waypoint.Width = SurfaceWidth;
        }
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                SurfaceWidth = OldSurfaceWidth;
                for (FSurfaceWaypoint& Waypoint : Waypoints)
                {
                    Waypoint.Width = OldSurfaceWidth;
                }
                UE_LOG(LogTemp, Warning, TEXT("SetSurfaceWidth: 网格生成失败，参数已恢复为 %f"), OldSurfaceWidth);
            }
        }
    }
}

void AEditableSurface::SetEnableThickness(bool bNewEnableThickness)
{
    if (bNewEnableThickness != bEnableThickness)
    {
        bool OldEnableThickness = bEnableThickness;
        bEnableThickness = bNewEnableThickness;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                bEnableThickness = OldEnableThickness;
                UE_LOG(LogTemp, Warning, TEXT("SetEnableThickness: 网格生成失败，参数已恢复"));
            }
        }
    }
}

void AEditableSurface::SetThicknessValue(float NewThicknessValue)
{
    if (NewThicknessValue > 0.0f && !FMath::IsNearlyEqual(NewThicknessValue, ThicknessValue))
    {
        float OldThicknessValue = ThicknessValue;
        ThicknessValue = NewThicknessValue;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                ThicknessValue = OldThicknessValue;
                UE_LOG(LogTemp, Warning, TEXT("SetThicknessValue: 网格生成失败，参数已恢复为 %f"), OldThicknessValue);
            }
        }
    }
}

void AEditableSurface::SetSideSmoothness(int32 NewSideSmoothness)
{
    if (NewSideSmoothness >= 0 && NewSideSmoothness <= 5 && NewSideSmoothness != SideSmoothness)
    {
        int32 OldSideSmoothness = SideSmoothness;
        SideSmoothness = NewSideSmoothness;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                SideSmoothness = OldSideSmoothness;
                UE_LOG(LogTemp, Warning, TEXT("SetSideSmoothness: 网格生成失败，参数已恢复为 %d"), OldSideSmoothness);
            }
        }
    }
}

void AEditableSurface::SetRightSlopeLength(float NewRightSlopeLength)
{
    if (NewRightSlopeLength > 0.0f && !FMath::IsNearlyEqual(NewRightSlopeLength, RightSlopeLength))
    {
        float OldRightSlopeLength = RightSlopeLength;
        RightSlopeLength = NewRightSlopeLength;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                RightSlopeLength = OldRightSlopeLength;
                UE_LOG(LogTemp, Warning, TEXT("SetRightSlopeLength: 网格生成失败，参数已恢复为 %f"), OldRightSlopeLength);
            }
        }
    }
}

void AEditableSurface::SetRightSlopeGradient(float NewRightSlopeGradient)
{
    if (NewRightSlopeGradient >= -9.0f && NewRightSlopeGradient <= 9.0f && 
        !FMath::IsNearlyEqual(NewRightSlopeGradient, RightSlopeGradient))
    {
        float OldRightSlopeGradient = RightSlopeGradient;
        RightSlopeGradient = NewRightSlopeGradient;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                RightSlopeGradient = OldRightSlopeGradient;
                UE_LOG(LogTemp, Warning, TEXT("SetRightSlopeGradient: 网格生成失败，参数已恢复为 %f"), OldRightSlopeGradient);
            }
        }
    }
}

void AEditableSurface::SetLeftSlopeLength(float NewLeftSlopeLength)
{
    if (NewLeftSlopeLength > 0.0f && !FMath::IsNearlyEqual(NewLeftSlopeLength, LeftSlopeLength))
    {
        float OldLeftSlopeLength = LeftSlopeLength;
        LeftSlopeLength = NewLeftSlopeLength;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                LeftSlopeLength = OldLeftSlopeLength;
                UE_LOG(LogTemp, Warning, TEXT("SetLeftSlopeLength: 网格生成失败，参数已恢复为 %f"), OldLeftSlopeLength);
            }
        }
    }
}

void AEditableSurface::SetLeftSlopeGradient(float NewLeftSlopeGradient)
{
    if (NewLeftSlopeGradient >= -9.0f && NewLeftSlopeGradient <= 9.0f && 
        !FMath::IsNearlyEqual(NewLeftSlopeGradient, LeftSlopeGradient))
    {
        float OldLeftSlopeGradient = LeftSlopeGradient;
        LeftSlopeGradient = NewLeftSlopeGradient;
        
        if (ProceduralMeshComponent)
        {
            if (!TryGenerateMeshInternal())
            {
                LeftSlopeGradient = OldLeftSlopeGradient;
                UE_LOG(LogTemp, Warning, TEXT("SetLeftSlopeGradient: 网格生成失败，参数已恢复为 %f"), OldLeftSlopeGradient);
            }
        }
    }
}

