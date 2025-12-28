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
            if (Profile.bIsSlope)
            {
                float SlopeScale = bIsPointRightSide ? Corner.RightSlopeMiterScale : Corner.LeftSlopeMiterScale;
                AppliedScale *= SlopeScale;
            }

            float IntendedMiterDist = BaseOffsetH * AppliedScale;
            FinalPos = Sample.Location + (Corner.MiterVector * IntendedMiterDist) + (Sample.Normal * Profile.OffsetV);

            if (i > 0)
            {
                FVector PrevPos = PreviousRowPositions[j];
                FVector MovementVector = FinalPos - PrevPos;
                float ForwardProgress = FVector::DotProduct(MovementVector, Sample.Tangent);

                if (ForwardProgress <= 0.1f)
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
    BuildProfileDefinition();
    GenerateGridMesh();
    GenerateThickness();
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
