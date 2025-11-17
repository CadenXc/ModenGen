// Copyright (c) 2024. All rights reserved.

#include "HollowPrismBuilder.h"
#include "HollowPrism.h"
#include "ModelGenMeshData.h"
#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"

FHollowPrismBuilder::FHollowPrismBuilder(const AHollowPrism& InHollowPrism)
    : HollowPrism(InHollowPrism)
{
    Clear();
    // 默认倒角段数，应从 HollowPrism 获取或保持一致
    BevelSegments = 4;
}

void FHollowPrismBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    // 清空所有环
    TopInnerCapRing.Empty();
    TopOuterCapRing.Empty();
    BottomInnerCapRing.Empty();
    BottomOuterCapRing.Empty();
    TopInnerWallRing.Empty();
    TopOuterWallRing.Empty();
    BottomInnerWallRing.Empty();
    BottomOuterWallRing.Empty();
    // 清空端盖顶点索引记录
    StartOuterCapIndices.Empty();
    StartInnerCapIndices.Empty();
    EndOuterCapIndices.Empty();
    EndInnerCapIndices.Empty();
}

bool FHollowPrismBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!HollowPrism.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    // 计算是否启用倒角
    bEnableBevel = HollowPrism.BevelRadius > 0.0f && BevelSegments > 0;

    // 1. 生成主体侧面几何体 (墙体和倒角)
    if (bEnableBevel)
    {
        // 倒角启用：一次性生成墙体和所有倒角，并填充 Wall/Cap 环
        GenerateSideAndBevelGeometry(EInnerOuter::Inner);
        GenerateSideAndBevelGeometry(EInnerOuter::Outer);
    }
    else
    {
        // 无倒角：生成墙体，并直接用墙体顶点填充 Cap 环
        GenerateWalls(HollowPrism.InnerRadius, HollowPrism.InnerSides, EInnerOuter::Inner);
        GenerateWalls(HollowPrism.OuterRadius, HollowPrism.OuterSides, EInnerOuter::Outer);
    }

    // 2. 使用生成的 Cap 环创建盖子三角形
    GenerateCapTriangles(TopInnerCapRing, TopOuterCapRing, EHeightPosition::Top);
    GenerateCapTriangles(BottomInnerCapRing, BottomOuterCapRing, EHeightPosition::Bottom);


    // 3. 如果不是一个完整的360度圆环，则需要生成端盖来封闭开口
    if (!HollowPrism.IsFullCircle())
    {
        GenerateEndCapWithBevel(EEndCapType::Start);   // 起始端盖
        GenerateEndCapWithBevel(EEndCapType::End);  // 结束端盖
    }

    if (!ValidateGeneratedData())
    {
        return false;
    }

    // 计算正确的切线（用于法线贴图等）
    MeshData.CalculateTangents();

    OutMeshData = MeshData;
    return true;
}

int32 FHollowPrismBuilder::CalculateVertexCountEstimate() const
{
    return HollowPrism.CalculateVertexCountEstimate();
}

int32 FHollowPrismBuilder::CalculateTriangleCountEstimate() const
{
    return HollowPrism.CalculateTriangleCountEstimate();
}

/**
 * @brief 生成墙体 (仅在 bEnableBevel=false 时使用)
 */
void FHollowPrismBuilder::GenerateWalls(float Radius, int32 Sides, EInnerOuter InnerOuter)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Sides);

    TArray<int32> TopVertices, BottomVertices;
    TArray<int32>& TopCapRingRef = (InnerOuter == EInnerOuter::Inner) ? this->TopInnerCapRing : this->TopOuterCapRing;
    TArray<int32>& BottomCapRingRef = (InnerOuter == EInnerOuter::Inner) ? this->BottomInnerCapRing : this->BottomOuterCapRing;

    TopCapRingRef.Empty(Sides + 1);
    BottomCapRingRef.Empty(Sides + 1);

    for (int32 i = 0; i <= Sides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;

        const float WallTopZ = HalfHeight;
        const float WallBottomZ = -HalfHeight;

        const FVector TopPos = CalculateVertexPosition(Radius, Angle, WallTopZ);
        const FVector BottomPos = CalculateVertexPosition(Radius, Angle, WallBottomZ);

        // 法线方向：内壁向内，外壁向外
        const FVector Normal = (InnerOuter == EInnerOuter::Inner)
            ? FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f).GetSafeNormal()
            : FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f).GetSafeNormal();

        // 计算 UV 坐标
        const FVector2D TopUV = CalculateWallUV(Angle, WallTopZ, InnerOuter);
        const FVector2D BottomUV = CalculateWallUV(Angle, WallBottomZ, InnerOuter);

        const int32 TopVertex = GetOrAddVertex(TopPos, Normal, TopUV);
        const int32 BottomVertex = GetOrAddVertex(BottomPos, Normal, BottomUV);

        TopVertices.Add(TopVertex);
        BottomVertices.Add(BottomVertex);

        // 填充 Cap 环
        TopCapRingRef.Add(TopVertex);
        BottomCapRingRef.Add(BottomVertex);
    }

    // 生成四边形面
    for (int32 i = 0; i < Sides; ++i)
    {
        if (InnerOuter == EInnerOuter::Inner)
        {
            // 内壁：逆时针顺序
            AddQuad(TopVertices[i], BottomVertices[i], BottomVertices[i + 1], TopVertices[i + 1]);
        }
        else
        {
            // 外壁：顺时针顺序
            AddQuad(TopVertices[i], TopVertices[i + 1], BottomVertices[i + 1], BottomVertices[i]);
        }
    }
}


/**
 * @brief (新) 统一生成墙体和倒角几何体。
 */
void FHollowPrismBuilder::GenerateSideAndBevelGeometry(EInnerOuter InnerOuter)
{
    const int32 Sides = (InnerOuter == EInnerOuter::Inner) ? HollowPrism.InnerSides : HollowPrism.OuterSides;
    if (Sides == 0) return;

    const int32 Segments = BevelSegments;
    const float BevelRadius = HollowPrism.BevelRadius;
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Sides);

    // 1. 获取所有连接环的引用并清空
    TArray<int32>& CapRingTop = (InnerOuter == EInnerOuter::Inner) ? this->TopInnerCapRing : this->TopOuterCapRing;
    TArray<int32>& CapRingBottom = (InnerOuter == EInnerOuter::Inner) ? this->BottomInnerCapRing : this->BottomOuterCapRing;
    TArray<int32>& WallRingTop = (InnerOuter == EInnerOuter::Inner) ? this->TopInnerWallRing : this->TopOuterWallRing;
    TArray<int32>& WallRingBottom = (InnerOuter == EInnerOuter::Inner) ? this->BottomInnerWallRing : this->BottomOuterWallRing;

    CapRingTop.Empty(Sides + 1);
    CapRingBottom.Empty(Sides + 1);
    WallRingTop.Empty(Sides + 1);
    WallRingBottom.Empty(Sides + 1);

    // 总垂直点数: CapTop(0) + BevelTop(1..Segments) + WallBottom(Segments+1) + BevelBottom(Segments+2..2*Segments+1)
    const int32 TotalVerticalPoints = 2 * Segments + 2;

    TArray<TArray<int32>> SideVertexGrid;
    SideVertexGrid.SetNum(Sides + 1);

    // 半径和Z的符号因子
    const float R_Base = (InnerOuter == EInnerOuter::Inner) ? HollowPrism.InnerRadius : HollowPrism.OuterRadius;
    // 内环：内半径增加倒角 (R + R_bevel), 外环：外半径减小倒角 (R - R_bevel)
    const float dR_Sign = (InnerOuter == EInnerOuter::Inner) ? 1.0f : -1.0f;

    for (int32 s = 0; s <= Sides; ++s)
    {
        const float SideAngle = StartAngle + s * AngleStep;
        SideVertexGrid[s].SetNum(TotalVerticalPoints);

        // 倒角圆弧的中心坐标
        const float CenterRadius = R_Base + dR_Sign * BevelRadius;

        for (int32 i = 0; i < TotalVerticalPoints; ++i)
        {
            FVector Position;
            FVector Normal;
            FVector2D UV;
            float CurrentRadius;
            float CurrentZ;

            // =======================================================
            // 1. 顶部倒角部分 (i=0 到 Segments)
            // =======================================================
            if (i <= Segments)
            {
                // Alpha 从 0.0 (Cap Edge) 到 1.0 (Wall Edge)
                const float Alpha = static_cast<float>(i) / Segments;
                const float Theta = Alpha * HALF_PI; // Theta 从 0 (Cap) 到 90 (Wall)

                // Top Bevel Arc Center: (Z: HalfHeight - R_bevel)
                const float CenterZ = HalfHeight - BevelRadius;

                // Z 坐标: ZCenter + R_bevel * Cos(Theta) (Cos(0)=1, Cos(90)=0)
                CurrentZ = CenterZ + BevelRadius * FMath::Cos(Theta);

                // R 坐标: RCenter - R_bevel * Sin(Theta) (Sin(0)=0, Sin(90)=1)
                if (InnerOuter == EInnerOuter::Inner)
                {
                    CurrentRadius = CenterRadius - BevelRadius * FMath::Sin(Theta); // (Inner+Bevel) -> Inner
                }
                else // Outer
                {
                    CurrentRadius = CenterRadius + BevelRadius * FMath::Sin(Theta); // (Outer-Bevel) -> Outer
                }

                Position = CalculateVertexPosition(CurrentRadius, SideAngle, CurrentZ);

                // 法线: 从中心指向顶点
                const FVector CenterPos = CalculateVertexPosition(CenterRadius, SideAngle, CenterZ);
                Normal = (Position - CenterPos).GetSafeNormal();
                if (Normal.IsNearlyZero()) { Normal = FVector(0.0f, 0.0f, 1.0f); }

                // UV V 坐标: 0.0 (Cap Edge) 到 1.0 (Wall Edge)
                UV = CalculateBevelUV(SideAngle, Alpha, InnerOuter, EHeightPosition::Top);
            }
            // =======================================================
            // 2. 墙体底部环 (i=Segments+1)
            // =======================================================
            else if (i == Segments + 1)
            {
                // Wall Bottom Edge: Z = -HalfHeight + R_bevel
                CurrentZ = -HalfHeight + BevelRadius;
                CurrentRadius = R_Base;
                Position = CalculateVertexPosition(CurrentRadius, SideAngle, CurrentZ);

                // Wall Normal
                Normal = (InnerOuter == EInnerOuter::Inner)
                    ? FVector(-FMath::Cos(SideAngle), -FMath::Sin(SideAngle), 0.0f).GetSafeNormal()
                    : FVector(FMath::Cos(SideAngle), FMath::Sin(SideAngle), 0.0f).GetSafeNormal();

                // UV V 坐标: 1.0 (Wall Top) 到 0.0 (Wall Bottom)
                UV = CalculateWallUV(SideAngle, CurrentZ, InnerOuter);
            }
            // =======================================================
            // 3. 底部倒角部分 (i=Segments+2 到 2*Segments+1)
            // =======================================================
            else // i >= Segments + 2 (Bottom Bevel)
            {
                // BevelStep: 1 to Segments (1 is Wall Edge, Segments is Cap Edge)
                const int32 BevelStep = i - (Segments + 1);
                // Alpha 从 0.0 (Wall Edge) 到 1.0 (Cap Edge)
                const float Alpha = static_cast<float>(BevelStep) / Segments;
                const float Theta = Alpha * HALF_PI; // Theta 从 0 (Wall) 到 90 (Cap)

                // Bottom Bevel Arc Center: (Z: -HalfHeight + R_bevel)
                const float CenterZ = -HalfHeight + BevelRadius;

                // Z 坐标: ZCenter - R_bevel * Cos(Theta) (Cos(0)=1, Cos(90)=0)
                CurrentZ = CenterZ - BevelRadius * FMath::Cos(Theta);

                // R 坐标: RCenter - R_bevel * Sin(Theta) (Sin(0)=0, Sin(90)=1)
                if (InnerOuter == EInnerOuter::Inner)
                {
                    CurrentRadius = CenterRadius - BevelRadius * FMath::Sin(Theta); // Inner -> (Inner+Bevel)
                }
                else // Outer
                {
                    CurrentRadius = CenterRadius + BevelRadius * FMath::Sin(Theta); // Outer -> (Outer-Bevel)
                }

                Position = CalculateVertexPosition(CurrentRadius, SideAngle, CurrentZ);

                // 法线: 从中心指向顶点
                const FVector CenterPos = CalculateVertexPosition(CenterRadius, SideAngle, CenterZ);
                Normal = (Position - CenterPos).GetSafeNormal();
                if (Normal.IsNearlyZero()) { Normal = FVector(0.0f, 0.0f, -1.0f); }

                // UV V 坐标: 0.0 (Wall Edge) 到 1.0 (Cap Edge)
                UV = CalculateBevelUV(SideAngle, Alpha, InnerOuter, EHeightPosition::Bottom);
            }

            const int32 VertexIndex = GetOrAddVertex(Position, Normal, UV);
            SideVertexGrid[s][i] = VertexIndex;

            // 4. 记录连接环
            if (i == 0) // Top Cap Edge
            {
                CapRingTop.Add(VertexIndex);
            }
            else if (i == Segments) // Top Wall Edge (与 CapRingTop 之间是 Top Bevel)
            {
                WallRingTop.Add(VertexIndex);
            }
            else if (i == Segments + 1) // Bottom Wall Edge (与 WallRingTop 之间是 Wall)
            {
                WallRingBottom.Add(VertexIndex);
            }
            else if (i == TotalVerticalPoints - 1) // Bottom Cap Edge (与 WallRingBottom 之间是 Bottom Bevel)
            {
                CapRingBottom.Add(VertexIndex);
            }
        }
    }

    // 5. 生成四边形面
    const int32 VerticalSteps = TotalVerticalPoints - 1;
    for (int32 s = 0; s < Sides; ++s)
    {
        for (int32 i = 0; i < VerticalSteps; ++i)
        {
            const int32 V00 = SideVertexGrid[s][i];     // (s, i)
            const int32 V10 = SideVertexGrid[s][i + 1];   // (s, i+1)
            const int32 V01 = SideVertexGrid[s + 1][i];   // (s+1, i)
            const int32 V11 = SideVertexGrid[s + 1][i + 1]; // (s+1, i+1)

            if (InnerOuter == EInnerOuter::Outer)
            {
                // 外壁：顺时针顺序 (从外向里看)
                AddQuad(V00, V01, V11, V10);
            }
            else
            {
                // 内壁：逆时针顺序 (从外向里看)
                AddQuad(V00, V10, V11, V01);
            }
        }
    }
}


/**
 * @brief 使用提供的 Cap 环生成盖子三角形。
 */
void FHollowPrismBuilder::GenerateCapTriangles(const TArray<int32>& InnerVertices, const TArray<int32>& OuterVertices, EHeightPosition HeightPosition)
{
    // 确保环有效
    if (InnerVertices.Num() == 0 || OuterVertices.Num() == 0) return;

    const int32 MaxSides = FMath::Max(HollowPrism.InnerSides, HollowPrism.OuterSides);
    if (MaxSides == 0) return;

    for (int32 i = 0; i < MaxSides; ++i)
    {
        // 算法保持不变，但在顶点选择上进行插值
        const int32 InnerV1 = FMath::RoundToInt(static_cast<float>(i) / MaxSides * HollowPrism.InnerSides);
        const int32 OuterV1 = FMath::RoundToInt(static_cast<float>(i) / MaxSides * HollowPrism.OuterSides);
        const int32 InnerV2 = FMath::RoundToInt(static_cast<float>(i + 1) / MaxSides * HollowPrism.InnerSides);
        const int32 OuterV2 = FMath::RoundToInt(static_cast<float>(i + 1) / MaxSides * HollowPrism.OuterSides);

        // 确保索引在有效范围内 (因为 Ring 数组是 Sides+1)
        if (InnerV1 >= InnerVertices.Num() || InnerV2 >= InnerVertices.Num() ||
            OuterV1 >= OuterVertices.Num() || OuterV2 >= OuterVertices.Num())
        {
            continue;
        }

        if (HeightPosition == EHeightPosition::Top)
        {
            // 顶盖：逆时针 (从上往下看)
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV2], OuterVertices[OuterV1]);
            AddTriangle(InnerVertices[InnerV1], InnerVertices[InnerV2], OuterVertices[OuterV2]);
        }
        else // 底盖
        {
            // 底盖：顺时针 (从上往下看，从下往上看为逆时针)
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV1], OuterVertices[OuterV2]);
            AddTriangle(InnerVertices[InnerV1], OuterVertices[OuterV2], InnerVertices[InnerV2]);
        }
    }
}


void FHollowPrismBuilder::GenerateEndCapColumn(float Angle, const FVector& Normal, TArray<int32>& OutOrderedVertices, EEndCapType EndCapType)
{
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float BevelRadius = HollowPrism.BevelRadius;

    OutOrderedVertices.Empty();

    // 1. 无倒角情况 (与 GenerateWalls 对应)
    if (!bEnableBevel)
    {
        const FVector OuterTop = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, HalfHeight);
        const FVector InnerTop = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, HalfHeight);
        const FVector OuterBottom = CalculateVertexPosition(HollowPrism.OuterRadius, Angle, -HalfHeight);
        const FVector InnerBottom = CalculateVertexPosition(HollowPrism.InnerRadius, Angle, -HalfHeight);

        const FVector2D OuterTopUV = CalculateEndCapUVWithRadius(Angle, HalfHeight, HollowPrism.OuterRadius, EndCapType);
        const FVector2D InnerTopUV = CalculateEndCapUVWithRadius(Angle, HalfHeight, HollowPrism.InnerRadius, EndCapType);
        const FVector2D OuterBottomUV = CalculateEndCapUVWithRadius(Angle, -HalfHeight, HollowPrism.OuterRadius, EndCapType);
        const FVector2D InnerBottomUV = CalculateEndCapUVWithRadius(Angle, -HalfHeight, HollowPrism.InnerRadius, EndCapType);

        OutOrderedVertices.Add(GetOrAddVertex(OuterTop, Normal, OuterTopUV));
        OutOrderedVertices.Add(GetOrAddVertex(InnerTop, Normal, InnerTopUV));
        OutOrderedVertices.Add(GetOrAddVertex(OuterBottom, Normal, OuterBottomUV));
        OutOrderedVertices.Add(GetOrAddVertex(InnerBottom, Normal, InnerBottomUV));
        return;
    }

    // 2. 有倒角情况 (与 GenerateSideAndBevelGeometry 统一)
    const int32 Segments = BevelSegments;
    // 总垂直点数: 2 * Segments + 2 (0到2*Segments+1)
    const int32 TotalVerticalPoints = 2 * Segments + 2;

    // 总共需要 2 * TotalVerticalPoints 个顶点 (Outer + Inner)
    OutOrderedVertices.Reserve(2 * TotalVerticalPoints);

    // 倒角圆弧的中心坐标
    const float CenterRadius_Inner = HollowPrism.InnerRadius + BevelRadius;
    const float CenterRadius_Outer = HollowPrism.OuterRadius - BevelRadius;

    for (int32 i = 0; i < TotalVerticalPoints; ++i)
    {
        float CurrentZ, CurrentRadius_Inner, CurrentRadius_Outer;

        // =======================================================
        // A. 顶部倒角部分 (i=0 到 Segments)
        // =======================================================
        if (i <= Segments)
        {
            // Alpha 从 0.0 (Cap Edge) 到 1.0 (Wall Edge)
            const float Alpha = static_cast<float>(i) / Segments;
            const float Theta = Alpha * HALF_PI; // Theta 从 0 (Cap) 到 90 (Wall)

            // Top Bevel Arc Center: (Z: HalfHeight - R_bevel)
            const float CenterZ = HalfHeight - BevelRadius;

            // Z 坐标
            CurrentZ = CenterZ + BevelRadius * FMath::Cos(Theta);

            // R 坐标
            CurrentRadius_Inner = CenterRadius_Inner - BevelRadius * FMath::Sin(Theta);
            CurrentRadius_Outer = CenterRadius_Outer + BevelRadius * FMath::Sin(Theta);
        }
        // =======================================================
        // B. 中间墙体部分 (i=Segments+1)
        // =======================================================
        else if (i == Segments + 1)
        {
            CurrentZ = -HalfHeight + BevelRadius;
            CurrentRadius_Inner = HollowPrism.InnerRadius;
            CurrentRadius_Outer = HollowPrism.OuterRadius;
        }
        // =======================================================
        // C. 底部倒角部分 (i=Segments+2 到 2*Segments+1)
        // =======================================================
        else // i >= Segments + 2 (Bottom Bevel)
        {
            // BevelStep: 1 to Segments (1 is Wall Edge, Segments is Cap Edge)
            const int32 BevelStep = i - (Segments + 1);
            // Alpha 从 0.0 (Wall Edge) 到 1.0 (Cap Edge)
            const float Alpha = static_cast<float>(BevelStep) / Segments;
            const float Theta = Alpha * HALF_PI; // Theta 从 0 (Wall) 到 90 (Cap)

            // Bottom Bevel Arc Center: (Z: -HalfHeight + R_bevel)
            const float CenterZ = -HalfHeight + BevelRadius;

            // Z 坐标
            CurrentZ = CenterZ - BevelRadius * FMath::Cos(Theta);

            // R 坐标
            CurrentRadius_Inner = CenterRadius_Inner - BevelRadius * FMath::Sin(Theta);
            CurrentRadius_Outer = CenterRadius_Outer + BevelRadius * FMath::Sin(Theta);
        }

        // 生成顶点 (端盖法线)
        const FVector OuterPos = CalculateVertexPosition(CurrentRadius_Outer, Angle, CurrentZ);
        const FVector InnerPos = CalculateVertexPosition(CurrentRadius_Inner, Angle, CurrentZ);

        // 注意：端盖UV与侧面UV计算不同
        const FVector2D OuterUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Outer, EndCapType);
        const FVector2D InnerUV = CalculateEndCapUVWithRadius(Angle, CurrentZ, CurrentRadius_Inner, EndCapType);

        OutOrderedVertices.Add(GetOrAddVertex(OuterPos, Normal, OuterUV));
        OutOrderedVertices.Add(GetOrAddVertex(InnerPos, Normal, InnerUV));
    }
}


void FHollowPrismBuilder::GenerateEndCapTriangles(const TArray<int32>& OrderedVertices, EEndCapType EndCapType)
{
    // OrderedVertices 是 [Outer0, Inner0, Outer1, Inner1, ...] 顺序
    // 段数是 (顶点数 / 2) - 1
    const int32 NumProfileSegments = (OrderedVertices.Num() / 2) - 1;

    for (int32 i = 0; i < NumProfileSegments; ++i)
    {
        const int32 V_Outer_Curr = OrderedVertices[i * 2];
        const int32 V_Inner_Curr = OrderedVertices[i * 2 + 1];
        const int32 V_Outer_Next = OrderedVertices[(i + 1) * 2];
        const int32 V_Inner_Next = OrderedVertices[(i + 1) * 2 + 1];

        if (EndCapType == EEndCapType::Start)
        {
            // 起始端盖，法线朝外，逆时针
            AddQuad(V_Outer_Curr, V_Outer_Next, V_Inner_Next, V_Inner_Curr);
        }
        else
        {
            // 结束端盖，法线朝外，顺时针
            AddQuad(V_Outer_Curr, V_Inner_Curr, V_Inner_Next, V_Outer_Next);
        }
    }
}


void FHollowPrismBuilder::GenerateEndCapWithBevel(EEndCapType EndCapType)
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float Angle = (EndCapType == EEndCapType::Start) ? -ArcAngleRadians / 2.0f : ArcAngleRadians / 2.0f;
    // 计算端盖法线
    const FVector Normal = (EndCapType == EEndCapType::Start) ?
        FVector(FMath::Sin(Angle), -FMath::Cos(Angle), 0.0f).GetSafeNormal() :
        FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0f).GetSafeNormal();

    TArray<int32> OrderedVertices;
    GenerateEndCapColumn(Angle, Normal, OrderedVertices, EndCapType);
    GenerateEndCapTriangles(OrderedVertices, EndCapType);
}

// =======================================================
// ~ Begin Utility Functions
// =======================================================

float FHollowPrismBuilder::CalculateStartAngle() const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    return -ArcAngleRadians / 2.0f;
}

float FHollowPrismBuilder::CalculateAngleStep(int32 Sides) const
{
    if (Sides == 0) return 0.0f;
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    return ArcAngleRadians / Sides;
}

FVector FHollowPrismBuilder::CalculateVertexPosition(float Radius, float Angle, float Z) const
{
    // 将原点移动到最底面（底部Z=0）：所有顶点向上偏移HalfHeight
    const float HalfHeight = HollowPrism.GetHalfHeight();
    return FVector(
        Radius * FMath::Cos(Angle),
        Radius * FMath::Sin(Angle),
        Z + HalfHeight
    );
}

// =======================================================
// ~ Begin UV Functions
// =======================================================

FVector2D FHollowPrismBuilder::CalculateWallUV(float Angle, float Z, EInnerOuter InnerOuter) const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();
    const float HalfHeight = HollowPrism.GetHalfHeight();

    float BaseU = 0.0f;
    if (ArcAngleRadians > KINDA_SMALL_NUMBER)
    {
        const float AngleRange = (Angle - StartAngle) / ArcAngleRadians;
        BaseU = FMath::Clamp(AngleRange, 0.0f, 1.0f);
    }
    else
    {
        BaseU = 0.5f;
    }

    const float TotalHeight = 2.0f * HalfHeight;
    const float BevelHeight = bEnableBevel ? HollowPrism.BevelRadius : 0.0f;
    const float WallHeight = TotalHeight - 2.0f * BevelHeight;
    const float OuterCircumference = 2.0f * PI * HollowPrism.OuterRadius;
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;

    // UV 分配比例
    const float WallVScale = WallHeight / OuterCircumference;
    const float BevelVScale = BevelHeight / OuterCircumference;
    const float CapVScale = RadiusRange / OuterCircumference;

    // V 坐标分段偏移
    const float OuterWallVOffset = 0.0f;
    const float InnerWallVOffset = OuterWallVOffset + WallVScale + 2.0f * BevelVScale + 2.0f * CapVScale;

    float V_Start, V_End;
    if (InnerOuter == EInnerOuter::Outer)
    {
        V_Start = OuterWallVOffset;
        V_End = OuterWallVOffset + WallVScale;
    }
    else
    {
        V_Start = InnerWallVOffset;
        V_End = InnerWallVOffset + WallVScale;
    }

    // Z 轴的 UV 映射
    const float ActualWallHeight = WallHeight;
    const float ActualWallBottom = -HalfHeight + BevelHeight; // -HalfHeight + R_bevel

    float V = 0.5f;
    if (ActualWallHeight > KINDA_SMALL_NUMBER)
    {
        // Wall Bottom (-HalfHeight + R_bevel) 是 V=0.0
        // Wall Top (HalfHeight - R_bevel) 是 V=1.0
        const float HeightRatio = (Z - ActualWallBottom) / ActualWallHeight;
        V = HeightRatio;
    }

    const float ActualV = V_Start + FMath::Clamp(V, 0.0f, 1.0f) * (V_End - V_Start);

    return FVector2D(BaseU, ActualV);
}

FVector2D FHollowPrismBuilder::CalculateCapUV(float Angle, float Radius, EHeightPosition HeightPosition) const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();

    // U坐标：沿着角度方向，从0到1
    const float U = (ArcAngleRadians > KINDA_SMALL_NUMBER) ?
        ((Angle - StartAngle) / ArcAngleRadians) : 0.5f;
    const float NormalizedU = FMath::Clamp(U, 0.0f, 1.0f);

    // V坐标：沿着径向方向，从内半径到外半径
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    const float V = (RadiusRange > KINDA_SMALL_NUMBER) ?
        ((Radius - HollowPrism.InnerRadius) / RadiusRange) : 0.5f;
    const float NormalizedV = FMath::Clamp(V, 0.0f, 1.0f);

    // 计算 V 坐标偏移
    const float TotalHeight = 2.0f * HollowPrism.GetHalfHeight();
    const float BevelHeight = bEnableBevel ? HollowPrism.BevelRadius : 0.0f;
    const float WallHeight = TotalHeight - 2.0f * BevelHeight;
    const float OuterCircumference = 2.0f * PI * HollowPrism.OuterRadius;
    const float CapRadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;

    const float WallVScale = WallHeight / OuterCircumference;
    const float BevelVScale = BevelHeight / OuterCircumference;
    const float CapVScale = CapRadiusRange / OuterCircumference;

    // V 坐标分段偏移
    const float OuterWallVOffset = 0.0f;
    const float OuterTopBevelVOffset = OuterWallVOffset + WallVScale;
    const float TopCapVOffset = OuterTopBevelVOffset + BevelVScale;
    const float InnerTopBevelVOffset = TopCapVOffset + CapVScale;
    const float InnerWallVOffset = InnerTopBevelVOffset + BevelVScale;
    const float InnerBottomBevelVOffset = InnerWallVOffset + WallVScale;
    const float BottomCapVOffset = InnerBottomBevelVOffset + BevelVScale;

    float V_Start, V_End;
    if (HeightPosition == EHeightPosition::Top)
    {
        // 顶盖 (内到外)
        V_Start = TopCapVOffset;
        V_End = TopCapVOffset + CapVScale;
    }
    else
    {
        // 底盖 (内到外)
        V_Start = BottomCapVOffset;
        V_End = BottomCapVOffset + CapVScale;
    }

    const float ActualV = V_Start + NormalizedV * (V_End - V_Start);

    return FVector2D(NormalizedU, ActualV);
}

FVector2D FHollowPrismBuilder::CalculateBevelUV(float Angle, float Alpha, EInnerOuter InnerOuter, EHeightPosition HeightPosition) const
{
    // Alpha is 0.0 (Cap Edge) to 1.0 (Wall Edge) for Top Bevel
    // Alpha is 0.0 (Wall Edge) to 1.0 (Cap Edge) for Bottom Bevel

    const float ArcAngleRadians = FMath::DegreesToRadians(HollowPrism.ArcAngle);
    const float StartAngle = CalculateStartAngle();

    float NormalizedAngle = (ArcAngleRadians > KINDA_SMALL_NUMBER) ?
        ((Angle - StartAngle) / ArcAngleRadians) : 0.5f;
    NormalizedAngle = FMath::Clamp(NormalizedAngle, 0.0f, 1.0f);

    // 计算 V 坐标偏移
    const float TotalHeight = 2.0f * HollowPrism.GetHalfHeight();
    const float BevelHeight = bEnableBevel ? HollowPrism.BevelRadius : 0.0f;
    const float WallHeight = TotalHeight - 2.0f * BevelHeight;
    const float OuterCircumference = 2.0f * PI * HollowPrism.OuterRadius;
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;

    const float BevelVScale = BevelHeight / OuterCircumference;
    const float WallVScale = WallHeight / OuterCircumference;
    const float CapVScale = RadiusRange / OuterCircumference;

    // V 坐标分段偏移
    const float OuterWallVOffset = 0.0f;
    const float OuterTopBevelVOffset = OuterWallVOffset + WallVScale;
    const float TopCapVOffset = OuterTopBevelVOffset + BevelVScale;
    const float InnerTopBevelVOffset = TopCapVOffset + CapVScale;
    const float InnerWallVOffset = InnerTopBevelVOffset + BevelVScale;
    const float InnerBottomBevelVOffset = InnerWallVOffset + WallVScale;
    const float BottomCapVOffset = InnerBottomBevelVOffset + BevelVScale;
    const float OuterBottomBevelVOffset = BottomCapVOffset + CapVScale;

    float V_Start, V_End;
    if (HeightPosition == EHeightPosition::Top)
    {
        if (InnerOuter == EInnerOuter::Outer)
        {
            // Outer Top: V 向上 (从 Cap 到 Wall)
            V_Start = OuterTopBevelVOffset;         // Cap Edge
            V_End = OuterTopBevelVOffset + BevelVScale;  // Wall Edge
        }
        else
        {
            // Inner Top: V 向上 (从 Cap 到 Wall)
            V_Start = InnerTopBevelVOffset;         // Cap Edge
            V_End = InnerTopBevelVOffset + BevelVScale;  // Wall Edge
        }
    }
    else
    {
        if (InnerOuter == EInnerOuter::Outer)
        {
            // Outer Bottom: V 向上 (从 Wall 到 Cap)
            // 注意: Alpha 传入的是 Wall Edge(0.0) 到 Cap Edge(1.0)
            V_Start = OuterBottomBevelVOffset + BevelVScale; // Wall Edge
            V_End = OuterBottomBevelVOffset;         // Cap Edge
        }
        else
        {
            // Inner Bottom: V 向上 (从 Wall 到 Cap)
            // 注意: Alpha 传入的是 Wall Edge(0.0) 到 Cap Edge(1.0)
            V_Start = InnerBottomBevelVOffset + BevelVScale; // Wall Edge
            V_End = InnerBottomBevelVOffset;         // Cap Edge
        }
    }

    const float U = NormalizedAngle;
    // Lerp from V_Start (Alpha=0) to V_End (Alpha=1)
    const float V = V_Start + FMath::Clamp(Alpha, 0.0f, 1.0f) * (V_End - V_Start);

    return FVector2D(U, V);
}

FVector2D FHollowPrismBuilder::CalculateEndCapUVWithRadius(float Angle, float Z, float Radius, EEndCapType EndCapType) const
{
    // 端盖 UV 映射逻辑与侧面/盖子 UV 逻辑是独立的

    // U 坐标：基于 Z 高度 (从底到顶)
    const float HalfHeight = HollowPrism.GetHalfHeight();
    const float HeightRatio = FMath::Clamp((Z + HalfHeight) / (2.0f * HalfHeight), 0.0f, 1.0f);

    // V 坐标：基于径向距离 (从内到外)
    const float RadiusRange = HollowPrism.OuterRadius - HollowPrism.InnerRadius;
    const float RadiusRatio = (Radius - HollowPrism.InnerRadius) / FMath::Max(RadiusRange, 0.001f);
    const float RadiusOffsetClamped = FMath::Clamp(RadiusRatio, 0.0f, 1.0f);

    float BaseU = HeightRatio * 0.2f;

    // V 坐标偏移，将端盖放在 UV 空间的一个特定角落
    const float EndCapU = FMath::Clamp(BaseU, 0.0f, 1.0f);
    const float EndCapV = (EndCapType == EEndCapType::Start) ?
        0.7f + RadiusOffsetClamped * 0.15f :
        0.85f + RadiusOffsetClamped * 0.15f;

    return FVector2D(EndCapU, EndCapV);
}