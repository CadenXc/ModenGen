// Copyright (c) 2024. All rights reserved.

#include "EditableSurfaceBuilder.h"

#include "ModelGenConstants.h"

#include "Components/SplineComponent.h"

FEditableSurfaceBuilder::FEditableSurfaceBuilder(const AEditableSurface& InSurface)
    : Surface(InSurface)
{
    // 缓存配置数据
    SplineComponent = InSurface.SplineComponent;
    PathSampleCount = InSurface.PathSampleCount;
    SurfaceWidth = InSurface.SurfaceWidth;
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
    FModelGenMeshBuilder::Clear();
    FrontCrossSections.Empty();
    FrontVertexStartIndex = 0;
    FrontVertexCount = 0;
}

int32 FEditableSurfaceBuilder::CalculateVertexCountEstimate() const
{
    int32 CrossSectionPoints = 2 + (SideSmoothness * 2);
    int32 NumSamples = FMath::Max(PathSampleCount, 20) * 2;
    int32 BaseCount = NumSamples * CrossSectionPoints;
    return bEnableThickness ? BaseCount * 2 + (NumSamples * 2) : BaseCount;
}

int32 FEditableSurfaceBuilder::CalculateTriangleCountEstimate() const
{
    return CalculateVertexCountEstimate() * 3;
}

bool FEditableSurfaceBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2)
    {
        return false;
    }

    Clear();

    // =========================================================
    // [逻辑修改] 处理厚度开关
    // 关闭时将厚度设为 0.01，保证模型有底面且闭合，避免单面网格的光照问题
    // =========================================================
    if (!bEnableThickness)
    {
        ThicknessValue = 0.01f;
    }

    // 1. 生成上表面
    GenerateSurfaceMesh();

    // 2. 生成厚度 (现在总是执行，保证闭合)
    GenerateThickness();

    // 3. 计算切线
    MeshData.CalculateTangents();

    OutMeshData = MoveTemp(MeshData);

    return OutMeshData.IsValid();
}

void FEditableSurfaceBuilder::GetAdaptiveSamplePoints(TArray<float>& OutAlphas, float AngleThresholdDeg) const
{
    // (此处逻辑保持简化，如果需要更复杂的自适应采样可在此扩展)
    // 目前使用 GenerateSurfaceMesh 中的固定采样
}

void FEditableSurfaceBuilder::GenerateSurfaceMesh()
{
    TArray<float> SampleAlphas;
    // 使用 PathSampleCount 进行等距采样
    SampleAlphas.Empty();
    for (int32 i = 0; i <= PathSampleCount; ++i)
    {
        SampleAlphas.Add(static_cast<float>(i) / static_cast<float>(PathSampleCount));
    }

    TArray<FPathSampleInfo> Samples;
    Samples.Reserve(SampleAlphas.Num());

    // 1. 预计算所有采样点信息
    for (float Alpha : SampleAlphas)
    {
        Samples.Add(GetPathSample(Alpha));
    }

    FrontVertexStartIndex = MeshData.Vertices.Num();
    FrontCrossSections.Reserve(Samples.Num());

    // 2. 生成每个截面的几何体
    for (int32 i = 0; i < Samples.Num(); ++i)
    {
        const FPathSampleInfo& CurrSample = Samples[i];

        // 计算斜切方向
        FVector MiterDir;
        float MiterScale = 1.0f;

        if (i == 0 || i == Samples.Num() - 1)
        {
            MiterDir = CurrSample.Binormal;
        }
        else
        {
            MiterDir = CalculateMiterDirection(Samples[i - 1], CurrSample, Samples[i + 1], false);
            float Dot = FVector::DotProduct(MiterDir, CurrSample.Binormal);
            if (Dot < 0.2f) Dot = 0.2f;
            MiterScale = 1.0f / Dot;
        }

        TArray<int32> RowIndices = GenerateCrossSection(
            CurrSample,
            SurfaceWidth * 0.5f,
            SideSmoothness,
            SampleAlphas[i],
            MiterDir,
            MiterScale
        );

        FrontCrossSections.Add(RowIndices);

        // 连接三角形
        if (i > 0)
        {
            const TArray<int32>& PrevRow = FrontCrossSections[i - 1];
            const TArray<int32>& CurrRow = RowIndices;

            if (PrevRow.Num() == CurrRow.Num())
            {
                for (int32 j = 0; j < CurrRow.Num() - 1; ++j)
                {
                    AddQuad(PrevRow[j], CurrRow[j], CurrRow[j + 1], PrevRow[j + 1]);
                }
            }
        }
    }
    FrontVertexCount = MeshData.Vertices.Num() - FrontVertexStartIndex;
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

    Info.Location = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
    Info.Tangent = SplineComponent->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local).GetSafeNormal();

    // 稳定坐标系计算
    FVector ReferenceUp = FVector::UpVector;
    bool bIsVertical = FMath::Abs(FVector::DotProduct(Info.Tangent, ReferenceUp)) > 0.99f;

    if (bIsVertical)
    {
        Info.Normal = SplineComponent->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local).GetSafeNormal();
        Info.Binormal = FVector::CrossProduct(Info.Tangent, Info.Normal).GetSafeNormal();
    }
    else
    {
        Info.Binormal = FVector::CrossProduct(Info.Tangent, ReferenceUp).GetSafeNormal();
        Info.Normal = FVector::CrossProduct(Info.Binormal, Info.Tangent).GetSafeNormal();
    }

    return Info;
}

FVector FEditableSurfaceBuilder::CalculateMiterDirection(const FPathSampleInfo& Prev, const FPathSampleInfo& Curr, const FPathSampleInfo& Next, bool bIsLeft) const
{
    FVector Dir1 = (Curr.Location - Prev.Location).GetSafeNormal();
    FVector Dir2 = (Next.Location - Curr.Location).GetSafeNormal();

    if (FVector::DotProduct(Dir1, Dir2) > 0.999f) return Curr.Binormal;

    FVector CornerTangent = (Dir1 + Dir2).GetSafeNormal();
    FVector MiterDir = FVector::CrossProduct(CornerTangent, Curr.Normal).GetSafeNormal();

    if (FVector::DotProduct(MiterDir, Curr.Binormal) < 0.0f) MiterDir = -MiterDir;

    return MiterDir;
}

TArray<int32> FEditableSurfaceBuilder::GenerateCrossSection(
    const FPathSampleInfo& SampleInfo,
    float RoadHalfWidth,
    int32 SlopeSegments,
    float Alpha,
    const FVector& MiterDir,
    float MiterScale)
{
    TArray<int32> RowIndices;
    FVector RightVec = MiterDir * MiterScale;

    struct FPointDef {
        float OffsetX;
        float OffsetZ;
        float U;
    };

    TArray<FPointDef> Points;

    // 计算路面宽度的 UV 比例因子
    // 路面 UV 从 0 到 1，覆盖宽度 2 * RoadHalfWidth
    // 因此：物理距离 1 单位 = UV 1.0 / (2 * Width)
    float RoadTotalWidth = RoadHalfWidth * 2.0f;
    float UVFactor = (RoadTotalWidth > KINDA_SMALL_NUMBER) ? (1.0f / RoadTotalWidth) : 0.0f;

    // lambda: 计算倒角上的相对偏移 (X, Z)
    auto CalculateBevelOffset = [&](float Ratio, float Len, float Grad, float& OutX, float& OutZ)
    {
        float Angle = Ratio * HALF_PI;
        float ArcX = FMath::Sin(Angle);
        float ArcZ = 1.0f - FMath::Cos(Angle);
        OutX = Len * ArcX;
        OutZ = (Len * Grad) * ArcZ;
    };

    // =================================================================================
    // 1. 左侧护坡 (Left Slope) - 从路边向外计算几何距离，然后反向添加
    // =================================================================================
    if (SlopeSegments > 0)
    {
        TArray<FPointDef> LeftPoints;
        LeftPoints.Reserve(SlopeSegments);

        // 从路边缘 (Ratio=0) 开始向外延伸到 (Ratio=1)
        float PrevX = 0.0f;
        float PrevZ = 0.0f;
        float CurrentU = 0.0f; // 左边缘起始 U = 0

        for (int32 i = 1; i <= SlopeSegments; ++i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SlopeSegments);

            float RelX, RelZ;
            CalculateBevelOffset(Ratio, LeftSlopeLength, LeftSlopeGradient, RelX, RelZ);

            // 计算该段的实际物理长度 (弧长近似值)
            float SegDist = FMath::Sqrt(FMath::Square(RelX - PrevX) + FMath::Square(RelZ - PrevZ));

            // 向左延伸，U 减小
            CurrentU -= SegDist * UVFactor;

            // 转换为绝对坐标偏移
            float FinalDist = RoadHalfWidth + RelX;

            // 添加到临时数组
            LeftPoints.Add({ -FinalDist, RelZ, CurrentU });

            PrevX = RelX;
            PrevZ = RelZ;
        }

        // 反向添加到主数组 (因为主数组顺序是 FarLeft -> NearLeft)
        for (int32 i = LeftPoints.Num() - 1; i >= 0; --i)
        {
            Points.Add(LeftPoints[i]);
        }
    }

    // --- 路面左边缘 (U=0) ---
    Points.Add({ -RoadHalfWidth, 0.0f, 0.0f });

    // --- 路面右边缘 (U=1) ---
    Points.Add({ RoadHalfWidth, 0.0f, 1.0f });

    // =================================================================================
    // 2. 右侧护坡 (Right Slope) - 从路边向外计算
    // =================================================================================
    if (SlopeSegments > 0)
    {
        float PrevX = 0.0f;
        float PrevZ = 0.0f;
        float CurrentU = 1.0f; // 右边缘起始 U = 1

        for (int32 i = 1; i <= SlopeSegments; ++i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SlopeSegments);

            float RelX, RelZ;
            CalculateBevelOffset(Ratio, RightSlopeLength, RightSlopeGradient, RelX, RelZ);

            float SegDist = FMath::Sqrt(FMath::Square(RelX - PrevX) + FMath::Square(RelZ - PrevZ));

            // 向右延伸，U 增加
            CurrentU += SegDist * UVFactor;

            float FinalDist = RoadHalfWidth + RelX;

            Points.Add({ FinalDist, RelZ, CurrentU });

            PrevX = RelX;
            PrevZ = RelZ;
        }
    }

    // =================================================================================
    // 3. 生成顶点
    // =================================================================================
    float CoordV = 0.0f;
    if (TextureMapping == ESurfaceTextureMapping::Stretch)
    {
        CoordV = Alpha;
    }
    else
    {
        CoordV = SampleInfo.DistanceAlongSpline * ModelGenConstants::GLOBAL_UV_SCALE;
    }

    for (const FPointDef& Pt : Points)
    {
        FVector Position = SampleInfo.Location + (RightVec * Pt.OffsetX) + (SampleInfo.Normal * Pt.OffsetZ);

        // 法线暂时使用 Up，依赖后续 CalculateTangents 平滑
        FVector Normal = SampleInfo.Normal;

        FVector2D UV(Pt.U, CoordV);

        int32 Idx = AddVertex(Position, Normal, UV);
        RowIndices.Add(Idx);
    }

    return RowIndices;
}
void FEditableSurfaceBuilder::GenerateThickness()
{
    if (FrontCrossSections.Num() < 2) return;

    TArray<TArray<int32>> BackCrossSections;
    int32 NumRows = FrontCrossSections.Num();
    
    // 侧壁垂直纹理长度 (基于物理厚度)
    // 无论路面是否 Stretch，厚度方向通常保持物理比例比较美观
    float ThicknessV = ThicknessValue * ModelGenConstants::GLOBAL_UV_SCALE;

    // =================================================================================
    // 1. 生成底面 (Bottom Surface)
    // =================================================================================
    for (int32 i = 0; i < NumRows; ++i)
    {
        TArray<int32> BackRow;
        const TArray<int32>& FrontRow = FrontCrossSections[i];

        for (int32 Idx : FrontRow)
        {
            FVector Pos = GetPosByIndex(Idx);
            FVector Normal = MeshData.Normals[Idx];
            FVector2D TopUV = MeshData.UVs[Idx];

            // [UV 优化] 底面处理
            // U: 翻转，防止纹理镜像
            // V: 跟随顶面 (若是Stretch则Stretch，若是Default则物理)
            FVector2D BottomUV(1.0f - TopUV.X, TopUV.Y);

            FVector BackPos = Pos - Normal * ThicknessValue;
            FVector BackNormal = -Normal;

            BackRow.Add(AddVertex(BackPos, BackNormal, BottomUV));
        }
        BackCrossSections.Add(BackRow);
    }

    // 缝合底面
    for (int32 i = 0; i < NumRows - 1; ++i)
    {
        const TArray<int32>& RowA = BackCrossSections[i];
        const TArray<int32>& RowB = BackCrossSections[i + 1];

        int32 NumVerts = RowA.Num();
        for (int32 j = 0; j < NumVerts - 1; ++j)
        {
            AddQuad(RowA[j], RowA[j + 1], RowB[j + 1], RowB[j]);
        }
    }

    // =================================================================================
    // 2. 生成侧壁 (Side Walls)
    // =================================================================================
    int32 LastIdx = FrontCrossSections[0].Num() - 1;

    for (int32 i = 0; i < NumRows - 1; ++i)
    {
        // 获取四个角的索引 (FL: FrontLeft, FR: FrontRight)
        int32 FL_Idx = FrontCrossSections[i][0];
        int32 FL_Next_Idx = FrontCrossSections[i + 1][0];
        int32 FR_Idx = FrontCrossSections[i][LastIdx];
        int32 FR_Next_Idx = FrontCrossSections[i + 1][LastIdx];

        // 获取位置
        FVector P_FL = GetPosByIndex(FL_Idx);
        FVector P_FL_Next = GetPosByIndex(FL_Next_Idx);
        FVector P_FR = GetPosByIndex(FR_Idx);
        FVector P_FR_Next = GetPosByIndex(FR_Next_Idx);

        // 计算底部位置
        FVector P_BL = P_FL - MeshData.Normals[FL_Idx] * ThicknessValue;
        FVector P_BL_Next = P_FL_Next - MeshData.Normals[FL_Next_Idx] * ThicknessValue;
        FVector P_BR = P_FR - MeshData.Normals[FR_Idx] * ThicknessValue;
        FVector P_BR_Next = P_FR_Next - MeshData.Normals[FR_Next_Idx] * ThicknessValue;

        // [UV 优化] 侧壁处理
        // U (沿路方向): 继承顶面 V 坐标。保证侧壁和路面在长度方向对齐。
        // V (垂直方向): 使用物理厚度 ThicknessV。
        float WallU_Curr = MeshData.UVs[FL_Idx].Y;
        float WallU_Next = MeshData.UVs[FL_Next_Idx].Y;

        // --- Left Side Wall ---
        FVector LeftDir = (P_FL_Next - P_FL).GetSafeNormal();
        FVector LeftNormal = FVector::CrossProduct(-MeshData.Normals[FL_Idx], LeftDir).GetSafeNormal();

        int32 V_TL = AddVertex(P_FL, LeftNormal, FVector2D(WallU_Curr, 0.0f));
        int32 V_TR = AddVertex(P_FL_Next, LeftNormal, FVector2D(WallU_Next, 0.0f));
        int32 V_BL = AddVertex(P_BL, LeftNormal, FVector2D(WallU_Curr, ThicknessV));
        int32 V_BR = AddVertex(P_BL_Next, LeftNormal, FVector2D(WallU_Next, ThicknessV));

        AddQuad(V_TL, V_BL, V_BR, V_TR);

        // --- Right Side Wall ---
        FVector RightDir = (P_FR_Next - P_FR).GetSafeNormal();
        FVector RightNormal = FVector::CrossProduct(RightDir, MeshData.Normals[FR_Idx]).GetSafeNormal();

        // 注意：右侧壁使用相同的 U (路长) 和 V (高度)
        int32 V_R_TL = AddVertex(P_FR, RightNormal, FVector2D(WallU_Curr, 0.0f));
        int32 V_R_TR = AddVertex(P_FR_Next, RightNormal, FVector2D(WallU_Next, 0.0f));
        int32 V_R_BL = AddVertex(P_BR, RightNormal, FVector2D(WallU_Curr, ThicknessV));
        int32 V_R_BR = AddVertex(P_BR_Next, RightNormal, FVector2D(WallU_Next, ThicknessV));

        AddQuad(V_R_TL, V_R_TR, V_R_BR, V_R_BL);
    }

    // =================================================================================
    // 3. 缝合端盖 (Caps) - "瀑布流" UV 映射
    // =================================================================================
    auto StitchCap = [&](const TArray<int32>& FRow, bool bIsStart)
    {
        if (FRow.Num() < 2 || FrontCrossSections.Num() < 2) return;

        // 计算端面法线
        int32 AdjIdx = bIsStart ? 1 : FrontCrossSections.Num() - 2;
        FVector P0 = GetPosByIndex(FRow[0]);
        FVector P_Adj = GetPosByIndex(FrontCrossSections[AdjIdx][0]);
        FVector FaceNormal = (P0 - P_Adj).GetSafeNormal();

        for (int32 j = 0; j < FRow.Num() - 1; ++j)
        {
            int32 Idx0 = FRow[j];
            int32 Idx1 = FRow[j + 1];

            FVector P_F0 = GetPosByIndex(Idx0);
            FVector P_F1 = GetPosByIndex(Idx1);
            
            FVector P_B0 = P_F0 - MeshData.Normals[Idx0] * ThicknessValue;
            FVector P_B1 = P_F1 - MeshData.Normals[Idx1] * ThicknessValue;

            // [UV 优化] 端盖处理
            // U: 继承顶面顶点的 U 坐标 (0~1)。这样纹理会横向对齐路面。
            // V: 垂直方向使用物理厚度。
            // 效果：路面纹理流过边缘，自然下垂。
            float U0 = MeshData.UVs[Idx0].X;
            float U1 = MeshData.UVs[Idx1].X;

            int32 V_TL = AddVertex(P_F0, FaceNormal, FVector2D(U0, 0.0f));
            int32 V_TR = AddVertex(P_F1, FaceNormal, FVector2D(U1, 0.0f));
            int32 V_BL = AddVertex(P_B0, FaceNormal, FVector2D(U0, ThicknessV));
            int32 V_BR = AddVertex(P_B1, FaceNormal, FVector2D(U1, ThicknessV));

            if (bIsStart)
            {
                // 面向后: TL -> TR -> BR -> BL
                AddQuad(V_TL, V_TR, V_BR, V_BL);
            }
            else
            {
                // 面向前: TR -> TL -> BL -> BR
                AddQuad(V_TR, V_TL, V_BL, V_BR);
            }
        }
    };

    StitchCap(FrontCrossSections[0], true);
    StitchCap(FrontCrossSections.Last(), false);
}
