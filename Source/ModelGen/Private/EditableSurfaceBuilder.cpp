// Copyright (c) 2024. All rights reserved.

#include "EditableSurfaceBuilder.h"
#include "ModelGenConstants.h"
#include "Components/SplineComponent.h"

FEditableSurfaceBuilder::FEditableSurfaceBuilder(const AEditableSurface& InSurface)
    : Surface(InSurface)
{
    // 缓存配置数据，减少对 Actor 的访问
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
    // (中间路面2个点 + 左右坡度点数) * 采样段数
    // 如果有厚度，大致翻倍
    int32 CrossSectionPoints = 2 + (SideSmoothness * 2);
    // 动态采样可能比 PathSampleCount 多，这里给个安全倍数
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

    // 生成上表面
    GenerateSurfaceMesh();

    // 生成厚度（如果启用）
    if (bEnableThickness)
    {
        GenerateThickness();
    }

    // 计算切线（使用 MeshData 中基于 UV 的计算）
    MeshData.CalculateTangents();

    // 移动数据到输出
    OutMeshData = MoveTemp(MeshData);

    return OutMeshData.IsValid();
}

void FEditableSurfaceBuilder::GetAdaptiveSamplePoints(TArray<float>& OutAlphas, float AngleThresholdDeg) const
{
    if (!SplineComponent) return;

    const float Length = SplineComponent->GetSplineLength();
    // 基础采样步长，确保不会漏掉细节
    const float StepSize = Length / static_cast<float>(FMath::Max(PathSampleCount, 2));

    float CurrentDist = 0.0f;
    OutAlphas.Add(0.0f);

    FVector LastTangent = SplineComponent->GetTangentAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::Local).GetSafeNormal();

    // 总是包含样条点（Key Points）
    int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
    TArray<float> SplinePointDists;
    for (int32 i = 0; i < NumPoints; ++i)
    {
        SplinePointDists.Add(SplineComponent->GetDistanceAlongSplineAtSplinePoint(i));
    }

    while (CurrentDist < Length)
    {
        float NextDist = FMath::Min(CurrentDist + StepSize, Length);

        // 检查这一段中间是否有曲率剧烈变化
        // 简化版：这里我们使用固定步长+关键点，也可以加入二分法检测曲率
        // 为了响应 Surface 的 SampleSpacing 属性，我们可以调整 StepSize

        // 如果跨越了样条点，强制在样条点采样以保证形状准确
        for (float KeyDist : SplinePointDists)
        {
            if (KeyDist > CurrentDist && KeyDist < NextDist - KINDA_SMALL_NUMBER)
            {
                OutAlphas.Add(KeyDist / Length);
            }
        }

        if (NextDist < Length)
        {
            OutAlphas.Add(NextDist / Length);
        }

        CurrentDist = NextDist;
    }

    // 确保终点存在
    if (OutAlphas.Last() < 1.0f - KINDA_SMALL_NUMBER)
    {
        OutAlphas.Add(1.0f);
    }
}

void FEditableSurfaceBuilder::GenerateSurfaceMesh()
{
    TArray<float> SampleAlphas;

    // 使用 SampleSpacing 来决定采样密度
    // 这里简单转换一下：如果 SampleSpacing 较小，增加采样点
    float Length = SplineComponent->GetSplineLength();
    float Spacing = FMath::Max(Surface.SampleSpacing, 10.0f);
    int32 AdaptiveSampleCount = FMath::CeilToInt(Length / Spacing);

    // 结合 PathSampleCount (UI设置) 和基于距离的采样
    // 实际项目中可以更智能地混合这两个逻辑

    // 这里我们直接按等距采样生成 Alphas，因为这是最稳定的方式
    // 并在 GetPathSample 中获取信息
    SampleAlphas.Empty();
    int32 FinalSampleCount = FMath::Max(PathSampleCount, AdaptiveSampleCount);

    for (int32 i = 0; i <= FinalSampleCount; ++i)
    {
        SampleAlphas.Add(static_cast<float>(i) / static_cast<float>(FinalSampleCount));
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

        // 计算斜切方向 (Miter Direction)
        // 这对于急转弯处保持路面宽度一致非常重要
        FVector MiterDir;
        float MiterScale = 1.0f;

        if (i == 0)
        {
            // 起点：直接使用 Binormal
            MiterDir = CurrSample.Binormal;
        }
        else if (i == Samples.Num() - 1)
        {
            // 终点：直接使用 Binormal
            MiterDir = CurrSample.Binormal;
        }
        else
        {
            // 中间点：计算前后切线的角平分线
            const FPathSampleInfo& PrevSample = Samples[i - 1];
            const FPathSampleInfo& NextSample = Samples[i + 1];

            // 这里的切线是 Forward，我们需要 Binormal 的平分线
            // MiterDir 应该垂直于角平分线切线

            // 简单且稳健的方法：计算切线的平均值，然后叉乘 Up
            FVector AvgTangent = (CurrSample.Tangent + PrevSample.Tangent).GetSafeNormal(); // 前一段切线和当前点切线近似
            // 更精确的是使用前后点的连线方向
            FVector DirToPrev = (PrevSample.Location - CurrSample.Location).GetSafeNormal();
            FVector DirToNext = (NextSample.Location - CurrSample.Location).GetSafeNormal();

            // 角平分线方向 (指向弯道内侧或外侧)
            FVector Bisector = (DirToNext - DirToPrev).GetSafeNormal();

            // 修正：我们需要的是横向的扩展方向
            // 如果路径是直的，DirToNext = -DirToPrev，Bisector 接近0
            // 此时回退到 Binormal

            // 使用 CalculateMiterDirection 辅助函数处理数学细节
            MiterDir = CalculateMiterDirection(Samples[i - 1], CurrSample, Samples[i + 1], false);

            // 计算缩放因子：1 / dot(MiterDir, Binormal)
            // 防止过度尖锐
            float Dot = FVector::DotProduct(MiterDir, CurrSample.Binormal);
            if (Dot < 0.2f) Dot = 0.2f; // 钳制最大拉伸
            MiterScale = 1.0f / Dot;
        }

        // 获取该点的路宽 (从样条线 Scale 获取)
        float CurrentWidth = GetPathWidth(SampleAlphas[i]);

        // 生成当前截面顶点
        TArray<int32> RowIndices = GenerateCrossSection(
            CurrSample,
            CurrentWidth * 0.5f, // 半宽
            SideSmoothness,
            SampleAlphas[i],
            MiterDir,
            MiterScale
        );

        FrontCrossSections.Add(RowIndices);

        // 如果不是第一行，连接三角形
        if (i > 0)
        {
            const TArray<int32>& PrevRow = FrontCrossSections[i - 1];
            const TArray<int32>& CurrRow = RowIndices;

            if (PrevRow.Num() == CurrRow.Num())
            {
                for (int32 j = 0; j < CurrRow.Num() - 1; ++j)
                {
                    // 添加四边形 (两个三角形)
                    // PrevRow[j] -- PrevRow[j+1]
                    //     |             |
                    // CurrRow[j] -- CurrRow[j+1]
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
    // 初始化默认值
    Info.Location = FVector::ZeroVector;
    Info.Tangent = FVector::ForwardVector;
    Info.Normal = FVector::UpVector;
    Info.Binormal = FVector::RightVector;
    Info.DistanceAlongSpline = 0.0f;

    if (!SplineComponent) return Info;

    float SplineLength = SplineComponent->GetSplineLength();
    float Distance = Alpha * SplineLength;
    Info.DistanceAlongSpline = Distance;

    // 1. 获取位置和切线
    // 我们使用 Local 坐标系，意味着"Up"是 Actor 的上方
    Info.Location = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
    Info.Tangent = SplineComponent->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local).GetSafeNormal();

    // 2. 【核心修复】计算稳定的坐标系 (Stable Frame Calculation)
    // 问题根源：SplineComponent->GetUpVectorAtDistanceAlongSpline() 会尝试平滑旋转，
    // 但在急转弯或切角处容易发生翻转 (Twist)。
    // 解决方案：强制以 Actor 的 Z 轴 (FVector::UpVector) 作为参考，除非路面完全垂直。
    
    FVector ReferenceUp = FVector::UpVector; // (0, 0, 1)

    // 检测是否垂直（切线与 Up 平行），防止叉乘失效
    bool bIsVertical = FMath::Abs(FVector::DotProduct(Info.Tangent, ReferenceUp)) > 0.99f;

    if (bIsVertical)
    {
        // 如果路面垂直（如过山车垂直爬升），我们无法使用 Z 轴作为参考
        // 此时回退到样条线自带的 Up Vector
        Info.Normal = SplineComponent->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local).GetSafeNormal();
        Info.Binormal = FVector::CrossProduct(Info.Tangent, Info.Normal).GetSafeNormal();
    }
    else
    {
        // 【稳定模式】：强制路面"水平"
        // 先计算 Binormal (右向量) = Tangent x ReferenceUp
        // 这样可以保证右向量始终平行于地面，不会出现倾斜（Banking）
        Info.Binormal = FVector::CrossProduct(Info.Tangent, ReferenceUp).GetSafeNormal();
        
        // 再反算 Normal (法线) = Binormal x Tangent
        // 这样算出的 Normal 会略微倾斜以适应坡度，但绝不会左右翻转
        Info.Normal = FVector::CrossProduct(Info.Binormal, Info.Tangent).GetSafeNormal();
    }

    return Info;
}

float FEditableSurfaceBuilder::GetPathWidth(float Alpha) const
{
    if (!SplineComponent) return SurfaceWidth;

    float Distance = Alpha * SplineComponent->GetSplineLength();
    FVector Scale = SplineComponent->GetScaleAtDistanceAlongSpline(Distance);

    // 在 Actor 中我们把 Width 存到了 Scale.X
    // 如果 Scale.X 为 0 或 1 (未设置)，则使用全局 SurfaceWidth
    // 但为了支持变化宽度，我们假设 Scale.X 就是绝对宽度（因为在 UpdateSplineFromWaypoints 里是直接赋值的）
    // 注意：Scale 默认是 1.0，如果 Width 是 100，Scale 就是 100

    return FMath::Max(Scale.X, 1.0f);
}

FVector FEditableSurfaceBuilder::CalculateMiterDirection(const FPathSampleInfo& Prev, const FPathSampleInfo& Curr, const FPathSampleInfo& Next, bool bIsLeft) const
{
    // 计算路径在当前点的转角平分线方向，用于处理尖角
    FVector Dir1 = (Curr.Location - Prev.Location).GetSafeNormal();
    FVector Dir2 = (Next.Location - Curr.Location).GetSafeNormal();

    // 如果共线，直接返回 Binormal
    if (FVector::DotProduct(Dir1, Dir2) > 0.999f)
    {
        return Curr.Binormal;
    }

    // 拐角切线（平均方向）
    FVector CornerTangent = (Dir1 + Dir2).GetSafeNormal();

    // Miter 方向垂直于 CornerTangent 和 UpVector
    // 注意：这里的 UpVector 应该取 Curr.Normal
    FVector MiterDir = FVector::CrossProduct(CornerTangent, Curr.Normal).GetSafeNormal();

    // 确保 MiterDir 指向右侧 (与 Binormal 同向)
    if (FVector::DotProduct(MiterDir, Curr.Binormal) < 0.0f)
    {
        MiterDir = -MiterDir;
    }

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

    // 我们从左到右生成顶点
    // 结构: [左坡...] [左路肩] [路中] [右路肩] [右坡...]
    // 但为了简化，我们将 "路面" 视为中心部分，坡度视为延伸

    // 修正 MiterDir 的强度，避免在极锐角处过度拉伸
    // MiterScale 已经在外面计算好了

    FVector RightVec = MiterDir * MiterScale;

    // 1. 计算关键点的相对偏移（相对于 SampleInfo.Location）
    // 中心点：SampleInfo.Location

    // 这里的 RoadHalfWidth 是路面的一半宽度
    // 我们需要生成的点序列（从最左侧到最右侧）

    struct FPointDef {
        float OffsetX; // 横向距离（负为左，正为右）
        float OffsetZ; // 垂直距离
        float U;       // UV 的 U 坐标
    };

    TArray<FPointDef> Points;

    // --- 左侧坡度 (Left Slope) ---
    if (SlopeSegments > 0)
    {
        for (int32 i = SlopeSegments; i > 0; --i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SlopeSegments);
            float Dist = RoadHalfWidth + (LeftSlopeLength * Ratio); // 越远越左
            float Height = -(LeftSlopeLength * Ratio) * LeftSlopeGradient; // 向下或向上

            // U坐标：路面边缘是0，坡度延展到负数或保持0（取决于需求）
            // 这里为了纹理连续，让路面是 0~1，坡度是额外区域
            float U = -0.2f * Ratio;

            Points.Add({ -Dist, Height, U });
        }
    }

    // --- 路面左边缘 ---
    Points.Add({ -RoadHalfWidth, 0.0f, 0.0f });

    // --- 路面中心 (可选，为了顶点色混合或精度) ---
    // Points.Add({ 0.0f, 0.0f, 0.5f }); // 如果需要中点

    // --- 路面右边缘 ---
    Points.Add({ RoadHalfWidth, 0.0f, 1.0f });

    // --- 右侧坡度 (Right Slope) ---
    if (SlopeSegments > 0)
    {
        for (int32 i = 1; i <= SlopeSegments; ++i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SlopeSegments);
            float Dist = RoadHalfWidth + (RightSlopeLength * Ratio);
            float Height = -(RightSlopeLength * Ratio) * RightSlopeGradient;

            float U = 1.0f + (0.2f * Ratio);

            Points.Add({ Dist, Height, U });
        }
    }

    // 2. 生成实际顶点
    for (const FPointDef& Pt : Points)
    {
        // 位置 = 中心 + (右向量 * 横向偏移) + (上向量 * 垂直偏移)
        FVector Position = SampleInfo.Location + (RightVec * Pt.OffsetX) + (SampleInfo.Normal * Pt.OffsetZ);

        // 法线：需要根据坡度计算，或者简化使用 SampleInfo.Normal (平滑着色会自动处理)
        // 为了更好的效果，路面部分使用 Up，坡度部分应该倾斜
        // 这里暂时统一使用 Up，依赖 CalculateTangents 后期计算平滑法线
        FVector Normal = SampleInfo.Normal;

        // UV 计算
        FVector2D UV;
        UV.X = Pt.U;

        if (TextureMapping == ESurfaceTextureMapping::Stretch)
        {
            UV.Y = Alpha; // 0~1 纵向拉伸
        }
        else
        {
            // 真实世界距离 * 缩放因子
            UV.Y = SampleInfo.DistanceAlongSpline * ModelGenConstants::GLOBAL_UV_SCALE;
        }

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

    // =================================================================================
    // 1. 生成背面 (Bottom Surface)
    // =================================================================================
    for (int32 i = 0; i < NumRows; ++i)
    {
        TArray<int32> BackRow;
        const TArray<int32>& FrontRow = FrontCrossSections[i];

        for (int32 Idx : FrontRow)
        {
            FVector Pos = GetPosByIndex(Idx);
            FVector Normal = MeshData.Normals[Idx];
            
            // [UV 优化]：获取顶面 UV 并翻转 X (U坐标)，防止底面纹理镜像
            FVector2D UV = MeshData.UVs[Idx];
            UV.X *= -1.0f;

            // 向下挤出
            FVector BackPos = Pos - Normal * ThicknessValue;
            FVector BackNormal = -Normal; // 法线向下

            BackRow.Add(AddVertex(BackPos, BackNormal, UV));
        }
        BackCrossSections.Add(BackRow);
    }

    // 缝合底面三角形
    for (int32 i = 0; i < NumRows - 1; ++i)
    {
        const TArray<int32>& RowA = BackCrossSections[i];
        const TArray<int32>& RowB = BackCrossSections[i + 1];

        int32 NumVerts = RowA.Num();
        for (int32 j = 0; j < NumVerts - 1; ++j)
        {
            // 保持正确的渲染方向 (法线向下)
            // 顺序: FrontLeft -> FrontRight -> BackRight -> BackLeft
            AddQuad(RowA[j], RowA[j + 1], RowB[j + 1], RowB[j]);
        }
    }

    // =================================================================================
    // 2. 生成侧壁 (Side Walls)
    // =================================================================================
    int32 LastIdx = FrontCrossSections[0].Num() - 1;
    // 侧壁垂直方向的 UV 长度基于实际物理厚度
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

        // 计算对应的底部位置
        FVector P_BL = P_FL - MeshData.Normals[FL_Idx] * ThicknessValue;
        FVector P_BL_Next = P_FL_Next - MeshData.Normals[FL_Next_Idx] * ThicknessValue;
        FVector P_BR = P_FR - MeshData.Normals[FR_Idx] * ThicknessValue;
        FVector P_BR_Next = P_FR_Next - MeshData.Normals[FR_Next_Idx] * ThicknessValue;

        // [UV 优化]：侧壁 U 坐标跟随路面长度，V 坐标对应厚度
        float U_Curr = MeshData.UVs[FL_Idx].Y;
        float U_Next = MeshData.UVs[FL_Next_Idx].Y;

        // --- Left Side Wall ---
        FVector LeftDir = (P_FL_Next - P_FL).GetSafeNormal();

        FVector LeftNormal = FVector::CrossProduct(-MeshData.Normals[FL_Idx], LeftDir).GetSafeNormal();

        int32 V_TL = AddVertex(P_FL, LeftNormal, FVector2D(U_Curr, 0.0f));
        int32 V_TR = AddVertex(P_FL_Next, LeftNormal, FVector2D(U_Next, 0.0f));
        int32 V_BL = AddVertex(P_BL, LeftNormal, FVector2D(U_Curr, ThicknessV));
        int32 V_BR = AddVertex(P_BL_Next, LeftNormal, FVector2D(U_Next, ThicknessV));

        // 保持正确的渲染方向 (TL -> BL -> BR -> TR)
        AddQuad(V_TL, V_BL, V_BR, V_TR);

        // --- Right Side Wall ---
        FVector RightDir = (P_FR_Next - P_FR).GetSafeNormal();

        FVector RightNormal = FVector::CrossProduct(RightDir, MeshData.Normals[FR_Idx]).GetSafeNormal();

        int32 V_R_TL = AddVertex(P_FR, RightNormal, FVector2D(U_Curr, 0.0f));
        int32 V_R_TR = AddVertex(P_FR_Next, RightNormal, FVector2D(U_Next, 0.0f));
        int32 V_R_BL = AddVertex(P_BR, RightNormal, FVector2D(U_Curr, ThicknessV));
        int32 V_R_BR = AddVertex(P_BR_Next, RightNormal, FVector2D(U_Next, ThicknessV));

        // 保持正确的渲染方向 (TL -> TR -> BR -> BL)
        AddQuad(V_R_TL, V_R_TR, V_R_BR, V_R_BL);
    }

    // =================================================================================
    // 3. 缝合端盖 (Caps)
    // =================================================================================
    auto StitchCap = [&](const TArray<int32>& FRow, bool bIsStart)
    {
        if (FRow.Num() < 2 || FrontCrossSections.Num() < 2) return;

        FVector P0 = GetPosByIndex(FRow[0]);
        int32 AdjSectionIdx = bIsStart ? 1 : FrontCrossSections.Num() - 2;
        FVector P_Adj = GetPosByIndex(FrontCrossSections[AdjSectionIdx][0]);
        
        // 计算封口平面的法线
        FVector FaceNormal = (P0 - P_Adj).GetSafeNormal();

        // 确定平面投影轴 (参考 PolygonTorusBuilder 和 FrustumBuilder 的做法)
        int32 MidIdx = FRow.Num() / 2;
        FVector CenterPos = GetPosByIndex(FRow[MidIdx]);
        
        // 投影轴 Y: 沿厚度方向 (向下)
        FVector ThicknessDir = -MeshData.Normals[FRow[MidIdx]]; 
        // 投影轴 X: 沿宽度方向 (向右)
        FVector WidthDir = FVector::CrossProduct(FaceNormal, ThicknessDir).GetSafeNormal();

        for (int32 j = 0; j < FRow.Num() - 1; ++j)
        {
            int32 Idx0 = FRow[j];
            int32 Idx1 = FRow[j + 1];

            FVector P_F0 = GetPosByIndex(Idx0);
            FVector P_F1 = GetPosByIndex(Idx1);
            
            // 重新计算底部点以获得独立的 Hard Edge 法线
            FVector P_B0 = P_F0 - MeshData.Normals[Idx0] * ThicknessValue;
            FVector P_B1 = P_F1 - MeshData.Normals[Idx1] * ThicknessValue;

            // [UV 优化]：平面投影 UV 计算
            auto GetCapUV = [&](const FVector& P) {
                FVector Rel = P - CenterPos;
                // 基于物理距离投影
                float U = FVector::DotProduct(Rel, WidthDir);
                float V = FVector::DotProduct(Rel, ThicknessDir);
                
                // 翻转 Start 面的 U，防止首尾纹理镜像 (参考 FrustumBuilder 逻辑)
                if (bIsStart) U = -U;
                
                // 应用全局缩放
                return FVector2D(U * ModelGenConstants::GLOBAL_UV_SCALE, V * ModelGenConstants::GLOBAL_UV_SCALE);
            };

            int32 V_TL = AddVertex(P_F0, FaceNormal, GetCapUV(P_F0));
            int32 V_TR = AddVertex(P_F1, FaceNormal, GetCapUV(P_F1));
            int32 V_BL = AddVertex(P_B0, FaceNormal, GetCapUV(P_B0));
            int32 V_BR = AddVertex(P_B1, FaceNormal, GetCapUV(P_B1));

            // 保持正确的渲染方向
            if (bIsStart)
            {
                // Start Cap (面向后): TL -> TR -> BR -> BL
                AddQuad(V_TL, V_TR, V_BR, V_BL);
            }
            else
            {
                // End Cap (面向前): TR -> TL -> BL -> BR
                AddQuad(V_TR, V_TL, V_BL, V_BR);
            }
        }
    };

    StitchCap(FrontCrossSections[0], true);
    StitchCap(FrontCrossSections.Last(), false);
}