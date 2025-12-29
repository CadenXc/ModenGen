// Copyright (c) 2024. All rights reserved.

#include "EditableSurfaceBuilder.h"

#include "ModelGenConstants.h"

#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

FEditableSurfaceBuilder::FEditableSurfaceBuilder(const AEditableSurface& InSurface)
    : Surface(InSurface)
{
    // 缓存配置数据
    SplineComponent = InSurface.SplineComponent;
    SurfaceWidth = InSurface.SurfaceWidth;
    SplineSampleStep = InSurface.SplineSampleStep;
    LoopRemovalThreshold = InSurface.LoopRemovalThreshold;
    
    // 安全检查：防止用户输入过小的值导致崩溃
    if (SplineSampleStep < 1.0f) SplineSampleStep = 10.0f;
    if (LoopRemovalThreshold < 1.0f) LoopRemovalThreshold = 10.0f;

    bEnableThickness = InSurface.bEnableThickness;
    ThicknessValue = InSurface.ThicknessValue;
    SideSmoothness = InSurface.SideSmoothness;
    RightSlopeLength = InSurface.RightSlopeLength;
    RightSlopeGradient = InSurface.RightSlopeGradient;
    LeftSlopeLength = InSurface.LeftSlopeLength;
    LeftSlopeGradient = InSurface.LeftSlopeGradient;
    TextureMapping = InSurface.TextureMapping;

    Clear();
}

void FEditableSurfaceBuilder::Clear()
{
    // 调用父类清理
    FModelGenMeshBuilder::Clear();
    
    // 清理新的缓存数据
    SampledPath.Empty();
    ProfileDefinition.Empty();
    PathCornerData.Empty();
    
    // 清理 Grid 结构信息
    GridStartIndex = 0;
    GridNumRows = 0;
    GridNumCols = 0;
    
    // 清理拉链法数据
    LeftRailRaw.Empty();
    RightRailRaw.Empty();
    LeftRailResampled.Empty();
    RightRailResampled.Empty();
    FinalLeftRail.Empty();
    FinalRightRail.Empty();
    TopStartIndices.Empty();
    TopEndIndices.Empty();
}

int32 FEditableSurfaceBuilder::CalculateVertexCountEstimate() const
{
    // 基于新的网格方案估算
    // 横截面点数：路面2个点 + 左右护坡各 SideSmoothness 个点
    int32 ProfilePointCount = 2 + (SideSmoothness * 2);
    
    // 路径采样点数：基于样条长度，使用 SplineSampleStep
    float SplineLength = SplineComponent ? SplineComponent->GetSplineLength() : 1000.0f;
    int32 PathSampleCount = FMath::CeilToInt(SplineLength / SplineSampleStep) + 1;
    
    // 总顶点数 = 路径采样点数 × 横截面点数
    int32 BaseCount = PathSampleCount * ProfilePointCount;
    
    // 如果启用厚度，需要底面和侧壁
    return bEnableThickness ? BaseCount * 2 + (PathSampleCount * 2) : BaseCount;
}

int32 FEditableSurfaceBuilder::CalculateTriangleCountEstimate() const
{
    return CalculateVertexCountEstimate() * 3;
}

void FEditableSurfaceBuilder::BuildProfileDefinition()
{
    float HalfWidth = SurfaceWidth * 0.5f;
    float GlobalUVScale = ModelGenConstants::GLOBAL_UV_SCALE;
    
    float UVFactor = 0.0f;
    float U_RoadLeft = 0.0f;
    float U_RoadRight = 0.0f;

    if (TextureMapping == ESurfaceTextureMapping::Stretch)
    {
        float SafeWidth = FMath::Max(SurfaceWidth, 0.01f);
        UVFactor = 1.0f / SafeWidth; 
        U_RoadLeft = 0.0f;
        U_RoadRight = 1.0f;
    }
    else
    {
        UVFactor = GlobalUVScale;
        U_RoadLeft = -HalfWidth * UVFactor;
        U_RoadRight = HalfWidth * UVFactor;
    }

    auto AppendSlopePoints = [&](bool bIsRightSide, float Len, float Grad, float StartU)
    {
        if (SideSmoothness <= 0 || Len < KINDA_SMALL_NUMBER) return;

        TArray<FProfilePoint> TempPoints;
        TempPoints.Reserve(SideSmoothness);

        float LastEmittedRelX = 0.0f;
        float LastEmittedRelZ = 0.0f;
        float RunningU = StartU;
        float TotalHeight = Len * Grad;

        const float MergeThresholdSq = 1.0f;

        for (int32 i = 1; i <= SideSmoothness; ++i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SideSmoothness);
            bool bIsLastPoint = (i == SideSmoothness);

            float TargetRelX = Len * Ratio;
            float TargetRelZ;
            
            if (SideSmoothness == 1) TargetRelZ = TotalHeight * Ratio;
            else TargetRelZ = TotalHeight * (Ratio * Ratio);

            float DistSq = FMath::Square(TargetRelX - LastEmittedRelX) + FMath::Square(TargetRelZ - LastEmittedRelZ);

            if (DistSq < MergeThresholdSq)
            {
                if (bIsLastPoint && TempPoints.Num() > 0)
                {
                    float SegDist = FMath::Sqrt(DistSq); 
                    float ExtraUV = SegDist * (TextureMapping == ESurfaceTextureMapping::Stretch ? GlobalUVScale : UVFactor);
                    
                    if (bIsRightSide) TempPoints.Last().U += ExtraUV;
                    else              TempPoints.Last().U -= ExtraUV;

                    float FinalDist = HalfWidth + TargetRelX;
                    TempPoints.Last().OffsetH = bIsRightSide ? FinalDist : -FinalDist;
                    TempPoints.Last().OffsetV = TargetRelZ;
                }
                continue;
            }

            float SegDist = FMath::Sqrt(DistSq);
            float UVStep = SegDist * (TextureMapping == ESurfaceTextureMapping::Stretch ? GlobalUVScale : UVFactor);

            if (bIsRightSide) RunningU += UVStep;
            else              RunningU -= UVStep;

            float FinalDist = HalfWidth + TargetRelX;
            float FinalX = bIsRightSide ? FinalDist : -FinalDist;

            TempPoints.Add({ FinalX, TargetRelZ, RunningU, true });

            LastEmittedRelX = TargetRelX;
            LastEmittedRelZ = TargetRelZ;
        }

        if (bIsRightSide)
        {
            ProfileDefinition.Append(TempPoints);
        }
        else
        {
            for (int32 i = TempPoints.Num() - 1; i >= 0; --i)
            {
                ProfileDefinition.Add(TempPoints[i]);
            }
        }
    };

    AppendSlopePoints(false, LeftSlopeLength, LeftSlopeGradient, U_RoadLeft);

    ProfileDefinition.Add({ -HalfWidth, 0.0f, U_RoadLeft, false });
    ProfileDefinition.Add({ HalfWidth, 0.0f, U_RoadRight, false });

    AppendSlopePoints(true, RightSlopeLength, RightSlopeGradient, U_RoadRight);
}

void FEditableSurfaceBuilder::CalculateCornerGeometry()
{
    int32 NumPoints = SampledPath.Num();
    PathCornerData.SetNum(NumPoints);

    bool bLastValidTurnLeft = false; 

    for (int32 i = 0; i < NumPoints; ++i)
    {
        FVector CurrPos = SampledPath[i].Location;
        FVector UpVec = SampledPath[i].Normal;
        FVector PrevPos, NextPos;

        if (i == 0) {
            PrevPos = CurrPos - SampledPath[i].Tangent * 100.0f;
            NextPos = SampledPath[i + 1].Location;
        } else if (i == NumPoints - 1) {
            PrevPos = SampledPath[i - 1].Location;
            NextPos = CurrPos + SampledPath[i].Tangent * 100.0f;
        } else {
            PrevPos = SampledPath[i - 1].Location;
            NextPos = SampledPath[i + 1].Location;
        }

        FVector DirIn = (CurrPos - PrevPos).GetSafeNormal();
        FVector DirOut = (NextPos - CurrPos).GetSafeNormal();

        FVector CornerTangent = (DirIn + DirOut).GetSafeNormal();
        if (CornerTangent.IsZero()) CornerTangent = SampledPath[i].Tangent;

        FVector GeometricRight = FVector::CrossProduct(UpVec, CornerTangent).GetSafeNormal();
        FVector StandardRight = SampledPath[i].RightVector;
        float MiterDot = FMath::Abs(FVector::DotProduct(StandardRight, GeometricRight));
        
        if (MiterDot < 0.1f) 
        {
            GeometricRight = StandardRight;
            MiterDot = 1.0f;
        }

        float Scale = 1.0f;
        if (MiterDot > 1.e-4f) Scale = 1.0f / MiterDot;

        float TurnDir = FVector::DotProduct(FVector::CrossProduct(DirIn, DirOut), UpVec);
        bool bIsLeftTurn = bLastValidTurnLeft; 
        if (FMath::Abs(TurnDir) > 0.001f)
        {
            bIsLeftTurn = (TurnDir > 0.0f);
            bLastValidTurnLeft = bIsLeftTurn;
        }

        float TurnRadius = FLT_MAX;
        FVector RotationCenter = CurrPos;
        float CosTheta = FMath::Clamp(FVector::DotProduct(DirIn, DirOut), -1.0f, 1.0f);
        
        if (CosTheta < 0.99999f) 
        {
            float Theta = FMath::Acos(CosTheta);
            float StepLen = FVector::Dist(PrevPos, CurrPos);
            if (Theta > 1.e-4f)
            {
                TurnRadius = StepLen / Theta;
            }
            FVector DirToCenter = bIsLeftTurn ? -GeometricRight : GeometricRight;
            RotationCenter = CurrPos + (DirToCenter * TurnRadius);
        }

        float CurrentHalfWidth = SampledPath[i].InterpolatedWidth * 0.5f;
        float LeftSlopeScale = 1.0f;
        float RightSlopeScale = 1.0f;
        
        float LeftEffectiveRadius = CurrentHalfWidth + LeftSlopeLength;
        float RightEffectiveRadius = CurrentHalfWidth + RightSlopeLength;
        LeftSlopeScale = bIsLeftTurn ? (CurrentHalfWidth / LeftEffectiveRadius) : (RightEffectiveRadius / CurrentHalfWidth);
        RightSlopeScale = bIsLeftTurn ? (RightEffectiveRadius / CurrentHalfWidth) : (CurrentHalfWidth / RightEffectiveRadius);
        
        float MiterIntensity = (Scale - 1.0f) / 1.0f;
        LeftSlopeScale = FMath::Lerp(1.0f, LeftSlopeScale, MiterIntensity);
        RightSlopeScale = FMath::Lerp(1.0f, RightSlopeScale, MiterIntensity);

        PathCornerData[i].MiterVector = GeometricRight;
        PathCornerData[i].MiterScale = Scale;
        PathCornerData[i].bIsConvexLeft = bIsLeftTurn;
        PathCornerData[i].LeftSlopeMiterScale = LeftSlopeScale;
        PathCornerData[i].RightSlopeMiterScale = RightSlopeScale;
        PathCornerData[i].TurnRadius = TurnRadius;
        PathCornerData[i].RotationCenter = RotationCenter;
    }
}

void FEditableSurfaceBuilder::GenerateGridMesh()
{
    int32 NumRows = SampledPath.Num();
    int32 NumCols = ProfileDefinition.Num();

    if (NumRows < 2 || NumCols < 2) return;

    GridStartIndex = MeshData.Vertices.Num();
    GridNumRows = NumRows;
    GridNumCols = NumCols;

    float BaseHalfWidth = SurfaceWidth * 0.5f;

    TArray<FVector> PreviousRowPositions;
    PreviousRowPositions.SetNumZeroed(NumCols);

    for (int32 i = 0; i < NumRows; ++i)
    {
        const FSurfaceSamplePoint& Sample = SampledPath[i];
        const FCornerData& Corner = PathCornerData[i];
        FVector PivotPoint = Corner.RotationCenter;

        // 获取当前行的实际半宽
        float CurrentHalfWidth = Sample.InterpolatedWidth * 0.5f;
        float WidthScaleRatio = (BaseHalfWidth > KINDA_SMALL_NUMBER) ? (CurrentHalfWidth / BaseHalfWidth) : 1.0f;

        float V_Coord = (TextureMapping == ESurfaceTextureMapping::Stretch) 
            ? Sample.Alpha 
            : Sample.Distance * ModelGenConstants::GLOBAL_UV_SCALE;

        for (int32 j = 0; j < NumCols; ++j)
        {
            const FProfilePoint& Profile = ProfileDefinition[j];
            
            FVector FinalPos;
            FVector VertexNormal = Sample.Normal;

            float ScaledOffsetH = Profile.OffsetH * WidthScaleRatio;
            float BaseOffsetH = ScaledOffsetH;
            bool bIsPointRightSide = (BaseOffsetH > 0.0f);

            float AppliedScale = Corner.MiterScale;

            if (AppliedScale > 2.5f) AppliedScale = 2.5f;

            if (Profile.bIsSlope)
            {
                float SlopeScale = bIsPointRightSide ? Corner.RightSlopeMiterScale : Corner.LeftSlopeMiterScale;
                AppliedScale *= SlopeScale;
            }

            float IntendedMiterDist = BaseOffsetH * AppliedScale;

            bool bIsInnerGeometric = (Corner.bIsConvexLeft && BaseOffsetH < 0.0f) || (!Corner.bIsConvexLeft && BaseOffsetH > 0.0f);
            if (bIsInnerGeometric && Corner.TurnRadius < 10000.0f)
            {
                float SafeRadius = FMath::Max(Corner.TurnRadius - 5.0f, 0.0f);
                if (FMath::Abs(IntendedMiterDist) > SafeRadius)
                {
                    IntendedMiterDist = (IntendedMiterDist > 0.0f) ? SafeRadius : -SafeRadius;
                }
            }

            FinalPos = Sample.Location + (Corner.MiterVector * IntendedMiterDist) + (Sample.Normal * Profile.OffsetV);

            if (i > 0)
            {
                FVector PrevPos = PreviousRowPositions[j];
                FVector MovementVector = FinalPos - PrevPos;
                float ForwardProgress = FVector::DotProduct(MovementVector, Sample.Tangent);

                float CollapseThreshold = bIsInnerGeometric ? (SplineSampleStep * 0.05f) : 0.0f;

                if (ForwardProgress <= CollapseThreshold)
                {
                    FinalPos = PrevPos;
                }
            }

            PreviousRowPositions[j] = FinalPos;

            bool bIsInnerSide = false;
            if (Corner.TurnRadius < FLT_MAX - 1.0f)
            {
                float DistFromCenterToVertex = FVector::Dist(PivotPoint, FinalPos);
                float DistFromCenterToSample = Corner.TurnRadius;
                bIsInnerSide = (DistFromCenterToVertex < DistFromCenterToSample);
            }

            if (Profile.bIsSlope)
            {
                float Sign = bIsPointRightSide ? 1.0f : -1.0f;
                VertexNormal = (Sample.Normal + (Corner.MiterVector * Sign * 0.5f)).GetSafeNormal();
            }

            if (Surface.GetWorld() && BaseOffsetH < 0.0f)
            {
                FColor LineColor = FColor::Yellow;
                FColor PointColor = FColor::Cyan;
                FColor CenterLineColor = FColor::Green;
                
                if (j == 0)
                {
                    DrawDebugSphere(Surface.GetWorld(), PivotPoint, 20.0f, 8, FColor::Red, true, -1.0f, 0, 2.0f);
                }
                
                DrawDebugLine(Surface.GetWorld(), Sample.Location, FinalPos, LineColor, true, -1.0f, 0, 3.0f);
                DrawDebugPoint(Surface.GetWorld(), FinalPos, 10.0f, PointColor, true, -1.0f, 0);
                DrawDebugLine(Surface.GetWorld(), PivotPoint, FinalPos, CenterLineColor, true, -1.0f, 0, 1.0f);
            }

            FVector2D UV(Profile.U, V_Coord);
            AddVertex(FinalPos, VertexNormal, UV);
        }
    }

    // 三角形索引生成 (保持不变)
    for (int32 i = 0; i < NumRows - 1; ++i)
    {
        for (int32 j = 0; j < NumCols - 1; ++j)
        {
            int32 Row0 = GridStartIndex + (i * GridNumCols);
            int32 Row1 = GridStartIndex + ((i + 1) * GridNumCols);
            int32 TL = Row0 + j;
            int32 TR = Row0 + j + 1;
            int32 BL = Row1 + j;
            int32 BR = Row1 + j + 1;
            AddQuad(TL, TR, BR, BL);
        }
    }
}

bool FEditableSurfaceBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2)
    {
        return false;
    }

    if (Surface.GetWorld())
    {
        FlushPersistentDebugLines(Surface.GetWorld());
    }

    Clear();

    if (!bEnableThickness)
    {
        ThicknessValue = 0.01f;
    }

    SampleSplinePath();
    CalculateCornerGeometry();
    GenerateZipperRoadMesh();
    GenerateZipperThickness();
    if (MeshData.Triangles.Num() > 0)
    {
        TArray<int32> CleanTriangles;
        CleanTriangles.Reserve(MeshData.Triangles.Num());

        int32 NumTriangles = MeshData.Triangles.Num() / 3;
        for (int32 i = 0; i < NumTriangles; ++i)
        {
            int32 Idx0 = MeshData.Triangles[i * 3 + 0];
            int32 Idx1 = MeshData.Triangles[i * 3 + 1];
            int32 Idx2 = MeshData.Triangles[i * 3 + 2];

            if (Idx0 == Idx1 || Idx1 == Idx2 || Idx0 == Idx2)
            {
                continue;
            }
            if (Idx0 >= 0 && Idx0 < MeshData.Vertices.Num() &&
                Idx1 >= 0 && Idx1 < MeshData.Vertices.Num() &&
                Idx2 >= 0 && Idx2 < MeshData.Vertices.Num())
            {
                const FVector& P0 = MeshData.Vertices[Idx0];
                const FVector& P1 = MeshData.Vertices[Idx1];
                const FVector& P2 = MeshData.Vertices[Idx2];

                FVector Edge1 = P1 - P0;
                FVector Edge2 = P2 - P0;
                FVector UnnormalizedNormal = FVector::CrossProduct(Edge1, Edge2);

                if (UnnormalizedNormal.SizeSquared() > KINDA_SMALL_NUMBER)
                {
                    CleanTriangles.Add(Idx0);
                    CleanTriangles.Add(Idx1);
                    CleanTriangles.Add(Idx2);
                }
            }
        }

        MeshData.Triangles = CleanTriangles;
        MeshData.TriangleCount = CleanTriangles.Num() / 3;
    }

    MeshData.CalculateTangents();

    OutMeshData = MoveTemp(MeshData);

    return OutMeshData.IsValid();
}

void FEditableSurfaceBuilder::SampleSplinePath()
{
    if (!SplineComponent) return;

    float SplineLen = SplineComponent->GetSplineLength();
    int32 NumSteps = FMath::CeilToInt(SplineLen / SplineSampleStep);

    SampledPath.Reserve(NumSteps + 1);

    for (int32 i = 0; i <= NumSteps; ++i)
    {
        float Dist = FMath::Min(static_cast<float>(i) * SplineSampleStep, SplineLen);
        
        FSurfaceSamplePoint Point;
        Point.Distance = Dist;
        Point.Alpha = (SplineLen > KINDA_SMALL_NUMBER) ? (Dist / SplineLen) : 0.0f;
        
        FTransform TF = SplineComponent->GetTransformAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);
        
        Point.Location = TF.GetLocation();
        Point.Tangent = TF.GetUnitAxis(EAxis::X);
        Point.RightVector = TF.GetUnitAxis(EAxis::Y);
        Point.Normal = TF.GetUnitAxis(EAxis::Z);

        FVector Scale = SplineComponent->GetScaleAtDistanceAlongSpline(Dist);
        Point.InterpolatedWidth = Scale.Y;

        SampledPath.Add(Point);
    }
}

void FEditableSurfaceBuilder::GenerateThickness()
{
    if (GridNumRows < 2 || GridNumCols < 2) return;

    float ThicknessV = ThicknessValue * ModelGenConstants::GLOBAL_UV_SCALE;
    int32 BottomGridStartIndex = MeshData.Vertices.Num();

    for (int32 i = 0; i < GridNumRows; ++i)
    {
        for (int32 j = 0; j < GridNumCols; ++j)
        {
            int32 TopIdx = GridStartIndex + (i * GridNumCols) + j;
            
            FVector TopPos = MeshData.Vertices[TopIdx];
            FVector TopNormal = MeshData.Normals[TopIdx]; 
            FVector2D TopUV = MeshData.UVs[TopIdx];

            FVector BottomPos = TopPos - TopNormal * ThicknessValue;
            FVector BottomNormal = TopNormal;
            FVector2D BottomUV(1.0f - TopUV.X, TopUV.Y);

            AddVertex(BottomPos, BottomNormal, BottomUV);
        }
    }

    for (int32 i = 0; i < GridNumRows - 1; ++i)
    {
        for (int32 j = 0; j < GridNumCols - 1; ++j)
        {
            int32 Row0 = BottomGridStartIndex + (i * GridNumCols);
            int32 Row1 = BottomGridStartIndex + ((i + 1) * GridNumCols);

            int32 TL = Row0 + j;
            int32 TR = Row0 + j + 1;
            int32 BL = Row1 + j;
            int32 BR = Row1 + j + 1;

            AddQuad(BL, BR, TR, TL);
        }
    }
    auto StitchSideStrip = [&](int32 ColIndex, bool bIsRightSide)
    {
        for (int32 i = 0; i < GridNumRows - 1; ++i)
        {
            int32 TopCurr = GridStartIndex + (i * GridNumCols) + ColIndex;
            int32 TopNext = GridStartIndex + ((i + 1) * GridNumCols) + ColIndex;
            int32 BotCurr = BottomGridStartIndex + (i * GridNumCols) + ColIndex;
            int32 BotNext = BottomGridStartIndex + ((i + 1) * GridNumCols) + ColIndex;

            FVector P_TopCurr = MeshData.Vertices[TopCurr];
            FVector P_TopNext = MeshData.Vertices[TopNext];
            FVector N_TopCurr = MeshData.Normals[TopCurr];
            FVector N_TopNext = MeshData.Normals[TopNext];

            FVector P_BotCurr = P_TopCurr - N_TopCurr * ThicknessValue;
            FVector P_BotNext = P_TopNext - N_TopNext * ThicknessValue;

            float WallU_Curr = MeshData.UVs[TopCurr].Y;
            float WallU_Next = MeshData.UVs[TopNext].Y;

            FVector SideDir = (P_TopNext - P_TopCurr).GetSafeNormal();
            FVector SideNormal;
            if (bIsRightSide)
            {
                SideNormal = FVector::CrossProduct(SideDir, N_TopCurr).GetSafeNormal();
            }
            else
            {
                SideNormal = FVector::CrossProduct(-N_TopCurr, SideDir).GetSafeNormal();
            }

            int32 V_TL = AddVertex(P_TopCurr, SideNormal, FVector2D(WallU_Curr, 0.0f));
            int32 V_TR = AddVertex(P_TopNext, SideNormal, FVector2D(WallU_Next, 0.0f));
            int32 V_BL = AddVertex(P_BotCurr, SideNormal, FVector2D(WallU_Curr, ThicknessV));
            int32 V_BR = AddVertex(P_BotNext, SideNormal, FVector2D(WallU_Next, ThicknessV));

            if (bIsRightSide)
            {
                AddQuad(V_TL, V_BL, V_BR, V_TR);
            }
            else
            {
                AddQuad(V_TL, V_TR, V_BR, V_BL);
            }
        }
    };

    StitchSideStrip(0, false);
    StitchSideStrip(GridNumCols - 1, true);

    auto StitchCapStrip = [&](int32 RowIndex, bool bIsStart)
    {
        for (int32 j = 0; j < GridNumCols - 1; ++j)
        {
            int32 TopStart = GridStartIndex + (RowIndex * GridNumCols);
            int32 BotStart = BottomGridStartIndex + (RowIndex * GridNumCols);
            
            int32 TopIdx0 = TopStart + j;
            int32 TopIdx1 = TopStart + j + 1;
            int32 BotIdx0 = BotStart + j;
            int32 BotIdx1 = BotStart + j + 1;

            FVector P_F0 = MeshData.Vertices[TopIdx0];
            FVector P_F1 = MeshData.Vertices[TopIdx1];
            FVector N_F0 = MeshData.Normals[TopIdx0];
            FVector N_F1 = MeshData.Normals[TopIdx1];
            
            FVector P_B0 = P_F0 - N_F0 * ThicknessValue;
            FVector P_B1 = P_F1 - N_F1 * ThicknessValue;

            int32 AdjRowIndex = bIsStart ? RowIndex + 1 : RowIndex - 1;
            if (AdjRowIndex >= 0 && AdjRowIndex < GridNumRows)
            {
                int32 AdjTopIdx = GridStartIndex + (AdjRowIndex * GridNumCols) + j;
                FVector P_Adj = MeshData.Vertices[AdjTopIdx];
                FVector FaceNormal = (P_F0 - P_Adj).GetSafeNormal();

                float U0 = MeshData.UVs[TopIdx0].X;
                float U1 = MeshData.UVs[TopIdx1].X;

                int32 V_TL = AddVertex(P_F0, FaceNormal, FVector2D(U0, 0.0f));
                int32 V_TR = AddVertex(P_F1, FaceNormal, FVector2D(U1, 0.0f));
                int32 V_BL = AddVertex(P_B0, FaceNormal, FVector2D(U0, ThicknessV));
                int32 V_BR = AddVertex(P_B1, FaceNormal, FVector2D(U1, ThicknessV));

                if (bIsStart)
                {
                    AddQuad(V_TL, V_BL, V_BR, V_TR);
                }
                else
                {
                    AddQuad(V_TR, V_BR, V_BL, V_TL);
                }
            }
        }
    };

    StitchCapStrip(0, true);
    StitchCapStrip(GridNumRows - 1, false);
}

void FEditableSurfaceBuilder::InitializeForDebug()
{
    Clear();
    SampleSplinePath();
    CalculateCornerGeometry();
    BuildProfileDefinition();
}

void FEditableSurfaceBuilder::PrintAntiPenetrationDebugInfo()
{
    // 如果数据未初始化，先执行采样和计算
    if (SampledPath.Num() == 0 || PathCornerData.Num() == 0 || ProfileDefinition.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("数据未初始化，正在执行采样和计算..."));
        InitializeForDebug();
    }

    UE_LOG(LogTemp, Warning, TEXT("========== 防穿模调试信息 =========="));
    UE_LOG(LogTemp, Warning, TEXT("采样点数量: %d"), SampledPath.Num());
    UE_LOG(LogTemp, Warning, TEXT("横截面点数: %d"), ProfileDefinition.Num());
    UE_LOG(LogTemp, Warning, TEXT("全局宽度: %.2f"), SurfaceWidth);
    UE_LOG(LogTemp, Warning, TEXT("采样步长: %.2f"), SplineSampleStep);
    UE_LOG(LogTemp, Warning, TEXT(""));
    int32 InnerPointCount = 0;
    int32 ClampedCount = 0;

    // 计算全局基础宽度的一半，用于归一化
    float BaseHalfWidth = SurfaceWidth * 0.5f;

    for (int32 i = 0; i < SampledPath.Num(); ++i)
    {
        const FSurfaceSamplePoint& Sample = SampledPath[i];
        const FCornerData& Corner = PathCornerData[i];
        FVector PivotPoint = Corner.RotationCenter;

        // 获取当前行的实际半宽
        float CurrentHalfWidth = Sample.InterpolatedWidth * 0.5f;
        float WidthScaleRatio = (BaseHalfWidth > KINDA_SMALL_NUMBER) ? (CurrentHalfWidth / BaseHalfWidth) : 1.0f;

        for (int32 j = 0; j < ProfileDefinition.Num(); ++j)
        {
            const FProfilePoint& Profile = ProfileDefinition[j];
            
            float ScaledOffsetH = Profile.OffsetH * WidthScaleRatio;
            float BaseOffsetH = ScaledOffsetH;
            bool bIsPointRightSide = (BaseOffsetH > 0.0f);

            float AppliedScale = Corner.MiterScale;
            if (Profile.bIsSlope)
            {
                float SlopeScale = bIsPointRightSide ? Corner.RightSlopeMiterScale : Corner.LeftSlopeMiterScale;
                AppliedScale *= SlopeScale;
            }

            float IntendedMiterDist = BaseOffsetH * AppliedScale;
            FVector FinalPos = Sample.Location + (Corner.MiterVector * IntendedMiterDist) + (Sample.Normal * Profile.OffsetV);

            // 直接根据几何关系判断内外侧：比较从旋转中心到顶点的距离和转弯半径
            bool bIsInnerSide = false;
            if (Corner.TurnRadius < FLT_MAX - 1.0f)  // 如果是弯道（有有效的旋转中心）
            {
                float DistFromCenterToVertex = FVector::Dist(PivotPoint, FinalPos);
                float DistFromCenterToSample = Corner.TurnRadius;
                bIsInnerSide = (DistFromCenterToVertex < DistFromCenterToSample);
            }

            if (bIsInnerSide)
            {
                InnerPointCount++;
                float DistToCenter = Corner.TurnRadius;
                float SafetyBuffer = (DistToCenter < 100.0f) ? (DistToCenter * 0.1f) : 2.0f;
                float MaxAllowedDist = FMath::Max(DistToCenter - SafetyBuffer, 0.0f);
                float AbsDist = FMath::Abs(IntendedMiterDist);

                if (AbsDist > MaxAllowedDist)
                {
                    ClampedCount++;
                    
                    if (InnerPointCount <= 15 || ClampedCount <= 8)
                    {
                        float ClampedDist = (IntendedMiterDist > 0.0f) ? MaxAllowedDist : -MaxAllowedDist;
                        FVector ClampedFinalPos = Sample.Location + (Corner.MiterVector * ClampedDist);
                        ClampedFinalPos.Z = Sample.Location.Z + Profile.OffsetV;
                        FVector TheoreticalPos = Sample.Location + (Corner.MiterVector * IntendedMiterDist);

                        UE_LOG(LogTemp, Warning, TEXT("--- 内侧点 [采样%d, 横截面%d] (被钳制) ---"), i, j);
                        UE_LOG(LogTemp, Warning, TEXT("  采样点位置: %s"), *Sample.Location.ToString());
                        UE_LOG(LogTemp, Warning, TEXT("  旋转中心: %s"), *PivotPoint.ToString());
                        UE_LOG(LogTemp, Warning, TEXT("  转弯半径: %.2f"), Corner.TurnRadius);
                        UE_LOG(LogTemp, Warning, TEXT("  Miter缩放: %.3f"), Corner.MiterScale);
                        UE_LOG(LogTemp, Warning, TEXT("  基础偏移H: %.2f"), BaseOffsetH);
                        UE_LOG(LogTemp, Warning, TEXT("  应用缩放后偏移: %.2f"), AppliedScale);
                        UE_LOG(LogTemp, Warning, TEXT("  理论偏移距离: %.2f"), IntendedMiterDist);
                        UE_LOG(LogTemp, Warning, TEXT("  最大允许距离: %.2f"), MaxAllowedDist);
                        UE_LOG(LogTemp, Warning, TEXT("  安全缓冲区: %.2f"), SafetyBuffer);
                        UE_LOG(LogTemp, Warning, TEXT("  钳制后距离: %.2f"), ClampedDist);
                        UE_LOG(LogTemp, Warning, TEXT("  理论位置: %s"), *TheoreticalPos.ToString());
                        UE_LOG(LogTemp, Warning, TEXT("  最终位置: %s"), *ClampedFinalPos.ToString());
                        UE_LOG(LogTemp, Warning, TEXT("  当前宽度: %.2f"), Sample.InterpolatedWidth);
                        UE_LOG(LogTemp, Warning, TEXT("  宽度缩放比: %.3f"), WidthScaleRatio);
                        UE_LOG(LogTemp, Warning, TEXT("  是否护坡: %s"), Profile.bIsSlope ? TEXT("是") : TEXT("否"));
                        UE_LOG(LogTemp, Warning, TEXT(""));
                    }
                }
                else if (InnerPointCount <= 10)
                {
                    UE_LOG(LogTemp, Log, TEXT("--- 内侧点 [采样%d, 横截面%d] (未钳制) ---"), i, j);
                    UE_LOG(LogTemp, Log, TEXT("  采样点位置: %s"), *Sample.Location.ToString());
                    UE_LOG(LogTemp, Log, TEXT("  转弯半径: %.2f"), Corner.TurnRadius);
                    UE_LOG(LogTemp, Log, TEXT("  理论偏移距离: %.2f"), IntendedMiterDist);
                    UE_LOG(LogTemp, Log, TEXT("  最大允许距离: %.2f"), MaxAllowedDist);
                    UE_LOG(LogTemp, Log, TEXT("  最终位置: %s"), *FinalPos.ToString());
                    UE_LOG(LogTemp, Log, TEXT(""));
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("统计信息:"));
    UE_LOG(LogTemp, Warning, TEXT("  内侧点总数: %d"), InnerPointCount);
    UE_LOG(LogTemp, Warning, TEXT("  被钳制的点数: %d"), ClampedCount);
    UE_LOG(LogTemp, Warning, TEXT("  钳制比例: %.1f%%"), InnerPointCount > 0 ? (ClampedCount * 100.0f / InnerPointCount) : 0.0f);
    UE_LOG(LogTemp, Warning, TEXT("====================================="));
}

void FEditableSurfaceBuilder::GenerateZipperRoadMesh()
{
    // 步骤 1: 构建原始边线 (基于中心样条的切片)
    BuildRawRails();

    float UserLoopThreshold = LoopRemovalThreshold;

    int32 LeftRawCountBefore = LeftRailRaw.Num();
    int32 RightRawCountBefore = RightRailRaw.Num();
    
    RemoveGeometricLoops(LeftRailRaw, UserLoopThreshold);
    RemoveGeometricLoops(RightRailRaw, UserLoopThreshold);

    // 调试日志：查看去环前后的点数变化
    UE_LOG(LogTemp, Log, TEXT("Zipper Loop Removal: Left %d -> %d, Right %d -> %d"), 
        LeftRawCountBefore, LeftRailRaw.Num(), RightRawCountBefore, RightRailRaw.Num());

    float WeldThreshold = 5.0f; 
    TArray<FRailPoint> LeftRailSimplified;
    TArray<FRailPoint> RightRailSimplified;

    SimplifyRail(LeftRailRaw, LeftRailSimplified, WeldThreshold);
    SimplifyRail(RightRailRaw, RightRailSimplified, WeldThreshold);

    // 调试日志：查看简化前后的点数变化
    UE_LOG(LogTemp, Log, TEXT("Zipper Simplify: Left Raw=%d -> Simplified=%d, Right Raw=%d -> Simplified=%d"), 
        LeftRailRaw.Num(), LeftRailSimplified.Num(), RightRailRaw.Num(), RightRailSimplified.Num());

    float ResampleStep = FMath::Max(SplineSampleStep, 10.0f);
    
    ResampleSingleRail(LeftRailSimplified, LeftRailResampled, ResampleStep);
    ResampleSingleRail(RightRailSimplified, RightRailResampled, ResampleStep);

    UE_LOG(LogTemp, Log, TEXT("Zipper Resample: Left Points=%d, Right Points=%d"), 
        LeftRailResampled.Num(), RightRailResampled.Num());

    if (Surface.GetWorld())
    if (Surface.GetWorld())
    {
        FColor LeftPointColor = FColor::Cyan;
        FColor RightPointColor = FColor::Yellow;
        FColor LeftLineColor = FColor::Cyan;
        FColor RightLineColor = FColor::Yellow;
        FColor CenterLineColor = FColor::Green;

        // 绘制左侧边线
        for (int32 i = 0; i < LeftRailResampled.Num(); ++i)
        {
            const FRailPoint& Pt = LeftRailResampled[i];
            DrawDebugPoint(Surface.GetWorld(), Pt.Position, 10.0f, LeftPointColor, true, -1.0f, 0);
            
            if (i > 0)
            {
                DrawDebugLine(Surface.GetWorld(), LeftRailResampled[i - 1].Position, Pt.Position, LeftLineColor, true, -1.0f, 0, 3.0f);
            }
        }

        // 绘制右侧边线
        for (int32 i = 0; i < RightRailResampled.Num(); ++i)
        {
            const FRailPoint& Pt = RightRailResampled[i];
            DrawDebugPoint(Surface.GetWorld(), Pt.Position, 10.0f, RightPointColor, true, -1.0f, 0);
            
            if (i > 0)
            {
                DrawDebugLine(Surface.GetWorld(), RightRailResampled[i - 1].Position, Pt.Position, RightLineColor, true, -1.0f, 0, 3.0f);
            }
        }

        // 绘制从原始采样点到左右边线的连线（仅显示部分点，避免过于密集）
        int32 DebugSampleStep = FMath::Max(1, SampledPath.Num() / 20); // 最多显示20个采样点
        for (int32 i = 0; i < SampledPath.Num(); i += DebugSampleStep)
        {
            const FSurfaceSamplePoint& Sample = SampledPath[i];
            const FCornerData& Corner = PathCornerData[i];
            
            // 找到最近的左右边线点（简化版，使用原始边线）
            if (i < LeftRailRaw.Num() && i < RightRailRaw.Num())
            {
                DrawDebugLine(Surface.GetWorld(), Sample.Location, LeftRailRaw[i].Position, CenterLineColor, true, -1.0f, 0, 1.0f);
                DrawDebugLine(Surface.GetWorld(), Sample.Location, RightRailRaw[i].Position, CenterLineColor, true, -1.0f, 0, 1.0f);
            }

            // 绘制旋转中心（仅第一个点）
            if (i == 0 && Corner.TurnRadius < FLT_MAX - 1.0f)
            {
                DrawDebugSphere(Surface.GetWorld(), Corner.RotationCenter, 20.0f, 8, FColor::Red, true, -1.0f, 0, 2.0f);
            }
        }
    }

    auto GetBlendedNormal = [&](const FRailPoint& Pt, bool bIsRightSide) -> FVector
    {
        float Len = bIsRightSide ? RightSlopeLength : LeftSlopeLength;
        float Grad = bIsRightSide ? RightSlopeGradient : LeftSlopeGradient;
        
        if (SideSmoothness < 1 || Len < KINDA_SMALL_NUMBER) 
        {
            return Pt.Normal;
        }

        float TotalH = Len * Grad;
        float Ratio = 1.0f / static_cast<float>(SideSmoothness);
        float dH = Len * Ratio;
        float dV = (SideSmoothness > 1) ? (TotalH * Ratio * Ratio) : (TotalH * Ratio);

        FVector Up = Pt.Normal;
        FVector Tan = Pt.Tangent;
        FVector Outward;
        
        if (bIsRightSide)
            Outward = FVector::CrossProduct(Up, Tan).GetSafeNormal();
        else
            Outward = FVector::CrossProduct(Tan, Up).GetSafeNormal();

        FVector SlopeVec = (Outward * dH) + (Up * dV);

        FVector SlopeNormal;
        if (bIsRightSide)
            SlopeNormal = FVector::CrossProduct(Tan, SlopeVec).GetSafeNormal();
        else
            SlopeNormal = FVector::CrossProduct(SlopeVec, Tan).GetSafeNormal();

        return (Up + SlopeNormal).GetSafeNormal();
    };

    int32 LeftRoadStartIdx = MeshData.Vertices.Num();
    for (const FRailPoint& Pt : LeftRailResampled)
    {
        FVector FinalN = GetBlendedNormal(Pt, false);
        AddVertex(Pt.Position, FinalN, Pt.UV);
    }

    int32 RightRoadStartIdx = MeshData.Vertices.Num();
    for (const FRailPoint& Pt : RightRailResampled)
    {
        FVector FinalN = GetBlendedNormal(Pt, true);
        AddVertex(Pt.Position, FinalN, Pt.UV);
    }
    
    StitchRailsInternal(LeftRoadStartIdx, RightRoadStartIdx, LeftRailResampled.Num(), RightRailResampled.Num());

    TopStartIndices.Reset();
    TopEndIndices.Reset();
    TopStartIndices.Add(LeftRoadStartIdx);
    TopStartIndices.Add(RightRoadStartIdx);
    TopEndIndices.Add(LeftRoadStartIdx + LeftRailResampled.Num() - 1);
    TopEndIndices.Add(RightRoadStartIdx + RightRailResampled.Num() - 1);

    FinalLeftRail = LeftRailResampled;
    FinalRightRail = RightRailResampled;

    GenerateZipperSlopes(LeftRoadStartIdx, RightRoadStartIdx);
}

void FEditableSurfaceBuilder::BuildRawRails()
{
    int32 NumPoints = SampledPath.Num();
    LeftRailRaw.Reset(NumPoints);
    RightRailRaw.Reset(NumPoints);

    float HalfWidth = SurfaceWidth * 0.5f;

    // 缓存上一个有效点的位置，用于防止"倒车"
    FVector LastValidLeftPos = FVector::ZeroVector;
    FVector LastValidRightPos = FVector::ZeroVector;

    for (int32 i = 0; i < NumPoints; ++i)
    {
        const FSurfaceSamplePoint& Sample = SampledPath[i];
        const FCornerData& Corner = PathCornerData[i];

        // 1. 基础宽度计算
        float CurrentHalfWidth = Sample.InterpolatedWidth * 0.5f;
        float WidthScaleRatio = (HalfWidth > KINDA_SMALL_NUMBER) ? (CurrentHalfWidth / HalfWidth) : 1.0f;

        float LeftOffset = -HalfWidth * WidthScaleRatio;
        float RightOffset = HalfWidth * WidthScaleRatio;

        // ====================================================================
        // 左侧计算 (Left Calculation)
        // ====================================================================
        FVector LeftPos;
        bool bIsLeftInner = Corner.bIsConvexLeft; // 左转时，左侧是内侧

        if (bIsLeftInner)
        {
            // --- 内侧逻辑 (保持 Miter + 防穿模) ---
            float AppliedScaleLeft = Corner.MiterScale;
            if (AppliedScaleLeft > 2.0f) AppliedScaleLeft = 2.0f; // 限制尖角
            
            float LeftMiterDist = LeftOffset * AppliedScaleLeft;

            if (Corner.TurnRadius < 10000.0f)
            {
                 float MaxDist = FMath::Max(Corner.TurnRadius - 2.0f, 0.0f);
                 if (FMath::Abs(LeftMiterDist) > MaxDist) LeftMiterDist = -MaxDist;
            }
            LeftPos = Sample.Location + (Corner.MiterVector * LeftMiterDist);
        }
        else
        {
            float AppliedScaleLeft = Corner.MiterScale;
            if (AppliedScaleLeft > 4.0f) AppliedScaleLeft = 4.0f;

            float LeftMiterDist = LeftOffset * AppliedScaleLeft;
            LeftPos = Sample.Location + (Corner.MiterVector * LeftMiterDist);
        }

        // ====================================================================
        // 右侧计算 (Right Calculation)
        // ====================================================================
        FVector RightPos;
        bool bIsRightInner = !Corner.bIsConvexLeft; // 右转时，右侧是内侧

        if (bIsRightInner)
        {
            // --- 内侧逻辑 (保持 Miter + 防穿模) ---
            float AppliedScaleRight = Corner.MiterScale;
            if (AppliedScaleRight > 2.0f) AppliedScaleRight = 2.0f; 
            
            float RightMiterDist = RightOffset * AppliedScaleRight;

            if (Corner.TurnRadius < 10000.0f)
            {
                 float MaxDist = FMath::Max(Corner.TurnRadius - 2.0f, 0.0f);
                 if (RightMiterDist > MaxDist) RightMiterDist = MaxDist;
            }
            RightPos = Sample.Location + (Corner.MiterVector * RightMiterDist);
        }
        else
        {
            float AppliedScaleRight = Corner.MiterScale;
            if (AppliedScaleRight > 4.0f) AppliedScaleRight = 4.0f;
            
            float RightMiterDist = RightOffset * AppliedScaleRight;
            RightPos = Sample.Location + (Corner.MiterVector * RightMiterDist);
        }

        if (i > 0)
        {
            // --- 左侧 ---
            FVector LeftDelta = LeftPos - LastValidLeftPos;
            float LeftProgress = FVector::DotProduct(LeftDelta, Sample.Tangent);
            
            if (LeftProgress < 0.001f) LeftPos = LastValidLeftPos; 

            // --- 右侧 ---
            FVector RightDelta = RightPos - LastValidRightPos;
            float RightProgress = FVector::DotProduct(RightDelta, Sample.Tangent);

            if (RightProgress < 0.001f) RightPos = LastValidRightPos;
        }
        
        LastValidLeftPos = LeftPos;
        LastValidRightPos = RightPos;

        FRailPoint LPoint;
        LPoint.Position = LeftPos;
        LPoint.Normal = Sample.Normal; 
        LPoint.Tangent = Sample.Tangent;
        LPoint.UV = FVector2D(0.0f, 0.0f); 
        LeftRailRaw.Add(LPoint);

        FRailPoint RPoint;
        RPoint.Position = RightPos;
        RPoint.Normal = Sample.Normal;
        RPoint.Tangent = Sample.Tangent;
        RPoint.UV = FVector2D(1.0f, 0.0f); 
        RightRailRaw.Add(RPoint);
    }
}

void FEditableSurfaceBuilder::ResampleRails()
{
    // 确保采样步长合理
    float ResampleStep = FMath::Max(SplineSampleStep, 10.0f);

    ResampleSingleRail(LeftRailRaw, LeftRailResampled, ResampleStep);
    ResampleSingleRail(RightRailRaw, RightRailResampled, ResampleStep);

    // 调试日志：查看左右顶点数量是否不同
    UE_LOG(LogTemp, Log, TEXT("Zipper Debug: Left Points: %d, Right Points: %d"), LeftRailResampled.Num(), RightRailResampled.Num());
}

void FEditableSurfaceBuilder::ResampleSingleRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float SegmentLength)
{
    if (InPoints.Num() < 2) return;

    TArray<FVector> GeoTangents;
    GeoTangents.SetNum(InPoints.Num());

    for (int32 i = 0; i < InPoints.Num(); ++i)
    {
        FVector P_Prev = InPoints[FMath::Max(0, i - 1)].Position;
        FVector P_Next = InPoints[FMath::Min(InPoints.Num() - 1, i + 1)].Position;
        FVector P_Curr = InPoints[i].Position;

        if (i == 0)
        {
            GeoTangents[i] = (P_Next - P_Curr).GetSafeNormal();
        }
        else if (i == InPoints.Num() - 1)
        {
            GeoTangents[i] = (P_Curr - P_Prev).GetSafeNormal();
        }
        else
        {
            GeoTangents[i] = (P_Next - P_Prev).GetSafeNormal();
        }
        
        if (GeoTangents[i].IsZero())
        {
            GeoTangents[i] = InPoints[i].Tangent;
        }
    }

    TArray<float> AccumulatedDists;
    AccumulatedDists.Add(0.0f);
    float TotalLength = 0.0f;

    for (int32 i = 0; i < InPoints.Num() - 1; ++i)
    {
        float SegLen = FVector::Dist(InPoints[i].Position, InPoints[i+1].Position);
        TotalLength += SegLen;
        AccumulatedDists.Add(TotalLength);
    }

    if (TotalLength < KINDA_SMALL_NUMBER)
    {
        OutPoints.Reset(2);
        FRailPoint StartPt = InPoints[0];
        StartPt.DistanceAlongRail = 0.0f;
        StartPt.UV.Y = 0.0f;
        OutPoints.Add(StartPt);
        FRailPoint EndPt = InPoints.Last();
        EndPt.DistanceAlongRail = 0.0f;
        EndPt.UV.Y = (TextureMapping == ESurfaceTextureMapping::Stretch) ? 1.0f : 0.0f;
        OutPoints.Add(EndPt);
        return;
    }

    int32 NumSegments = FMath::CeilToInt(TotalLength / SegmentLength);
    NumSegments = FMath::Max(NumSegments, 1);
    
    float ActualStep = TotalLength / NumSegments;

    OutPoints.Reserve(NumSegments + 1);

    int32 CurrentIndex = 0;

    for (int32 i = 0; i <= NumSegments; ++i)
    {
        float TargetDist = static_cast<float>(i) * ActualStep;
        
        while (CurrentIndex < AccumulatedDists.Num() - 1 && AccumulatedDists[CurrentIndex + 1] < TargetDist)
        {
            CurrentIndex++;
        }

        if (CurrentIndex >= InPoints.Num() - 1)
        {
            FRailPoint EndPt = InPoints.Last();
            EndPt.DistanceAlongRail = TotalLength;
            EndPt.Tangent = GeoTangents.Last();
            
            float V = (TextureMapping == ESurfaceTextureMapping::Stretch) ? 1.0f : TotalLength * ModelGenConstants::GLOBAL_UV_SCALE;
            EndPt.UV.Y = V;
            OutPoints.Add(EndPt);
            continue;
        }

        float StartD = AccumulatedDists[CurrentIndex];
        float EndD = AccumulatedDists[CurrentIndex + 1];
        float Span = EndD - StartD;
        float Alpha = (Span > KINDA_SMALL_NUMBER) ? (TargetDist - StartD) / Span : 0.0f;

        const FRailPoint& P0 = InPoints[CurrentIndex];
        const FRailPoint& P1 = InPoints[CurrentIndex + 1];

        FRailPoint NewPt;

        FVector Tangent0 = GeoTangents[CurrentIndex];
        FVector Tangent1 = GeoTangents[CurrentIndex + 1];
        float DistP0P1 = FVector::Dist(P0.Position, P1.Position);
        float TangentScale = 1.0f; 
        FVector T0 = Tangent0 * DistP0P1 * TangentScale;
        FVector T1 = Tangent1 * DistP0P1 * TangentScale;
        
        NewPt.Position = FMath::CubicInterp(P0.Position, T0, P1.Position, T1, Alpha);

        NewPt.Normal = FMath::Lerp(P0.Normal, P1.Normal, Alpha).GetSafeNormal();
        NewPt.Tangent = FMath::Lerp(Tangent0, Tangent1, Alpha).GetSafeNormal(); 
        
        NewPt.UV.X = P0.UV.X;
        NewPt.DistanceAlongRail = TargetDist;

        float V = (TextureMapping == ESurfaceTextureMapping::Stretch) ? (TargetDist / TotalLength) : (TargetDist * ModelGenConstants::GLOBAL_UV_SCALE);
        NewPt.UV.Y = V;

        OutPoints.Add(NewPt);
    }
}

void FEditableSurfaceBuilder::SimplifyRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float MergeThreshold)
{
    if (InPoints.Num() == 0) return;

    OutPoints.Reset();
    OutPoints.Add(InPoints[0]);

    float ThresholdSq = MergeThreshold * MergeThreshold;

    for (int32 i = 1; i < InPoints.Num(); ++i)
    {
        const FRailPoint& CurrentPt = InPoints[i];
        const FRailPoint& LastKeptPt = OutPoints.Last();

        float DistSq = FVector::DistSquared(CurrentPt.Position, LastKeptPt.Position);
        bool bIsLast = (i == InPoints.Num() - 1);

        if (DistSq > ThresholdSq || bIsLast)
        {
            OutPoints.Add(CurrentPt);
        }
        else
        {
        }
    }
}

void FEditableSurfaceBuilder::RemoveGeometricLoops(TArray<FRailPoint>& InPoints, float Threshold)
{
    if (InPoints.Num() < 4) return;

    float ThresholdSq = Threshold * Threshold;
    bool bLoopFound = true;

    int32 MaxIterations = 10; 
    int32 Iter = 0;

    while (bLoopFound && Iter < MaxIterations)
    {
        bLoopFound = false;
        
        for (int32 i = 0; i < InPoints.Num() - 2; ++i)
        {
            int32 BestJ = -1;

            for (int32 j = InPoints.Num() - 1; j > i + 1; --j)
            {
                float DistSq = FVector::DistSquared(InPoints[i].Position, InPoints[j].Position);
                
                if (DistSq < ThresholdSq)
                {
                    BestJ = j;
                    break;
                }
            }

            if (BestJ != -1)
            {
                int32 CountToRemove = BestJ - i - 1;
                
                if (CountToRemove > 0)
                {
                    UE_LOG(LogTemp, Log, TEXT("检测到几何环！删除索引 %d 到 %d 之间的 %d 个点"), i, BestJ, CountToRemove);
                    InPoints.RemoveAt(i + 1, CountToRemove);
                    bLoopFound = true;
                    break; // 数组变了，跳出 for 循环，重新开始下一轮 while
                }
            }
        }
        Iter++;
    }
}

void FEditableSurfaceBuilder::StitchRails()
{
    if (LeftRailResampled.Num() < 2 || RightRailResampled.Num() < 2) return;

    // Debug 绘制：显示左右边线
    if (Surface.GetWorld())
    {
        FColor LeftPointColor = FColor::Cyan;
        FColor RightPointColor = FColor::Yellow;
        FColor LeftLineColor = FColor::Cyan;
        FColor RightLineColor = FColor::Yellow;
        FColor CenterLineColor = FColor::Green;

        // 绘制左侧边线
        for (int32 i = 0; i < LeftRailResampled.Num(); ++i)
        {
            const FRailPoint& Pt = LeftRailResampled[i];
            DrawDebugPoint(Surface.GetWorld(), Pt.Position, 10.0f, LeftPointColor, true, -1.0f, 0);
            
            if (i > 0)
            {
                DrawDebugLine(Surface.GetWorld(), LeftRailResampled[i - 1].Position, Pt.Position, LeftLineColor, true, -1.0f, 0, 3.0f);
            }
        }

        // 绘制右侧边线
        for (int32 i = 0; i < RightRailResampled.Num(); ++i)
        {
            const FRailPoint& Pt = RightRailResampled[i];
            DrawDebugPoint(Surface.GetWorld(), Pt.Position, 10.0f, RightPointColor, true, -1.0f, 0);
            
            if (i > 0)
            {
                DrawDebugLine(Surface.GetWorld(), RightRailResampled[i - 1].Position, Pt.Position, RightLineColor, true, -1.0f, 0, 3.0f);
            }
        }

        // 绘制从原始采样点到左右边线的连线（仅显示部分点，避免过于密集）
        int32 DebugSampleStep = FMath::Max(1, SampledPath.Num() / 20); // 最多显示20个采样点
        for (int32 i = 0; i < SampledPath.Num(); i += DebugSampleStep)
        {
            const FSurfaceSamplePoint& Sample = SampledPath[i];
            const FCornerData& Corner = PathCornerData[i];
            
            // 找到最近的左右边线点（简化版，使用原始边线）
            if (i < LeftRailRaw.Num() && i < RightRailRaw.Num())
            {
                DrawDebugLine(Surface.GetWorld(), Sample.Location, LeftRailRaw[i].Position, CenterLineColor, true, -1.0f, 0, 1.0f);
                DrawDebugLine(Surface.GetWorld(), Sample.Location, RightRailRaw[i].Position, CenterLineColor, true, -1.0f, 0, 1.0f);
            }

            // 绘制旋转中心（仅第一个点）
            if (i == 0 && Corner.TurnRadius < FLT_MAX - 1.0f)
            {
                DrawDebugSphere(Surface.GetWorld(), Corner.RotationCenter, 20.0f, 8, FColor::Red, true, -1.0f, 0, 2.0f);
            }
        }
    }

    // 清空当前的 MeshData，开始写入
    int32 LeftStartIdx = MeshData.Vertices.Num();
    for (const FRailPoint& Pt : LeftRailResampled)
    {
        AddVertex(Pt.Position, Pt.Normal, Pt.UV);
    }

    int32 RightStartIdx = MeshData.Vertices.Num();
    for (const FRailPoint& Pt : RightRailResampled)
    {
        AddVertex(Pt.Position, Pt.Normal, Pt.UV);
    }

    // 调用内部缝合函数
    StitchRailsInternal(LeftStartIdx, RightStartIdx, LeftRailResampled.Num(), RightRailResampled.Num());
}

void FEditableSurfaceBuilder::StitchRailsInternal(int32 LeftStartIdx, int32 RightStartIdx, int32 LeftCount, int32 RightCount, bool bReverseWinding)
{
    if (LeftCount < 2 || RightCount < 2) return;

    // 开始缝合 (Triangulation)
    int32 L = 0;
    int32 R = 0;

    // 贪心算法：总是连接较短的对角线，以生成较均匀的三角形
    while (L < LeftCount - 1 && R < RightCount - 1)
    {
        int32 CurrentL = LeftStartIdx + L;
        int32 NextL    = LeftStartIdx + L + 1;
        int32 CurrentR = RightStartIdx + R;
        int32 NextR    = RightStartIdx + R + 1;

        const FVector& PosL_Curr = MeshData.Vertices[CurrentL];
        const FVector& PosL_Next = MeshData.Vertices[NextL];
        const FVector& PosR_Curr = MeshData.Vertices[CurrentR];
        const FVector& PosR_Next = MeshData.Vertices[NextR];

        // 计算两条对角线的长度平方
        float DistSq1 = FVector::DistSquared(PosL_Next, PosR_Curr);
        float DistSq2 = FVector::DistSquared(PosL_Curr, PosR_Next);

        // 选择较短的对角线进行分割
        if (DistSq1 < DistSq2)
        {
            if (bReverseWinding)
                AddTriangle(NextL, CurrentR, CurrentL); // 反转顺序
            else
                AddTriangle(CurrentL, CurrentR, NextL);
            L++;
        }
        else
        {
            if (bReverseWinding)
                AddTriangle(NextR, CurrentR, CurrentL); // 反转顺序
            else
                AddTriangle(CurrentL, CurrentR, NextR);
            R++;
        }
    }

    // 处理剩余的尾部
    while (L < LeftCount - 1)
    {
        int32 CurrentL = LeftStartIdx + L;
        int32 NextL    = LeftStartIdx + L + 1;
        int32 CurrentR = RightStartIdx + R;

        if (bReverseWinding)
            AddTriangle(NextL, CurrentR, CurrentL); // 反转顺序
        else
            AddTriangle(CurrentL, CurrentR, NextL);
        L++;
    }

    while (R < RightCount - 1)
    {
        int32 CurrentL = LeftStartIdx + L;
        int32 CurrentR = RightStartIdx + R;
        int32 NextR    = RightStartIdx + R + 1;

        if (bReverseWinding)
            AddTriangle(NextR, CurrentR, CurrentL); // 反转顺序
        else
            AddTriangle(CurrentL, CurrentR, NextR);
        R++;
    }
}

void FEditableSurfaceBuilder::BuildSingleSideSlopeVertices(const TArray<FRailPoint>& InputPoints, bool bIsRightSide)
{
    if (InputPoints.Num() < 2 || SideSmoothness < 1) return;

    float SlopeLen = bIsRightSide ? RightSlopeLength : LeftSlopeLength;
    float SlopeGrad = bIsRightSide ? RightSlopeGradient : LeftSlopeGradient;
    
    if (SlopeLen < KINDA_SMALL_NUMBER) return;

    float TotalHeight = SlopeLen * SlopeGrad;
    int32 NumCols = SideSmoothness;

    for (const FRailPoint& RailPt : InputPoints)
    {
        FVector UpVec = RailPt.Normal;
        FVector Tangent = RailPt.Tangent;
        FVector OutwardDir;

        if (bIsRightSide)
            OutwardDir = FVector::CrossProduct(UpVec, Tangent).GetSafeNormal();
        else
            OutwardDir = FVector::CrossProduct(Tangent, UpVec).GetSafeNormal();

        float CurrentU = RailPt.UV.X;
        float PreviousRelX = 0.0f;
        float PreviousRelZ = 0.0f;

        for (int32 j = 1; j <= NumCols; ++j)
        {
            float Ratio = static_cast<float>(j) / static_cast<float>(NumCols);
            float RelX = SlopeLen * Ratio;
            float RelZ = (SideSmoothness == 1) ? (TotalHeight * Ratio) : (TotalHeight * Ratio * Ratio);

            FVector Pos = RailPt.Position + (OutwardDir * RelX) + (UpVec * RelZ);
            
            // 计算简单的平滑法线
            FVector Normal = UpVec; 

            // UV 计算
            float SegDist = FMath::Sqrt(FMath::Square(RelX - PreviousRelX) + FMath::Square(RelZ - PreviousRelZ));
            float UVStep = SegDist * ModelGenConstants::GLOBAL_UV_SCALE;
            if (TextureMapping == ESurfaceTextureMapping::Stretch)
            {
                UVStep = (1.0f / SurfaceWidth) * SegDist;
            }

            if (bIsRightSide) CurrentU += UVStep;
            else              CurrentU -= UVStep;

            AddVertex(Pos, Normal, FVector2D(CurrentU, RailPt.UV.Y));

            // Debug 绘制：显示护坡顶点
            if (Surface.GetWorld())
            {
                FColor SlopePointColor = bIsRightSide ? FColor::Orange : FColor::Magenta;
                DrawDebugPoint(Surface.GetWorld(), Pos, 8.0f, SlopePointColor, true, -1.0f, 0);
                
                // 绘制从路面边缘到护坡顶点的连线
                if (j == 1)
                {
                    FColor SlopeLineColor = bIsRightSide ? FColor::Orange : FColor::Magenta;
                    DrawDebugLine(Surface.GetWorld(), RailPt.Position, Pos, SlopeLineColor, true, -1.0f, 0, 2.0f);
                }
            }

            PreviousRelX = RelX;
            PreviousRelZ = RelZ;
        }
    }
}

void FEditableSurfaceBuilder::BuildNextSlopeRail(const TArray<FRailPoint>& ReferenceRail, TArray<FRailPoint>& OutSlopeRail, bool bIsRightSide, float OffsetH, float OffsetV)
{
    int32 NumPoints = ReferenceRail.Num();
    if (NumPoints < 2) return;

    // 1. 构建原始点 (Raw Generation)
    TArray<FRailPoint> RawPoints;
    RawPoints.Reserve(NumPoints);

    for (int32 i = 0; i < NumPoints; ++i)
    {
        const FRailPoint& RefPt = ReferenceRail[i];
        FVector CurrentPos = RefPt.Position;
        FVector DirIn, DirOut;
        float DistToPrev = 0.0f;
        float DistToNext = 0.0f;

        // 计算进入向量和前向距离
        if (i == 0)
        {
            DirIn = (ReferenceRail[i + 1].Position - CurrentPos).GetSafeNormal();
            DistToPrev = FVector::Dist(ReferenceRail[i + 1].Position, CurrentPos); 
        }
        else
        {
            FVector PrevPos = ReferenceRail[i - 1].Position;
            DistToPrev = FVector::Dist(CurrentPos, PrevPos);
            DirIn = (CurrentPos - PrevPos).GetSafeNormal();
        }

        // 计算离开向量和后向距离
        if (i == NumPoints - 1)
        {
            DirOut = DirIn;
            DistToNext = DistToPrev; 
        }
        else
        {
            FVector NextPos = ReferenceRail[i + 1].Position;
            DistToNext = FVector::Dist(NextPos, CurrentPos);
            DirOut = (NextPos - CurrentPos).GetSafeNormal();
        }

        // 1. 计算平均切线 (角平分线切向)
        FVector SumDir = DirIn + DirOut;
        FVector AvgTangent;
        
        if (SumDir.SizeSquared() < KINDA_SMALL_NUMBER) AvgTangent = DirOut; 
        else AvgTangent = SumDir.GetSafeNormal();

        // 2. 计算挤出方向 (Outward Dir)
        FVector UpVec = RefPt.Normal; // 这里的 RefPt.Normal 仅作为参考坐标系的上方向
        FVector OutwardDir;

        if (bIsRightSide)
            OutwardDir = FVector::CrossProduct(UpVec, AvgTangent).GetSafeNormal();
        else
            OutwardDir = FVector::CrossProduct(AvgTangent, UpVec).GetSafeNormal();

        float DotProd = FVector::DotProduct(DirIn, DirOut);
        float ClampEffectiveOffsetH = OffsetH;

        if (DotProd < 0.99f)
        {
            FVector TurnCross = FVector::CrossProduct(DirIn, DirOut);
            float TurnDir = FVector::DotProduct(TurnCross, UpVec);
            bool bIsLeftTurn = (TurnDir > 0.0f);
            bool bIsInnerCorner = (bIsRightSide && !bIsLeftTurn) || (!bIsRightSide && bIsLeftTurn);

            if (bIsInnerCorner)
            {
                float HalfAngle = FMath::Acos(FMath::Clamp(DotProd, -1.0f, 1.0f)) * 0.5f;
                float TanHalfAngle = FMath::Tan(HalfAngle);
                float MinSegLen = FMath::Min(DistToPrev, DistToNext);
                MinSegLen = FMath::Max(MinSegLen, 0.1f);
                
                float MaxGeoOffset = MinSegLen * TanHalfAngle * 0.9f;
                if (DotProd < -0.5f) MaxGeoOffset *= 0.7f;

                if (ClampEffectiveOffsetH > MaxGeoOffset)
                {
                    ClampEffectiveOffsetH = MaxGeoOffset;
                }
            }
        }

        FVector NewPos = CurrentPos + (OutwardDir * ClampEffectiveOffsetH) + (UpVec * OffsetV);

        FVector ActualSlopeVec = NewPos - CurrentPos;
        FVector SlopeNormal;
        
        if (bIsRightSide)
        {
            SlopeNormal = FVector::CrossProduct(AvgTangent, ActualSlopeVec).GetSafeNormal();
        }
        else
        {
            SlopeNormal = FVector::CrossProduct(ActualSlopeVec, AvgTangent).GetSafeNormal();
        }

        FRailPoint NewPt;
        NewPt.Position = NewPos;
        NewPt.Normal = SlopeNormal;
        NewPt.Tangent = AvgTangent; 
        NewPt.UV = RefPt.UV; 
        
        RawPoints.Add(NewPt);
    }

    float BaseThreshold = FMath::Max(LoopRemovalThreshold, SplineSampleStep * 1.5f);
    float DynamicThreshold = FMath::Max(BaseThreshold, FMath::Abs(OffsetH) * 2.5f);
    
    UE_LOG(LogTemp, Log, TEXT("Slope Loop Removal Threshold: %.2f (Base: %.2f, OffsetH: %.2f)"), DynamicThreshold, BaseThreshold, OffsetH);

    RemoveGeometricLoops(RawPoints, DynamicThreshold);

    float WeldThreshold = 5.0f;
    TArray<FRailPoint> SimplifiedPoints;
    SimplifyRail(RawPoints, SimplifiedPoints, WeldThreshold);

    float ResampleStep = FMath::Max(SplineSampleStep, 10.0f);
    ResampleSingleRail(SimplifiedPoints, OutSlopeRail, ResampleStep);
    float UVOffset = OffsetH * ModelGenConstants::GLOBAL_UV_SCALE;
    if (TextureMapping == ESurfaceTextureMapping::Stretch)
    {
        float SafeWidth = FMath::Max(SurfaceWidth, 1.0f);
        UVOffset = (OffsetH / SafeWidth); 
    }

    for (FRailPoint& Pt : OutSlopeRail)
    {
        if (bIsRightSide) Pt.UV.X += UVOffset;
        else              Pt.UV.X -= UVOffset;
    }
}

void FEditableSurfaceBuilder::GenerateZipperSlopes(int32 LeftRoadStartIdx, int32 RightRoadStartIdx)
{
    if (SideSmoothness < 1) return;

    if (LeftSlopeLength > KINDA_SMALL_NUMBER)
    {
        const TArray<FRailPoint>& BaseRefRail = LeftRailResampled; 
        TArray<FRailPoint> StitchPrevRail = LeftRailResampled;
        int32 StitchPrevStartIdx = LeftRoadStartIdx;
        int32 StitchPrevCount = LeftRailResampled.Num();

        float TotalHeight = LeftSlopeLength * LeftSlopeGradient;

        for (int32 i = 1; i <= SideSmoothness; ++i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SideSmoothness);
            float AbsOffsetH = LeftSlopeLength * Ratio;
            float AbsOffsetV = (SideSmoothness > 1) 
                               ? (TotalHeight * Ratio * Ratio) 
                               : (TotalHeight * Ratio);

            TArray<FRailPoint> NextRail;
            BuildNextSlopeRail(BaseRefRail, NextRail, false, AbsOffsetH, AbsOffsetV);

            int32 NextStartIdx = MeshData.Vertices.Num();
            for (const FRailPoint& Pt : NextRail)
            {
                AddVertex(Pt.Position, Pt.Normal, Pt.UV);
            }

            // 4. 缝合 (Zipper Stitch)
            // 缝合是在 "上一层生成的结果" 和 "当前层" 之间进行的
            // 护坡法线需反转 (true)
            StitchRailsInternal(StitchPrevStartIdx, NextStartIdx, StitchPrevCount, NextRail.Num(), true);

            // Debug 绘制
            if (Surface.GetWorld())
            {
                FColor SlopePointColor = FColor::Magenta;
                for (const FRailPoint& P : NextRail)
                    DrawDebugPoint(Surface.GetWorld(), P.Position, 8.0f, SlopePointColor, true, -1.0f, 0);
            }

            TopStartIndices.Insert(NextStartIdx, 0);
            TopEndIndices.Insert(NextStartIdx + NextRail.Num() - 1, 0);

            StitchPrevRail = NextRail; 
            StitchPrevStartIdx = NextStartIdx;
            StitchPrevCount = NextRail.Num();
            
            // 更新最终边缘 (总是最新的一层)
            FinalLeftRail = NextRail;
        }
    }

    if (RightSlopeLength > KINDA_SMALL_NUMBER)
    {
        const TArray<FRailPoint>& BaseRefRail = RightRailResampled;

        TArray<FRailPoint> StitchPrevRail = RightRailResampled;
        int32 StitchPrevStartIdx = RightRoadStartIdx;
        int32 StitchPrevCount = RightRailResampled.Num();

        float TotalHeight = RightSlopeLength * RightSlopeGradient;

        for (int32 i = 1; i <= SideSmoothness; ++i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SideSmoothness);
            
            float AbsOffsetH = RightSlopeLength * Ratio;
            float AbsOffsetV = (SideSmoothness > 1) 
                               ? (TotalHeight * Ratio * Ratio) 
                               : (TotalHeight * Ratio);

            TArray<FRailPoint> NextRail;
            BuildNextSlopeRail(BaseRefRail, NextRail, true, AbsOffsetH, AbsOffsetV);

            int32 NextStartIdx = MeshData.Vertices.Num();
            for (const FRailPoint& Pt : NextRail)
            {
                AddVertex(Pt.Position, Pt.Normal, Pt.UV);
            }

            StitchRailsInternal(StitchPrevStartIdx, NextStartIdx, StitchPrevCount, NextRail.Num(), false);

            if (Surface.GetWorld())
            {
                FColor SlopePointColor = FColor::Orange;
                for (const FRailPoint& P : NextRail)
                    DrawDebugPoint(Surface.GetWorld(), P.Position, 8.0f, SlopePointColor, true, -1.0f, 0);
            }

            TopStartIndices.Add(NextStartIdx);
            TopEndIndices.Add(NextStartIdx + NextRail.Num() - 1);

            StitchPrevRail = NextRail;
            StitchPrevStartIdx = NextStartIdx;
            StitchPrevCount = NextRail.Num();
            FinalRightRail = NextRail;
        }
    }
}

void FEditableSurfaceBuilder::GenerateZipperThickness()
{
    // 如果没有生成任何顶点，或者不需要厚度，直接返回
    if (MeshData.Vertices.Num() < 3 || !bEnableThickness || ThicknessValue <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    int32 TopVertexCount = MeshData.Vertices.Num();
    int32 TopTriangleCount = MeshData.Triangles.Num();
    int32 BottomStartIndex = TopVertexCount;

    for (int32 i = 0; i < TopVertexCount; ++i)
    {
        FVector TopPos = MeshData.Vertices[i];
        FVector TopNormal = MeshData.Normals[i];
        FVector2D TopUV = MeshData.UVs[i];

        FVector BottomPos = TopPos - (TopNormal * ThicknessValue);
        FVector BottomNormal = -TopNormal;
        FVector2D BottomUV = TopUV; 

        AddVertex(BottomPos, BottomNormal, BottomUV);
    }

    for (int32 i = 0; i < TopTriangleCount; i += 3)
    {
        int32 Idx0 = MeshData.Triangles[i];
        int32 Idx1 = MeshData.Triangles[i + 1];
        int32 Idx2 = MeshData.Triangles[i + 2];

        int32 BottomIdx0 = Idx0 + BottomStartIndex;
        int32 BottomIdx1 = Idx1 + BottomStartIndex;
        int32 BottomIdx2 = Idx2 + BottomStartIndex;

        AddTriangle(BottomIdx0, BottomIdx2, BottomIdx1);
    }

    auto BuildSideWall = [&](const TArray<FRailPoint>& Rail, bool bIsRightSide)
    {
        if (Rail.Num() < 2) return;

        float V_Bottom = ThicknessValue * ModelGenConstants::GLOBAL_UV_SCALE;
        
        for (int32 i = 0; i < Rail.Num() - 1; ++i)
        {
            const FRailPoint& P0 = Rail[i];
            const FRailPoint& P1 = Rail[i + 1];

            FVector TopPos0 = P0.Position;
            FVector TopPos1 = P1.Position;
            
            // 计算该段墙面的法线
            FVector SegDir = (TopPos1 - TopPos0).GetSafeNormal();
            FVector WallNormal;
            
            if (bIsRightSide)
                WallNormal = FVector::CrossProduct(SegDir, P0.Normal).GetSafeNormal(); // 右侧向右
            else
                WallNormal = FVector::CrossProduct(P0.Normal, SegDir).GetSafeNormal(); // 左侧向左

            // 计算底面位置
            FVector BotPos0 = TopPos0 - (P0.Normal * ThicknessValue);
            FVector BotPos1 = TopPos1 - (P1.Normal * ThicknessValue);

            // UV 计算：U 沿路径，V 沿厚度
            float U0 = P0.DistanceAlongRail * ModelGenConstants::GLOBAL_UV_SCALE;
            float U1 = P1.DistanceAlongRail * ModelGenConstants::GLOBAL_UV_SCALE;
            
            // 添加 4 个顶点 (Top0, Top1, Bot0, Bot1)
            int32 V_TL = AddVertex(TopPos0, WallNormal, FVector2D(U0, 0.0f));
            int32 V_TR = AddVertex(TopPos1, WallNormal, FVector2D(U1, 0.0f));
            int32 V_BL = AddVertex(BotPos0, WallNormal, FVector2D(U0, V_Bottom));
            int32 V_BR = AddVertex(BotPos1, WallNormal, FVector2D(U1, V_Bottom));

            // 添加两个三角形组成 Quad
            if (bIsRightSide)
            {
                // 右侧墙壁：TL -> BL -> BR -> TR (逆时针看是背面，所以要顺时针?)
                // Standard Quad: TL, BL, BR, TR implies (TL, BL, BR) & (TL, BR, TR)
                // Need to check winding.
                // Right side wall faces "Right".
                AddQuad(V_TL, V_BL, V_BR, V_TR);
            }
            else
            {
                // 左侧墙壁：面向左侧
                AddQuad(V_TR, V_BR, V_BL, V_TL); 
            }
        }
    };

    BuildSideWall(FinalLeftRail, false);
    BuildSideWall(FinalRightRail, true);
    
    auto BuildCap = [&](const TArray<int32>& Indices, bool bIsStartCap)
    {
        if (Indices.Num() < 2) return;

        float V_Bottom = ThicknessValue * ModelGenConstants::GLOBAL_UV_SCALE;

        for (int32 i = 0; i < Indices.Num() - 1; ++i)
        {
            int32 TopIdx0 = Indices[i];
            int32 TopIdx1 = Indices[i + 1];
            int32 BotIdx0 = TopIdx0 + TopVertexCount;
            int32 BotIdx1 = TopIdx1 + TopVertexCount;

            FVector P_TL = MeshData.Vertices[TopIdx0];
            FVector P_TR = MeshData.Vertices[TopIdx1];
            FVector P_BL = MeshData.Vertices[BotIdx0];
            FVector P_BR = MeshData.Vertices[BotIdx1];

            FVector FaceNormal;
            if (bIsStartCap)
                FaceNormal = FVector::CrossProduct(P_BL - P_TL, P_TR - P_TL).GetSafeNormal();
            else
                FaceNormal = FVector::CrossProduct(P_TR - P_TL, P_BL - P_TL).GetSafeNormal();

            float U0 = 0.0f; 
            float U1 = 1.0f;

            int32 V_TL = AddVertex(P_TL, FaceNormal, FVector2D(U0, 0.0f));
            int32 V_TR = AddVertex(P_TR, FaceNormal, FVector2D(U1, 0.0f));
            int32 V_BL = AddVertex(P_BL, FaceNormal, FVector2D(U0, V_Bottom));
            int32 V_BR = AddVertex(P_BR, FaceNormal, FVector2D(U1, V_Bottom));

            if (bIsStartCap)
            {
                AddQuad(V_TL, V_BL, V_BR, V_TR);
            }
            else
            {
                AddQuad(V_TR, V_BR, V_BL, V_TL);
            }
        }
    };

    BuildCap(TopStartIndices, true);
    BuildCap(TopEndIndices, false);
}
