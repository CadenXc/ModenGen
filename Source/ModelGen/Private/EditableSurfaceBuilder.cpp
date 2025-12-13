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
    
    // [保留] 清理旧数据成员（将在后续步骤中删除）
    FrontCrossSections.Empty();
    FrontVertexStartIndex = 0;
    FrontVertexCount = 0;
}

int32 FEditableSurfaceBuilder::CalculateVertexCountEstimate() const
{
    // 基于新的网格方案估算
    // 横截面点数：路面2个点 + 左右护坡各 SideSmoothness 个点
    int32 ProfilePointCount = 2 + (SideSmoothness * 2);
    
    // 路径采样点数：基于样条长度，每50cm一个采样点
    float SplineLength = SplineComponent ? SplineComponent->GetSplineLength() : 1000.0f;
    int32 PathSampleCount = FMath::CeilToInt(SplineLength / 50.0f) + 1;
    
    // 总顶点数 = 路径采样点数 × 横截面点数
    int32 BaseCount = PathSampleCount * ProfilePointCount;
    
    // 如果启用厚度，需要底面和侧壁
    return bEnableThickness ? BaseCount * 2 + (PathSampleCount * 2) : BaseCount;
}

int32 FEditableSurfaceBuilder::CalculateTriangleCountEstimate() const
{
    return CalculateVertexCountEstimate() * 3;
}

// ==========================================================================
// 步骤 2: 定义横截面 (生成路面 + 护坡)
// ==========================================================================
void FEditableSurfaceBuilder::BuildProfileDefinition()
{
    float HalfWidth = SurfaceWidth * 0.5f;
    float GlobalUVScale = ModelGenConstants::GLOBAL_UV_SCALE;
    float RoadTotalWidth = SurfaceWidth;
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
        UVFactor = GlobalUVScale;
        U_RoadLeft = -HalfWidth * UVFactor;
        U_RoadRight = HalfWidth * UVFactor;
    }

    // lambda: 计算护坡上的相对偏移 (X, Z) - 使用旧的逻辑（支持圆弧）
    auto CalculateBevelOffset = [&](float Ratio, float Len, float Grad, float& OutX, float& OutZ)
    {
        // Ratio 从 0.0 (靠近路面) 到 1.0 (最远端)
        float WeightX, WeightZ;

        if (SideSmoothness <= 1)
        {
            // 直线护坡
            WeightX = Ratio;
            WeightZ = Ratio;
        }
        else
        {
            // 圆形护坡 - 模拟 1/4 圆弧形状 (凸起)
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
    if (SideSmoothness > 0)
    {
        TArray<FProfilePoint> LeftPoints;
        LeftPoints.Reserve(SideSmoothness);

        // 从路边缘 (Ratio=0) 开始向外延伸到 (Ratio=1)
        float PrevX = 0.0f;
        float PrevZ = 0.0f;
        
        // 起始 U 从路面左边缘开始算
        float CurrentU = U_RoadLeft;

        for (int32 i = 1; i <= SideSmoothness; ++i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SideSmoothness);
            
            float RelX, RelZ;
            CalculateBevelOffset(Ratio, LeftSlopeLength, LeftSlopeGradient, RelX, RelZ);

            // 计算该段的实际物理长度 (弧长近似值)
            float SegDist = FMath::Sqrt(FMath::Square(RelX - PrevX) + FMath::Square(RelZ - PrevZ));

            // 向左延伸，U 减小
            CurrentU -= SegDist * UVFactor;

            // 转换为绝对坐标偏移
            float FinalDist = HalfWidth + RelX;

            // 添加到临时数组
            LeftPoints.Add({ -FinalDist, RelZ, CurrentU, true }); // bIsSlope = true

            PrevX = RelX;
            PrevZ = RelZ;
        }

        // 反向添加到主数组 (因为主数组顺序是 FarLeft -> NearLeft)
        for (int32 i = LeftPoints.Num() - 1; i >= 0; --i)
        {
            ProfileDefinition.Add(LeftPoints[i]);
        }
    }

    // --- 路面左边缘 ---
    ProfileDefinition.Add({ -HalfWidth, 0.0f, U_RoadLeft, false });

    // --- 路面右边缘 ---
    ProfileDefinition.Add({ HalfWidth, 0.0f, U_RoadRight, false });

    // =================================================================================
    // 2. 右侧护坡 (Right Slope) - 从路边向外计算
    // =================================================================================
    if (SideSmoothness > 0)
    {
        float PrevX = 0.0f;
        float PrevZ = 0.0f;
        
        // 起始 U 从路面右边缘开始算
        float CurrentU = U_RoadRight;

        for (int32 i = 1; i <= SideSmoothness; ++i)
        {
            float Ratio = static_cast<float>(i) / static_cast<float>(SideSmoothness);

            float RelX, RelZ;
            CalculateBevelOffset(Ratio, RightSlopeLength, RightSlopeGradient, RelX, RelZ);

            float SegDist = FMath::Sqrt(FMath::Square(RelX - PrevX) + FMath::Square(RelZ - PrevZ));

            // 向右延伸，U 增加
            CurrentU += SegDist * UVFactor;

            float FinalDist = HalfWidth + RelX;

            ProfileDefinition.Add({ FinalDist, RelZ, CurrentU, true }); // bIsSlope = true

            PrevX = RelX;
            PrevZ = RelZ;
        }
    }
}

// ==========================================================================
// 核心算法改进：计算拐角几何 (Miter Algorithm)
// ==========================================================================
void FEditableSurfaceBuilder::CalculateCornerGeometry()
{
    int32 NumPoints = SampledPath.Num();
    PathCornerData.SetNum(NumPoints);

    // =========================================================
    // 第一遍 (Pass 1): 计算基础几何与瞬时圆心
    // =========================================================
    
    // 临时缓存，用于防抖动
    bool bLastValidTurnLeft = false; 

    for (int32 i = 0; i < NumPoints; ++i)
    {
        // 1. 获取基础向量
        FVector CurrPos = SampledPath[i].Location;
        FVector UpVec = SampledPath[i].Normal;
        FVector PrevPos, NextPos;

        // 边界处理
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

        // 2. Miter 方向计算
        FVector CornerTangent = (DirIn + DirOut).GetSafeNormal();
        // 如果 DirIn 和 DirOut 共线（反向），CornerTangent 会变成 0
        if (CornerTangent.IsZero()) CornerTangent = SampledPath[i].Tangent;

        FVector GeometricRight = FVector::CrossProduct(CornerTangent, UpVec).GetSafeNormal();
        
        // 翻转检查：确保 GeometricRight 指向路的一侧，而不是反向
        FVector StandardRight = SampledPath[i].RightVector;
        float MiterDot = FVector::DotProduct(StandardRight, GeometricRight);
        if (MiterDot < 0.0f)
        {
            GeometricRight = -GeometricRight;
            MiterDot = -MiterDot;
        }

        float Scale = (MiterDot > 1.e-4f) ? (1.0f / MiterDot) : 1.0f;
        Scale = FMath::Min(Scale, 3.0f);

        // 转弯方向判定 (带阈值防抖)
        float TurnDir = FVector::DotProduct(FVector::CrossProduct(DirIn, DirOut), UpVec);
        bool bIsLeftTurn = bLastValidTurnLeft; 
        if (FMath::Abs(TurnDir) > 0.001f)
        {
            bIsLeftTurn = (TurnDir > 0.0f);
            bLastValidTurnLeft = bIsLeftTurn;
        }

        // 圆心计算
        float TurnRadius = FLT_MAX;
        FVector RotationCenter = CurrPos;
        float CosTheta = FMath::Clamp(FVector::DotProduct(DirIn, DirOut), -1.0f, 1.0f);
        
        if (CosTheta < 0.9999f) 
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

        // 护坡缩放 (保持不变)
        float HalfWidth = SurfaceWidth * 0.5f;
        float LeftSlopeScale = 1.0f;
        float RightSlopeScale = 1.0f;
        if (TurnRadius < 10000.0f) 
        {
             float LeftEffectiveRadius = HalfWidth + LeftSlopeLength;
             float RightEffectiveRadius = HalfWidth + RightSlopeLength;
             LeftSlopeScale = bIsLeftTurn ? (HalfWidth / LeftEffectiveRadius) : (RightEffectiveRadius / HalfWidth);
             RightSlopeScale = bIsLeftTurn ? (RightEffectiveRadius / HalfWidth) : (HalfWidth / RightEffectiveRadius);
             
             float MiterIntensity = FMath::Clamp((Scale - 1.0f) / 1.0f, 0.0f, 1.0f);
             LeftSlopeScale = FMath::Lerp(1.0f, LeftSlopeScale, MiterIntensity);
             RightSlopeScale = FMath::Lerp(1.0f, RightSlopeScale, MiterIntensity);
        }

        PathCornerData[i].MiterVector = GeometricRight;
        PathCornerData[i].MiterScale = Scale;
        PathCornerData[i].bIsConvexLeft = bIsLeftTurn;
        PathCornerData[i].LeftSlopeMiterScale = LeftSlopeScale;
        PathCornerData[i].RightSlopeMiterScale = RightSlopeScale;
        PathCornerData[i].TurnRadius = TurnRadius;
        PathCornerData[i].RotationCenter = RotationCenter;
        
        // 标记是否是急弯：如果护坡长度可能超过半径，就算急弯
        float MaxSlopeLen = FMath::Max(LeftSlopeLength, RightSlopeLength);
        // 阈值设得稍微宽一点，确保能捕捉到整个弯道区域
        float ThresholdRadius = (SurfaceWidth * 0.5f + MaxSlopeLen) * 1.2f;
        
        // 额外保护：如果半径极大（接近直线），禁用急弯标记
        if (TurnRadius > 50000.0f)
        {
            PathCornerData[i].bIsSharpTurn = false;
        }
        else
        {
            PathCornerData[i].bIsSharpTurn = (TurnRadius < ThresholdRadius);
        }
    }

    // =========================================================
    // 第二遍 (Pass 2): 寻找弯道极点并强制归一 (Cluster & Snap)
    // 这是解决"抖动"和实现"单点汇聚"的关键！
    // =========================================================
    
    int32 Index = 0;
    while (Index < NumPoints)
    {
        // 1. 寻找连续的急弯段 (Cluster)
        if (PathCornerData[Index].bIsSharpTurn)
        {
            int32 StartIdx = Index;
            int32 EndIdx = Index;
            
            // 向后搜索直到弯道结束
            while (EndIdx < NumPoints && PathCornerData[EndIdx].bIsSharpTurn)
            {
                // 同时确保弯道方向一致（防止S弯连在一起处理）
                if (PathCornerData[EndIdx].bIsConvexLeft != PathCornerData[StartIdx].bIsConvexLeft)
                {
                    break;
                }
                EndIdx++;
            }
            // EndIdx 现在指向弯道外的第一个点
            int32 Count = EndIdx - StartIdx;

            // 2. 在这段弯道里，找到半径最小的那个点 (The Apex)
            int32 BestIdx = StartIdx;
            float MinRadius = FLT_MAX;
            
            for (int32 k = StartIdx; k < EndIdx; ++k)
            {
                if (PathCornerData[k].TurnRadius < MinRadius)
                {
                    MinRadius = PathCornerData[k].TurnRadius;
                    BestIdx = k;
                }
            }

            // 获取极点的圆心数据
            FVector BestCenter = PathCornerData[BestIdx].RotationCenter;
            float BestRadius = PathCornerData[BestIdx].TurnRadius;

            // 3. 【核心操作】将整个簇的圆心强制锁定到极点
            // 这样所有塌缩的点都会指向同一个物理位置，形成完美的扇形
            for (int32 k = StartIdx; k < EndIdx; ++k)
            {
                PathCornerData[k].RotationCenter = BestCenter;
                
                // 注意：不再强制统一半径，因为 GenerateGridMesh 中会使用真实距离判断
                // 保留原始半径用于其他用途（如护坡缩放计算）
                // PathCornerData[k].TurnRadius = BestRadius; 
            }

            // 更新循环索引
            Index = EndIdx;
        }
        else
        {
            Index++;
        }
    }
}

// ==========================================================================
// 重构：生成网格 (应用 Miter 逻辑 + 扇形塌缩)
// ==========================================================================
void FEditableSurfaceBuilder::GenerateGridMesh()
{
    int32 NumRows = SampledPath.Num();
    int32 NumCols = ProfileDefinition.Num();

    if (NumRows < 2 || NumCols < 2) return;

    // 记录这一批顶点的起始索引和 Grid 结构信息（用于厚度生成）
    GridStartIndex = MeshData.Vertices.Num();
    GridNumRows = NumRows;
    GridNumCols = NumCols;

    for (int32 i = 0; i < NumRows; ++i)
    {
        const FSurfaceSamplePoint& Sample = SampledPath[i];
        const FCornerData& Corner = PathCornerData[i];
        
        // 获取圆心 (用于扇形塌缩)
        FVector PivotPoint = Corner.RotationCenter;

        // UV 的 V 坐标
        float V_Coord = (TextureMapping == ESurfaceTextureMapping::Stretch) 
            ? Sample.Alpha 
            : Sample.Distance * ModelGenConstants::GLOBAL_UV_SCALE;

        for (int32 j = 0; j < NumCols; ++j)
        {
            const FProfilePoint& Profile = ProfileDefinition[j];
            
            FVector FinalPos;
            FVector VertexNormal = Sample.Normal;

            // 基础偏移量
            float BaseOffsetH = Profile.OffsetH;
            
            // 1. 判断当前点是在中心线的左侧还是右侧
            bool bIsPointRightSide = (BaseOffsetH > 0.0f);
            
            // 2. 确定当前点是否处于"内角" (Inner Corner)
            // 如果路向左弯 (bIsConvexLeft)，那么左侧的点是内角，右侧是外角
            // 如果路向右弯 (!bIsConvexLeft)，那么右侧的点是内角，左侧是外角
            bool bIsInnerSide = (Corner.bIsConvexLeft && !bIsPointRightSide) || 
                                (!Corner.bIsConvexLeft && bIsPointRightSide);

            // 基础方向
            FVector OffsetDir = Corner.MiterVector * (bIsPointRightSide ? 1.0f : -1.0f);
            
            // =================================================================
            // [扇形塌缩逻辑] 修正版 V2 - 增加方向对齐检查
            // =================================================================
            if (Profile.bIsSlope && bIsInnerSide)
            {
                float IdealDist = FMath::Abs(BaseOffsetH); 
                
                // 1. 距离检查：计算当前采样点到圆心的"实时物理距离"
                float RealDistanceToCenter = FVector::Dist2D(Sample.Location, PivotPoint);
                // 留 5cm 容差
                float SafeRadius = FMath::Max(RealDistanceToCenter - 5.0f, 1.0f);

                // 2. 【关键新增】方向对齐检查 (Alignment Check)
                // 计算"指向圆心的向量"与"当前路段内侧法线"的夹角
                // 如果圆心在"侧前方"（入口）或"侧后方"（出口），而不是"正侧方"，则不应塌缩。
                FVector DirToPivot = (PivotPoint - Sample.Location).GetSafeNormal();
                
                // 获取指向内侧的标准方向（不使用 Miter，因为 Miter 可能被缩放或扭曲）
                FVector InnerNormal = Corner.bIsConvexLeft ? -Sample.RightVector : Sample.RightVector;
                
                // 计算点积：1.0表示圆心在正侧方，0.0表示圆心在正前方
                float Alignment = FVector::DotProduct(DirToPivot, InnerNormal);

                // 阈值设为 0.5 (约60度范围)，只有当圆心确实在侧边时才允许塌缩
                // 这能完美过滤掉弯道出入口的误判
                bool bIsAligned = Alignment > 0.5f;

                // 同时满足：空间不足 且 方向对齐，才执行塌缩
                if (bIsAligned && IdealDist > SafeRadius)
                {
                    // === 触发汇聚 (Collapse) ===
                    FinalPos.X = PivotPoint.X;
                    FinalPos.Y = PivotPoint.Y;

                    // Z 坐标保持路点高度 + 护坡高差，形成"折扇"效果
                    FinalPos.Z = Sample.Location.Z + Profile.OffsetV;

                    // 法线修正：设为向上
                    VertexNormal = FVector::UpVector;
                }
                else
                {
                    // 空间足够 或 方向不对，正常伸展
                    // 使用 OffsetDir (基于 Miter) 进行投影
                    FinalPos = Sample.Location + (OffsetDir * IdealDist) + (Sample.Normal * Profile.OffsetV);
                    
                    // 简单平滑法线
                    float Sign = bIsPointRightSide ? 1.0f : -1.0f;
                    VertexNormal = (Sample.Normal + (Corner.MiterVector * Sign * 0.5f)).GetSafeNormal();
                }
            }
            else
            {
                // 路面 或 外弯护坡
                float AppliedScale = Corner.MiterScale;
                
                if (Profile.bIsSlope && !bIsInnerSide) // 外弯护坡
                {
                    AppliedScale *= bIsPointRightSide ? Corner.RightSlopeMiterScale : Corner.LeftSlopeMiterScale;
                    
                    // === [关键新增] 外侧护坡的智能限制 ===
                    // 防止外侧护坡在急弯时扩张过度导致交叉穿叠
                    if (Corner.bIsSharpTurn)
                    {
                        // 计算当前点到圆心的实际距离
                        float IdealDist = FMath::Abs(BaseOffsetH);
                        FVector TestPos = Sample.Location + (Corner.MiterVector * BaseOffsetH * AppliedScale);
                        float ActualRadius = FVector::Dist2D(TestPos, Corner.RotationCenter);
                        
                        // 如果外侧护坡"超出"了转弯圆的合理半径范围（过度扩张），进行限制
                        // 限制策略：不允许外侧点的半径超过 (内侧路面半径 + 护坡长度) 的 1.5 倍
                        float MaxAllowedRadius = (SurfaceWidth * 0.5f + (bIsPointRightSide ? RightSlopeLength : LeftSlopeLength)) * 1.5f;
                        
                        if (ActualRadius > MaxAllowedRadius)
                        {
                            // 限制扩张：重新计算 Scale，使其恰好达到最大允许半径
                            float SafeScale = (IdealDist > KINDA_SMALL_NUMBER) ? (MaxAllowedRadius / IdealDist) : 1.0f;
                            AppliedScale = FMath::Min(AppliedScale, SafeScale);
                        }
                    }
                }
                else if (!Profile.bIsSlope && bIsInnerSide) // 内弯路面(不延伸)
                {
                    AppliedScale = 1.0f;
                }

                FinalPos = Sample.Location + (Corner.MiterVector * BaseOffsetH * AppliedScale) + (Sample.Normal * Profile.OffsetV);
                
                if (Profile.bIsSlope)
                {
                    float Sign = bIsPointRightSide ? 1.0f : -1.0f;
                    VertexNormal = (Sample.Normal + (Corner.MiterVector * Sign * 0.5f)).GetSafeNormal();
                }
            }

            FVector2D UV(Profile.U, V_Coord);
            AddVertex(FinalPos, VertexNormal, UV);
        }
    }

    // 生成索引 (Triangles) - 上表面从上方看应该是逆时针
    for (int32 i = 0; i < NumRows - 1; ++i)
    {
        for (int32 j = 0; j < NumCols - 1; ++j)
        {
            int32 Row0 = GridStartIndex + (i * GridNumCols);
            int32 Row1 = GridStartIndex + ((i + 1) * GridNumCols);
            
            // 上表面顶点布局（从上方看）：
            // TL -- TR
            // |     |
            // BL -- BR
            int32 TL = Row0 + j;
            int32 TR = Row0 + j + 1;
            int32 BL = Row1 + j;
            int32 BR = Row1 + j + 1;

            // 上表面：从上方看应该是逆时针 (Unreal 默认 Front Face 是逆时针)
            // 顺序：TL -> TR -> BR -> BL (从上方看逆时针)
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

    Clear();

    // =========================================================
    // [逻辑修改] 处理厚度开关
    // 关闭时将厚度设为 0.01，保证模型有底面且闭合，避免单面网格的光照问题
    // =========================================================
    if (!bEnableThickness)
    {
        ThicknessValue = 0.01f;
    }

    // =========================================================
    // 新流程：使用 Grid 方式生成路面
    // =========================================================
    
    // 1. 采样路径
    SampleSplinePath();
    
    // 2. 预计算 Miter 几何
    CalculateCornerGeometry();

    // 3. 构建横截面（只生成路面，不含护坡）
    BuildProfileDefinition();
    
    // 4. 生成网格（Grid 方式）
    GenerateGridMesh();
    
    // 5. 生成厚度（基于 Grid 结构）
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

// ==========================================================================
// 步骤 1: 路径采样
// 核心逻辑：沿样条线等距采集坐标系 (Location, Tangent, Normal, Right)
// 替代了旧方案中的递归细分，逻辑更接近参考代码中的 PointsAlongSpline
// ==========================================================================
void FEditableSurfaceBuilder::SampleSplinePath()
{
    if (!SplineComponent) return;

    float SplineLen = SplineComponent->GetSplineLength();
    
    // 采样步长：50cm。这个密度足以保证弯道平滑，且性能开销可控。
    // 如果需要更高精度，可以改为 20.0f 或做成变量。
    const float SampleStep = 50.0f; 
    int32 NumSteps = FMath::CeilToInt(SplineLen / SampleStep);

    // 预分配内存，多加1是为了包含终点
    SampledPath.Reserve(NumSteps + 1);

    for (int32 i = 0; i <= NumSteps; ++i)
    {
        // 确保不超过总长度
        float Dist = FMath::Min(static_cast<float>(i) * SampleStep, SplineLen);
        
        FSurfaceSamplePoint Point;
        Point.Distance = Dist;
        Point.Alpha = (SplineLen > KINDA_SMALL_NUMBER) ? (Dist / SplineLen) : 0.0f;
        
        // [关键] 获取局部空间的完整变换 (Local Space)
        // 这样生成的网格是相对于 Actor 的，移动 Actor 时网格会跟随
        FTransform TF = SplineComponent->GetTransformAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);
        
        Point.Location = TF.GetLocation();
        
        // 提取正交基向量：这样获取的 Right 和 Up 是严格垂直于切线的，不会产生扭曲
        Point.Tangent = TF.GetUnitAxis(EAxis::X);     // Forward
        Point.RightVector = TF.GetUnitAxis(EAxis::Y); // Right
        Point.Normal = TF.GetUnitAxis(EAxis::Z);      // Up

        SampledPath.Add(Point);
    }
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
    // 检查 Grid 结构是否有效
    if (GridNumRows < 2 || GridNumCols < 2) return;

    // 侧壁垂直纹理长度 (基于物理厚度)
    // 无论路面是否 Stretch，厚度方向通常保持物理比例比较美观
    float ThicknessV = ThicknessValue * ModelGenConstants::GLOBAL_UV_SCALE;

    // 底面顶点将从当前数组末尾开始添加
    int32 BottomGridStartIndex = MeshData.Vertices.Num();

    // =================================================================================
    // 1. 生成底面 (Bottom Surface) - 基于 Grid 结构
    // =================================================================================
    for (int32 i = 0; i < GridNumRows; ++i)
    {
        for (int32 j = 0; j < GridNumCols; ++j)
        {
            // 找到对应的上表面顶点索引
            int32 TopIdx = GridStartIndex + (i * GridNumCols) + j;
            
            // 获取上表面数据
            FVector TopPos = MeshData.Vertices[TopIdx];
            FVector TopNormal = MeshData.Normals[TopIdx]; 
            FVector2D TopUV = MeshData.UVs[TopIdx];

            // 向下挤出 (沿样条线法线反方向)
            FVector BottomPos = TopPos - TopNormal * ThicknessValue;
            
            // 使用与上表面相同的法线方向（底面三角形的缠绕顺序已经反向）
            FVector BottomNormal = TopNormal;

            // 底面 UV (翻转 U)
            FVector2D BottomUV(1.0f - TopUV.X, TopUV.Y);

            AddVertex(BottomPos, BottomNormal, BottomUV);
        }
    }

    // 缝合底面 - 完全反向缠绕，确保法线朝下
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

            // 底面需要完全反向：与上表面 TL->BL->BR->TR 相反
            // 从下方看应该是逆时针，但从上方看是顺时针：BL -> BR -> TR -> TL
            AddQuad(BL, BR, TR, TL);
        }
    }

    // =================================================================================
    // 2. 生成侧壁 (Side Walls) - 左侧和右侧
    // =================================================================================
    auto StitchSideStrip = [&](int32 ColIndex, bool bIsRightSide)
    {
        for (int32 i = 0; i < GridNumRows - 1; ++i)
        {
            // 上下表面的对应索引
            int32 TopCurr = GridStartIndex + (i * GridNumCols) + ColIndex;
            int32 TopNext = GridStartIndex + ((i + 1) * GridNumCols) + ColIndex;
            int32 BotCurr = BottomGridStartIndex + (i * GridNumCols) + ColIndex;
            int32 BotNext = BottomGridStartIndex + ((i + 1) * GridNumCols) + ColIndex;

            // 获取位置和法线
            FVector P_TopCurr = MeshData.Vertices[TopCurr];
            FVector P_TopNext = MeshData.Vertices[TopNext];
            FVector N_TopCurr = MeshData.Normals[TopCurr];
            FVector N_TopNext = MeshData.Normals[TopNext];

            // 计算底部位置
            FVector P_BotCurr = P_TopCurr - N_TopCurr * ThicknessValue;
            FVector P_BotNext = P_TopNext - N_TopNext * ThicknessValue;

            // [UV 优化] 侧壁处理
            // U (沿路方向): 继承顶面 V 坐标。保证侧壁和路面在长度方向对齐。
            // V (垂直方向): 使用物理厚度 ThicknessV。
            float WallU_Curr = MeshData.UVs[TopCurr].Y;
            float WallU_Next = MeshData.UVs[TopNext].Y;

            // 计算侧壁法线
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

            // 侧面 Quad - 重新添加顶点以使用正确的侧壁法线
            int32 V_TL = AddVertex(P_TopCurr, SideNormal, FVector2D(WallU_Curr, 0.0f));
            int32 V_TR = AddVertex(P_TopNext, SideNormal, FVector2D(WallU_Next, 0.0f));
            int32 V_BL = AddVertex(P_BotCurr, SideNormal, FVector2D(WallU_Curr, ThicknessV));
            int32 V_BR = AddVertex(P_BotNext, SideNormal, FVector2D(WallU_Next, ThicknessV));

            if (bIsRightSide)
            {
                // 右侧面：从外侧看应该是逆时针，反转顺序：TL -> BL -> BR -> TR
                AddQuad(V_TL, V_BL, V_BR, V_TR);
            }
            else
            {
                // 左侧面：从外侧看应该是逆时针，反转顺序：TL -> TR -> BR -> BL
                AddQuad(V_TL, V_TR, V_BR, V_BL);
            }
        }
    };

    StitchSideStrip(0, false);             // 左侧壁 (ColIndex = 0)
    StitchSideStrip(GridNumCols - 1, true);    // 右侧壁 (ColIndex = GridNumCols - 1)

    // =================================================================================
    // 3. 缝合端盖 (Caps) - 起点和终点
    // =================================================================================
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

            // 计算端面法线
            int32 AdjRowIndex = bIsStart ? RowIndex + 1 : RowIndex - 1;
            if (AdjRowIndex >= 0 && AdjRowIndex < GridNumRows)
            {
                int32 AdjTopIdx = GridStartIndex + (AdjRowIndex * GridNumCols) + j;
                FVector P_Adj = MeshData.Vertices[AdjTopIdx];
                FVector FaceNormal = (P_F0 - P_Adj).GetSafeNormal();

                // [UV 优化] 端盖处理
                // U: 继承顶面顶点的 U 坐标。这样纹理会横向对齐路面。
                // V: 垂直方向使用物理厚度。
                float U0 = MeshData.UVs[TopIdx0].X;
                float U1 = MeshData.UVs[TopIdx1].X;

                int32 V_TL = AddVertex(P_F0, FaceNormal, FVector2D(U0, 0.0f));
                int32 V_TR = AddVertex(P_F1, FaceNormal, FVector2D(U1, 0.0f));
                int32 V_BL = AddVertex(P_B0, FaceNormal, FVector2D(U0, ThicknessV));
                int32 V_BR = AddVertex(P_B1, FaceNormal, FVector2D(U1, ThicknessV));

                if (bIsStart)
                {
                    // 起点端面（从外向内看，逆时针）：TL -> BL -> BR -> TR
                    AddQuad(V_TL, V_BL, V_BR, V_TR);
                }
                else
                {
                    // 终点端面（从外向内看，逆时针）：TR -> BR -> BL -> TL
                    AddQuad(V_TR, V_BR, V_BL, V_TL);
                }
            }
        }
    };

    StitchCapStrip(0, true);               // 起点封口
    StitchCapStrip(GridNumRows - 1, false);  // 终点封口
}
