// Copyright (c) 2024. All rights reserved.

#include "EditableSurfaceBuilder.h"
#include "EditableSurface.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"
#include "Components/SplineComponent.h"

FEditableSurfaceBuilder::FEditableSurfaceBuilder(const AEditableSurface& InSurface)
    : Surface(InSurface)
{
    Clear();
}

void FEditableSurfaceBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    FrontCrossSections.Empty();
    FrontVertexStartIndex = 0;
    FrontVertexCount = 0;
}

bool FEditableSurfaceBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Surface.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    SplineComponent = Surface.SplineComponent;
    // PathSampleCount 在 GenerateSurfaceMesh 中会被动态重新计算，这里只需要初始赋值
    PathSampleCount = Surface.PathSampleCount;
    SurfaceWidth = Surface.SurfaceWidth;
    bEnableThickness = Surface.bEnableThickness;
    ThicknessValue = Surface.ThicknessValue;
    SideSmoothness = Surface.SideSmoothness;
    RightSlopeLength = Surface.RightSlopeLength;
    RightSlopeGradient = Surface.RightSlopeGradient;
    LeftSlopeLength = Surface.LeftSlopeLength;
    LeftSlopeGradient = Surface.LeftSlopeGradient;
    TextureMapping = Surface.TextureMapping;

    // 如果厚度关闭，强制生成极薄的厚度
    float GenerationThickness = bEnableThickness ? ThicknessValue : 0.01f;
    float OriginalThickness = ThicknessValue;
    ThicknessValue = GenerationThickness;

    GenerateSurfaceMesh();

    GenerateThickness();

    ThicknessValue = OriginalThickness;

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;
    return true;
}

int32 FEditableSurfaceBuilder::CalculateVertexCountEstimate() const
{
    return Surface.CalculateVertexCountEstimate();
}

int32 FEditableSurfaceBuilder::CalculateTriangleCountEstimate() const
{
    return Surface.CalculateTriangleCountEstimate();
}

FEditableSurfaceBuilder::FPathSampleInfo FEditableSurfaceBuilder::GetPathSample(float Alpha) const
{
    FPathSampleInfo Info;
    Info.Location = FVector::ZeroVector;
    Info.Tangent = FVector::ForwardVector;
    Info.Normal = FVector::UpVector;
    Info.Binormal = FVector::RightVector;
    Info.DistanceAlongSpline = 0.0f;

    if (!SplineComponent) return Info;

    float SplineLength = SplineComponent->GetSplineLength();
    float Distance = Alpha * SplineLength;
    Info.DistanceAlongSpline = Distance;

    FTransform WorldTransform = SplineComponent->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
    FTransform LocalTransform = WorldTransform.GetRelativeTransform(SplineComponent->GetComponentTransform());

    Info.Location = LocalTransform.GetLocation();
    Info.Tangent = LocalTransform.GetUnitAxis(EAxis::X);
    Info.Binormal = LocalTransform.GetUnitAxis(EAxis::Y);
    Info.Normal = LocalTransform.GetUnitAxis(EAxis::Z);

    return Info;
}

float FEditableSurfaceBuilder::GetPathWidth(float Alpha) const
{
    float CurrentWidth = SurfaceWidth;

    // 如果没有样条线组件，直接返回原始宽度
    if (!SplineComponent) return CurrentWidth;

    // 1. 获取当前样条线长度和采样位置
    float SplineLength = SplineComponent->GetSplineLength();
    float Distance = Alpha * SplineLength;

    // 2. 计算当前位置的"曲率" (通过采样前后极小距离的切线夹角来估算)
    // 采样步长：取一个较小的值，例如 10cm 或 1% 长度，避免在极短路径出错
    float SampleStep = 10.0f;
    float PrevDist = FMath::Max(0.0f, Distance - SampleStep);
    float NextDist = FMath::Min(SplineLength, Distance + SampleStep);
    float ActualStep = NextDist - PrevDist;

    if (ActualStep > KINDA_SMALL_NUMBER)
    {
        // 获取前后点的切线方向 (Tangent)
        FVector TangentPrev = SplineComponent->GetTangentAtDistanceAlongSpline(PrevDist, ESplineCoordinateSpace::World).GetSafeNormal();
        FVector TangentNext = SplineComponent->GetTangentAtDistanceAlongSpline(NextDist, ESplineCoordinateSpace::World).GetSafeNormal();

        // 计算切线夹角 (弧度)
        float Dot = FMath::Clamp(FVector::DotProduct(TangentPrev, TangentNext), -1.0f, 1.0f);
        float AngleRad = FMath::Acos(Dot);

        // 3. 计算转弯半径 R = 弧长 / 角度
        float TurnRadius = 100000.0f; // 默认很大
        if (AngleRad > KINDA_SMALL_NUMBER)
        {
            TurnRadius = ActualStep / AngleRad;
        }

        // 4. 计算最大安全宽度
        float MaxSafeHalfWidth = TurnRadius * 0.9f;
        float MaxSafeWidth = MaxSafeHalfWidth * 2.0f;

        // 5. 自动限制宽度
        if (CurrentWidth > MaxSafeWidth)
        {
            CurrentWidth = MaxSafeWidth;
        }
    }

    return CurrentWidth;
}

TArray<int32> FEditableSurfaceBuilder::GenerateCrossSection(const FPathSampleInfo& SampleInfo, float Width, int32 SlopeSegments, float Alpha)
{
    TArray<int32> Indices;

    float HalfWidth = Width * 0.5f;
    FVector RightDir = SampleInfo.Binormal;
    FVector UpDir = SampleInfo.Normal;
    FVector Center = SampleInfo.Location;

    FVector P_MainLeft = Center - RightDir * HalfWidth;
    FVector P_MainRight = Center + RightDir * HalfWidth;

    float V = 0.0f;
    if (TextureMapping == ESurfaceTextureMapping::Stretch)
    {
        V = Alpha;
    }
    else
    {
        V = SampleInfo.DistanceAlongSpline * ModelGenConstants::GLOBAL_UV_SCALE;
    }

    bool bGenerateSlope = (SideSmoothness > 0);

    // ============ 1. 计算转弯半径和方向 (用于护坡裁剪) ============
    float SafeRadius = 100000.0f; // 默认无限大
    bool bIsRightTurn = false;    // 标记是否向右转

    if (SplineComponent)
    {
        float Dist = SampleInfo.DistanceAlongSpline;
        float Step = 20.0f; // 采样步长
        float PrevDist = FMath::Max(0.0f, Dist - Step);
        float NextDist = FMath::Min(SplineComponent->GetSplineLength(), Dist + Step);

        // 获取前后点的切线
        FVector TanPrev = SplineComponent->GetTangentAtDistanceAlongSpline(PrevDist, ESplineCoordinateSpace::World).GetSafeNormal();
        FVector TanNext = SplineComponent->GetTangentAtDistanceAlongSpline(NextDist, ESplineCoordinateSpace::World).GetSafeNormal();

        // 计算转向：使用 2D 投影判断左右
        FVector TanPrev2D = FVector(TanPrev.X, TanPrev.Y, 0.0f).GetSafeNormal();
        FVector TanNext2D = FVector(TanNext.X, TanNext.Y, 0.0f).GetSafeNormal();
        float CrossZ = (TanPrev2D.X * TanNext2D.Y) - (TanPrev2D.Y * TanNext2D.X);
        bIsRightTurn = (CrossZ < 0); // 根据坐标系定义，通常负值为右转

        // 计算半径 R = 弧长 / 角度
        float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(TanPrev2D, TanNext2D), -1.0f, 1.0f));
        if (FMath::Abs(Angle) > KINDA_SMALL_NUMBER)
        {
            SafeRadius = (Step * 2.0f) / Angle;
        }
    }

    // 2. 计算内侧剩余的安全空间
    float MaxSlopeW = FMath::Max(0.0f, SafeRadius - HalfWidth) * 0.95f;


    // 3. 生成 Left Slope
    if (bGenerateSlope && LeftSlopeLength > KINDA_SMALL_NUMBER)
    {
        float ActualSlopeW = LeftSlopeLength;

        // 【逻辑】：如果是左转（左侧是内侧），限制护坡长度
        if (!bIsRightTurn)
        {
            ActualSlopeW = FMath::Min(LeftSlopeLength, MaxSlopeW);
        }

        // 高度按比例缩小，保持坡度(Gradient)不变
        float ActualSlopeH = ActualSlopeW * LeftSlopeGradient;

        if (SlopeSegments > 1)
        {
            for (int32 i = 0; i < SlopeSegments; ++i)
            {
                float t = 1.0f - (float)i / SlopeSegments;
                float Angle = t * HALF_PI;

                float OffX = ActualSlopeW * FMath::Sin(Angle);
                float OffZ = ActualSlopeH * (1.0f - FMath::Cos(Angle));

                FVector Pos = P_MainLeft - RightDir * OffX + UpDir * OffZ;

                FVector Normal2D_Left = -RightDir * (ActualSlopeH * FMath::Sin(Angle));
                FVector Normal2D_Up = UpDir * (ActualSlopeW * FMath::Cos(Angle));
                FVector SmoothNormal = (Normal2D_Left + Normal2D_Up).GetSafeNormal();

                float LocalU = (-HalfWidth - OffX) * ModelGenConstants::GLOBAL_UV_SCALE;
                FVector2D UV(LocalU, V);

                Indices.Add(GetOrAddVertex(Pos, SmoothNormal, UV));
            }
        }
        else
        {
            FVector P_SlopeLeftEnd = P_MainLeft - RightDir * ActualSlopeW + UpDir * ActualSlopeH;
            FVector Pos = P_SlopeLeftEnd;

            FVector SlopeVec = (P_MainLeft - P_SlopeLeftEnd).GetSafeNormal();
            FVector SlopeNormal = FVector::CrossProduct(SampleInfo.Tangent, SlopeVec).GetSafeNormal();
            if ((SlopeNormal | UpDir) < 0) SlopeNormal = -SlopeNormal;

            float LocalU = (-HalfWidth - ActualSlopeW) * ModelGenConstants::GLOBAL_UV_SCALE;
            Indices.Add(GetOrAddVertex(Pos, SlopeNormal, FVector2D(LocalU, V)));
        }
    }

    // 4. 生成 Main Road
    float LocalU_Left = -HalfWidth * ModelGenConstants::GLOBAL_UV_SCALE;
    Indices.Add(GetOrAddVertex(P_MainLeft, UpDir, FVector2D(LocalU_Left, V)));

    float LocalU_Right = HalfWidth * ModelGenConstants::GLOBAL_UV_SCALE;
    Indices.Add(GetOrAddVertex(P_MainRight, UpDir, FVector2D(LocalU_Right, V)));

    // 5. 生成 Right Slope
    if (bGenerateSlope && RightSlopeLength > KINDA_SMALL_NUMBER)
    {
        float ActualSlopeW = RightSlopeLength;

        // 【逻辑】：如果是右转（右侧是内侧），限制护坡长度
        if (bIsRightTurn)
        {
            ActualSlopeW = FMath::Min(RightSlopeLength, MaxSlopeW);
        }

        // 高度按比例缩小，保持坡度(Gradient)不变
        float ActualSlopeH = ActualSlopeW * RightSlopeGradient;
        FVector P_SlopeRightEnd = P_MainRight + RightDir * ActualSlopeW + UpDir * ActualSlopeH;

        if (SlopeSegments > 1)
        {
            for (int32 i = 1; i <= SlopeSegments; ++i)
            {
                float t = (float)i / SlopeSegments;
                float Angle = t * HALF_PI;

                float OffX = ActualSlopeW * FMath::Sin(Angle);
                float OffZ = ActualSlopeH * (1.0f - FMath::Cos(Angle));

                FVector Pos = P_MainRight + RightDir * OffX + UpDir * OffZ;

                FVector Normal2D_Right = RightDir * (-ActualSlopeH * FMath::Sin(Angle));
                FVector Normal2D_Up = UpDir * (ActualSlopeW * FMath::Cos(Angle));
                FVector SmoothNormal = (Normal2D_Right + Normal2D_Up).GetSafeNormal();

                float LocalU = (HalfWidth + OffX) * ModelGenConstants::GLOBAL_UV_SCALE;
                FVector2D UV(LocalU, V);

                Indices.Add(GetOrAddVertex(Pos, SmoothNormal, UV));
            }
        }
        else
        {
            FVector Pos = P_SlopeRightEnd;
            FVector SlopeVec = (P_SlopeRightEnd - P_MainRight).GetSafeNormal();
            FVector SlopeNormal = FVector::CrossProduct(SampleInfo.Tangent, SlopeVec).GetSafeNormal();
            if ((SlopeNormal | UpDir) < 0) SlopeNormal = -SlopeNormal;

            float LocalU = (HalfWidth + ActualSlopeW) * ModelGenConstants::GLOBAL_UV_SCALE;
            Indices.Add(GetOrAddVertex(Pos, SlopeNormal, FVector2D(LocalU, V)));
        }
    }

    return Indices;
}

void FEditableSurfaceBuilder::GenerateSurfaceMesh()
{
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2)
    {
        return;
    }

    // ============ 【修改】动态采样逻辑 (修复外侧尖角问题) ============
    float SplineLength = SplineComponent->GetSplineLength();

    // 使用 SampleSpacing 来决定分段密度
    // 如果您还没有更新 Header，可以临时用 float Spacing = 50.0f; 代替
    float Spacing = FMath::Max(10.0f, Surface.SampleSpacing);

    // 动态计算需要的段数
    // 例如：路长 1000米，间距 20米 -> 需要 50 段
    int32 CalculatedSegments = FMath::CeilToInt(SplineLength / Spacing);

    // 安全限制：防止生成过多顶点导致卡死 (限制在 5000 段以内)
    const int32 Segments = FMath::Clamp(CalculatedSegments, 2, 5000);

    // 更新内部计数器 (供其他逻辑参考)
    PathSampleCount = Segments + 1;
    // =============================================================

    const int32 SlopeSegments = FMath::Max(1, SideSmoothness);

    FrontVertexStartIndex = MeshData.Vertices.Num();
    FrontCrossSections.Empty();
    FrontCrossSections.Reserve(Segments + 1);

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Alpha = (float)i / Segments;
        FPathSampleInfo Info = GetPathSample(Alpha);
        float CurrentWidth = GetPathWidth(Alpha);

        TArray<int32> SectionIndices = GenerateCrossSection(Info, CurrentWidth, SlopeSegments, Alpha);
        FrontCrossSections.Add(SectionIndices);
    }

    for (int32 i = 0; i < Segments; ++i)
    {
        const TArray<int32>& RowA = FrontCrossSections[i];
        const TArray<int32>& RowB = FrontCrossSections[i + 1];

        int32 NumVerts = RowA.Num();
        if (RowB.Num() != NumVerts) continue;

        for (int32 j = 0; j < NumVerts - 1; ++j)
        {
            AddQuad(RowA[j], RowA[j + 1], RowB[j + 1], RowB[j]);
        }
    }

    FrontVertexCount = MeshData.Vertices.Num() - FrontVertexStartIndex;
}

void FEditableSurfaceBuilder::GenerateThickness()
{
    if (FrontCrossSections.Num() < 2) return;

    TArray<TArray<int32>> BackCrossSections;
    int32 NumRows = FrontCrossSections.Num();

    // 1. 背面 (Back)
    for (int32 i = 0; i < NumRows; ++i)
    {
        TArray<int32> BackRow;
        const TArray<int32>& FrontRow = FrontCrossSections[i];

        for (int32 Idx : FrontRow)
        {
            FVector Pos = GetPosByIndex(Idx);
            FVector Normal = MeshData.Normals[Idx];
            FVector2D UV = MeshData.UVs[Idx];

            FVector BackPos = Pos - Normal * ThicknessValue;
            FVector BackNormal = -Normal;

            BackRow.Add(AddVertex(BackPos, BackNormal, UV));
        }
        BackCrossSections.Add(BackRow);
    }

    for (int32 i = 0; i < NumRows - 1; ++i)
    {
        const TArray<int32>& RowA = BackCrossSections[i];
        const TArray<int32>& RowB = BackCrossSections[i + 1];

        int32 NumVerts = RowA.Num();
        for (int32 j = 0; j < NumVerts - 1; ++j)
        {
            AddQuad(RowA[j], RowB[j], RowB[j + 1], RowA[j + 1]);
        }
    }

    // 3. 侧壁 (Side Walls)
    int32 LastIdx = FrontCrossSections[0].Num() - 1;
    float ThicknessV = ThicknessValue * ModelGenConstants::GLOBAL_UV_SCALE;

    for (int32 i = 0; i < NumRows - 1; ++i)
    {
        int32 FL_Idx = FrontCrossSections[i][0];
        int32 FL_Next_Idx = FrontCrossSections[i + 1][0];
        int32 FR_Idx = FrontCrossSections[i][LastIdx];
        int32 FR_Next_Idx = FrontCrossSections[i + 1][LastIdx];

        FVector P_FL = GetPosByIndex(FL_Idx);
        FVector P_FL_Next = GetPosByIndex(FL_Next_Idx);
        FVector P_FR = GetPosByIndex(FR_Idx);
        FVector P_FR_Next = GetPosByIndex(FR_Next_Idx);

        FVector P_BL = P_FL - MeshData.Normals[FL_Idx] * ThicknessValue;
        FVector P_BL_Next = P_FL_Next - MeshData.Normals[FL_Next_Idx] * ThicknessValue;
        FVector P_BR = P_FR - MeshData.Normals[FR_Idx] * ThicknessValue;
        FVector P_BR_Next = P_FR_Next - MeshData.Normals[FR_Next_Idx] * ThicknessValue;

        // 侧壁 UV 修复
        float U_Curr = MeshData.UVs[FL_Idx].Y;
        float U_Next = MeshData.UVs[FL_Next_Idx].Y;

        // Left Side Wall
        FVector LeftDir = (P_FL_Next - P_FL).GetSafeNormal();
        FVector LeftNormal = FVector::CrossProduct(LeftDir, -MeshData.Normals[FL_Idx]).GetSafeNormal();

        int32 V_TL = AddVertex(P_FL, LeftNormal, FVector2D(U_Curr, 0.0f));
        int32 V_TR = AddVertex(P_FL_Next, LeftNormal, FVector2D(U_Next, 0.0f));
        int32 V_BL = AddVertex(P_BL, LeftNormal, FVector2D(U_Curr, ThicknessV));
        int32 V_BR = AddVertex(P_BL_Next, LeftNormal, FVector2D(U_Next, ThicknessV));

        AddQuad(V_TL, V_TR, V_BR, V_BL);

        // Right Side Wall
        FVector RightDir = (P_FR_Next - P_FR).GetSafeNormal();
        FVector RightNormal = FVector::CrossProduct(RightDir, MeshData.Normals[FR_Idx]).GetSafeNormal();

        int32 V_R_TL = AddVertex(P_FR, RightNormal, FVector2D(U_Curr, 0.0f));
        int32 V_R_TR = AddVertex(P_FR_Next, RightNormal, FVector2D(U_Next, 0.0f));
        int32 V_R_BL = AddVertex(P_BR, RightNormal, FVector2D(U_Curr, ThicknessV));
        int32 V_R_BR = AddVertex(P_BR_Next, RightNormal, FVector2D(U_Next, ThicknessV));

        AddQuad(V_R_TL, V_R_BL, V_R_BR, V_R_TR);
    }

    // 4. 缝合端盖 (Caps)
    auto StitchCap = [&](const TArray<int32>& FRow, const TArray<int32>& BRow, bool bIsStart)
    {
        if (FRow.Num() < 2 || FrontCrossSections.Num() < 2) return;

        FVector P0 = GetPosByIndex(FRow[0]);
        int32 AdjSectionIdx = bIsStart ? 1 : FrontCrossSections.Num() - 2;
        FVector P_Adj = GetPosByIndex(FrontCrossSections[AdjSectionIdx][0]);
        FVector Normal = (P0 - P_Adj).GetSafeNormal();

        int32 MidIdx = FRow.Num() / 2;
        FVector CenterPos = GetPosByIndex(FRow[MidIdx]);
        FVector ThicknessDir = -MeshData.Normals[FRow[MidIdx]];
        FVector WidthDir = FVector::CrossProduct(Normal, ThicknessDir).GetSafeNormal();

        for (int32 j = 0; j < FRow.Num() - 1; ++j)
        {
            int32 Idx0 = FRow[j];
            int32 Idx1 = FRow[j + 1];

            FVector P_F0 = GetPosByIndex(Idx0);
            FVector P_F1 = GetPosByIndex(Idx1);
            FVector P_B0 = P_F0 - MeshData.Normals[Idx0] * ThicknessValue;
            FVector P_B1 = P_F1 - MeshData.Normals[Idx1] * ThicknessValue;

            auto GetCapUV = [&](const FVector& P) {
                FVector Rel = P - CenterPos;
                float U = FVector::DotProduct(Rel, WidthDir);
                float V = FVector::DotProduct(Rel, ThicknessDir);
                if (bIsStart) U = -U;
                return FVector2D(U * ModelGenConstants::GLOBAL_UV_SCALE, V * ModelGenConstants::GLOBAL_UV_SCALE);
            };

            int32 V_TL = AddVertex(P_F0, Normal, GetCapUV(P_F0));
            int32 V_TR = AddVertex(P_F1, Normal, GetCapUV(P_F1));
            int32 V_BL = AddVertex(P_B0, Normal, GetCapUV(P_B0));
            int32 V_BR = AddVertex(P_B1, Normal, GetCapUV(P_B1));

            if (bIsStart)
            {
                AddQuad(V_TR, V_TL, V_BL, V_BR);
            }
            else
            {
                AddQuad(V_TL, V_TR, V_BR, V_BL);
            }
        }
    };

    StitchCap(FrontCrossSections[0], BackCrossSections[0], true);
    StitchCap(FrontCrossSections.Last(), BackCrossSections.Last(), false);
}