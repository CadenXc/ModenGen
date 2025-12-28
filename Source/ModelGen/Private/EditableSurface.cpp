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
        // 【关键修改】设置为 Movable，确保运行时更新样条线能正确触发相关组件更新
        SplineComponent->SetMobility(EComponentMobility::Movable);
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
        // 【修改】使用 -1.0f 让它们默认跟随全局宽度
        Waypoints.Add(FSurfaceWaypoint(FVector(0, 0, 0), -1.0f));
        Waypoints.Add(FSurfaceWaypoint(FVector(500, 400, 0.0), -1.0f));
        Waypoints.Add(FSurfaceWaypoint(FVector(1000, 0, 0.0), -1.0f));
    }
}

void AEditableSurface::RelaxWaypoints()
{
    if (Waypoints.Num() < 3) return; // 至少需要3个点才能进行松弛

    // 计算最小距离：路面宽度的1.2倍
    float MinDist = SurfaceWidth * 1.2f;
    float MinDistSquared = MinDist * MinDist;

    const int32 MaxIterations = 10;
    
    for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
    {
        bool bAnyMoved = false;

        // 遍历所有相邻的三元组 (P0, P1, P2)
        for (int32 i = 0; i < Waypoints.Num() - 2; ++i)
        {
            FVector& P0 = Waypoints[i].Position;
            FVector& P1 = Waypoints[i + 1].Position;
            FVector& P2 = Waypoints[i + 2].Position;

            // 检查 P1 和 P2 是否太近
            FVector Delta = P2 - P1;
            float DistSquared = Delta.SizeSquared();

            if (DistSquared < MinDistSquared)
            {
                bAnyMoved = true;
                
                // 计算需要移动的距离
                float CurrentDist = FMath::Sqrt(DistSquared);
                float PushDistance = (MinDist - CurrentDist) * 0.5f; // 各移动一半
                
                if (CurrentDist > KINDA_SMALL_NUMBER)
                {
                    FVector PushDir = Delta / CurrentDist;
                    
                    // 第一轮迭代：优先移动 P2
                    // 后续迭代：同时移动 P1 和 P2
                    if (Iter == 0)
                    {
                        P2 += PushDir * PushDistance * 2.0f; // P2 移动全部距离
                    }
                    else
                    {
                        P1 -= PushDir * PushDistance;
                        P2 += PushDir * PushDistance;
                    }
                }
                else
                {
                    // 如果两点重合，沿 P0->P1 方向推开
                    FVector Dir = (P1 - P0).GetSafeNormal();
                    if (Dir.IsZero())
                    {
                        Dir = FVector::ForwardVector;
                    }
                    
                    if (Iter == 0)
                    {
                        P2 = P1 + Dir * MinDist;
                    }
                    else
                    {
                        P1 -= Dir * PushDistance;
                        P2 = P1 + Dir * MinDist;
                    }
                }
            }
        }

        // 如果没有移动任何点，提前退出
        if (!bAnyMoved)
        {
            break;
        }
    }
}

void AEditableSurface::UpdateSplineFromWaypoints()
{
    if (!SplineComponent) return;

    // 自动松弛路点（如果启用）
    if (bAutoRelaxWaypoints)
    {
        RelaxWaypoints();
    }

    SplineComponent->ClearSplinePoints(false);

    if (Waypoints.Num() < 2) return;

    // 辅助 Lambda：获取实际宽度
    // 如果路点宽度 <= 0，则使用全局 SurfaceWidth，否则使用路点自定义宽度
    auto GetActualWidth = [&](float WPWidth) -> float {
        return (WPWidth > 0.0f) ? WPWidth : SurfaceWidth;
    };

    // 【标准曲线模式】：曲线必须严格穿过每一个路点 (Interpolating)
    if (CurveType == ESurfaceCurveType::Standard)
    {
        for (int32 i = 0; i < Waypoints.Num(); ++i)
        {
            SplineComponent->AddSplinePoint(Waypoints[i].Position, ESplineCoordinateSpace::Local, false);
            
            // 设置类型为 Curve (Catmull-Rom)，确保平滑穿过
            SplineComponent->SetSplinePointType(i, ESplinePointType::Curve, false);
            
            // 【关键修改】使用计算后的实际宽度
            float ActualWidth = GetActualWidth(Waypoints[i].Width);
            SplineComponent->SetScaleAtSplinePoint(i, FVector(1.0f, ActualWidth, 1.0f), false);
        }
    }
    // --- 光滑模式：中点逼近算法 (Midpoint Approximation) ---
    // 这种算法生成的曲线最平滑，且点数少，非常适合赛道
    else 
    {
        // 1. 添加起点 (保持不变)
        SplineComponent->AddSplinePoint(Waypoints[0].Position, ESplineCoordinateSpace::Local, false);
        // 起点设为 Curve 以保证起始方向准确
        SplineComponent->SetSplinePointType(0, ESplinePointType::Curve, false);
        
        // 【关键修改】起点宽度
        float StartWidth = GetActualWidth(Waypoints[0].Width);
        SplineComponent->SetScaleAtSplinePoint(0, FVector(1.0f, StartWidth, 1.0f), false);

        // 2. 添加中间点：取相邻路点的中点
        // 这样样条线会"悬空"在路点连线之间，完美切过弯角
        for (int32 i = 0; i < Waypoints.Num() - 1; ++i)
        {
            const FSurfaceWaypoint& P0 = Waypoints[i];
            const FSurfaceWaypoint& P1 = Waypoints[i+1];

            // 计算中点 (50% 处)
            FVector MidPos = (P0.Position + P1.Position) * 0.5f;

            // 【关键修改】计算两个点的实际宽度，然后取平均
            float W0 = GetActualWidth(P0.Width);
            float W1 = GetActualWidth(P1.Width);
            float MidWidth = (W0 + W1) * 0.5f;

            SplineComponent->AddSplinePoint(MidPos, ESplineCoordinateSpace::Local, false);
            int32 NewIdx = SplineComponent->GetNumberOfSplinePoints() - 1;
            
            // 使用 Curve 类型，UE 会自动让曲线圆滑地通过中点
            SplineComponent->SetSplinePointType(NewIdx, ESplinePointType::Curve, false);
            
            // 设置中间点宽度
            SplineComponent->SetScaleAtSplinePoint(NewIdx, FVector(1.0f, MidWidth, 1.0f), false);
        }

        // 3. 添加终点 (保持不变)
        SplineComponent->AddSplinePoint(Waypoints.Last().Position, ESplineCoordinateSpace::Local, false);
        int32 LastIdx = SplineComponent->GetNumberOfSplinePoints() - 1;
        SplineComponent->SetSplinePointType(LastIdx, ESplinePointType::Curve, false);
        
        // 【关键修改】终点宽度
        float EndWidth = GetActualWidth(Waypoints.Last().Width);
        SplineComponent->SetScaleAtSplinePoint(LastIdx, FVector(1.0f, EndWidth, 1.0f), false);
    }

    SplineComponent->UpdateSpline();
    
    // 【关键修复】更新完样条线后，必须立即触发网格重建！
    // 否则从外部（如TS/蓝图）调用 UpdateSplineFromWaypoints 后，画面不会有任何变化。
    // 注意：在 OnConstruction 中会再次调用 GenerateMesh，但这是幂等的，不会造成问题。
    GenerateMesh();
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
        
        Waypoints[i].Position = Pos;
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
    // 【关键修复】防御性检查：如果有路点但样条线没数据（可能因为时序问题或错误的Clear），强制恢复
    if (SplineComponent && SplineComponent->GetNumberOfSplinePoints() < 2 && Waypoints.Num() >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("AEditableSurface: 检测到样条线数据丢失，正在尝试从 Waypoints 恢复..."));
        
        // 直接重建 Spline 数据，避免调用 UpdateSplineFromWaypoints（防止递归）
        SplineComponent->ClearSplinePoints(false);
        
        // 定义 Lambda (与 UpdateSplineFromWaypoints 中的逻辑一致)
        auto GetActualWidth = [&](float WPWidth) -> float {
            return (WPWidth > 0.0f) ? WPWidth : SurfaceWidth;
        };
        
        if (CurveType == ESurfaceCurveType::Standard)
        {
            for (int32 i = 0; i < Waypoints.Num(); ++i)
            {
                SplineComponent->AddSplinePoint(Waypoints[i].Position, ESplineCoordinateSpace::Local, false);
                SplineComponent->SetSplinePointType(i, ESplinePointType::Curve, false);
                
                // 【修改】使用计算后的实际宽度
                float ActualWidth = GetActualWidth(Waypoints[i].Width);
                SplineComponent->SetScaleAtSplinePoint(i, FVector(1.0f, ActualWidth, 1.0f), false);
            }
        }
        else
        {
            // Smooth 模式：中点逼近算法
            SplineComponent->AddSplinePoint(Waypoints[0].Position, ESplineCoordinateSpace::Local, false);
            SplineComponent->SetSplinePointType(0, ESplinePointType::Curve, false);
            
            // 【修改】起点宽度
            float StartWidth = GetActualWidth(Waypoints[0].Width);
            SplineComponent->SetScaleAtSplinePoint(0, FVector(1.0f, StartWidth, 1.0f), false);
            
            for (int32 i = 0; i < Waypoints.Num() - 1; ++i)
            {
                FVector MidPos = (Waypoints[i].Position + Waypoints[i + 1].Position) * 0.5f;
                
                // 【修改】计算两个点的实际宽度，然后取平均
                float W0 = GetActualWidth(Waypoints[i].Width);
                float W1 = GetActualWidth(Waypoints[i + 1].Width);
                float MidWidth = (W0 + W1) * 0.5f;
                
                SplineComponent->AddSplinePoint(MidPos, ESplineCoordinateSpace::Local, false);
                int32 NewIdx = SplineComponent->GetNumberOfSplinePoints() - 1;
                SplineComponent->SetSplinePointType(NewIdx, ESplinePointType::Curve, false);
                SplineComponent->SetScaleAtSplinePoint(NewIdx, FVector(1.0f, MidWidth, 1.0f), false);
            }
            
            SplineComponent->AddSplinePoint(Waypoints.Last().Position, ESplineCoordinateSpace::Local, false);
            int32 LastIdx = SplineComponent->GetNumberOfSplinePoints() - 1;
            SplineComponent->SetSplinePointType(LastIdx, ESplinePointType::Curve, false);
            
            // 【修改】终点宽度
            float EndWidth = GetActualWidth(Waypoints.Last().Width);
            SplineComponent->SetScaleAtSplinePoint(LastIdx, FVector(1.0f, EndWidth, 1.0f), false);
        }
        
        SplineComponent->UpdateSpline();
    }
    
    // 检查样条线是否有足够的数据点
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2 || Waypoints.Num() < 2)
    {
        return false;
    }

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
        
        // 【修改】新加的点默认继承全局宽度 (-1.0f)
        NewWP.Width = -1.0f;
        
        Waypoints.Add(NewWP);
    }
    else
    {
        Waypoints.Add(FSurfaceWaypoint(FVector::ZeroVector, -1.0f));
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
        UE_LOG(LogTemp, Log, TEXT("WP[%d]: Pos=%s"),
            i, *Waypoints[i].Position.ToString());
    }
}

void AEditableSurface::SetSurfaceWidth(float NewSurfaceWidth)
{
    SurfaceWidth = NewSurfaceWidth;

    // 【关键修改】全局宽度改变了，必须重新计算样条线上所有"继承宽度"点的 Scale
    UpdateSplineFromWaypoints();
    
    // UpdateSplineFromWaypoints 内部已经调用了 GenerateMesh，所以这里不需要重复调用
}

void AEditableSurface::SetSplineSampleStep(float NewStep)
{
    // 限制最小值防止死循环或显存爆炸
    SplineSampleStep = FMath::Max(NewStep, 5.0f);
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
    // 基于自适应采样估算：使用样条长度和路点数量
    int32 CrossSectionPoints = 2 + (SideSmoothness * 2);
    
    // 估算采样点数量：基于路点数量和样条长度
    int32 NumWaypoints = Waypoints.Num();
    float SplineLength = SplineComponent ? SplineComponent->GetSplineLength() : 1000.0f;
    
    // 保守估算：每100单位长度至少1个采样点，每个路点之间至少2个采样点
    int32 EstimatedSamples = FMath::Max(
        FMath::CeilToInt(SplineLength / 100.0f),
        NumWaypoints * 2
    );
    
    return EstimatedSamples * CrossSectionPoints * (bEnableThickness ? 2 : 1);
}

int32 AEditableSurface::CalculateTriangleCountEstimate() const
{
    return CalculateVertexCountEstimate() * 2; // 粗略估算三角形数
}

void AEditableSurface::SetWaypoints(const FString& WaypointsString)
{
    if (WaypointsString.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("AEditableSurface::SetWaypoints - 输入字符串为空"));
        return;
    }

    // 清空现有路点
    Waypoints.Empty();

    // 解析字符串格式：x1,y1,z1;x2,y2,z2;...
    // 使用分号分隔不同的路点，逗号分隔坐标
    TArray<FString> WaypointStrings;
    WaypointsString.ParseIntoArray(WaypointStrings, TEXT(";"), true);

    for (const FString& WaypointStr : WaypointStrings)
    {
        if (WaypointStr.IsEmpty())
        {
            continue;
        }

        // 解析坐标：x,y,z
        TArray<FString> Coords;
        WaypointStr.ParseIntoArray(Coords, TEXT(","), true);

        if (Coords.Num() >= 3)
        {
            float X = FCString::Atof(*Coords[0]);
            float Y = FCString::Atof(*Coords[1]);
            float Z = FCString::Atof(*Coords[2]);
            
            Waypoints.Add(FSurfaceWaypoint(FVector(X, Y, Z)));
        }
        else if (Coords.Num() == 2)
        {
            // 如果只有两个坐标，Z 默认为 0
            float X = FCString::Atof(*Coords[0]);
            float Y = FCString::Atof(*Coords[1]);
            
            Waypoints.Add(FSurfaceWaypoint(FVector(X, Y, 0.0f)));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AEditableSurface::SetWaypoints - 无效的路点格式: %s"), *WaypointStr);
        }
    }

    // 如果解析成功且路点数量足够，更新样条线和网格
    if (Waypoints.Num() >= 2)
    {
        // 这里会触发 UpdateSplineFromWaypoints -> GenerateMesh
        UpdateSplineFromWaypoints();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AEditableSurface::SetWaypoints - 路点数量不足（需要至少2个），当前: %d"), Waypoints.Num());
    }
}