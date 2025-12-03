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
        // DotProduct 可能会因为浮点误差略微大于1，Clamp一下
        float Dot = FMath::Clamp(FVector::DotProduct(TangentPrev, TangentNext), -1.0f, 1.0f);
        float AngleRad = FMath::Acos(Dot);

        // 3. 计算转弯半径 R = 弧长 / 角度
        // 如果角度极小(直路)，半径会无穷大，我们设一个很大的安全值
        float TurnRadius = 100000.0f; // 默认很大
        if (AngleRad > KINDA_SMALL_NUMBER)
        {
            TurnRadius = ActualStep / AngleRad;
        }

        // 4. 计算最大安全宽度
        // 理论上 半宽 (HalfWidth) 不能超过 TurnRadius
        // 我们给一个安全系数 0.9，留一点余量防止顶点刚好重合
        float MaxSafeHalfWidth = TurnRadius * 0.9f;
        float MaxSafeWidth = MaxSafeHalfWidth * 2.0f;

        // 5. 自动限制宽度
        // 取 "设定宽度" 和 "最大安全宽度" 之间的较小值
        if (CurrentWidth > MaxSafeWidth)
        {
            // 这里可以加一个插值让变化更平滑，但直接限制通常效果也不错
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

    // 1. Left Slope
    if (bGenerateSlope && LeftSlopeLength > KINDA_SMALL_NUMBER)
    {
        float SlopeH = LeftSlopeLength * LeftSlopeGradient;
        float SlopeW = LeftSlopeLength;

        if (SlopeSegments > 1)
        {
            for (int32 i = 0; i < SlopeSegments; ++i)
            {
                float t = 1.0f - (float)i / SlopeSegments;
                float Angle = t * HALF_PI;

                float OffX = SlopeW * FMath::Sin(Angle);
                float OffZ = SlopeH * (1.0f - FMath::Cos(Angle));

                FVector Pos = P_MainLeft - RightDir * OffX + UpDir * OffZ;

                FVector Normal2D_Left = -RightDir * (SlopeH * FMath::Sin(Angle));
                FVector Normal2D_Up = UpDir * (SlopeW * FMath::Cos(Angle));
                FVector SmoothNormal = (Normal2D_Left + Normal2D_Up).GetSafeNormal();

                float LocalU = (-HalfWidth - OffX) * ModelGenConstants::GLOBAL_UV_SCALE;
                FVector2D UV(LocalU, V);

                Indices.Add(GetOrAddVertex(Pos, SmoothNormal, UV));
            }
        }
        else
        {
            FVector P_SlopeLeftEnd = P_MainLeft - RightDir * SlopeW + UpDir * SlopeH;
            FVector Pos = P_SlopeLeftEnd;

            FVector SlopeVec = (P_MainLeft - P_SlopeLeftEnd).GetSafeNormal();
            FVector SlopeNormal = FVector::CrossProduct(SampleInfo.Tangent, SlopeVec).GetSafeNormal();
            if ((SlopeNormal | UpDir) < 0) SlopeNormal = -SlopeNormal;

            float LocalU = (-HalfWidth - LeftSlopeLength) * ModelGenConstants::GLOBAL_UV_SCALE;
            Indices.Add(GetOrAddVertex(Pos, SlopeNormal, FVector2D(LocalU, V)));
        }
    }

    // 2. Main Road
    float LocalU_Left = -HalfWidth * ModelGenConstants::GLOBAL_UV_SCALE;
    Indices.Add(GetOrAddVertex(P_MainLeft, UpDir, FVector2D(LocalU_Left, V)));

    float LocalU_Right = HalfWidth * ModelGenConstants::GLOBAL_UV_SCALE;
    Indices.Add(GetOrAddVertex(P_MainRight, UpDir, FVector2D(LocalU_Right, V)));

    // 3. Right Slope
    if (bGenerateSlope && RightSlopeLength > KINDA_SMALL_NUMBER)
    {
        float SlopeH = RightSlopeLength * RightSlopeGradient;
        float SlopeW = RightSlopeLength;
        FVector P_SlopeRightEnd = P_MainRight + RightDir * SlopeW + UpDir * SlopeH;

        if (SlopeSegments > 1)
        {
            for (int32 i = 1; i <= SlopeSegments; ++i)
            {
                float t = (float)i / SlopeSegments;
                float Angle = t * HALF_PI;

                float OffX = SlopeW * FMath::Sin(Angle);
                float OffZ = SlopeH * (1.0f - FMath::Cos(Angle));

                FVector Pos = P_MainRight + RightDir * OffX + UpDir * OffZ;

                FVector Normal2D_Right = RightDir * (-SlopeH * FMath::Sin(Angle));
                FVector Normal2D_Up = UpDir * (SlopeW * FMath::Cos(Angle));
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

            float LocalU = (HalfWidth + RightSlopeLength) * ModelGenConstants::GLOBAL_UV_SCALE;
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

    const int32 Segments = PathSampleCount - 1;
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
            FVector2D UV = MeshData.UVs[Idx]; // UV can be shared or mirrored

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
    // 侧壁使用垂直高度作为 V 轴
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

        // 【UV修复】：侧壁的 U 应该是沿路径的距离，V 是厚度
        // 我们复用顶面该点的 U 作为侧壁的 U
        // MeshData.UVs[Idx].Y 在 GenerateCrossSection 中被设为 V (沿路径距离)
        // 所以这里用 Y 作为 U
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

    // 4. 缝合端盖 (Caps) - 【修复渲染方向和UV】
    auto StitchCap = [&](const TArray<int32>& FRow, const TArray<int32>& BRow, bool bIsStart)
    {
        if (FRow.Num() < 2 || FrontCrossSections.Num() < 2) return;

        // 计算法线
        FVector P0 = GetPosByIndex(FRow[0]);
        int32 AdjSectionIdx = bIsStart ? 1 : FrontCrossSections.Num() - 2;
        FVector P_Adj = GetPosByIndex(FrontCrossSections[AdjSectionIdx][0]);
        FVector Normal = (P0 - P_Adj).GetSafeNormal();

        // 确定中心点，用于计算相对 UV (Planar Mapping)
        // 取截面中间点
        int32 MidIdx = FRow.Num() / 2;
        FVector CenterPos = GetPosByIndex(FRow[MidIdx]);
        // 计算截面的 Up 向量 (Thickness方向) 和 Right 向量 (Width方向)
        FVector ThicknessDir = -MeshData.Normals[FRow[MidIdx]]; // Down
        FVector WidthDir = FVector::CrossProduct(Normal, ThicknessDir).GetSafeNormal(); // Right

        for (int32 j = 0; j < FRow.Num() - 1; ++j)
        {
            int32 Idx0 = FRow[j];
            int32 Idx1 = FRow[j + 1];

            FVector P_F0 = GetPosByIndex(Idx0);
            FVector P_F1 = GetPosByIndex(Idx1);
            FVector P_B0 = P_F0 - MeshData.Normals[Idx0] * ThicknessValue;
            FVector P_B1 = P_F1 - MeshData.Normals[Idx1] * ThicknessValue;

            // 【UV修复】：使用平面投影 UV
            // U: 沿宽度的距离
            // V: 沿厚度的距离
            auto GetCapUV = [&](const FVector& P) {
                FVector Rel = P - CenterPos;
                float U = FVector::DotProduct(Rel, WidthDir);
                float V = FVector::DotProduct(Rel, ThicknessDir);
                // 如果是 StartFace，反转 U 以防止镜像
                if (bIsStart) U = -U;
                return FVector2D(U * ModelGenConstants::GLOBAL_UV_SCALE, V * ModelGenConstants::GLOBAL_UV_SCALE);
            };

            int32 V_TL = AddVertex(P_F0, Normal, GetCapUV(P_F0));
            int32 V_TR = AddVertex(P_F1, Normal, GetCapUV(P_F1));
            int32 V_BL = AddVertex(P_B0, Normal, GetCapUV(P_B0));
            int32 V_BR = AddVertex(P_B1, Normal, GetCapUV(P_B1));

            // 【方向修复】：交换顺序
            if (bIsStart)
            {
                // Start: Face Back. 
                // Quad: TopRight -> TopLeft -> BotLeft -> BotRight (CW looking from front = CCW looking from back)
                // V_TR -> V_TL -> V_BL -> V_BR
                AddQuad(V_TR, V_TL, V_BL, V_BR);
            }
            else
            {
                // End: Face Forward.
                // Quad: TopLeft -> TopRight -> BotRight -> BotLeft
                // V_TL -> V_TR -> V_BR -> V_BL
                AddQuad(V_TL, V_TR, V_BR, V_BL);
            }
        }
    };

    StitchCap(FrontCrossSections[0], BackCrossSections[0], true);
    StitchCap(FrontCrossSections.Last(), BackCrossSections.Last(), false);
}