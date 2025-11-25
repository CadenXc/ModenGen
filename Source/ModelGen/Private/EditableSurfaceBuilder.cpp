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

    // 【需求3】：如果厚度关闭，强制生成极薄的厚度
    // 我们不修改成员变量 bEnableThickness，而是修改用于生成的临时变量
    float GenerationThickness = bEnableThickness ? ThicknessValue : 0.01f;

    // 使用成员变量临时存储（为了 GenerateThickness 能访问），生成完恢复
    float OriginalThickness = ThicknessValue;
    ThicknessValue = GenerationThickness;

    GenerateSurfaceMesh();

    // 总是生成厚度几何体（因为我们现在总是有一个有效的 ThicknessValue）
    GenerateThickness();

    // 恢复
    ThicknessValue = OriginalThickness;

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;
    return true;
}

// ... (Calculate counts, GetPathSample, GetPathWidth unchanged) ...
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
    return SurfaceWidth;
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

    // 【需求1】：如果 SideSmoothness == 0，不生成护坡
    bool bGenerateSlope = (SideSmoothness > 0);

    // --- 1. 左护坡 (Left Slope) ---
    if (bGenerateSlope && LeftSlopeLength > KINDA_SMALL_NUMBER)
    {
        float SlopeH = LeftSlopeLength * LeftSlopeGradient;
        float SlopeW = LeftSlopeLength;

        // 如果 SlopeSegments > 1 使用圆弧，否则线性
        // 既然 SideSmoothness > 0 才进来，这里至少是 1

        if (SlopeSegments > 1)
        {
            // 椭圆弧逻辑 (t=1 -> t=0)
            for (int32 i = 0; i < SlopeSegments; ++i)
            {
                float t = 1.0f - (float)i / SlopeSegments;
                float Angle = t * HALF_PI;

                float OffX = SlopeW * FMath::Sin(Angle);
                float OffZ = SlopeH * (1.0f - FMath::Cos(Angle));

                FVector Pos = P_MainLeft - RightDir * OffX + UpDir * OffZ;

                // 左护坡法线：应该指向外侧（向右上方）
                // 水平分量应该是 RightDir（向右），垂直分量是 UpDir（向上）
                FVector Normal2D_Left = RightDir * (SlopeH * FMath::Sin(Angle));
                FVector Normal2D_Up = UpDir * (SlopeW * FMath::Cos(Angle));
                FVector SmoothNormal = (Normal2D_Left + Normal2D_Up).GetSafeNormal();

                float LocalU = (-HalfWidth - OffX) * ModelGenConstants::GLOBAL_UV_SCALE;
                FVector2D UV(LocalU, V);

                Indices.Add(GetOrAddVertex(Pos, SmoothNormal, UV));
            }
        }
        else
        {
            // 线性模式 (SlopeSegments == 1)
            FVector P_SlopeLeftEnd = P_MainLeft - RightDir * SlopeW + UpDir * SlopeH;
            FVector Pos = P_SlopeLeftEnd;

            FVector SlopeVec = (P_MainLeft - P_SlopeLeftEnd).GetSafeNormal();
            FVector SlopeNormal = FVector::CrossProduct(SampleInfo.Tangent, SlopeVec).GetSafeNormal();

            // 确保法线向上 (UpDir 方向)
            if ((SlopeNormal | UpDir) < 0) SlopeNormal = -SlopeNormal;

            float LocalU = (-HalfWidth - LeftSlopeLength) * ModelGenConstants::GLOBAL_UV_SCALE;
            Indices.Add(GetOrAddVertex(Pos, SlopeNormal, FVector2D(LocalU, V)));
        }
    }

    // 2. 主路面左边缘
    float LocalU_Left = -HalfWidth * ModelGenConstants::GLOBAL_UV_SCALE;
    Indices.Add(GetOrAddVertex(P_MainLeft, UpDir, FVector2D(LocalU_Left, V)));

    // 3. 主路面右边缘
    float LocalU_Right = HalfWidth * ModelGenConstants::GLOBAL_UV_SCALE;
    Indices.Add(GetOrAddVertex(P_MainRight, UpDir, FVector2D(LocalU_Right, V)));

    // --- 4. 右护坡 (Right Slope) ---
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
            // 线性模式
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

    // 【需求1】：如果 SideSmoothness 为 0，SlopeSegments 设为 0 (不生成)，否则为平滑度
    // 但为了 GenerateCrossSection 的逻辑 (它依赖 bGenerateSlope)，我们传 SideSmoothness 即可
    // 在 GenerateCrossSection 内部判断 > 0

    FrontVertexStartIndex = MeshData.Vertices.Num();
    FrontCrossSections.Empty();
    FrontCrossSections.Reserve(Segments + 1);

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Alpha = (float)i / Segments;
        FPathSampleInfo Info = GetPathSample(Alpha);
        float CurrentWidth = GetPathWidth(Alpha);

        TArray<int32> SectionIndices = GenerateCrossSection(Info, CurrentWidth, SideSmoothness, Alpha);
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

    // 1. 生成背面顶点 (法线反向)
    for (int32 i = 0; i < NumRows; ++i)
    {
        TArray<int32> BackRow;
        const TArray<int32>& FrontRow = FrontCrossSections[i];

        for (int32 Idx : FrontRow)
        {
            FVector Pos = GetPosByIndex(Idx);
            FVector Normal = MeshData.Normals[Idx];
            FVector2D UV = MeshData.UVs[Idx];

            // 【需求2】：左护坡厚度方向反了？
            // 我们使用 Pos - Normal * Thickness。
            // 如果 Normal 指向上/外，那么 BackPos 指向下/内。这是正确的挤出。
            // 如果左护坡看起来反了，可能是 Normal 计算错误。
            // 在 GenerateCrossSection 中，我们确保了 SlopeNormal 和 UpDir 点积 > 0 (朝上)。
            // 所以挤出应该是朝下的。

            FVector BackPos = Pos - Normal * ThicknessValue;
            FVector BackNormal = -Normal;

            BackRow.Add(AddVertex(BackPos, BackNormal, UV));
        }
        BackCrossSections.Add(BackRow);
    }

    // 2. 缝合背面
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

    // 3. 缝合侧边
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

        float U_Curr = MeshData.UVs[FL_Idx].Y;
        float U_Next = MeshData.UVs[FL_Next_Idx].Y;

        // Left Side
        FVector LeftDir = (P_FL_Next - P_FL).GetSafeNormal();
        FVector LeftNormal = FVector::CrossProduct(LeftDir, -MeshData.Normals[FL_Idx]).GetSafeNormal();

        int32 V_TL = AddVertex(P_FL, LeftNormal, FVector2D(U_Curr, 0.0f));
        int32 V_TR = AddVertex(P_FL_Next, LeftNormal, FVector2D(U_Next, 0.0f));
        int32 V_BL = AddVertex(P_BL, LeftNormal, FVector2D(U_Curr, ThicknessV));
        int32 V_BR = AddVertex(P_BL_Next, LeftNormal, FVector2D(U_Next, ThicknessV));

        AddQuad(V_TL, V_TR, V_BR, V_BL);

        // Right Side
        FVector RightDir = (P_FR_Next - P_FR).GetSafeNormal();
        FVector RightNormal = FVector::CrossProduct(RightDir, MeshData.Normals[FR_Idx]).GetSafeNormal();

        int32 V_R_TL = AddVertex(P_FR, RightNormal, FVector2D(U_Curr, 0.0f));
        int32 V_R_TR = AddVertex(P_FR_Next, RightNormal, FVector2D(U_Next, 0.0f));
        int32 V_R_BL = AddVertex(P_BR, RightNormal, FVector2D(U_Curr, ThicknessV));
        int32 V_R_BR = AddVertex(P_BR_Next, RightNormal, FVector2D(U_Next, ThicknessV));

        AddQuad(V_R_TL, V_R_BL, V_R_BR, V_R_TR);
    }

    // 4. 缝合端盖
    auto StitchCap = [&](const TArray<int32>& FRow, const TArray<int32>& BRow, bool bIsStart)
    {
        // 确保有足够的点计算切线
        if (FRow.Num() < 2 || FrontCrossSections.Num() < 2) return;

        // 计算法线：Forward (End) 或 Backward (Start)
        FVector P0 = GetPosByIndex(FRow[0]);
        // 使用相邻截面计算切线
        int32 AdjSectionIdx = bIsStart ? 1 : FrontCrossSections.Num() - 2;
        FVector P_Adj = GetPosByIndex(FrontCrossSections[AdjSectionIdx][0]);
        FVector Normal = (bIsStart) ? (P0 - P_Adj).GetSafeNormal() : (P0 - P_Adj).GetSafeNormal();
        // Wait, if bIsStart, P0 is at 0, P_Adj is at 1. P0 - P_Adj points Back. Correct.
        // If End, P0 is at Last, P_Adj is Last-1. P0 - P_Adj points Forward. Correct.

        for (int32 j = 0; j < FRow.Num() - 1; ++j)
        {
            int32 Idx0 = FRow[j];
            int32 Idx1 = FRow[j + 1];

            FVector P_F0 = GetPosByIndex(Idx0);
            FVector P_F1 = GetPosByIndex(Idx1);
            FVector P_B0 = P_F0 - MeshData.Normals[Idx0] * ThicknessValue;
            FVector P_B1 = P_F1 - MeshData.Normals[Idx1] * ThicknessValue;

            float U0 = MeshData.UVs[Idx0].X;
            float U1 = MeshData.UVs[Idx1].X;

            int32 V_TL = AddVertex(P_F0, Normal, FVector2D(U0, 0.0f));
            int32 V_TR = AddVertex(P_F1, Normal, FVector2D(U1, 0.0f));
            int32 V_BL = AddVertex(P_B0, Normal, FVector2D(U0, ThicknessV));
            int32 V_BR = AddVertex(P_B1, Normal, FVector2D(U1, ThicknessV));

            if (bIsStart)
            {
                AddQuad(V_TL, V_BL, V_BR, V_TR);
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