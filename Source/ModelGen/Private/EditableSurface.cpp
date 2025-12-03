// Copyright (c) 2024. All rights reserved.

#include "EditableSurface.h"
#include "EditableSurfaceBuilder.h"
#include "ModelGenMeshData.h"

AEditableSurface::AEditableSurface()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建样条线组件
    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    if (SplineComponent)
    {
        SplineComponent->SetupAttachment(RootComponent);
        SplineComponent->SetMobility(EComponentMobility::Static);
    }

    // 初始化默认路点
    InitializeDefaultWaypoints();
}

void AEditableSurface::OnConstruction(const FTransform& Transform)
{
    // 调用父类的 OnConstruction (父类会初始化 PMC 等)
    Super::OnConstruction(Transform);

    // 确保数据同步
    UpdateSplineFromWaypoints();

    // 生成网格
    GenerateMesh();
}

void AEditableSurface::InitializeDefaultWaypoints()
{
    if (Waypoints.Num() == 0)
    {
        Waypoints.Add(FSurfaceWaypoint(FVector(0, 0, 0), 100.0f));
        Waypoints.Add(FSurfaceWaypoint(FVector(500, 0, 0), 100.0f));
        Waypoints.Add(FSurfaceWaypoint(FVector(1000, 200, 50), 100.0f));
    }
}

void AEditableSurface::UpdateSplineFromWaypoints()
{
    if (!SplineComponent) return;

    SplineComponent->ClearSplinePoints(false);

    if (Waypoints.Num() < 2) return;

    // 【标准曲线模式】：曲线必须严格穿过每一个路点 (Interpolating)
    if (CurveType == ESurfaceCurveType::Standard)
    {
        for (int32 i = 0; i < Waypoints.Num(); ++i)
        {
            SplineComponent->AddSplinePoint(Waypoints[i].Position, ESplineCoordinateSpace::Local, false);
            
            // 设置宽度 (Scale X)
            FVector Scale(Waypoints[i].Width, 1.0f, 1.0f);
            SplineComponent->SetScaleAtSplinePoint(i, Scale, false);
            
            // 设置类型为 Curve (Catmull-Rom)，确保平滑穿过
            SplineComponent->SetSplinePointType(i, ESplinePointType::Curve, false);
        }
    }
    // --- 光滑模式：中点逼近算法 (Midpoint Approximation) ---
    // 这种算法生成的曲线最平滑，且点数少，非常适合赛道
    else 
    {
        // 1. 添加起点 (保持不变)
        SplineComponent->AddSplinePoint(Waypoints[0].Position, ESplineCoordinateSpace::Local, false);
        SplineComponent->SetScaleAtSplinePoint(0, FVector(Waypoints[0].Width, 1.0f, 1.0f), false);
        // 起点设为 Curve 以保证起始方向准确
        SplineComponent->SetSplinePointType(0, ESplinePointType::Curve, false); 

        // 2. 添加中间点：取相邻路点的中点
        // 这样样条线会"悬空"在路点连线之间，完美切过弯角
        for (int32 i = 0; i < Waypoints.Num() - 1; ++i)
        {
            const FSurfaceWaypoint& P0 = Waypoints[i];
            const FSurfaceWaypoint& P1 = Waypoints[i+1];

            // 计算中点 (50% 处)
            FVector MidPos = (P0.Position + P1.Position) * 0.5f;
            float MidWidth = (P0.Width + P1.Width) * 0.5f;

            SplineComponent->AddSplinePoint(MidPos, ESplineCoordinateSpace::Local, false);
            int32 NewIdx = SplineComponent->GetNumberOfSplinePoints() - 1;
            SplineComponent->SetScaleAtSplinePoint(NewIdx, FVector(MidWidth, 1.0f, 1.0f), false);
            
            // 使用 Curve 类型，UE 会自动让曲线圆滑地通过中点
            SplineComponent->SetSplinePointType(NewIdx, ESplinePointType::Curve, false);
        }

        // 3. 添加终点 (保持不变)
        SplineComponent->AddSplinePoint(Waypoints.Last().Position, ESplineCoordinateSpace::Local, false);
        int32 LastIdx = SplineComponent->GetNumberOfSplinePoints() - 1;
        SplineComponent->SetScaleAtSplinePoint(LastIdx, FVector(Waypoints.Last().Width, 1.0f, 1.0f), false);
        SplineComponent->SetSplinePointType(LastIdx, ESplinePointType::Curve, false);
    }

    SplineComponent->UpdateSpline();
}

void AEditableSurface::UpdateWaypointsFromSpline()
{
    if (!SplineComponent) return;

    // 【重要修复】光滑模式下，样条线是生成的，绝不能反向写入路点！
    // 否则路点会被吸附到曲线上，导致控制杆丢失，所有平滑效果失效。
    if (CurveType != ESurfaceCurveType::Standard)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateWaypointsFromSpline blocked in Smooth Mode to prevent data collapse."));
        return;
    }

    int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
    // 只有数量一致时才尝试同步，避免误操作
    if (NumPoints != Waypoints.Num())
    {
        return; 
    }

    for (int32 i = 0; i < NumPoints; ++i)
    {
        FVector Pos = SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
        FVector Scale = SplineComponent->GetScaleAtSplinePoint(i);
        
        Waypoints[i].Position = Pos;
        Waypoints[i].Width = Scale.X;
    }
}

void AEditableSurface::GenerateMesh()
{
    if (!TryGenerateMeshInternal())
    {
        // [修复] 使用继承自父类的 ProceduralMeshComponent
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
        }
    }
}

bool AEditableSurface::TryGenerateMeshInternal()
{
    if (!SplineComponent || Waypoints.Num() < 2) return false;

    FEditableSurfaceBuilder Builder(*this);

    FModelGenMeshData MeshData;

    // 预分配内存以优化性能
    int32 EstVerts = Builder.CalculateVertexCountEstimate();
    int32 EstTris = Builder.CalculateTriangleCountEstimate();
    MeshData.Reserve(EstVerts, EstTris);

    if (Builder.Generate(MeshData))
    {
        // [修复] 使用继承自父类的 ProceduralMeshComponent
        if (ProceduralMeshComponent)
        {
            // 应用生成的网格数据到组件
            MeshData.ToProceduralMesh(ProceduralMeshComponent, 0);

            // 设置材质（使用新添加的 Material 属性，或父类的 ProceduralDefaultMaterial）
            if (Material)
            {
                ProceduralMeshComponent->SetMaterial(0, Material);
            }
            else if (ProceduralDefaultMaterial)
            {
                // 如果用户没有指定特殊材质，使用父类加载的默认材质
                ProceduralMeshComponent->SetMaterial(0, ProceduralDefaultMaterial);
            }
        }

        return true;
    }

    return false;
}

// ---------------- Getters and Setters ----------------

FVector AEditableSurface::GetWaypointPosition(int32 Index) const
{
    if (Waypoints.IsValidIndex(Index))
    {
        return Waypoints[Index].Position;
    }
    return FVector::ZeroVector;
}

bool AEditableSurface::SetWaypointPosition(int32 Index, const FVector& NewPosition)
{
    if (Waypoints.IsValidIndex(Index))
    {
        Waypoints[Index].Position = NewPosition;
        UpdateSplineFromWaypoints();
        GenerateMesh();
        return true;
    }
    return false;
}

float AEditableSurface::GetWaypointWidth(int32 Index) const
{
    if (Waypoints.IsValidIndex(Index))
    {
        return Waypoints[Index].Width;
    }
    return 0.0f;
}

bool AEditableSurface::SetWaypointWidth(int32 Index, float NewWidth)
{
    if (Waypoints.IsValidIndex(Index))
    {
        Waypoints[Index].Width = NewWidth;
        UpdateSplineFromWaypoints();
        GenerateMesh();
        return true;
    }
    return false;
}

void AEditableSurface::SetWaypointCount(int32 NewWaypointCount)
{
    if (NewWaypointCount < 2) NewWaypointCount = 2;

    if (NewWaypointCount != Waypoints.Num())
    {
        if (NewWaypointCount > Waypoints.Num())
        {
            // 增加路点：在末尾延伸
            FSurfaceWaypoint LastWP = Waypoints.Last();
            int32 AddCount = NewWaypointCount - Waypoints.Num();
            for (int32 i = 0; i < AddCount; ++i)
            {
                LastWP.Position.X += 100.0f;
                Waypoints.Add(LastWP);
            }
        }
        else
        {
            // 减少路点
            Waypoints.SetNum(NewWaypointCount);
        }

        UpdateSplineFromWaypoints();
        GenerateMesh();
    }
}

void AEditableSurface::SetPathSampleCount(int32 NewValue)
{
    PathSampleCount = FMath::Clamp(NewValue, 2, 1000);
    GenerateMesh();
}

void AEditableSurface::AddNewWaypoint()
{
    if (Waypoints.Num() > 0)
    {
        FSurfaceWaypoint NewWP = Waypoints.Last();
        // 尝试沿最后两个点的方向延伸
        if (Waypoints.Num() >= 2)
        {
            FVector Dir = (Waypoints.Last().Position - Waypoints[Waypoints.Num() - 2].Position).GetSafeNormal();
            if (Dir.IsZero()) Dir = FVector::ForwardVector;
            NewWP.Position += Dir * 100.0f;
        }
        else
        {
            NewWP.Position += FVector(100, 0, 0);
        }
        Waypoints.Add(NewWP);
    }
    else
    {
        Waypoints.Add(FSurfaceWaypoint(FVector::ZeroVector));
    }

    UpdateSplineFromWaypoints();
    GenerateMesh();
}

void AEditableSurface::RemoveWaypoint()
{
    RemoveWaypointByIndex(RemoveWaypointIndex);
}

void AEditableSurface::RemoveWaypointByIndex(int32 Index)
{
    if (Waypoints.IsValidIndex(Index) && Waypoints.Num() > 2)
    {
        Waypoints.RemoveAt(Index);
        UpdateSplineFromWaypoints();
        GenerateMesh();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot remove waypoint: Index invalid or too few waypoints left."));
    }
}

void AEditableSurface::PrintWaypointInfo()
{
    for (int32 i = 0; i < Waypoints.Num(); ++i)
    {
        UE_LOG(LogTemp, Log, TEXT("WP[%d]: Pos=%s, Width=%f"),
            i, *Waypoints[i].Position.ToString(), Waypoints[i].Width);
    }
}

void AEditableSurface::SetSurfaceWidth(float NewSurfaceWidth)
{
    SurfaceWidth = NewSurfaceWidth;
    GenerateMesh();
}

void AEditableSurface::SetEnableThickness(bool bNewEnableThickness)
{
    bEnableThickness = bNewEnableThickness;
    GenerateMesh();
}

void AEditableSurface::SetThicknessValue(float NewThicknessValue)
{
    ThicknessValue = NewThicknessValue;
    GenerateMesh();
}

void AEditableSurface::SetSideSmoothness(int32 NewSideSmoothness)
{
    SideSmoothness = NewSideSmoothness;
    GenerateMesh();
}

void AEditableSurface::SetRightSlopeLength(float NewValue)
{
    RightSlopeLength = NewValue;
    GenerateMesh();
}

void AEditableSurface::SetRightSlopeGradient(float NewValue)
{
    RightSlopeGradient = NewValue;
    GenerateMesh();
}

void AEditableSurface::SetLeftSlopeLength(float NewValue)
{
    LeftSlopeLength = NewValue;
    GenerateMesh();
}

void AEditableSurface::SetLeftSlopeGradient(float NewValue)
{
    LeftSlopeGradient = NewValue;
    GenerateMesh();
}

void AEditableSurface::SetCurveType(ESurfaceCurveType NewCurveType)
{
    CurveType = NewCurveType;
    UpdateSplineFromWaypoints();
    GenerateMesh();
}

void AEditableSurface::SetTextureMapping(ESurfaceTextureMapping NewTextureMapping)
{
    TextureMapping = NewTextureMapping;
    GenerateMesh();
}

bool AEditableSurface::IsValid() const
{
    return SplineComponent != nullptr && Waypoints.Num() >= 2;
}

int32 AEditableSurface::CalculateVertexCountEstimate() const
{
    // 粗略估算
    return PathSampleCount * (2 + SideSmoothness * 2) * (bEnableThickness ? 2 : 1);
}

int32 AEditableSurface::CalculateTriangleCountEstimate() const
{
    return CalculateVertexCountEstimate() * 2; // 粗略估算三角形数
}