// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"

#include "ModelGenMeshData.h"

FFrustumBuilder::FFrustumBuilder(const FFrustumParameters& InParams) : Params(InParams)
{
}

void FFrustumBuilder::Clear()
{
    // 调用基类的Clear方法
    FModelGenMeshBuilder::Clear();

    // 清除端面连接点
    ClearEndCapConnectionPoints();
}

bool FFrustumBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    UE_LOG(LogTemp, Log, TEXT("FFrustumBuilder::Generate - Starting generation"));

    if (!ValidateParameters())
    {
        UE_LOG(LogTemp, Error, TEXT("FFrustumBuilder::Generate - Parameters validation failed"));
        return false;
    }

    // 清除现有数据
    Clear();

    // 预分配内存
    ReserveMemory();

    UE_LOG(LogTemp, Log, TEXT("FFrustumBuilder::Generate - Generating base geometry"));

    // 生成基础几何体
    GenerateBaseGeometry();

    // 只在调试模式下输出详细信息
#if WITH_EDITOR
    UE_LOG(LogTemp, Log, TEXT("FFrustumBuilder::Generate - Generated %d vertices, %d triangles"), MeshData.GetVertexCount(),
        MeshData.GetTriangleCount());
#endif

    // 验证生成的数据
    if (!ValidateGeneratedData())
    {
        UE_LOG(LogTemp, Error, TEXT("FFrustumBuilder::Generate - Generated data validation failed"));
        return false;
    }

    // 输出网格数据
    OutMeshData = MeshData;

    UE_LOG(LogTemp, Log, TEXT("FFrustumBuilder::Generate - Generation completed successfully"));
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
    GenerateTopGeometry();
    GenerateTopBevelGeometry();
    CreateSideGeometry();
    GenerateBottomBevelGeometry();
    GenerateBottomGeometry();

    GenerateEndCaps();
}

void FFrustumBuilder::CreateSideGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();

    // 生成上下两个环的顶点 - 使用通用方法
    TArray<int32> TopRing = GenerateVertexRing(Params.TopRadius, HalfHeight - Params.BevelRadius, Params.TopSides, 0.0f);

    TArray<int32> BottomRing = GenerateVertexRing(Params.BottomRadius, -HalfHeight + Params.BevelRadius, Params.BottomSides, 1.0f);

    // 将顶环赋值给成员变量
    SideTopRing = TopRing;
    SideBottomRing = BottomRing;

    // 生成不受Params.MinBendRadius影响的上下环，为了计算中间值
    TArray<int32> TopRingOrigin = GenerateVertexRing(Params.TopRadius, HalfHeight, Params.TopSides, 0.0f);

    TArray<int32> BottomRingOrigin = GenerateVertexRing(Params.BottomRadius, -HalfHeight, Params.BottomSides, 1.0f);

    // 计算下底顶点和上底顶点的对应关系
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

            // 为每个下底顶点生成对应的中间顶点
            for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
            {
                const int32 TopIndex = BottomToTopMapping[BottomIndex];

                // 根据Z值对X和Y进行线性插值（不应用弯曲）
                const float XR = FMath::Lerp(
                    GetPosByIndex(BottomRingOrigin[BottomIndex]).X, GetPosByIndex(TopRingOrigin[TopIndex]).X, HeightRatio);
                const float YR = FMath::Lerp(
                    GetPosByIndex(BottomRingOrigin[BottomIndex]).Y, GetPosByIndex(TopRingOrigin[TopIndex]).Y, HeightRatio);

                // 计算当前高度对应的基础半径
                const float BaseRadius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, HeightRatio);

                // 使用统一的弯曲半径计算方法
                const float BentRadius = CalculateBentRadius(BaseRadius, HeightRatio);

                // 应用弯曲：将顶点从中心向外推或向内拉
                const float Scale = BentRadius / BaseRadius;
                const float X = XR * Scale;
                const float Y = YR * Scale;

                // X和Y已经在上面计算完成，现在直接使用

                // 生成插值后的顶点位置
                const FVector InterpolatedPos(X, Y, CurrentHeight);

                // 计算法线：从中心轴指向顶点
                FVector Normal = FVector(X, Y, 0.0f).GetSafeNormal();
                if (Normal.IsNearlyZero())
                {
                    Normal = FVector(1.0f, 0.0f, 0.0f);
                }

                // 计算UV坐标
                const float U = static_cast<float>(BottomIndex) / Params.BottomSides;
                const FVector2D UV(U, HeightRatio);

                // 添加顶点到MeshData并获取索引
                const int32 VertexIndex = GetOrAddVertex(InterpolatedPos, Normal, UV);
                CurrentRing.Add(VertexIndex);
            }

            VertexRings.Add(CurrentRing);
        }
    }
    VertexRings.Add(TopRing);

    for (int32 i = VertexRings.Num() - 2; i > 0; i--)
    {
        // 记录顶部环起始点的顶点索引
        RecordEndCapConnectionPoint(VertexRings[i][0]);
    }

    // 连接三角形 - 确保只有外面可见
    for (int i = 0; i < VertexRings.Num() - 1; i++)
    {
        TArray<int32> CurrentRing = VertexRings[i];
        TArray<int32> NextRing = VertexRings[i + 1];

        // 生成四边形，确保法线方向正确
        for (int32 CurrentIndex = 0; CurrentIndex < CurrentRing.Num(); ++CurrentIndex)
        {
            // 当 ArcAngle = 360 时，连接最后一个顶点到第一个顶点
            const int32 NextCurrentIndex =
                (Params.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? (CurrentIndex + 1) % CurrentRing.Num() : (CurrentIndex + 1);

            // 如果下一个索引超出范围，跳过这个四边形
            if (NextCurrentIndex >= CurrentRing.Num())
                continue;

            // 计算对应的下顶点索引
            const float CurrentRatio = static_cast<float>(CurrentIndex) / CurrentRing.Num();
            const float NextCurrentRatio = static_cast<float>(NextCurrentIndex) / CurrentRing.Num();

            const int32 NextRingIndex = FMath::Clamp(FMath::RoundToInt(CurrentRatio * NextRing.Num()), 0, NextRing.Num() - 1);
            const int32 NextRingNextIndex =
                FMath::Clamp(FMath::RoundToInt(NextCurrentRatio * NextRing.Num()), 0, NextRing.Num() - 1);

            // 使用四边形连接，确保法线方向正确
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
    const float StartAngle = 0.0f;
    const float EndAngle = FMath::DegreesToRadians(Params.ArcAngle);

    // 生成起始端面和结束端面
    GenerateEndCap(StartAngle, true);
    GenerateEndCap(EndAngle, false);
}

void FFrustumBuilder::GenerateEndCap(float Angle, bool IsStart)
{
    // 检查是否有足够的连接点来生成端面
    if (EndCapConnectionPoints.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateEndCap - %s端面连接点不足，无法生成端面"), IsStart ? TEXT("起始") : TEXT("结束"));
        return;
    }

    // 根据角度生成对应的端面连接点
    TArray<int32> RotatedConnectionPoints;
    RotatedConnectionPoints.Reserve(EndCapConnectionPoints.Num());

    // 计算旋转矩阵
    const FQuat Rotation = FQuat(FVector::UpVector, Angle);

    for (int32 VertexIndex : EndCapConnectionPoints)
    {
        // 获取原始顶点位置
        FVector OriginalPos = GetPosByIndex(VertexIndex);

        // 应用旋转变换
        FVector RotatedPos = Rotation.RotateVector(OriginalPos);

        // 创建新的旋转后顶点
        FVector Normal = FVector(RotatedPos.X, RotatedPos.Y, 0.0f).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector(1.0f, 0.0f, 0.0f);
        }

        // 计算UV坐标（保持与原始顶点一致，使用默认UV）
        FVector2D UV(0.5f, 0.5f);

        // 添加旋转后的顶点
        int32 NewVertexIndex = GetOrAddVertex(RotatedPos, Normal, UV);
        RotatedConnectionPoints.Add(NewVertexIndex);
    }

    // 使用旋转后的连接点生成端面三角形
    GenerateEndCapTrianglesFromVertices(RotatedConnectionPoints, IsStart);

    // 调试输出：验证端面生成
    UE_LOG(LogTemp, Log, TEXT("GenerateEndCap - %s端面生成完成，使用 %d 个旋转后的连接点"), IsStart ? TEXT("起始") : TEXT("结束"),
        RotatedConnectionPoints.Num());
}

TArray<int32> FFrustumBuilder::GenerateVertexRing(float Radius, float Z, int32 Sides, float UVV)
{
    TArray<int32> VertexRing;
    // 使用通用方法计算角度步长
    const float AngleStep = CalculateAngleStep(Sides);

    // 当ArcAngle < 360时，多生成一个起点以正确连接
    const int32 VertexCount = (Params.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides;

    for (int32 i = 0; i < VertexCount; ++i)
    {
        const float Angle = i * AngleStep;
        const float X = Radius * FMath::Cos(Angle);
        const float Y = Radius * FMath::Sin(Angle);
        const FVector Pos(X, Y, Z);
        const FVector2D UV(static_cast<float>(i) / Sides, UVV);

        // 计算法线：从中心轴指向顶点
        FVector Normal = FVector(X, Y, 0.0f).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector(1.0f, 0.0f, 0.0f);
        }

        const int32 VertexIndex = GetOrAddVertex(Pos, Normal, UV);
        VertexRing.Add(VertexIndex);
    }

    return VertexRing;
}

void FFrustumBuilder::GenerateCapGeometry(float Z, int32 Sides, float Radius, bool bIsTop)
{
    // 顶底面法线：垂直于表面
    FVector Normal(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);

    // 添加中心顶点
    const FVector CenterPos(0.0f, 0.0f, Z);
    const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, FVector2D(0.5f, 0.5f));

    if (bIsTop)
    {
        //RecordEndCapConnectionPoint(CenterVertex);
    }

    const float AngleStep = CalculateAngleStep(Sides);

    for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
    {
        const float CurrentAngle = SideIndex * AngleStep;
        const float NextAngle = (SideIndex + 1) * AngleStep;

        // 使用指定半径，减去倒角半径
        const float CurrentRadius = Radius - Params.BevelRadius;

        // 计算顶点位置
        const FVector CurrentPos(CurrentRadius * FMath::Cos(CurrentAngle), CurrentRadius * FMath::Sin(CurrentAngle), Z);
        const FVector NextPos(CurrentRadius * FMath::Cos(NextAngle), CurrentRadius * FMath::Sin(NextAngle), Z);

        // 计算UV坐标
        const FVector2D UV1 = CalculateUV(SideIndex, Sides, 0.0f);
        const FVector2D UV2 = CalculateUV(SideIndex + 1, Sides, 0.0f);

        // 添加顶点
        const int32 V1 = GetOrAddVertex(CurrentPos, Normal, UV1);
        const int32 V2 = GetOrAddVertex(NextPos, Normal, UV2);

        if (SideIndex == 0)
        {
            RecordEndCapConnectionPoint(V1);
        }

        // 添加三角形，确保正确的顶点顺序
        if (bIsTop)
        {
            AddTriangle(CenterVertex, V2, V1);
        }
        else
        {
            AddTriangle(CenterVertex, V1, V2);
        }
    }

    if (!bIsTop)
    {
        //RecordEndCapConnectionPoint(CenterVertex);
    }
}

void FFrustumBuilder::GenerateBevelGeometry(bool bIsTop)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSections = Params.BevelSegments;

    // 根据是顶部还是底部选择参数
    const float Radius = bIsTop ? Params.TopRadius : Params.BottomRadius;
    const int32 Sides = bIsTop ? Params.TopSides : Params.BottomSides;
    const TArray<int32>& SideRing = bIsTop ? SideTopRing : SideBottomRing;
    // 使用通用方法计算倒角高度
    const float StartZ = bIsTop ? (HalfHeight - CalculateBevelHeight(Radius)) : (-HalfHeight + CalculateBevelHeight(Radius));
    const float EndZ = bIsTop ? HalfHeight : -HalfHeight;

    if (BevelRadius <= 0.0f || BevelSections <= 0)
        return;

    // 使用侧边环的顶点位置作为倒角起点，确保完全一致
    TArray<int32> PrevRing;
    // 使用通用方法计算角度步长
    const float AngleStep = CalculateAngleStep(Sides);
    TArray<int32> StartRingForEndCap;

    for (int32 i = 0; i <= BevelSections; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSections;    // 沿倒角弧线的进度

        // 倒角起点
        FVector StartPos;
        if (i == 0 && SideRing.Num() > 0)
        {
            StartPos = GetPosByIndex(SideRing[0]);
        }
        else
        {
            const float Alpha_Height = (StartZ + HalfHeight) / Params.Height;
            const float RadiusAtZ = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Height);
            // 倒角不受BendAmount影响，保持原始半径
            const float BentRadius = FMath::Max(RadiusAtZ, KINDA_SMALL_NUMBER);
            StartPos = FVector(BentRadius, 0.0f, StartZ);
        }

        const float CapRadius = FMath::Max(0.0f, Radius - Params.BevelRadius);
        const float CurrentRadius = FMath::Lerp(StartPos.Size2D(), CapRadius, alpha);
        const float CurrentZ = FMath::Lerp(StartZ, EndZ, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Sides + 1);

        // 当ArcAngle < 360时，多生成一个起点以正确连接
        const int32 VertexCount = (Params.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides;

        for (int32 s = 0; s <= VertexCount; ++s)
        {
            FVector Position;

            if (i == 0 && s < SideRing.Num())
            {
                Position = GetPosByIndex(SideRing[s]);
            }
            else
            {
                const float angle = s * AngleStep;
                Position = FVector(CurrentRadius * FMath::Cos(angle), CurrentRadius * FMath::Sin(angle), CurrentZ);
            }

            // 计算法线：从中心轴指向顶点
            FVector Normal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
            if (Normal.IsNearlyZero())
            {
                Normal = FVector(1.0f, 0.0f, 0.0f);
            }

            // 计算 UV - 使用与主体一致的UV映射
            const FVector2D UV = CalculateUV(s, Sides, (Position.Z + HalfHeight) / Params.Height);

            CurrentRing.Add(GetOrAddVertex(Position, Normal, UV));
        }

        // 只在有前一个环时才生成四边形，避免重复
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Sides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];

                // 根据顶部/底部选择顶点顺序
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
        // 记录当前环的第一个点
        StartRingForEndCap.Add(CurrentRing[0]);
    }

    // 根据顶部/底部选择记录顺序
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

FVector2D FFrustumBuilder::CalculateUV(float SideIndex, float Sides, float HeightRatio)
{
    // 计算U坐标：基于边数索引
    const float U = SideIndex / Sides;

    // 计算V坐标：基于高度比例
    const float V = HeightRatio;

    return FVector2D(U, V);
}

float FFrustumBuilder::CalculateBentRadius(float BaseRadius, float HeightRatio)
{
    // 计算弯曲因子
    const float BendFactor = FMath::Sin(HeightRatio * PI);

    // 计算弯曲后的半径
    const float BentRadius = BaseRadius + Params.BendAmount * BendFactor * BaseRadius;

    // 应用MinBendRadius限制（如果设置了的话）
    // 这是关键：在弯曲计算后立即应用MinBendRadius，防止过度收缩
    if (Params.MinBendRadius > KINDA_SMALL_NUMBER)
    {
        // 如果MinBendRadius > 0，则半径不能小于MinBendRadius
        return FMath::Max(BentRadius, Params.MinBendRadius);
    }

    // 如果没有设置MinBendRadius，只确保不为负值
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
    return FMath::DegreesToRadians(Params.ArcAngle) / Sides;
}

// 端面生成重构完成 - 所有逻辑已整合到GenerateEndCap函数中

void FFrustumBuilder::GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, bool IsStart)
{
    // 检查顶点数量
    if (OrderedVertices.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenerateEndCapTrianglesFromVertices - 顶点数量不足，无法生成面"));
        return;
    }

    const int32 VertexCount = OrderedVertices.Num();
    const float HalfHeight = Params.GetHalfHeight();

    // 生成从弧线到中心线的面
    // 使用梯形和三角形的组合方式
    
    if (VertexCount == 2)
    {
        // 只有两个顶点：生成一个三角形
        const int32 V1 = OrderedVertices[0];
        const int32 V2 = OrderedVertices[1];
        
        // 获取V1和V2的Z坐标
        FVector Pos1 = GetPosByIndex(V1);
        FVector Pos2 = GetPosByIndex(V2);
        
        // 在中心线上创建对应的顶点
        const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), FVector(0, 0, 1), FVector2D(0.5f, 0.5f));
        const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), FVector(0, 0, 1), FVector2D(0.5f, 0.5f));
        
        if (IsStart)
        {
            // 起始端：确保法线方向正确
            AddTriangle(V1, V2, CenterV1);
            AddTriangle(V2, CenterV2, CenterV1);
        }
        else
        {
            // 结束端：反转渲染方向
            AddTriangle(V2, V1, CenterV1);
            AddTriangle(CenterV1, CenterV2, V2);
        }
    }
    else
    {
        // 多个顶点：使用梯形和三角形的组合
        for (int32 i = 0; i < VertexCount - 1; ++i)
        {
            const int32 V1 = OrderedVertices[i];
            const int32 V2 = OrderedVertices[i + 1];
            
            // 获取V1和V2的Z坐标
            FVector Pos1 = GetPosByIndex(V1);
            FVector Pos2 = GetPosByIndex(V2);
            
            // 在中心线上创建对应的顶点
            const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), FVector(0, 0, 1), FVector2D(0.5f, 0.5f));
            const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), FVector(0, 0, 1), FVector2D(0.5f, 0.5f));
            
            if (IsStart)
            {
                // 起始端：生成梯形（两个三角形）
                // 第一个三角形：V1, V2, CenterV1
                AddTriangle(V1, V2, CenterV1);
                // 第二个三角形：V2, CenterV2, CenterV1
                AddTriangle(V2, CenterV2, CenterV1);
            }
            else
            {
                // 结束端：反转渲染方向，保持法线方向一致
                // 第一个三角形：V2, V1, CenterV1
                AddTriangle(V2, V1, CenterV1);
                // 第二个三角形：CenterV1, CenterV2, V2
                AddTriangle(CenterV1, CenterV2, V2);
            }
        }
    }
}

// 这些函数已被整合到GenerateEndCap中，不再需要

float FFrustumBuilder::CalculateEndCapRadiusAtHeight(float Z) const
{
    const float HalfHeight = Params.GetHalfHeight();
    const float Alpha = (Z + HalfHeight) / Params.Height;

    // 计算当前高度对应的半径
    const float Radius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha);
    // 端面侧边不受BendAmount影响，保持原始半径
    const float BentRadius = Radius;

    // 应用MinBendRadius限制（如果设置了的话）
    // 这是关键：在弯曲计算后立即应用MinBendRadius，防止过度收缩
    if (Params.MinBendRadius > KINDA_SMALL_NUMBER)
    {
        // 如果MinBendRadius > 0，则半径不能小于MinBendRadius
        return FMath::Max(BentRadius, Params.MinBendRadius);
    }

    // 如果没有设置MinBendRadius，只确保不为负值
    return FMath::Max(BentRadius, KINDA_SMALL_NUMBER);
}

// 此函数已被整合到GenerateEndCap中，不再需要

// 新增：端面连接点管理方法实现
void FFrustumBuilder::RecordEndCapConnectionPoint(int32 VertexIndex)
{
    EndCapConnectionPoints.Add(VertexIndex);

    UE_LOG(LogTemp, Log, TEXT("RecordEndCapConnectionPoint - 记录连接点: 顶点索引=%d"), VertexIndex);
}

const TArray<int32>& FFrustumBuilder::GetEndCapConnectionPoints() const
{
    return EndCapConnectionPoints;
}

// 此方法已被简化，使用GetEndCapConnectionPoints()获取所有连接点

void FFrustumBuilder::ClearEndCapConnectionPoints()
{
    EndCapConnectionPoints.Empty();

    UE_LOG(LogTemp, Log, TEXT("ClearEndCapConnectionPoints - 已清除所有端面连接点"));
}

// 此方法已被简化，使用RecordEndCapConnectionPoint()记录连接点

// 此方法已被简化，使用GetEndCapConnectionPoints()获取所有连接点
