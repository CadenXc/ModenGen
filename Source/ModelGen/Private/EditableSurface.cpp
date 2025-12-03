// Copyright (c) 2024. All rights reserved.

#include "EditableSurface.h"
#include "EditableSurfaceBuilder.h"
#include "ModelGenMeshData.h"
#include "Components/SplineComponent.h"

AEditableSurface::AEditableSurface()
{
    PrimaryActorTick.bCanEverTick = false;

    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    SplineComponent->SetupAttachment(RootComponent);
    SplineComponent->SetMobility(EComponentMobility::Movable);

    InitializeDefaultWaypoints();
}

void AEditableSurface::OnConstruction(const FTransform& Transform)
{
    // 单向数据流：Waypoints -> Spline
    if (Waypoints.Num() >= 2)
    {
        UpdateSplineFromWaypoints();
    }

    Super::OnConstruction(Transform);
}

void AEditableSurface::AddNewWaypoint()
{
    FVector NewPosition = FVector::ZeroVector;
    float NewWidth = SurfaceWidth;

    if (Waypoints.Num() > 0)
    {
        // 获取最后一个点
        const FSurfaceWaypoint& LastWP = Waypoints.Last();
        NewWidth = LastWP.Width;

        // 计算方向
        FVector Direction = FVector::ForwardVector; // 默认 X 轴

        if (Waypoints.Num() >= 2)
        {
            // 如果有两个以上点，沿最后一段的方向延伸
            const FSurfaceWaypoint& PrevWP = Waypoints[Waypoints.Num() - 2];
            Direction = (LastWP.Position - PrevWP.Position).GetSafeNormal();

            // 确保 Z 轴水平 (如果需要 "同一水平面")
            // 如果您希望完全沿切线（可能向上/向下），注释掉下面这行
            // Direction.Z = 0.0f; 
            // Direction.Normalize();
        }
        else if (SplineComponent && SplineComponent->GetNumberOfSplinePoints() > 0)
        {
            // 尝试从 Spline 获取末端切线
            Direction = SplineComponent->GetTangentAtSplinePoint(SplineComponent->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::Local).GetSafeNormal();
        }

        // 新位置 = 旧位置 + 方向 * 100
        NewPosition = LastWP.Position + Direction * 100.0f;

        // 强制 Z 值与最后一个点相同（"同一水平面"）
        NewPosition.Z = LastWP.Position.Z;
    }
    else
    {
        // 如果没有点，在原点创建
        NewPosition = FVector::ZeroVector;
    }

    // 修改数组
    Waypoints.Add(FSurfaceWaypoint(NewPosition, NewWidth));

    // 更新计数器以保持一致
    WaypointCount = Waypoints.Num();

    // 立即刷新
    UpdateSplineFromWaypoints();
    GenerateMesh();
}

void AEditableSurface::RemoveWaypoint()
{
    // 使用 RemoveWaypointIndex 属性作为索引
    RemoveWaypointByIndex(RemoveWaypointIndex);
}

void AEditableSurface::RemoveWaypointByIndex(int32 Index)
{
    // 检查索引是否有效
    if (Index < 0 || Index >= Waypoints.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveWaypointByIndex: 无效的索引 %d，当前路点数量: %d"), Index, Waypoints.Num());
        return;
    }

    // 确保删除后至少还有2个路点（样条线至少需要2个点）
    if (Waypoints.Num() <= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveWaypointByIndex: 无法删除，样条线至少需要2个路点"));
        return;
    }

    // 删除指定索引的路点
    Waypoints.RemoveAt(Index);

    // 更新计数器以保持一致
    WaypointCount = Waypoints.Num();

    // 立即刷新
    UpdateSplineFromWaypoints();
    GenerateMesh();
}

void AEditableSurface::PrintWaypointInfo()
{
    UE_LOG(LogTemp, Log, TEXT("========== 路点信息 =========="));
    UE_LOG(LogTemp, Log, TEXT("路点总数: %d"), Waypoints.Num());
    UE_LOG(LogTemp, Log, TEXT("曲线类型: %s"), CurveType == ESurfaceCurveType::Standard ? TEXT("Standard") : TEXT("Smooth"));
    
    for (int32 i = 0; i < Waypoints.Num(); ++i)
    {
        const FSurfaceWaypoint& WP = Waypoints[i];
        UE_LOG(LogTemp, Log, TEXT("  路点[%d]: 位置=(%.2f, %.2f, %.2f), 宽度=%.2f"), 
            i, WP.Position.X, WP.Position.Y, WP.Position.Z, WP.Width);
    }
    
    if (SplineComponent && SplineComponent->GetNumberOfSplinePoints() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("样条线点数: %d"), SplineComponent->GetNumberOfSplinePoints());
        float SplineLength = SplineComponent->GetSplineLength();
        UE_LOG(LogTemp, Log, TEXT("样条线长度: %.2f"), SplineLength);
    }
    
    UE_LOG(LogTemp, Log, TEXT("=============================="));
}

void AEditableSurface::InitializeDefaultWaypoints()
{
    if (Waypoints.Num() > 0) return;

    Waypoints.Empty();
    Waypoints.Reserve(WaypointCount);

    // 生成拉长的Z字形上坡路径（5个点）
    if (WaypointCount == 5)
    {
        // 点0: 起点，低处
        Waypoints.Add(FSurfaceWaypoint(FVector(0.0f, 0.0f, 0.0f), SurfaceWidth));
        // 点1: 向前移动，高度上升
        Waypoints.Add(FSurfaceWaypoint(FVector(200.0f, 0.0f, 50.0f), SurfaceWidth));
        // 点2: 向右移动，继续上升
        Waypoints.Add(FSurfaceWaypoint(FVector(400.0f, 200.0f, 100.0f), SurfaceWidth));
        // 点3: 向前移动，继续上升
        Waypoints.Add(FSurfaceWaypoint(FVector(600.0f, 200.0f, 150.0f), SurfaceWidth));
        // 点4: 终点，最高处
        Waypoints.Add(FSurfaceWaypoint(FVector(800.0f, 0.0f, 200.0f), SurfaceWidth));
    }
    else
    {
        // 如果路点数量不是5，使用原来的线性分布方式
        for (int32 i = 0; i < WaypointCount; ++i)
        {
            float Alpha = static_cast<float>(i) / FMath::Max(1, WaypointCount - 1);
            FVector Position(Alpha * 200.0f, 0.0f, 0.0f);
            Waypoints.Add(FSurfaceWaypoint(Position, SurfaceWidth));
        }
    }
}

static void ChaikinSubdivide(const TArray<FVector>& InPoints, TArray<FVector>& OutPoints, int32 Iterations)
{
    TArray<FVector> CurrentPoints = InPoints;

    for (int32 k = 0; k < Iterations; ++k)
    {
        if (CurrentPoints.Num() < 2) break;

        TArray<FVector> NextPoints;
        NextPoints.Add(CurrentPoints[0]);

        for (int32 i = 0; i < CurrentPoints.Num() - 1; ++i)
        {
            FVector P0 = CurrentPoints[i];
            FVector P1 = CurrentPoints[i + 1];

            FVector Q = P0 * 0.75f + P1 * 0.25f;
            FVector R = P0 * 0.25f + P1 * 0.75f;

            NextPoints.Add(Q);
            NextPoints.Add(R);
        }

        NextPoints.Add(CurrentPoints.Last());
        CurrentPoints = NextPoints;
    }

    OutPoints = CurrentPoints;
}

void AEditableSurface::UpdateSplineFromWaypoints()
{
    if (!SplineComponent || Waypoints.Num() < 2) return;

    SplineComponent->ClearSplinePoints();

    if (CurveType == ESurfaceCurveType::Smooth)
    {
        TArray<FVector> ControlPoints;
        for (const auto& WP : Waypoints) ControlPoints.Add(WP.Position);

        TArray<FVector> SmoothPoints;
        ChaikinSubdivide(ControlPoints, SmoothPoints, 3);

        for (int32 i = 0; i < SmoothPoints.Num(); ++i)
        {
            SplineComponent->AddSplinePoint(SmoothPoints[i], ESplineCoordinateSpace::Local, false);
            SplineComponent->SetSplinePointType(i, ESplinePointType::Type::CurveClamped, false);
        }
    }
    else // Standard
    {
        for (int32 i = 0; i < Waypoints.Num(); ++i)
        {
            const FSurfaceWaypoint& Waypoint = Waypoints[i];
            SplineComponent->AddSplinePoint(Waypoint.Position, ESplineCoordinateSpace::Local, false);
            SplineComponent->SetSplinePointType(i, ESplinePointType::Type::Curve, false);
        }
    }

    SplineComponent->UpdateSpline();
}

void AEditableSurface::GenerateMesh()
{
    TryGenerateMeshInternal();
}

bool AEditableSurface::TryGenerateMeshInternal()
{
    if (!IsValid()) return false;

    FEditableSurfaceBuilder Builder(*this);
    FModelGenMeshData MeshData;

    if (!Builder.Generate(MeshData)) return false;

    if (!MeshData.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("AEditableSurface::TryGenerateMeshInternal - Generated mesh data invalid"));
        return false;
    }

    MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
    return true;
}

bool AEditableSurface::IsValid() const
{
    return SplineComponent != nullptr &&
        SplineComponent->GetNumberOfSplinePoints() >= 2 &&
        SurfaceWidth > 0.0f &&
        (!bEnableThickness || (ThicknessValue > 0.0f)) &&
        PathSampleCount >= 2;
}

int32 AEditableSurface::CalculateVertexCountEstimate() const
{
    if (!IsValid()) return 0;
    const int32 WidthSegments = FMath::Max(2, SideSmoothness + 1);
    const int32 BaseVertices = PathSampleCount * WidthSegments;
    const int32 ThicknessMultiplier = bEnableThickness ? 4 : 1;
    return BaseVertices * ThicknessMultiplier;
}

int32 AEditableSurface::CalculateTriangleCountEstimate() const
{
    if (!IsValid()) return 0;
    const int32 WidthSegments = FMath::Max(2, SideSmoothness + 1);
    const int32 BaseTriangles = (PathSampleCount - 1) * (WidthSegments - 1) * 2;
    int32 ThicknessTriangles = 0;
    if (bEnableThickness)
    {
        ThicknessTriangles = PathSampleCount * (WidthSegments - 1) * 2;
        ThicknessTriangles += (WidthSegments - 1) * 2;
        ThicknessTriangles += PathSampleCount * 2 * 2;
    }
    return BaseTriangles + ThicknessTriangles;
}

void AEditableSurface::SetWaypointCount(int32 NewWaypointCount)
{
    if (NewWaypointCount != WaypointCount)
    {
        WaypointCount = NewWaypointCount;
        InitializeDefaultWaypoints();
        UpdateSplineFromWaypoints();
        TryGenerateMeshInternal();
    }
}

FVector AEditableSurface::GetWaypointPosition(int32 Index) const
{
    if (Index >= 0 && Index < Waypoints.Num())
    {
        return Waypoints[Index].Position;
    }
    return FVector::ZeroVector;
}

bool AEditableSurface::SetWaypointPosition(int32 Index, const FVector& NewPosition)
{
    if (Index >= 0 && Index < Waypoints.Num())
    {
        if (Waypoints[Index].Position != NewPosition)
        {
            Waypoints[Index].Position = NewPosition;
            UpdateSplineFromWaypoints();
            GenerateMesh();
        }
        return true;
    }
    return false;
}

float AEditableSurface::GetWaypointWidth(int32 Index) const
{
    if (Index >= 0 && Index < Waypoints.Num())
    {
        return Waypoints[Index].Width;
    }
    return 0.0f;
}

bool AEditableSurface::SetWaypointWidth(int32 Index, float NewWidth)
{
    if (Index >= 0 && Index < Waypoints.Num())
    {
        if (Waypoints[Index].Width != NewWidth)
        {
            Waypoints[Index].Width = NewWidth;
            GenerateMesh();
        }
        return true;
    }
    return false;
}

void AEditableSurface::SetSurfaceWidth(float NewSurfaceWidth)
{
    if (NewSurfaceWidth != SurfaceWidth)
    {
        SurfaceWidth = NewSurfaceWidth;
        for (auto& WP : Waypoints) WP.Width = SurfaceWidth;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetEnableThickness(bool bNewEnableThickness)
{
    if (bEnableThickness != bNewEnableThickness)
    {
        bEnableThickness = bNewEnableThickness;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetThicknessValue(float NewThicknessValue)
{
    if (ThicknessValue != NewThicknessValue)
    {
        ThicknessValue = NewThicknessValue;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetSideSmoothness(int32 NewSideSmoothness)
{
    if (SideSmoothness != NewSideSmoothness)
    {
        SideSmoothness = NewSideSmoothness;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetRightSlopeLength(float NewValue)
{
    if (RightSlopeLength != NewValue)
    {
        RightSlopeLength = NewValue;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetRightSlopeGradient(float NewValue)
{
    if (RightSlopeGradient != NewValue)
    {
        RightSlopeGradient = NewValue;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetLeftSlopeLength(float NewValue)
{
    if (LeftSlopeLength != NewValue)
    {
        LeftSlopeLength = NewValue;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetLeftSlopeGradient(float NewValue)
{
    if (LeftSlopeGradient != NewValue)
    {
        LeftSlopeGradient = NewValue;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetPathSampleCount(int32 NewValue)
{
    if (PathSampleCount != NewValue)
    {
        PathSampleCount = NewValue;
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetCurveType(ESurfaceCurveType NewCurveType)
{
    if (CurveType != NewCurveType)
    {
        CurveType = NewCurveType;
        UpdateSplineFromWaypoints();
        TryGenerateMeshInternal();
    }
}

void AEditableSurface::SetTextureMapping(ESurfaceTextureMapping NewTextureMapping)
{
    if (TextureMapping != NewTextureMapping)
    {
        TextureMapping = NewTextureMapping;
        TryGenerateMeshInternal();
    }
}