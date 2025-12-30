// Copyright (c) 2024. All rights reserved.

#include "EditableSurfaceBuilder.h"

#include "ModelGenConstants.h"

#include "Components/SplineComponent.h"
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
    
    SampledPath.Empty();
    PathCornerData.Empty();
    
    LeftRailRaw.Empty();
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

bool FEditableSurfaceBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2)
    {
        return false;
    }

    Clear();

    if (!bEnableThickness)
    {
        ThicknessValue = 0.01f;
    }

    SampleSplinePath();
    CalculateCornerGeometry();
    GenerateRoadSurface();
    
    if (bEnableThickness)
    {
        GenerateThickness();
    }

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

void FEditableSurfaceBuilder::GenerateRoadSurface()
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

    int32 LeftRoadStartIdx = MeshData.Vertices.Num();
    for (const FRailPoint& Pt : LeftRailResampled)
    {
        FVector FinalN = CalculateSurfaceNormal(Pt, false);
        AddVertex(Pt.Position, FinalN, Pt.UV);
    }

    int32 RightRoadStartIdx = MeshData.Vertices.Num();
    for (const FRailPoint& Pt : RightRailResampled)
    {
        FVector FinalN = CalculateSurfaceNormal(Pt, true);
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

    GenerateSlopes(LeftRoadStartIdx, RightRoadStartIdx);
}

FVector FEditableSurfaceBuilder::CalculateSurfaceNormal(const FRailPoint& Pt, bool bIsRightSide) const
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
}

void FEditableSurfaceBuilder::BuildRawRails()
{
    int32 NumPoints = SampledPath.Num();
    LeftRailRaw.Reset(NumPoints);
    RightRailRaw.Reset(NumPoints);

    float HalfWidth = SurfaceWidth * 0.5f;
    FVector LastValidLeftPos = FVector::ZeroVector;
    FVector LastValidRightPos = FVector::ZeroVector;

    for (int32 i = 0; i < NumPoints; ++i)
    {
        const FSurfaceSamplePoint& Sample = SampledPath[i];
        const FCornerData& Corner = PathCornerData[i];

        FVector LeftPos = CalculateRawPointPosition(Sample, Corner, HalfWidth, false);
        FVector RightPos = CalculateRawPointPosition(Sample, Corner, HalfWidth, true);

        if (i > 0)
        {
            ApplyMonotonicityConstraint(LeftPos, LastValidLeftPos, Sample.Tangent);
            ApplyMonotonicityConstraint(RightPos, LastValidRightPos, Sample.Tangent);
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

FVector FEditableSurfaceBuilder::CalculateRawPointPosition(const FSurfaceSamplePoint& Sample, const FCornerData& Corner, float HalfWidth, bool bIsRightSide)
{
    float CurrentHalfWidth = Sample.InterpolatedWidth * 0.5f;
    float WidthScaleRatio = (HalfWidth > KINDA_SMALL_NUMBER) ? (CurrentHalfWidth / HalfWidth) : 1.0f;
    
    float BaseOffset = bIsRightSide ? (HalfWidth * WidthScaleRatio) : (-HalfWidth * WidthScaleRatio);

    // 直接使用基础偏移量，不使用 Miter 缩放
    return Sample.Location + (Sample.RightVector * BaseOffset);
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
        
        FVector Tangent = (P_Next - P_Prev).GetSafeNormal();
        
        if (Tangent.IsZero())
        {
            Tangent = InPoints[i].Tangent;
        }
        GeoTangents[i] = Tangent;
    }

    TArray<float> AccumulatedDists;
    AccumulatedDists.Reserve(InPoints.Num());
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
        OutPoints.Reset();
        OutPoints.Add(InPoints[0]);
        OutPoints.Add(InPoints.Last());
        return;
    }

    int32 NumSegments = FMath::Max(1, FMath::CeilToInt(TotalLength / SegmentLength));
    float ActualStep = TotalLength / NumSegments;

    OutPoints.Reset(NumSegments + 1);
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

        FVector T0 = GeoTangents[CurrentIndex] * Span;
        FVector T1 = GeoTangents[CurrentIndex + 1] * Span;
        
        FRailPoint NewPt;
        NewPt.Position = FMath::CubicInterp(P0.Position, T0, P1.Position, T1, Alpha);
        NewPt.Normal = FMath::Lerp(P0.Normal, P1.Normal, Alpha).GetSafeNormal();
        NewPt.Tangent = FMath::Lerp(P0.Tangent, P1.Tangent, Alpha).GetSafeNormal();
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
        FVector UpVec = RefPt.Normal;
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

void FEditableSurfaceBuilder::GenerateSlopes(int32 LeftRoadStartIdx, int32 RightRoadStartIdx)
{
    if (SideSmoothness < 1) return;

    if (LeftSlopeLength > KINDA_SMALL_NUMBER)
    {
        GenerateSingleSideSlope(LeftRailResampled, LeftSlopeLength, LeftSlopeGradient, LeftRoadStartIdx, false);
    }

    if (RightSlopeLength > KINDA_SMALL_NUMBER)
    {
        GenerateSingleSideSlope(RightRailResampled, RightSlopeLength, RightSlopeGradient, RightRoadStartIdx, true);
    }
}

void FEditableSurfaceBuilder::GenerateSingleSideSlope(const TArray<FRailPoint>& BaseRail, float Length, float Gradient, int32 StartIndex, bool bIsRightSide)
{
    TArray<FRailPoint> StitchPrevRail = BaseRail;
    int32 StitchPrevStartIdx = StartIndex;
    int32 StitchPrevCount = BaseRail.Num();

    float TotalHeight = Length * Gradient;

    for (int32 i = 1; i <= SideSmoothness; ++i)
    {
        float Ratio = static_cast<float>(i) / static_cast<float>(SideSmoothness);
        float AbsOffsetH = Length * Ratio;
        float AbsOffsetV = (SideSmoothness > 1) ? (TotalHeight * Ratio * Ratio) : (TotalHeight * Ratio);

        TArray<FRailPoint> NextRail;
        BuildNextSlopeRail(BaseRail, NextRail, bIsRightSide, AbsOffsetH, AbsOffsetV);

        int32 NextStartIdx = MeshData.Vertices.Num();
        for (const FRailPoint& Pt : NextRail)
        {
            AddVertex(Pt.Position, Pt.Normal, Pt.UV);
        }

        bool bReverseWinding = !bIsRightSide;
        StitchRailsInternal(StitchPrevStartIdx, NextStartIdx, StitchPrevCount, NextRail.Num(), bReverseWinding);

        if (bIsRightSide)
        {
            TopStartIndices.Add(NextStartIdx);
            TopEndIndices.Add(NextStartIdx + NextRail.Num() - 1);
            FinalRightRail = NextRail;
                }
                else
                {
            TopStartIndices.Insert(NextStartIdx, 0);
            TopEndIndices.Insert(NextStartIdx + NextRail.Num() - 1, 0);
            FinalLeftRail = NextRail;
        }

        StitchPrevRail = NextRail;
        StitchPrevStartIdx = NextStartIdx;
        StitchPrevCount = NextRail.Num();
    }
}

void FEditableSurfaceBuilder::GenerateThickness()
{
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
        
        AddVertex(BottomPos, BottomNormal, TopUV);
    }

    for (int32 i = 0; i < TopTriangleCount; i += 3)
    {
        int32 Idx0 = MeshData.Triangles[i];
        int32 Idx1 = MeshData.Triangles[i + 1];
        int32 Idx2 = MeshData.Triangles[i + 2];

        AddTriangle(Idx0 + BottomStartIndex, Idx2 + BottomStartIndex, Idx1 + BottomStartIndex);
    }

    BuildSideWall(FinalLeftRail, false);
    BuildSideWall(FinalRightRail, true);

    BuildCap(TopStartIndices, true);
    BuildCap(TopEndIndices, false);
}

void FEditableSurfaceBuilder::BuildSideWall(const TArray<FRailPoint>& Rail, bool bIsRightSide)
{
    if (Rail.Num() < 2) return;

    float V_Bottom = ThicknessValue * ModelGenConstants::GLOBAL_UV_SCALE;
    
    for (int32 i = 0; i < Rail.Num() - 1; ++i)
    {
        const FRailPoint& P0 = Rail[i];
        const FRailPoint& P1 = Rail[i + 1];

        FVector SegDir = (P1.Position - P0.Position).GetSafeNormal();
        FVector WallNormal;
        if (bIsRightSide)
            WallNormal = FVector::CrossProduct(SegDir, P0.Normal).GetSafeNormal();
        else
            WallNormal = FVector::CrossProduct(P0.Normal, SegDir).GetSafeNormal();

        FVector TopPos0 = P0.Position;
        FVector TopPos1 = P1.Position;
        FVector BotPos0 = TopPos0 - (P0.Normal * ThicknessValue);
        FVector BotPos1 = TopPos1 - (P1.Normal * ThicknessValue);

        float U0 = P0.DistanceAlongRail * ModelGenConstants::GLOBAL_UV_SCALE;
        float U1 = P1.DistanceAlongRail * ModelGenConstants::GLOBAL_UV_SCALE;
        
        int32 V_TL = AddVertex(TopPos0, WallNormal, FVector2D(U0, 0.0f));
        int32 V_TR = AddVertex(TopPos1, WallNormal, FVector2D(U1, 0.0f));
        int32 V_BL = AddVertex(BotPos0, WallNormal, FVector2D(U0, V_Bottom));
        int32 V_BR = AddVertex(BotPos1, WallNormal, FVector2D(U1, V_Bottom));

        if (bIsRightSide)
            AddQuad(V_TL, V_BL, V_BR, V_TR);
        else
                    AddQuad(V_TR, V_BR, V_BL, V_TL);
                }
}

void FEditableSurfaceBuilder::BuildCap(const TArray<int32>& Indices, bool bIsStartCap)
{
    if (Indices.Num() < 2) return;

    float V_Bottom = ThicknessValue * ModelGenConstants::GLOBAL_UV_SCALE;

    for (int32 i = 0; i < Indices.Num() - 1; ++i)
    {
        int32 TopIdx0 = Indices[i];
        int32 TopIdx1 = Indices[i + 1];

        FVector P_TL = MeshData.Vertices[TopIdx0];
        FVector P_TR = MeshData.Vertices[TopIdx1];
        FVector Normal0 = MeshData.Normals[TopIdx0];
        FVector Normal1 = MeshData.Normals[TopIdx1];
        
        FVector P_BL = P_TL - (Normal0 * ThicknessValue);
        FVector P_BR = P_TR - (Normal1 * ThicknessValue);

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
            AddQuad(V_TL, V_BL, V_BR, V_TR);
        else
            AddQuad(V_TR, V_BR, V_BL, V_TL);
    }
}

void FEditableSurfaceBuilder::ApplyMonotonicityConstraint(FVector& Pos, const FVector& LastValidPos, const FVector& Tangent)
{
    FVector Delta = Pos - LastValidPos;
    float Progress = FVector::DotProduct(Delta, Tangent);
    
    if (Progress < 0.001f)
    {
        Pos = LastValidPos;
    }
}