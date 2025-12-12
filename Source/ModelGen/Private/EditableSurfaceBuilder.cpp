// Copyright (c) 2024. All rights reserved.

#include "EditableSurfaceBuilder.h"

#include "ModelGenConstants.h"

#include "Components/SplineComponent.h"

FEditableSurfaceBuilder::FEditableSurfaceBuilder(const AEditableSurface& InSurface)
    : Surface(InSurface)
{
    // 缓存配置数据
    SplineComponent = InSurface.SplineComponent;
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
    // 基于自适应采样估算：使用样条长度和路点数量
    // 自适应采样会根据曲率自动调整采样密度，这里给出保守估算
    int32 CrossSectionPoints = 2 + (SideSmoothness * 2);
    
    // 估算采样点数量：基于路点数量和样条长度
    // 每个路点之间至少会有一些采样点，急弯处会更多
    int32 NumWaypoints = SplineComponent ? SplineComponent->GetNumberOfSplinePoints() : 2;
    float SplineLength = SplineComponent ? SplineComponent->GetSplineLength() : 1000.0f;
    
    // 保守估算：每100单位长度至少1个采样点，每个路点之间至少2个采样点
    int32 EstimatedSamples = FMath::Max(
        FMath::CeilToInt(SplineLength / 100.0f),
        NumWaypoints * 2
    );

    int32 BaseCount = EstimatedSamples * CrossSectionPoints;
    return bEnableThickness ? BaseCount * 2 + (EstimatedSamples * 2) : BaseCount;
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

    // =========================================================
    // 3. [关键新增] 清理无效多边形 (Remove Degenerate Triangles)
    // 防止 StaticMesh 转换时的 ComputeTangents 报错或物理烘焙失败
    // =========================================================
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

            // 检查 1: 索引去重 (防止同一个点构成线段)
            if (Idx0 == Idx1 || Idx1 == Idx2 || Idx0 == Idx2)
            {
                continue;
            }

            // 检查 2: 面积检查 (防止三点共线或距离极近)
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

                // 如果叉积模长的平方非常小，说明面积接近0，是无效三角形
                if (UnnormalizedNormal.SizeSquared() > KINDA_SMALL_NUMBER)
                {
                    CleanTriangles.Add(Idx0);
                    CleanTriangles.Add(Idx1);
                    CleanTriangles.Add(Idx2);
                }
            }
        }

        // 应用清理后的三角形索引
        MeshData.Triangles = CleanTriangles;
        MeshData.TriangleCount = CleanTriangles.Num() / 3;
    }

    // =========================================================
    // 4. 后处理
    // =========================================================
    // 只有在移除了退化三角形后，计算切线才是安全的
    MeshData.CalculateTangents();

    OutMeshData = MoveTemp(MeshData);

    return OutMeshData.IsValid();
}

void FEditableSurfaceBuilder::GetAdaptiveSamplePoints(TArray<float>& OutAlphas, float AngleThresholdDeg) const
{
    // (此处逻辑保持简化，如果需要更复杂的自适应采样可在此扩展)
    // 目前使用 GenerateSurfaceMesh 中的固定采样
}

// ====================================================================
// 核心算法：递归自适应采样
// 参考了渲染管线中处理贝塞尔曲线细分的思路
// ====================================================================
void FEditableSurfaceBuilder::RecursiveAdaptiveSampling(
    float StartDist, 
    float EndDist, 
    const FVector& StartTan, 
    const FVector& EndTan, 
    float AngleThresholdCos, 
    float MinStepLen,
    TArray<float>& OutDistanceSamples) const
{
    float SegmentLength = EndDist - StartDist;

    // 1. 递归终止条件：长度太短，不再细分
    if (SegmentLength < MinStepLen)
    {
        return;
    }

    // 计算两端切线的夹角余弦值
    float Dot = FVector::DotProduct(StartTan, EndTan);

    // 2. 递归判断：如果夹角小于阈值 (Dot > ThresholdCos 表示夹角很小)
    // 并且长度也没有特别长，则认为这段已经足够平滑
    if (Dot > AngleThresholdCos)
    {
        return;
    }

    // 3. 需要细分：取中点
    float MidDist = (StartDist + EndDist) * 0.5f;
    FVector MidTan = SplineComponent->GetTangentAtDistanceAlongSpline(MidDist, ESplineCoordinateSpace::Local).GetSafeNormal();

    // 先处理左半段
    RecursiveAdaptiveSampling(StartDist, MidDist, StartTan, MidTan, AngleThresholdCos, MinStepLen, OutDistanceSamples);
    
    // 添加中点
    OutDistanceSamples.Add(MidDist);
    
    // 再处理右半段
    RecursiveAdaptiveSampling(MidDist, EndDist, MidTan, EndTan, AngleThresholdCos, MinStepLen, OutDistanceSamples);
}

void FEditableSurfaceBuilder::GenerateSurfaceMesh()
{
    if (!SplineComponent) return;

    // =========================================================
    // 第一步：生成自适应采样点 (The Good Way)
    // =========================================================
    TArray<float> DistanceSamples;
    float SplineLen = SplineComponent->GetSplineLength();
    
    // 阈值设置：
    // 5.0度：非常平滑 (cos(5) ≈ 0.996)
    // MinStep: 10cm，防止无限递归
    const float AngleThresholdDeg = 5.0f;
    const float AngleThresholdCos = FMath::Cos(FMath::DegreesToRadians(AngleThresholdDeg));
    const float MinStepLen = 10.0f; 

    // 添加起点
    DistanceSamples.Add(0.0f);

    // 获取路点数量 (基于 Spline 的控制点)
    int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
    // 遍历每一段原始 Spline Segment 进行细分检查
    // 这样能保证一定会经过原始控制点，不会丢失形状
    int32 NumSegments = SplineComponent->IsClosedLoop() ? NumPoints : NumPoints - 1;

    for (int32 i = 0; i < NumSegments; ++i)
    {
        float DistStart = SplineComponent->GetDistanceAlongSplineAtSplinePoint(i);
        float DistEnd = SplineComponent->GetDistanceAlongSplineAtSplinePoint((i + 1) % NumPoints);

        // 处理闭环的最后一段回绕情况
        if (DistEnd < DistStart) DistEnd = SplineLen;

        FVector TanStart = SplineComponent->GetTangentAtDistanceAlongSpline(DistStart, ESplineCoordinateSpace::Local).GetSafeNormal();
        FVector TanEnd = SplineComponent->GetTangentAtDistanceAlongSpline(DistEnd, ESplineCoordinateSpace::Local).GetSafeNormal();

        // 执行递归细分
        RecursiveAdaptiveSampling(DistStart, DistEnd, TanStart, TanEnd, AngleThresholdCos, MinStepLen, DistanceSamples);

        // 添加该段终点
        // 注意：如果是最后一段且非闭环，DistEnd 就是 SplineLen
        // 如果是闭环的最后一段，DistEnd 也是 SplineLen (逻辑上)，但在 DistanceSamples 里我们要处理好
        if (i < NumSegments - 1 || !SplineComponent->IsClosedLoop())
    {
            DistanceSamples.Add(DistEnd);
    }
}

    // 处理闭环终点：如果不是闭环，最后一个点在上面已经加了。如果是闭环，通常首尾重合处理。
    // 这里简单处理：确保最后一个采样点是 SplineLen
    if (!FMath::IsNearlyEqual(DistanceSamples.Last(), SplineLen, 0.1f))
    {
        DistanceSamples.Add(SplineLen);
    }

    // 对距离采样进行排序和去重（递归可能产生重复点）
    DistanceSamples.Sort();
    for (int32 i = DistanceSamples.Num() - 1; i > 0; --i)
    {
        if (FMath::IsNearlyEqual(DistanceSamples[i], DistanceSamples[i - 1], 0.1f))
        {
            DistanceSamples.RemoveAt(i);
    }
    }

    // =========================================================
    // 第二步：预计算采样信息
    // =========================================================
    TArray<FPathSampleInfo> Samples;
    Samples.Reserve(DistanceSamples.Num());

    for (float Dist : DistanceSamples)
    {
        // 将距离转换为 Alpha (0~1) 传给 GetPathSample
        // 注意：你的 GetPathSample 内部是用 Alpha * Length 计算距离的，这里反算一下
        float Alpha = (SplineLen > KINDA_SMALL_NUMBER) ? (Dist / SplineLen) : 0.0f;
        Samples.Add(GetPathSample(Alpha));
    }

    FrontVertexStartIndex = MeshData.Vertices.Num();
    FrontCrossSections.Reserve(Samples.Num());

    // =========================================================
    // 第三步：生成几何体 (带内侧防穿插修正)
    // =========================================================
    
    // 用于记录上一帧的边缘顶点位置（世界空间/相对空间）
    // 我们需要这些来检测"逆行"
    FVector PrevLeftEdgePos = FVector::ZeroVector;
    FVector PrevRightEdgePos = FVector::ZeroVector;
    bool bHasPrev = false;

    // 预先计算路面半宽 (不含护坡)
    float RoadHalfWidth = SurfaceWidth * 0.5f;

    for (int32 i = 0; i < Samples.Num(); ++i)
    {
        const FPathSampleInfo& CurrSample = Samples[i];
        FVector MiterDir;
        float MiterScale = 1.0f;

        // --- 1. 计算 Miter 方向 (保持刚才优化的逻辑) ---
        if (i == 0 || i == Samples.Num() - 1)
        {
            MiterDir = CurrSample.Binormal;
        }
        else
        {
            const FPathSampleInfo& Prev = Samples[i-1];
            const FPathSampleInfo& Next = Samples[i+1];

            FVector Dir1 = (CurrSample.Location - Prev.Location).GetSafeNormal();
            FVector Dir2 = (Next.Location - CurrSample.Location).GetSafeNormal();
            
            // 快速共线检查
            if (FVector::DotProduct(Dir1, Dir2) > 0.9999f)
            {
                MiterDir = CurrSample.Binormal;
            }
            else
            {
                FVector AvgTangent = (Dir1 + Dir2).GetSafeNormal();
                MiterDir = FVector::CrossProduct(AvgTangent, CurrSample.Normal).GetSafeNormal();
                
                if (FVector::DotProduct(MiterDir, CurrSample.Binormal) < 0.0f)
                {
                    MiterDir = -MiterDir;
                }

                float Dot = FVector::DotProduct(MiterDir, CurrSample.Binormal);
                // 限制最小夹角，防止除以接近0的数
                if (Dot < 0.25f) Dot = 0.25f; 
                float RawScale = 1.0f / Dot;
                
                // 硬限制最大延伸倍数 (刚才加的 Step 1)
                const float MAX_MITER_SCALE = 2.0f;
                MiterScale = FMath::Min(RawScale, MAX_MITER_SCALE);
            }
        }

        // --- 2. 预测并修正边缘点 (Step 2: Inner Edge Correction) ---
        // 我们先算出如果不做修正，左右边缘点会在哪里
        FVector RightOffsetVec = MiterDir * MiterScale * RoadHalfWidth;
        FVector RawLeftPos  = CurrSample.Location - RightOffsetVec;
        FVector RawRightPos = CurrSample.Location + RightOffsetVec;

        // 如果不是第一排，就进行防穿插检查
        if (bHasPrev)
        {
            FVector ForwardDir = CurrSample.Tangent; // 或者用 (Curr - Prev).GetSafeNormal()

            // 检查左侧 (Left Edge)
            FVector LeftDelta = RawLeftPos - PrevLeftEdgePos;
            if (FVector::DotProduct(LeftDelta, ForwardDir) <= 0.0f)
            {
                // 发现逆行！内侧打结了。
                // 策略：保持在上一帧的位置，或者稍微往前挪一点点
                // 这里简单粗暴地"钉"在上一帧位置，虽然会导致纹理拉伸，但保证了几何体不破面
                RawLeftPos = PrevLeftEdgePos + ForwardDir * 0.1f; 
            }

            // 检查右侧 (Right Edge)
            FVector RightDelta = RawRightPos - PrevRightEdgePos;
            if (FVector::DotProduct(RightDelta, ForwardDir) <= 0.0f)
            {
                // 发现逆行！
                RawRightPos = PrevRightEdgePos + ForwardDir * 0.1f;
            }
        }

        // 更新上一帧记录
        PrevLeftEdgePos = RawLeftPos;
        PrevRightEdgePos = RawRightPos;
        bHasPrev = true;

        // --- 3. 重新反算 MiterDir 和 Scale 传给 GenerateCrossSection ---
        // 因为我们要利用 GenerateCrossSection 统一生成护坡和UV，
        // 所以我们得"骗"它一下。
        // 我们根据修正后的左右点，重新计算一个"等效"的 RightVec
        
        FVector CorrectedRightVec = (RawRightPos - RawLeftPos) * 0.5f; // 新的半宽向量
        float NewHalfWidth = CorrectedRightVec.Size(); // 新的实际物理半宽 (可能变窄了)
        
        // 如果宽度压缩太厉害，可能导致除零，做个保护
        FVector NewMiterDir = FVector::RightVector; 
        float NewMiterScale = 1.0f;

        if (NewHalfWidth > KINDA_SMALL_NUMBER)
        {
            NewMiterDir = CorrectedRightVec / NewHalfWidth; // 归一化方向
            // Scale = 修正后的半宽 / 原始设计的半宽
            // 这样 GenerateCrossSection 乘回去的时候就是正确的位置
            NewMiterScale = NewHalfWidth / RoadHalfWidth; 
        }
        else
        {
            // 极度挤压，退化成中心点
            NewMiterDir = CurrSample.Binormal;
            NewMiterScale = 0.0f; 
        }

        // --- 4. 生成横截面 ---
        float CurrentAlpha = (SplineLen > 0.0f) ? (DistanceSamples[i] / SplineLen) : 0.0f;
        
        TArray<int32> RowIndices = GenerateCrossSection(
            CurrSample,
            RoadHalfWidth, // 传入原始设计宽度
            SideSmoothness,
            CurrentAlpha,
            NewMiterDir,   // 传入修正后的方向
            NewMiterScale  // 传入修正后的缩放 (包含了挤压信息)
        );

        FrontCrossSections.Add(RowIndices);

        // --- 5. 缝合网格 ---
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

    // 1. 获取基础变换
    Info.Location = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
    Info.Tangent = SplineComponent->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local).GetSafeNormal();

    // 2. 稳定的参考系计算 (Standard Up)
    FVector RefUp = FVector::UpVector;
    // 如果 Tangent 垂直向上，更换参考向量防止万向节死锁
    if (FMath::Abs(FVector::DotProduct(Info.Tangent, RefUp)) > 0.99f) RefUp = FVector::RightVector;
    
    FVector BaseRight = FVector::CrossProduct(Info.Tangent, RefUp).GetSafeNormal();
    FVector BaseUp = FVector::CrossProduct(BaseRight, Info.Tangent).GetSafeNormal();

    // 3. [核心算法优化] 自动计算弯道倾斜 (Auto-Banking)
    // 原理：根据曲线的"急促程度"（曲率）来决定倾斜角度
    // 我们向前探测一点距离，看 Tangent 变化了多少
    float LookAheadDist = FMath::Min(Distance + 50.0f, SplineLength);
    FVector NextTangent = SplineComponent->GetTangentAtDistanceAlongSpline(LookAheadDist, ESplineCoordinateSpace::Local).GetSafeNormal();
    
    // 计算当前切线和未来切线的叉积 -> 得到转向轴和转向力度
    FVector TurnAxis = FVector::CrossProduct(Info.Tangent, NextTangent);
    float TurnFactor = TurnAxis.Size(); // 转向越急，这个值越大
    
    // 判断向左转还是向右转: Project TurnAxis onto Up vector
    // 如果 TurnAxis 向上，说明是向左转（基于右手定则）-> 需要向左倾斜
    float TurnDirection = FVector::DotProduct(TurnAxis, BaseUp); 
    
    // 定义最大倾斜角度 (例如 25度) 和 敏感度
    const float MaxBankAngle = 25.0f;
    const float BankSensitivity = 3.0f; 
    
    // 计算最终 Roll 角度 (Degrees)
    float TargetRoll = TurnDirection * BankSensitivity * MaxBankAngle;
    TargetRoll = FMath::Clamp(TargetRoll, -MaxBankAngle, MaxBankAngle);

    // 4. 应用倾斜旋转
    // 将基础的 Up/Right 向量绕着 Tangent 轴旋转 TargetRoll 角度
    FQuat BankRot = FQuat(Info.Tangent, FMath::DegreesToRadians(TargetRoll));
    
    Info.Normal = BankRot.RotateVector(BaseUp);
    Info.Binormal = BankRot.RotateVector(BaseRight);

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

    // =================================================================================
    // 【修改开始】UV 映射逻辑重构
    // =================================================================================
    
    float RoadTotalWidth = RoadHalfWidth * 2.0f;
    float UVFactor = 0.0f;
    
    // 定义路面主体左右边缘的 U 坐标
    float U_RoadLeft = 0.0f;
    float U_RoadRight = 0.0f;

    if (TextureMapping == ESurfaceTextureMapping::Stretch)
    {
        // 拉伸模式：强制映射到 [0, 1]
        UVFactor = (RoadTotalWidth > KINDA_SMALL_NUMBER) ? (1.0f / RoadTotalWidth) : 0.0f;
        U_RoadLeft = 0.0f;
        U_RoadRight = 1.0f;
    }
    else // Default (物理模式)
    {
        // 物理模式：使用全局缩放，基于真实物理宽度计算 U
        UVFactor = ModelGenConstants::GLOBAL_UV_SCALE;

        // 【关键策略】为了让路面变宽时纹理向两侧自然生长而不是整体偏移，
        // 我们以中心线为基准 (U=0)，向左为负，向右为正。
        // 如果您希望左边缘永远是0，可以改为 U_RoadLeft = 0.0f; U_RoadRight = RoadTotalWidth * UVFactor;
        U_RoadLeft = -RoadHalfWidth * UVFactor;
        U_RoadRight = RoadHalfWidth * UVFactor;
    }

    // lambda: 计算护坡上的相对偏移 (X, Z)
    // [核心优化]：根据平滑度决定是 直线 还是 圆弧
    auto CalculateBevelOffset = [&](float Ratio, float Len, float Grad, float& OutX, float& OutZ)
    {
        // Ratio 从 0.0 (靠近路面) 到 1.0 (最远端)
        float WeightX, WeightZ;

        if (SideSmoothness <= 1)
        {
            // 直线护坡 (原逻辑)
            WeightX = Ratio;
            WeightZ = Ratio;
        }
        else
        {
            // 圆形护坡 (优化逻辑)
            // 模拟 1/4 圆弧形状 (凸起)
            // X 轴使用 sin (前期增长快，后期平缓)
            // Z 轴使用 1-cos (前期下沉慢，后期陡峭)
            float Angle = Ratio * HALF_PI;
            WeightX = FMath::Sin(Angle);
            WeightZ = 1.0f - FMath::Cos(Angle);
        }

        OutX = Len * WeightX;
        // Z 偏移 = 长度 * 梯度比率 * 权重
        OutZ = (Len * Grad) * WeightZ; 
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
        
        // 【修改】起始 U 必须从路面左边缘开始算，而不是写死 0.0
        float CurrentU = U_RoadLeft;

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

    // --- 路面左边缘 ---
    // 【修改】使用计算好的 U_RoadLeft
    Points.Add({ -RoadHalfWidth, 0.0f, U_RoadLeft });

    // --- 路面右边缘 ---
    // 【修改】使用计算好的 U_RoadRight
    Points.Add({ RoadHalfWidth, 0.0f, U_RoadRight });

    // =================================================================================
    // 2. 右侧护坡 (Right Slope) - 从路边向外计算
    // =================================================================================
    if (SlopeSegments > 0)
    {
        float PrevX = 0.0f;
        float PrevZ = 0.0f;
        
        // 【修改】起始 U 从路面右边缘开始算
        float CurrentU = U_RoadRight;

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
