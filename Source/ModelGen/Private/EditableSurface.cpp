// Copyright (c) 2024. All rights reserved.

#include "EditableSurface.h"
#include "EditableSurfaceBuilder.h"
#include "ModelGenMeshData.h"

AEditableSurface::AEditableSurface()
{
    PrimaryActorTick.bCanEverTick = false;

    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    if (SplineComponent)
    {
        SplineComponent->SetupAttachment(RootComponent);
        SplineComponent->SetMobility(EComponentMobility::Movable);
    }

    InitializeDefaultWaypoints();
}

void AEditableSurface::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateSplineFromWaypoints();
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
        return;
    }

    int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
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
        RebuildSplineData();
    }
    
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2 || Waypoints.Num() < 2)
    {
        return false;
    }

    FEditableSurfaceBuilder Builder(*this);
    FModelGenMeshData MeshData;

    int32 EstVerts = Builder.CalculateVertexCountEstimate();
    int32 EstTris = Builder.CalculateTriangleCountEstimate();
    MeshData.Reserve(EstVerts, EstTris);

    if (Builder.Generate(MeshData))
    {
        if (ProceduralMeshComponent)
        {
            MeshData.ToProceduralMesh(ProceduralMeshComponent, 0);
        }

        return true;
    }

    return false;
}


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

void AEditableSurface::SetWaypointWidth(int32 Index, float NewWidth)
{
    if (Waypoints.IsValidIndex(Index))
    {
        if (!FMath::IsNearlyEqual(Waypoints[Index].Width, NewWidth))
        {
            Waypoints[Index].Width = NewWidth;
            UpdateSplineFromWaypoints();
            GenerateMesh();
        }
    }
}

void AEditableSurface::SetNextWaypointDistance(float NewDistance)
{
    NextWaypointDistance = FMath::Max(NewDistance, 1.0f);
}

void AEditableSurface::AddNewWaypoint()
{
    if (Waypoints.Num() > 0)
    {
        FSurfaceWaypoint NewWP = Waypoints.Last();

        if (Waypoints.Num() >= 2)
        {
            FVector Dir = (Waypoints.Last().Position - Waypoints[Waypoints.Num() - 2].Position).GetSafeNormal();
            if (Dir.IsZero()) Dir = FVector::ForwardVector;

            NewWP.Position += Dir * NextWaypointDistance;
        }
        else
        {
            NewWP.Position += FVector(NextWaypointDistance, 0, 0);
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
}

void AEditableSurface::PrintWaypointInfo()
{
}

void AEditableSurface::PrintParametersInfo()
{
}

void AEditableSurface::SetSurfaceWidth(float NewSurfaceWidth)
{
    SurfaceWidth = NewSurfaceWidth;

    UpdateSplineFromWaypoints();
}

void AEditableSurface::SetSplineSampleStep(float NewStep)
{
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
    int32 CrossSectionPoints = 2 + (SideSmoothness * 2);
    
    int32 NumWaypoints = Waypoints.Num();
    float SplineLength = SplineComponent ? SplineComponent->GetSplineLength() : 1000.0f;
    
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
        return;
    }

    Waypoints.Empty();

    TArray<FString> WaypointStrings;
    WaypointsString.ParseIntoArray(WaypointStrings, TEXT(";"), true);

    for (const FString& WaypointStr : WaypointStrings)
    {
        if (WaypointStr.IsEmpty())
        {
            continue;
        }

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
            float X = FCString::Atof(*Coords[0]);
            float Y = FCString::Atof(*Coords[1]);
            
            Waypoints.Add(FSurfaceWaypoint(FVector(X, Y, 0.0f)));
        }
    }

    if (Waypoints.Num() >= 2)
    {
        UpdateSplineFromWaypoints();
    }
}