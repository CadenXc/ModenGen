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
        Waypoints.Add(FSurfaceWaypoint(FVector(0, 0, 0), -1.0f));
        Waypoints.Add(FSurfaceWaypoint(FVector(500, 0, 30.0), -1.0f));
        Waypoints.Add(FSurfaceWaypoint(FVector(1000, 0, 90.0), -1.0f));
    }
}

void AEditableSurface::RebuildSplineData()
{
    if (!SplineComponent) return;

    SplineComponent->ClearSplinePoints(false);

    if (Waypoints.Num() < 2) return;

    auto GetActualWidth = [&](float WPWidth) -> float {
        return (WPWidth > 0.0f) ? WPWidth : SurfaceWidth;
    };

    if (CurveType == ESurfaceCurveType::Standard)
    {
        for (int32 i = 0; i < Waypoints.Num(); ++i)
        {
            SplineComponent->AddSplinePoint(Waypoints[i].Position, ESplineCoordinateSpace::Local, false);
            SplineComponent->SetSplinePointType(i, ESplinePointType::Curve, false);
            
            float ActualWidth = GetActualWidth(Waypoints[i].Width);
            SplineComponent->SetScaleAtSplinePoint(i, FVector(1.0f, ActualWidth, 1.0f), false);
        }
    }
    else 
    {
        SplineComponent->AddSplinePoint(Waypoints[0].Position, ESplineCoordinateSpace::Local, false);
        SplineComponent->SetSplinePointType(0, ESplinePointType::Curve, false);
        
        float StartWidth = GetActualWidth(Waypoints[0].Width);
        SplineComponent->SetScaleAtSplinePoint(0, FVector(1.0f, StartWidth, 1.0f), false);

        for (int32 i = 0; i < Waypoints.Num() - 1; ++i)
        {
            const FSurfaceWaypoint& P0 = Waypoints[i];
            const FSurfaceWaypoint& P1 = Waypoints[i+1];

            FVector MidPos = (P0.Position + P1.Position) * 0.5f;

            float W0 = GetActualWidth(P0.Width);
            float W1 = GetActualWidth(P1.Width);
            float MidWidth = (W0 + W1) * 0.5f;

            SplineComponent->AddSplinePoint(MidPos, ESplineCoordinateSpace::Local, false);
            int32 NewIdx = SplineComponent->GetNumberOfSplinePoints() - 1;
            
            SplineComponent->SetSplinePointType(NewIdx, ESplinePointType::Curve, false);
            SplineComponent->SetScaleAtSplinePoint(NewIdx, FVector(1.0f, MidWidth, 1.0f), false);
        }

        SplineComponent->AddSplinePoint(Waypoints.Last().Position, ESplineCoordinateSpace::Local, false);
        int32 LastIdx = SplineComponent->GetNumberOfSplinePoints() - 1;
        SplineComponent->SetSplinePointType(LastIdx, ESplinePointType::Curve, false);
        
        float EndWidth = GetActualWidth(Waypoints.Last().Width);
        SplineComponent->SetScaleAtSplinePoint(LastIdx, FVector(1.0f, EndWidth, 1.0f), false);
    }

    SplineComponent->UpdateSpline();
}

void AEditableSurface::UpdateSplineFromWaypoints()
{
    RebuildSplineData();
    GenerateMesh();
}

void AEditableSurface::UpdateWaypointsFromSpline()
{
    if (!SplineComponent) return;

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
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
        }
    }
}

bool AEditableSurface::TryGenerateMeshInternal()
{
    if (SplineComponent && SplineComponent->GetNumberOfSplinePoints() < 2 && Waypoints.Num() >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("AEditableSurface: 检测到样条线数据丢失，正在恢复..."));
        RebuildSplineData();
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
        UE_LOG(LogTemp, Log, TEXT("WP[%d]: Pos=%s, Width=%.2f"),
            i, *Waypoints[i].Position.ToString(), Waypoints[i].Width);
    }
}

void AEditableSurface::PrintParametersInfo()
{
    UE_LOG(LogTemp, Warning, TEXT("========== EditableSurface 参数信息 =========="));
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    UE_LOG(LogTemp, Warning, TEXT("【几何参数】"));
    UE_LOG(LogTemp, Warning, TEXT("  曲面宽度: %.2f"), SurfaceWidth);
    UE_LOG(LogTemp, Warning, TEXT("  采样精度(步长): %.2f"), SplineSampleStep);
    UE_LOG(LogTemp, Warning, TEXT("  去环检测距离: %.2f"), LoopRemovalThreshold);
    UE_LOG(LogTemp, Warning, TEXT("  曲线类型: %d"), (int32)CurveType);
    UE_LOG(LogTemp, Warning, TEXT("  纹理映射: %d"), (int32)TextureMapping);
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    UE_LOG(LogTemp, Warning, TEXT("【厚度参数】"));
    UE_LOG(LogTemp, Warning, TEXT("  启用厚度: %s"), bEnableThickness ? TEXT("是") : TEXT("否"));
    if (bEnableThickness)
    {
        UE_LOG(LogTemp, Warning, TEXT("  厚度值: %.2f"), ThicknessValue);
    }
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    UE_LOG(LogTemp, Warning, TEXT("【平滑参数】"));
    UE_LOG(LogTemp, Warning, TEXT("  两侧平滑度: %d"), SideSmoothness);
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    UE_LOG(LogTemp, Warning, TEXT("【护坡参数】"));
    if (SideSmoothness >= 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("  右侧护坡长度: %.2f"), RightSlopeLength);
        UE_LOG(LogTemp, Warning, TEXT("  右侧护坡斜率: %.2f"), RightSlopeGradient);
        UE_LOG(LogTemp, Warning, TEXT("  左侧护坡长度: %.2f"), LeftSlopeLength);
        UE_LOG(LogTemp, Warning, TEXT("  左侧护坡斜率: %.2f"), LeftSlopeGradient);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("  (平滑度 < 1，护坡未启用)"));
    }
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    UE_LOG(LogTemp, Warning, TEXT("【路点信息】"));
    UE_LOG(LogTemp, Warning, TEXT("  路点数量: %d"), Waypoints.Num());
    if (SplineComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("  样条线点数: %d"), SplineComponent->GetNumberOfSplinePoints());
        UE_LOG(LogTemp, Warning, TEXT("  样条线长度: %.2f"), SplineComponent->GetSplineLength());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("  样条线组件: 无效"));
    }
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    UE_LOG(LogTemp, Warning, TEXT("【安全设置】"));
    UE_LOG(LogTemp, Warning, TEXT(""));
    
    UE_LOG(LogTemp, Warning, TEXT("=========================================="));
}

void AEditableSurface::SetSurfaceWidth(float NewSurfaceWidth)
{
    SurfaceWidth = NewSurfaceWidth;

    UpdateSplineFromWaypoints();
    
    // UpdateSplineFromWaypoints 内部已经调用了 GenerateMesh，所以这里不需要重复调用
}

void AEditableSurface::SetSplineSampleStep(float NewStep)
{
    // 限制最小值防止死循环或显存爆炸
    SplineSampleStep = FMath::Max(NewStep, 5.0f);
    GenerateMesh();
}

void AEditableSurface::SetLoopRemovalThreshold(float NewValue)
{
    LoopRemovalThreshold = FMath::Max(NewValue, 1.0f);
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