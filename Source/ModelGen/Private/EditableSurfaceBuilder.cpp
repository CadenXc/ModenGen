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
    
    // 安全检查：防止用户输入过小的值导致崩溃
    if (SplineSampleStep < 1.0f) SplineSampleStep = 10.0f;

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
    // BuildProfileDefinition(); // 拉链法第一步暂时跳过护坡逻辑
    GenerateZipperRoadMesh(); // 使用拉链法生成核心路面
    // GenerateThickness(); // 厚度生成暂时跳过 (待路面逻辑稳定后添加)
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

    // =========================================================
    // [新增]：强力去除"尾巴" (Loop Removal)
    // =========================================================
    
    // 阈值设定：也就是你图片里红框那个缺口的宽度容差。
    // 如果尾巴的根部宽度小于这个值，整个尾巴就会被切掉。
    // 建议设为采样步长的 1.5 倍或一个固定值 (例如 50-100)
    float LoopThreshold = 100.0f; 

    int32 LeftRawCountBefore = LeftRailRaw.Num();
    int32 RightRawCountBefore = RightRailRaw.Num();
    
    RemoveGeometricLoops(LeftRailRaw, LoopThreshold);
    RemoveGeometricLoops(RightRailRaw, LoopThreshold);

    // 调试日志：查看去环前后的点数变化
    UE_LOG(LogTemp, Log, TEXT("Zipper Loop Removal: Left %d -> %d, Right %d -> %d"), 
        LeftRawCountBefore, LeftRailRaw.Num(), RightRawCountBefore, RightRailRaw.Num());

    // 步骤 2: [新增] 简化边线 (Weld Vertices)
    // 阈值设定：比如 5.0f 或 10.0f。这意味着如果两个点相距小于 5cm，它们就会合并。
    // 对于急转弯内侧，那些点通常是完全重合的(距离为0)，所以 5.0f 足够了。
    float WeldThreshold = 5.0f; 
    
    // 定义中间数组
    TArray<FRailPoint> LeftRailSimplified;
    TArray<FRailPoint> RightRailSimplified;

    SimplifyRail(LeftRailRaw, LeftRailSimplified, WeldThreshold);
    SimplifyRail(RightRailRaw, RightRailSimplified, WeldThreshold);

    // 调试日志：查看简化前后的点数变化
    UE_LOG(LogTemp, Log, TEXT("Zipper Simplify: Left Raw=%d -> Simplified=%d, Right Raw=%d -> Simplified=%d"), 
        LeftRailRaw.Num(), LeftRailSimplified.Num(), RightRailRaw.Num(), RightRailSimplified.Num());

    // 步骤 3: 根据物理长度重采样
    // 注意：现在输入源变成了 Simplified 数组
    // 确保采样步长合理，Max(..., 10.0f) 是为了防止除以0
    float ResampleStep = FMath::Max(SplineSampleStep, 10.0f);
    
    ResampleSingleRail(LeftRailSimplified, LeftRailResampled, ResampleStep);
    ResampleSingleRail(RightRailSimplified, RightRailResampled, ResampleStep);

    // 调试日志：查看重采样后的点数
    UE_LOG(LogTemp, Log, TEXT("Zipper Resample: Left Points=%d, Right Points=%d"), 
        LeftRailResampled.Num(), RightRailResampled.Num());

    // 步骤 4: 缝合 (Zipper)
    StitchRails();
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

        // 2. 计算左侧点 (原始位置)
        // 判断左侧是否为内侧
        bool bIsLeftInner = Corner.bIsConvexLeft; // 左转时，左侧是内侧
        
        // 只有内侧才限制 MiterScale，外侧完全放开（不限制）
        float AppliedScaleLeft = Corner.MiterScale;
        if (bIsLeftInner)
        {
            // 内侧：限制 MiterScale 防止过度尖锐
            if (AppliedScaleLeft > 2.0f) AppliedScaleLeft = 2.0f;
        }
        // 外侧：完全不限制，使用原始的 MiterScale 值（可能很大，比如 5.0, 10.0 等）
        
        float LeftMiterDist = LeftOffset * AppliedScaleLeft;

        // [防穿模]：左侧如果是内侧，必须限制在旋转中心以内
        if (bIsLeftInner && Corner.TurnRadius < 10000.0f)
        {
             // 留一点安全距离(2.0f)，避免浮点数重合
             float MaxDist = FMath::Max(Corner.TurnRadius - 2.0f, 0.0f);
             if (FMath::Abs(LeftMiterDist) > MaxDist) LeftMiterDist = -MaxDist;
        }
        FVector LeftPos = Sample.Location + (Corner.MiterVector * LeftMiterDist);

        // 3. 计算右侧点 (原始位置)
        // 判断右侧是否为内侧
        bool bIsRightInner = !Corner.bIsConvexLeft; // 右转时，右侧是内侧
        
        // 只有内侧才限制 MiterScale，外侧完全放开（不限制）
        float AppliedScaleRight = Corner.MiterScale;
        if (bIsRightInner)
        {
            // 内侧：限制 MiterScale 防止过度尖锐
            if (AppliedScaleRight > 2.0f) AppliedScaleRight = 2.0f;
        }
        // 外侧：完全不限制，使用原始的 MiterScale 值（可能很大，比如 5.0, 10.0 等）
        
        float RightMiterDist = RightOffset * AppliedScaleRight;

        // [防穿模]：右侧如果是内侧，必须限制在旋转中心以内
        if (bIsRightInner && Corner.TurnRadius < 10000.0f)
        {
             float MaxDist = FMath::Max(Corner.TurnRadius - 2.0f, 0.0f);
             if (RightMiterDist > MaxDist) RightMiterDist = MaxDist;
        }
        FVector RightPos = Sample.Location + (Corner.MiterVector * RightMiterDist);

        // ==============================================================================
        // [修改]：单调性检查 V2 (直接吸附，不推移)
        // ==============================================================================

        if (i > 0)
        {
            // --- 左侧 ---
            FVector LeftDelta = LeftPos - LastValidLeftPos;
            float LeftProgress = FVector::DotProduct(LeftDelta, Sample.Tangent);
            
            // 如果倒退或进度极小，直接吸附到上一个点 (不要 + 0.1f 了)
            if (LeftProgress < 0.001f) 
            {
                LeftPos = LastValidLeftPos; 
            }

            // --- 右侧 ---
            FVector RightDelta = RightPos - LastValidRightPos;
            float RightProgress = FVector::DotProduct(RightDelta, Sample.Tangent);

            if (RightProgress < 0.001f)
            {
                RightPos = LastValidRightPos;
            }
        }
        
        // 更新 LastValidPos
        LastValidLeftPos = LeftPos;
        LastValidRightPos = RightPos;

        // 4. 存入数据
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

    // 1. 计算总长度并构建距离表
    TArray<float> AccumulatedDists;
    AccumulatedDists.Add(0.0f);
    float TotalLength = 0.0f;

    for (int32 i = 0; i < InPoints.Num() - 1; ++i)
    {
        float SegLen = FVector::Dist(InPoints[i].Position, InPoints[i+1].Position);
        TotalLength += SegLen;
        AccumulatedDists.Add(TotalLength);
    }

    // 处理长度为 0 或极小的特殊情况（单调性约束可能导致所有点重合）
    if (TotalLength < KINDA_SMALL_NUMBER)
    {
        // 只保留起点和终点（如果它们不同）
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

    // 2. 决定新点的数量
    // 即使内圈很短，我们也至少保持和样条线相似的密度，或者根据物理长度计算
    int32 NumSegments = FMath::CeilToInt(TotalLength / SegmentLength);
    // 即使长度极短，至少保留一段（起点和终点），否则无法成面
    NumSegments = FMath::Max(NumSegments, 1); 
    
    // 真实的步长 (为了平均分布)
    float ActualStep = TotalLength / NumSegments;

    OutPoints.Reserve(NumSegments + 1);

    // 3. 执行插值重采样
    int32 CurrentIndex = 0;

    for (int32 i = 0; i <= NumSegments; ++i)
    {
        float TargetDist = static_cast<float>(i) * ActualStep;
        
        // 找到包含 TargetDist 的原始线段 [CurrentIndex, CurrentIndex+1]
        while (CurrentIndex < AccumulatedDists.Num() - 1 && AccumulatedDists[CurrentIndex + 1] < TargetDist)
        {
            CurrentIndex++;
        }

        // 边界保护
        if (CurrentIndex >= InPoints.Num() - 1)
        {
            FRailPoint EndPt = InPoints.Last();
            EndPt.DistanceAlongRail = TotalLength;
            // 计算 UV V坐标
            float V = 0.0f;
            if (TextureMapping == ESurfaceTextureMapping::Stretch)
            {
                V = 1.0f;
            }
            else
            {
                V = TotalLength * ModelGenConstants::GLOBAL_UV_SCALE;
            }
            EndPt.UV.Y = V;
            OutPoints.Add(EndPt);
            continue;
        }

        // 计算插值 Alpha
        float StartD = AccumulatedDists[CurrentIndex];
        float EndD = AccumulatedDists[CurrentIndex + 1];
        float Span = EndD - StartD;
        float Alpha = (Span > KINDA_SMALL_NUMBER) ? (TargetDist - StartD) / Span : 0.0f;

        const FRailPoint& P0 = InPoints[CurrentIndex];
        const FRailPoint& P1 = InPoints[CurrentIndex + 1];

        FRailPoint NewPt;
        NewPt.Position = FMath::Lerp(P0.Position, P1.Position, Alpha);
        NewPt.Normal = FMath::Lerp(P0.Normal, P1.Normal, Alpha).GetSafeNormal();
        NewPt.Tangent = FMath::Lerp(P0.Tangent, P1.Tangent, Alpha).GetSafeNormal();
        NewPt.UV.X = P0.UV.X; // 保持 U 不变 (0 或 1)
        NewPt.DistanceAlongRail = TargetDist;

        // 计算 UV V坐标
        float V = 0.0f;
        if (TextureMapping == ESurfaceTextureMapping::Stretch)
        {
            V = TargetDist / TotalLength;
        }
        else
        {
            V = TargetDist * ModelGenConstants::GLOBAL_UV_SCALE;
        }
        NewPt.UV.Y = V;

        OutPoints.Add(NewPt);
    }
}

void FEditableSurfaceBuilder::SimplifyRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float MergeThreshold)
{
    if (InPoints.Num() == 0) return;

    OutPoints.Reset();
    OutPoints.Add(InPoints[0]); // 总是保留第一个点

    float ThresholdSq = MergeThreshold * MergeThreshold;

    for (int32 i = 1; i < InPoints.Num(); ++i)
    {
        const FRailPoint& CurrentPt = InPoints[i];
        const FRailPoint& LastKeptPt = OutPoints.Last();

        float DistSq = FVector::DistSquared(CurrentPt.Position, LastKeptPt.Position);

        // 只有当距离足够远时，或者是最后一个点时，才添加
        // 注意：我们也必须保留最后一个点，以保证路面闭合
        bool bIsLast = (i == InPoints.Num() - 1);

        if (DistSq > ThresholdSq || bIsLast)
        {
            OutPoints.Add(CurrentPt);
        }
        else
        {
            // 如果距离太近，我们就跳过这个点（相当于合并了）
            // 这里什么都不用做，直接 continue
        }
    }
}

void FEditableSurfaceBuilder::RemoveGeometricLoops(TArray<FRailPoint>& InPoints, float Threshold)
{
    if (InPoints.Num() < 4) return;

    float ThresholdSq = Threshold * Threshold;
    bool bLoopFound = true;

    // 使用 while 循环是因为删除一段后，可能会暴露出新的合并机会
    // 或者简单点，我们扫一遍就行。这里为了稳健，扫到没有环为止。
    int32 MaxIterations = 10; 
    int32 Iter = 0;

    while (bLoopFound && Iter < MaxIterations)
    {
        bLoopFound = false;
        
        // 遍历每一个点作为"环的起点"
        for (int32 i = 0; i < InPoints.Num() - 2; ++i)
        {
            // 从后往前找，找"环的终点"
            // 我们希望找到离 i 最近但在序列中最远的点，这样能切掉最大的尾巴
            int32 BestJ = -1;

            for (int32 j = InPoints.Num() - 1; j > i + 1; --j)
            {
                float DistSq = FVector::DistSquared(InPoints[i].Position, InPoints[j].Position);
                
                // 如果发现 i 和 j 很近（说明中间绕了一圈回来了）
                if (DistSq < ThresholdSq)
                {
                    BestJ = j;
                    break; // 找到了最大的环，直接锁定
                }
            }

            // 如果找到了闭环
            if (BestJ != -1)
            {
                // 计算中间有多少个点需要删除
                // 比如 i=5, j=8。保留5和8，删除 6,7。
                int32 CountToRemove = BestJ - i - 1;
                
                if (CountToRemove > 0)
                {
                    UE_LOG(LogTemp, Log, TEXT("检测到几何环！删除索引 %d 到 %d 之间的 %d 个点"), i, BestJ, CountToRemove);
                    InPoints.RemoveAt(i + 1, CountToRemove);
                    bLoopFound = true; // 标记发生了修改
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
    // 注意：在 Resample 阶段我们只是生成了点，现在才真正 AddVertex 到 MeshData
    // 为了优化，我们不复用顶点（Flat Shading 风格），或者复用顶点（Smooth Shading）
    // 这里使用复用顶点的方式，先将两排点全部加入 Vertex Buffer

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

    // 开始缝合 (Triangulation)
    int32 L = 0;
    int32 R = 0;
    int32 L_Num = LeftRailResampled.Num();
    int32 R_Num = RightRailResampled.Num();

    // 贪心算法：总是连接较短的对角线，以生成较均匀的三角形
    while (L < L_Num - 1 && R < R_Num - 1)
    {
        int32 CurrentL = LeftStartIdx + L;
        int32 NextL    = LeftStartIdx + L + 1;
        int32 CurrentR = RightStartIdx + R;
        int32 NextR    = RightStartIdx + R + 1;

        // 两个候选点的物理位置
        const FVector& PosL_Next = LeftRailResampled[L + 1].Position;
        const FVector& PosR_Curr = RightRailResampled[R].Position;
        
        const FVector& PosL_Curr = LeftRailResampled[L].Position;
        const FVector& PosR_Next = RightRailResampled[R + 1].Position;

        // 计算两条对角线的长度平方
        // 选项1: 形成三角形 (CurrentL, CurrentR, NextL) -> 对角线是 NextL 到 CurrentR
        float DistSq1 = FVector::DistSquared(PosL_Next, PosR_Curr);

        // 选项2: 形成三角形 (CurrentL, CurrentR, NextR) -> 对角线是 CurrentL 到 NextR
        float DistSq2 = FVector::DistSquared(PosL_Curr, PosR_Next);

        // 选择较短的对角线进行分割
        if (DistSq1 < DistSq2)
        {
            // 添加三角形 (CurrentL, CurrentR, NextL) -> 逆时针顺序（从上方看）
            AddTriangle(CurrentL, CurrentR, NextL);
            L++; // 左侧前进
        }
        else
        {
            // 添加三角形 (CurrentL, CurrentR, NextR) -> 逆时针顺序（从上方看）
            AddTriangle(CurrentL, CurrentR, NextR);
            R++; // 右侧前进
        }
    }

    // 处理剩余的尾部
    // 如果左边还有剩余，全部连到右边的最后一个点
    while (L < L_Num - 1)
    {
        int32 CurrentL = LeftStartIdx + L;
        int32 NextL    = LeftStartIdx + L + 1;
        int32 CurrentR = RightStartIdx + R; // R 已经是最后一个了

        AddTriangle(CurrentL, CurrentR, NextL);
        L++;
    }

    // 如果右边还有剩余，全部连到左边的最后一个点
    while (R < R_Num - 1)
    {
        int32 CurrentL = LeftStartIdx + L; // L 已经是最后一个了
        int32 CurrentR = RightStartIdx + R;
        int32 NextR    = RightStartIdx + R + 1;

        AddTriangle(CurrentL, CurrentR, NextR);
        R++;
    }
}
