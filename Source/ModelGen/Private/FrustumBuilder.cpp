// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"

#include "ModelGenMeshData.h"

FFrustumBuilder::FFrustumBuilder(const FFrustumParameters& InParams) : Params(InParams)
{
    // 计算并设置角度相关的成员变量
    CalculateAngles();
}

void FFrustumBuilder::Clear()
{
    // 调用基类的Clear方法
    FModelGenMeshBuilder::Clear();
    
    // 清除所有连接点
    ClearEndCapConnectionPoints();
}

bool FFrustumBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!ValidateParameters())
    {
        UE_LOG(LogTemp, Error, TEXT("FFrustumBuilder::Generate - Parameters validation failed"));
        return false;
    }

    // 清除现有数据
    Clear();

    // 预分配内存
    ReserveMemory();

    // 生成基础几何体
    GenerateBaseGeometry();

    // 验证生成的数据
    if (!ValidateGeneratedData())
    {
        UE_LOG(LogTemp, Error, TEXT("FFrustumBuilder::Generate - Generated data validation failed"));
        return false;
    }

    // 输出网格数据
    OutMeshData = MeshData;

    return true;
}

bool FFrustumBuilder::ValidateParameters() const
{
    return Params.IsValid();
}

int32 FFrustumBuilder::CalculateVertexCountEstimate() const
{
    return Params.CalculateVertexCountEstimate();
}

int32 FFrustumBuilder::CalculateTriangleCountEstimate() const
{
    return Params.CalculateTriangleCountEstimate();
}

void FFrustumBuilder::GenerateBaseGeometry()
{
	CreateSideGeometry();
    GenerateTopGeometry();
	GenerateTopBevelGeometry();
	GenerateBottomBevelGeometry();
    GenerateBottomGeometry();

	GenerateEndCaps();
}

void FFrustumBuilder::CreateSideGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();

    // 确保顶部环的位置与倒角底部完全一致
    const float TopBevelStartZ = HalfHeight - CalculateBevelHeight(Params.TopRadius);
    const float BottomBevelStartZ = -HalfHeight + CalculateBevelHeight(Params.BottomRadius);
    
    TArray<int32> TopRing = GenerateVertexRing(
        Params.TopRadius, 
        TopBevelStartZ, 
        Params.TopSides, 
        0.0f
    );
    
    TArray<int32> BottomRing = GenerateVertexRing(
        Params.BottomRadius, 
        BottomBevelStartZ, 
        Params.BottomSides, 
        1.0f
    );

    SideTopRing = TopRing;
    SideBottomRing = BottomRing;

    TArray<int32> TopRingOrigin = GenerateVertexRing(Params.TopRadius, HalfHeight, Params.TopSides, 0.0f);
    TArray<int32> BottomRingOrigin = GenerateVertexRing(Params.BottomRadius, -HalfHeight, Params.BottomSides, 1.0f);

    TArray<int32> BottomToTopMapping;
    BottomToTopMapping.SetNum(BottomRingOrigin.Num());
    for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
    {
        const float BottomRatio = static_cast<float>(BottomIndex) / BottomRingOrigin.Num();
        const int32 TopIndex = FMath::Clamp(FMath::RoundToInt(BottomRatio * TopRingOrigin.Num()), 0, TopRingOrigin.Num() - 1);
        BottomToTopMapping[BottomIndex] = TopIndex;
    }

    TArray<TArray<int32>> VertexRings;
    VertexRings.Add(BottomRing);

    // 生成中间环
    if (Params.HeightSegments > 1)
    {
        const float HeightStep = Params.Height / Params.HeightSegments;

        for (int32 h = Params.HeightSegments - 1; h > 0; --h)
        {
            const float CurrentHeight = HalfHeight - h * HeightStep;
            const float HeightRatio = static_cast<float>(Params.HeightSegments - h) / Params.HeightSegments;

            TArray<int32> CurrentRing;

            for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
            {
                const int32 TopIndex = BottomToTopMapping[BottomIndex];

                const float XR = FMath::Lerp(
                    GetPosByIndex(BottomRingOrigin[BottomIndex]).X, GetPosByIndex(TopRingOrigin[TopIndex]).X, HeightRatio);
                const float YR = FMath::Lerp(
                    GetPosByIndex(BottomRingOrigin[BottomIndex]).Y, GetPosByIndex(TopRingOrigin[TopIndex]).Y, HeightRatio);

                const float BaseRadius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, HeightRatio);
                const float BentRadius = CalculateBentRadius(BaseRadius, HeightRatio);

                const float Scale = BentRadius / BaseRadius;
                const float X = XR * Scale;
                const float Y = YR * Scale;

                const FVector InterpolatedPos(X, Y, CurrentHeight);

                FVector Normal = FVector(X, Y, 0.0f).GetSafeNormal();
                if (Normal.IsNearlyZero())
                {
                    Normal = FVector(1.0f, 0.0f, 0.0f);
                }
                
                // 对于弯曲的侧面，调整法线方向以考虑弯曲效果
                if (Params.BendAmount > KINDA_SMALL_NUMBER)
                {
                    // 计算弯曲后的法线方向
                    const float BendFactor = FMath::Sin(HeightRatio * PI);
                    const float NormalZ = -Params.BendAmount * FMath::Cos(HeightRatio * PI);
                    Normal = (Normal + FVector(0, 0, NormalZ)).GetSafeNormal();
                }

                // 使用稳定的UV映射
                const FVector2D UV = GenerateStableUV(InterpolatedPos, Normal);

                const int32 VertexIndex = GetOrAddVertex(InterpolatedPos, Normal, UV);
                CurrentRing.Add(VertexIndex);
            }

            VertexRings.Add(CurrentRing);
        }
    }
    VertexRings.Add(TopRing);

    for (int32 i = VertexRings.Num() - 1; i >= 0; i--)
    {
        // 记录中间环起始点的顶点索引到侧面连接点
        RecordEndCapConnectionPoint(VertexRings[i][0]);
    }

    // 连接三角形 - 确保只有外面可见
    for (int i = 0; i < VertexRings.Num() - 1; i++)
    {
        TArray<int32> CurrentRing = VertexRings[i];
        TArray<int32> NextRing = VertexRings[i + 1];

        for (int32 CurrentIndex = 0; CurrentIndex < CurrentRing.Num(); ++CurrentIndex)
        {
            const int32 NextCurrentIndex =
                (Params.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? (CurrentIndex + 1) % CurrentRing.Num() : (CurrentIndex + 1);

            if (NextCurrentIndex >= CurrentRing.Num())
                continue;

            const float CurrentRatio = static_cast<float>(CurrentIndex) / CurrentRing.Num();
            const float NextCurrentRatio = static_cast<float>(NextCurrentIndex) / CurrentRing.Num();

            const int32 NextRingIndex = FMath::Clamp(FMath::RoundToInt(CurrentRatio * NextRing.Num()), 0, NextRing.Num() - 1);
            const int32 NextRingNextIndex =
                FMath::Clamp(FMath::RoundToInt(NextCurrentRatio * NextRing.Num()), 0, NextRing.Num() - 1);

            AddQuad(CurrentRing[CurrentIndex], NextRing[NextRingIndex], NextRing[NextRingNextIndex], CurrentRing[NextCurrentIndex]);
        }
    }
}

void FFrustumBuilder::GenerateTopGeometry()
{
    GenerateCapGeometry(Params.GetHalfHeight(), Params.TopSides, Params.TopRadius, true);
}

void FFrustumBuilder::GenerateBottomGeometry()
{
    GenerateCapGeometry(-Params.GetHalfHeight(), Params.BottomSides, Params.BottomRadius, false);
}

void FFrustumBuilder::GenerateTopBevelGeometry()
{
    if (Params.BevelRadius <= 0.0f)
    {
        return;
    }

    GenerateBevelGeometry(true);
}

void FFrustumBuilder::GenerateBottomBevelGeometry()
{
    if (Params.BevelRadius <= 0.0f)
    {
        return;
    }

    GenerateBevelGeometry(false);
}

void FFrustumBuilder::GenerateEndCaps()
{
    if (Params.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
    {
        return;
    }
    
    // 使用成员变量，避免重复计算
    // StartAngle 和 EndAngle 已在构造函数中计算好

    // 生成起始端面和结束端面，参考HollowPrism的实现方式
    // 起始端盖：位于StartAngle位置，使用固定法线方向
    GenerateEndCap(StartAngle, FVector(-1, 0, 0), true);   
    // 结束端盖：位于EndAngle位置，使用固定法线方向
    GenerateEndCap(EndAngle, FVector(1, 0, 0), false);
}

void FFrustumBuilder::GenerateEndCap(float Angle, const FVector& Normal, bool IsStart)
{
    if (EndCapConnectionPoints.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateEndCap - %s端面连接点不足，无法生成端面"), IsStart ? TEXT("起始") : TEXT("结束"));
        return;
    }

    TArray<int32> RotatedConnectionPoints;
    RotatedConnectionPoints.Reserve(EndCapConnectionPoints.Num());

    // 参考HollowPrism的实现方式，但为终点端盖添加位置旋转
    for (int32 VertexIndex : EndCapConnectionPoints)
    {
        FVector OriginalPos = GetPosByIndex(VertexIndex);
        
        FVector EndCapPos;
        if (IsStart)
        {
            // 起始端盖：直接使用原始位置，因为StartAngle已经通过侧面几何体确定
            EndCapPos = OriginalPos;
        }
        else
        {
            // 终点端盖：需要旋转到EndAngle位置
            // 计算从当前位置到EndAngle的旋转
            const float Radius = FMath::Sqrt(OriginalPos.X * OriginalPos.X + OriginalPos.Y * OriginalPos.Y);
            const float CurrentAngle = FMath::Atan2(OriginalPos.Y, OriginalPos.X);
            
            // 计算需要旋转的角度：从StartAngle位置旋转到EndAngle位置
            const float RotationAngle = EndAngle - StartAngle;
            const float NewAngle = CurrentAngle + RotationAngle;
            
            const float NewX = Radius * FMath::Cos(NewAngle);
            const float NewY = Radius * FMath::Sin(NewAngle);
            EndCapPos = FVector(NewX, NewY, OriginalPos.Z);
        }
        
        // 使用传入的固定法线方向（参考HollowPrism）
        FVector EndCapNormal = Normal;
        
        // 对于弯曲的端盖，调整法线方向
        if (Params.BendAmount > KINDA_SMALL_NUMBER)
        {
            // 计算端盖在弯曲后的法线方向
            const float Z = EndCapPos.Z;
            const float HeightRatio = (Z + Params.GetHalfHeight()) / Params.Height;
            const float BendInfluence = FMath::Sin(HeightRatio * PI);
            
            // 混合端盖法线和弯曲法线
            FVector BendNormal = FVector(0, 0, -BendInfluence).GetSafeNormal();
            EndCapNormal = (EndCapNormal + BendNormal * Params.BendAmount).GetSafeNormal();
        }
        
        // 使用稳定的UV映射
        FVector2D UV = GenerateStableUV(EndCapPos, EndCapNormal);
        int32 NewVertexIndex = GetOrAddVertex(EndCapPos, EndCapNormal, UV);
        RotatedConnectionPoints.Add(NewVertexIndex);
    }

    GenerateEndCapTrianglesFromVertices(RotatedConnectionPoints, IsStart, Angle);
}

TArray<int32> FFrustumBuilder::GenerateVertexRing(float Radius, float Z, int32 Sides, float UVV)
{
    TArray<int32> VertexRing;
    const float AngleStep = CalculateAngleStep(Sides);

    const int32 VertexCount = (Params.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides;

    for (int32 i = 0; i < VertexCount; ++i)
    {
        // 使用成员变量，避免重复计算
        // 端盖从StartAngle开始到EndAngle结束，侧边也应该使用相同的角度范围
        const float Angle = StartAngle + (i * AngleStep);  // 从StartAngle开始，按步长递增
        
        const float X = Radius * FMath::Cos(Angle);
        const float Y = Radius * FMath::Sin(Angle);
        const FVector Pos(X, Y, Z);
        FVector Normal = FVector(X, Y, 0.0f).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector(1.0f, 0.0f, 0.0f);
        }

        // 使用稳定的UV映射
        const FVector2D UV = GenerateStableUV(Pos, Normal);
        const int32 VertexIndex = GetOrAddVertex(Pos, Normal, UV);
        VertexRing.Add(VertexIndex);
    }

    return VertexRing;
}

void FFrustumBuilder::GenerateCapGeometry(float Z, int32 Sides, float Radius, bool bIsTop)
{
    FVector Normal(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);

    const FVector CenterPos(0.0f, 0.0f, Z);
    const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, FVector2D(0.5f, 0.5f));

    if (bIsTop)
    {
        //RecordEndCapConnectionPoint(CenterVertex);
    }

    const float AngleStep = CalculateAngleStep(Sides);

    for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
    {
        // 使用成员变量，避免重复计算
        const float CurrentAngle = StartAngle + (SideIndex * AngleStep);  // 从StartAngle开始，按步长递增
        const float NextAngle = StartAngle + ((SideIndex + 1) * AngleStep);  // 下一个角度

        const float CurrentRadius = Radius - Params.BevelRadius;

        const FVector CurrentPos(CurrentRadius * FMath::Cos(CurrentAngle), CurrentRadius * FMath::Sin(CurrentAngle), Z);
        const FVector NextPos(CurrentRadius * FMath::Cos(NextAngle), CurrentRadius * FMath::Sin(NextAngle), Z);

        // 对于有倒角的顶底面，调整法线方向
        FVector AdjustedNormal = Normal;
        if (Params.BevelRadius > KINDA_SMALL_NUMBER)
        {
            // 计算倒角边缘的法线方向，考虑从顶底面到侧面的过渡
            const float EdgeDistance = (Radius - CurrentRadius) / Params.BevelRadius;
            const float NormalBlend = FMath::Clamp(EdgeDistance, 0.0f, 1.0f);
            
            // 混合顶底面法线和侧面法线
            FVector SideNormal = FVector(FMath::Cos(CurrentAngle), FMath::Sin(CurrentAngle), 0.0f);
            AdjustedNormal = FMath::Lerp(Normal, SideNormal, NormalBlend).GetSafeNormal();
        }

        // 使用稳定的UV映射
        const FVector2D UV1 = GenerateStableUV(CurrentPos, AdjustedNormal);
        const FVector2D UV2 = GenerateStableUV(NextPos, AdjustedNormal);

        const int32 V1 = GetOrAddVertex(CurrentPos, AdjustedNormal, UV1);
        const int32 V2 = GetOrAddVertex(NextPos, AdjustedNormal, UV2);

        if (SideIndex == 0)
        {
            // 端盖边缘顶点应该使用端盖法线，而不是顶底面法线
            // 这里记录的是用于生成端盖的顶点，但它们的法线方向需要后续修正
            RecordEndCapConnectionPoint(V1);
        }

        if (bIsTop)
        {
            AddTriangle(CenterVertex, V2, V1);
        }
        else
        {
            AddTriangle(CenterVertex, V1, V2);
        }
    }
}

void FFrustumBuilder::GenerateBevelGeometry(bool bIsTop)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSections = Params.BevelSegments;
    
    if (BevelRadius <= 0.0f || BevelSections <= 0)
    {
        return;
    }

    const float Radius = bIsTop ? Params.TopRadius : Params.BottomRadius;
    const int32 Sides = bIsTop ? Params.TopSides : Params.BottomSides;
    const TArray<int32>& SideRing = bIsTop ? SideTopRing : SideBottomRing;
    
    // 修复上倒角位置计算：直接使用侧边环的实际位置，确保与侧边环完全一致
    const float StartZ = GetPosByIndex(SideRing[0]).Z;
    const float EndZ = bIsTop ? HalfHeight : -HalfHeight;

    TArray<int32> PrevRing;
    const float AngleStep = CalculateAngleStep(Sides);
    TArray<int32> StartRingForEndCap;

    for (int32 i = 0; i <= BevelSections; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSections;

        // 计算当前半径和高度
        const float Alpha_Height = (StartZ + HalfHeight) / Params.Height;
        const float RadiusAtZ = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Height);
        const float BentRadius = FMath::Max(RadiusAtZ, KINDA_SMALL_NUMBER);
        const float StartRadius = BentRadius;
        
        const float CapRadius = FMath::Max(0.0f, Radius - Params.BevelRadius);
        const float CurrentRadius = FMath::Lerp(StartRadius, CapRadius, alpha);
        const float CurrentZ = FMath::Lerp(StartZ, EndZ, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Sides + 1);

        const int32 VertexCount = (Params.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides;

        for (int32 s = 0; s <= VertexCount; ++s)
        {
            FVector Position;
            
            if (i == 0 && s < SideRing.Num())
            {
                // 第一个环：直接使用侧边环的顶点，确保完全一致
                // 这样修复了上倒角与侧边环的缝隙问题
                Position = GetPosByIndex(SideRing[s]);
            }
            else
            {
                // 后续环：计算新的位置
                // 使用成员变量，避免重复计算
                const float angle = StartAngle + (s * AngleStep);  // 从StartAngle开始，按步长递增
                Position = FVector(CurrentRadius * FMath::Cos(angle), CurrentRadius * FMath::Sin(angle), CurrentZ);
            }

            FVector Normal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
            if (Normal.IsNearlyZero())
            {
                Normal = FVector(1.0f, 0.0f, 0.0f);
            }
            
            // 对于倒角面，调整法线方向以考虑倒角效果
            if (Params.BevelRadius > KINDA_SMALL_NUMBER)
            {
                // 计算倒角面的法线方向，考虑从侧面到顶底面的过渡
                const float HeightRatio = (Position.Z + Params.GetHalfHeight()) / Params.Height;
                const float BevelInfluence = FMath::Clamp(HeightRatio, 0.0f, 1.0f);
                
                // 混合侧面法线和顶底面法线
                FVector SideNormal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
                FVector CapNormal = FVector(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);
                
                Normal = FMath::Lerp(SideNormal, CapNormal, BevelInfluence).GetSafeNormal();
            }
            
            // 使用稳定的UV映射
            const FVector2D UV = GenerateStableUV(Position, Normal);

            CurrentRing.Add(GetOrAddVertex(Position, Normal, UV));
        }

        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];

                if (bIsTop)
                {
                    AddQuad(V00, V10, V11, V01);
                }
                else
                {
                    AddQuad(V00, V01, V11, V10);
                }
            }
        }
        PrevRing = CurrentRing;
        StartRingForEndCap.Add(CurrentRing[0]);
    }

    if (bIsTop)
    {
        for (int i = StartRingForEndCap.Num() - 1; i >= 0; i--)
        {
            RecordEndCapConnectionPoint(StartRingForEndCap[i]);
        }
    }
    else
    {
        for (int i = 0; i < StartRingForEndCap.Num(); i++)
        {
            RecordEndCapConnectionPoint(StartRingForEndCap[i]);
        }
    }
}



float FFrustumBuilder::CalculateBentRadius(float BaseRadius, float HeightRatio)
{
    const float BendFactor = FMath::Sin(HeightRatio * PI);
    const float BentRadius = BaseRadius + Params.BendAmount * BendFactor * BaseRadius;

    if (Params.MinBendRadius > KINDA_SMALL_NUMBER)
    {
        return FMath::Max(BentRadius, Params.MinBendRadius);
    }

    return FMath::Max(BentRadius, KINDA_SMALL_NUMBER);
}

float FFrustumBuilder::CalculateBevelHeight(float Radius)
{
    return FMath::Min(Params.BevelRadius, Radius);
}

float FFrustumBuilder::CalculateHeightRatio(float Z)
{
    const float HalfHeight = Params.GetHalfHeight();
    return (Z + HalfHeight) / Params.Height;
}

float FFrustumBuilder::CalculateAngleStep(int32 Sides)
{
    // 使用成员变量，避免重复计算
    return ArcAngleRadians / Sides;
}



void FFrustumBuilder::GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, bool IsStart, float Angle)
{
    // 检查顶点数量
    if (OrderedVertices.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateEndCapTrianglesFromVertices - 顶点数量不足，无法生成面"));
        return;
    }

    const int32 VertexCount = OrderedVertices.Num();
    const float HalfHeight = Params.GetHalfHeight();

    // 按Z值从上往下排序顶点
    TArray<int32> SortedVertices = OrderedVertices;
    SortedVertices.Sort([this](const int32& A, const int32& B) -> bool
    {
        // 获取顶点位置
        FVector PosA = GetPosByIndex(A);
        FVector PosB = GetPosByIndex(B);
        
        // 按Z值从大到小排序（从上往下）
        return PosA.Z > PosB.Z;
    });

    if (VertexCount == 2)
    {
        const int32 V1 = SortedVertices[0];
        const int32 V2 = SortedVertices[1];
        
        FVector Pos1 = GetPosByIndex(V1);
        FVector Pos2 = GetPosByIndex(V2);
        
        // 端盖中心顶点：参考HollowPrism的方式，使用固定的法线方向
        FVector EndCapNormal;
        if (IsStart)
        {
            // 起始端盖：使用固定法线方向，指向-X方向
            EndCapNormal = FVector(-1, 0, 0);
        }
        else
        {
            // 结束端盖：使用固定法线方向，指向+X方向
            EndCapNormal = FVector(1, 0, 0);
        }
        const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), EndCapNormal, GenerateStableUV(FVector(0, 0, Pos1.Z), EndCapNormal));
        const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), EndCapNormal, GenerateStableUV(FVector(0, 0, Pos2.Z), EndCapNormal));
        
        if (IsStart)
        {
            AddTriangle(V1, V2, CenterV1);
            AddTriangle(V2, CenterV2, CenterV1);
        }
        else
        {
            AddTriangle(V2, V1, CenterV1);
            AddTriangle(CenterV1, CenterV2, V2);
        }
    }
    else
    {
        for (int32 i = 0; i < SortedVertices.Num() - 1; ++i)
        {
            const int32 V1 = SortedVertices[i];
            const int32 V2 = SortedVertices[i + 1];
            
            FVector Pos1 = GetPosByIndex(V1);
            FVector Pos2 = GetPosByIndex(V2);
            
            // 端盖中心顶点：参考HollowPrism的方式，使用固定的法线方向
            FVector EndCapNormal;
            if (IsStart)
            {
                // 起始端盖：使用固定法线方向，指向-X方向
                EndCapNormal = FVector(-1, 0, 0);
            }
            else
            {
                // 结束端盖：使用固定法线方向，指向+X方向
                EndCapNormal = FVector(1, 0, 0);
            }
            const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), EndCapNormal, GenerateStableUV(FVector(0, 0, Pos1.Z), EndCapNormal));
            const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), EndCapNormal, GenerateStableUV(FVector(0, 0, Pos2.Z), EndCapNormal));
            
            if (IsStart)
            {
                AddTriangle(V1, V2, CenterV1);
                AddTriangle(V2, CenterV2, CenterV1);
            }
            else
            {
                AddTriangle(V2, V1, CenterV1);
                AddTriangle(CenterV1, CenterV2, V2);
            }
        }
    }
}



// 端面连接点管理方法实现
void FFrustumBuilder::RecordEndCapConnectionPoint(int32 VertexIndex)
{
    EndCapConnectionPoints.Add(VertexIndex);
}

const TArray<int32>& FFrustumBuilder::GetEndCapConnectionPoints() const
{
    return EndCapConnectionPoints;
}

void FFrustumBuilder::ClearEndCapConnectionPoints()
{
    EndCapConnectionPoints.Empty();
}

// 基于顶点位置的稳定UV映射 - 重写父类实现
FVector2D FFrustumBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const
{
    // 基于顶点位置生成稳定的UV坐标，确保模型变化时UV不变
    float X = Position.X;
    float Y = Position.Y;
    float Z = Position.Z;
    
    // 计算角度（基于位置）
    float Angle = FMath::Atan2(Y, X);
    if (Angle < 0) Angle += 2.0f * PI;
    
    // 计算到中心的距离
    float DistanceFromCenter = FMath::Sqrt(X * X + Y * Y);
    
    // 根据法线方向选择UV映射策略（2U系统：0-2范围）
    if (FMath::Abs(Normal.Z) > 0.9f)
    {
        // 顶底面：使用角度映射
        float U = Angle / (2.0f * PI) * 2.0f;  // 0 到 2
        float V = (Z + Params.GetHalfHeight()) / Params.Height;  // 0 到 1
        
        // 根据半径判断是顶面还是底面
        if (Z > 0)
        {
            // 顶面：U从0.0到1.0，V使用0.5
            U = U * 0.5f;
            V = 0.5f;
        }
        else
        {
            // 底面：U从1.0到2.0，V使用1.0
            U = 1.0f + U * 0.5f;
            V = 1.0f;
        }
        
        return FVector2D(U, V);
    }
    else if (FMath::Abs(Normal.X) > 0.9f || FMath::Abs(Normal.Y) > 0.9f)
    {
        // 端盖：使用专门的UV空间，避免与其他面重叠（端盖使用角度方向法线）
        // 判断是否为端盖：X或Y方向法线分量较大
        if (FMath::Abs(Normal.X) > 0.9f)
        {
            // X方向法线分量较大
            if (Normal.X < 0)
            {
                // 起始端盖：法线指向角度方向，使用独立的UV空间
                float U = 0.0f + (Angle / (2.0f * PI)) * 0.25f;  // 0.0 到 0.25
                // 对于起始端盖，V值应该基于高度，但避免与顶底面重叠
                float V = 0.0f + (Z + Params.GetHalfHeight()) / Params.Height;  // 0.0 到 1.0
                // 调整V值，避免与顶底面重叠
                V = 0.1f + V * 0.8f;  // 0.1 到 0.9
                return FVector2D(U, V);
            }
            else
            {
                // 结束端盖：法线指向角度方向，使用独立的UV空间
                float U = 1.75f + (Angle / (2.0f * PI)) * 0.25f;  // 1.75 到 2.0
                // 对于结束端盖，V值应该基于高度，但避免与顶底面重叠
                float V = 0.0f + (Z + Params.GetHalfHeight()) / Params.Height;  // 0.0 到 1.0
                // 调整V值，避免与顶底面重叠
                V = 0.1f + V * 0.8f;  // 0.1 到 0.9
                return FVector2D(U, V);
            }
        }
        else
        {
            // Y方向法线分量较大
            if (Normal.Y < 0)
            {
                // 起始端盖：法线指向角度方向，使用独立的UV空间
                float U = 0.0f + (Angle / (2.0f * PI)) * 0.25f;  // 0.0 到 0.25
                // 对于起始端盖，V值应该基于高度，但避免与顶底面重叠
                float V = 0.0f + (Z + Params.GetHalfHeight()) / Params.Height;  // 0.0 到 1.0
                // 调整V值，避免与顶底面重叠
                V = 0.1f + V * 0.8f;  // 0.1 到 0.9
                return FVector2D(U, V);
            }
            else
            {
                // 结束端盖：法线指向角度方向，使用独立的UV空间
                float U = 1.75f + (Angle / (2.0f * PI)) * 0.25f;  // 1.75 到 2.0
                // 对于结束端盖，V值应该基于高度，但避免与顶底面重叠
                float V = 0.0f + (Z + Params.GetHalfHeight()) / Params.Height;  // 0.0 到 1.0
                // 调整V值，避免与顶底面重叠
                V = 0.1f + V * 0.8f;  // 0.1 到 0.9
                return FVector2D(U, V);
            }
        }
    }
    else
    {
        // 侧面：使用角度和高度映射
        float U = Angle / (2.0f * PI) * 2.0f;  // 0 到 2
        float V = (Z + Params.GetHalfHeight()) / Params.Height;  // 0 到 1
        
        // 侧面占据UV空间的中间区域，避免与顶底面和端盖重叠
        // 调整侧面UV范围，确保与端盖完全分离
        U = 0.25f + U * 1.5f;  // 0.25 到 1.75
        
        return FVector2D(U, V);
    }
}

void FFrustumBuilder::CalculateAngles()
{
    // 参考HollowPrism的角度计算方式：使用对称角度，让开始和结束端盖都变化
    ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);  // ArcAngle的弧度值
    StartAngle = -ArcAngleRadians / 2.0f;  // 起始角度：从-ArcAngle/2开始
    EndAngle = ArcAngleRadians / 2.0f;     // 结束角度：到+ArcAngle/2结束
}
