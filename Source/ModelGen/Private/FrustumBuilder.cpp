// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"
#include "ModelGenMeshData.h"

FFrustumBuilder::FFrustumBuilder(const FFrustumParameters& InParams) : Params(InParams)
{
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
	CreateSideGeometry();

    // 生成倒角几何体
    if (Params.BevelRadius > 0.0f)
    {
        GenerateTopBevelGeometry();
        GenerateBottomBevelGeometry();
    }

    // 生成顶底面几何体
    GenerateTopGeometry();
    GenerateBottomGeometry();

    // 生成端面（如果不是完整的360度）
    if (Params.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps();
    }
}

void FFrustumBuilder::CreateSideGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();

    // 生成上下两个环的顶点 - 使用通用方法
    TArray<int32> TopRing = GenerateVertexRing(
        Params.TopRadius, 
        HalfHeight - Params.BevelRadius, 
        Params.TopSides, 
        0.0f
    );
    
    TArray<int32> BottomRing = GenerateVertexRing(
        Params.BottomRadius, 
        -HalfHeight + Params.BevelRadius, 
        Params.BottomSides, 
        1.0f
    );

    // 将顶环赋值给成员变量
    SideTopRing = TopRing;
    SideBottomRing = BottomRing;

    // 生成不受Params.MinBendRadius影响的上下环，为了计算中间值
    TArray<int32> TopRingOrigin = GenerateVertexRing(
        Params.TopRadius, 
        HalfHeight, 
        Params.TopSides, 
        0.0f
    );
    
    TArray<int32> BottomRingOrigin = GenerateVertexRing(
        Params.BottomRadius, 
        -HalfHeight, 
        Params.BottomSides, 
        1.0f
    );

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

        for (int32 h = 1; h < Params.HeightSegments; ++h)
        {
            const float CurrentHeight = -HalfHeight + h * HeightStep;
            const float HeightRatio = static_cast<float>(h) / Params.HeightSegments;

            TArray<int32> CurrentRing;

            // 为每个下底顶点生成对应的中间顶点
            for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
            {
                const int32 TopIndex = BottomToTopMapping[BottomIndex];

                // 根据Z值对X和Y进行线性插值（不应用弯曲）
                const float XR = FMath::Lerp(GetPosByIndex(BottomRingOrigin[BottomIndex]).X, GetPosByIndex(TopRingOrigin[TopIndex]).X, HeightRatio);
                const float YR = FMath::Lerp(GetPosByIndex(BottomRingOrigin[BottomIndex]).Y, GetPosByIndex(TopRingOrigin[TopIndex]).Y, HeightRatio);
                
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

    // 连接三角形 - 确保只有外面可见
    for (int i = 0; i < VertexRings.Num() - 1; i++)
    {
        TArray<int32> CurrentRing = VertexRings[i];
        TArray<int32> NextRing = VertexRings[i + 1];

        // 生成四边形，确保法线方向正确
        for (int32 CurrentIndex = 0; CurrentIndex < CurrentRing.Num(); ++CurrentIndex)
        {
            // 当 ArcAngle = 360 时，连接最后一个顶点到第一个顶点
            const int32 NextCurrentIndex = (Params.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? 
                (CurrentIndex + 1) % CurrentRing.Num() : (CurrentIndex + 1);

            // 如果下一个索引超出范围，跳过这个四边形
            if (NextCurrentIndex >= CurrentRing.Num())
                continue;
            
            // 计算对应的下顶点索引
            const float CurrentRatio = static_cast<float>(CurrentIndex) / CurrentRing.Num();
            const float NextCurrentRatio = static_cast<float>(NextCurrentIndex) / CurrentRing.Num();

            const int32 NextRingIndex = FMath::Clamp(FMath::RoundToInt(CurrentRatio * NextRing.Num()), 0, NextRing.Num() - 1);
            const int32 NextRingNextIndex = FMath::Clamp(FMath::RoundToInt(NextCurrentRatio * NextRing.Num()), 0, NextRing.Num() - 1);

            // 使用四边形连接，确保法线方向正确
            AddQuad(CurrentRing[CurrentIndex], NextRing[NextRingIndex], NextRing[NextRingNextIndex], CurrentRing[NextCurrentIndex]);
        }
    }
}

void FFrustumBuilder::GenerateTopGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float Z = HalfHeight; // 顶面的Z坐标
    
    // 使用通用方法生成顶面
    GenerateCapGeometry(Z, Params.TopSides, Params.TopRadius, true);
}

void FFrustumBuilder::GenerateBottomGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float Z = -HalfHeight; // 底面的Z坐标
    
    // 使用通用方法生成底面
    GenerateCapGeometry(Z, Params.BottomSides, Params.BottomRadius, false);
}

void FFrustumBuilder::GenerateTopBevelGeometry()
{
    // 使用通用方法生成顶部倒角
    GenerateBevelGeometry(true);
}

void FFrustumBuilder::GenerateBottomBevelGeometry()
{
    // 使用通用方法生成底部倒角
    GenerateBevelGeometry(false);
}

void FFrustumBuilder::GenerateEndCaps()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float StartAngle = 0.0f;
    const float EndAngle = FMath::DegreesToRadians(Params.ArcAngle);
    const float BevelRadius = Params.BevelRadius;

    // 使用合理的默认法线，让UE能够基于三角形面自动计算正确的法线
    // 对于端面，使用指向圆弧外部的方向作为初始法线
    const FVector Normal = FVector(1.0f, 0.0f, 0.0f);

    // 计算倒角高度 - 使用通用方法
    const float TopBevelHeight = CalculateBevelHeight(Params.TopRadius);
    const float BottomBevelHeight = CalculateBevelHeight(Params.BottomRadius);

    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomBevelHeight;
    const float EndZ = HalfHeight - TopBevelHeight;

    // 创建端面几何体 - 使用三角形连接到中心点
    GenerateEndCapTriangles(StartAngle, Normal, true);    // 起始端面
    GenerateEndCapTriangles(EndAngle, Normal, false);       // 结束端面
}

void FFrustumBuilder::GenerateEndCapTriangles(float Angle, const FVector& Normal, bool IsStart)
{
    // 使用重构后的方法生成端面
    TArray<int32> OrderedVertices;
    GenerateEndCapVertices(Angle, Normal, IsStart, OrderedVertices);
    
    // 调试输出：验证端面顶点数量
    UE_LOG(LogTemp, Log, TEXT("GenerateEndCapTriangles - %s端面生成 %d 个顶点"), 
           IsStart ? TEXT("起始") : TEXT("结束"), OrderedVertices.Num());
    
    // 生成端面三角形
    GenerateEndCapTrianglesFromVertices(OrderedVertices, IsStart);
}

void FFrustumBuilder::GenerateBevelArcTriangles(float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSections = Params.BevelSections;

    // 计算倒角弧线的质心（在指定角度和高度范围内）
    const float MidZ = (Z1 + Z2) * 0.5f;
    const float MidRadius =
        (FMath::Max(0.0f, Params.TopRadius - Params.BevelRadius) + FMath::Max(0.0f, Params.BottomRadius - Params.BevelRadius)) *
        0.5f;
    const FVector BevelCentroid = FVector(MidRadius * FMath::Cos(Angle), MidRadius * FMath::Sin(Angle), MidZ);

    const int32 BevelCenterVertex = GetOrAddVertex(BevelCentroid, Normal, FVector2D(0.5f, 0.5f));

    // 计算倒角弧线的起点和终点半径
    float StartRadius, EndRadius;
    if (IsTop)
    {
        // 顶部倒角：从侧面边缘到顶面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Params.Height;
        const float Radius_Start = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Start);
        // 倒角不受BendAmount影响，保持原始半径
        StartRadius = Radius_Start;
        
        EndRadius = FMath::Max(0.0f, Params.TopRadius - Params.BevelRadius);
    }
    else
    {
        // 底部倒角：从侧面边缘到底面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Params.Height;
        const float Radius_Start = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Start);
        // 倒角不受BendAmount影响，保持原始半径
        StartRadius = Radius_Start;
        
        EndRadius = FMath::Max(0.0f, Params.BottomRadius - Params.BevelRadius);
    }

    // 生成倒角弧线的三角形
    for (int32 i = 0; i < BevelSections; ++i)
    {
        const float Alpha = static_cast<float>(i) / BevelSections;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, Alpha);
        const float CurrentZ = FMath::Lerp(Z1, Z2, Alpha);

        // 当前倒角弧线点
        const FVector ArcPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
        const int32 ArcVertex =
            GetOrAddVertex(ArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, CalculateHeightRatio(CurrentZ)));

        // 下一个倒角弧线点
        const float NextAlpha = static_cast<float>(i + 1) / BevelSections;
        const float NextRadius = FMath::Lerp(StartRadius, EndRadius, NextAlpha);
        const float NextZ = FMath::Lerp(Z1, Z2, NextAlpha);

        const FVector NextArcPos = FVector(NextRadius * FMath::Cos(Angle), NextRadius * FMath::Sin(Angle), NextZ);
        const int32 NextArcVertex =
            GetOrAddVertex(NextArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, CalculateHeightRatio(NextZ)));

        // 创建三角形：当前弧线点 -> 下一个弧线点 -> 倒角质心
        AddTriangle(ArcVertex, NextArcVertex, BevelCenterVertex);
    }
}

void FFrustumBuilder::GenerateBevelArcTrianglesWithCaps(
    float Angle, const FVector& Normal, bool IsStart, float Z1, float Z2, bool IsTop, int32 CenterVertex, int32 CapCenterVertex)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSections = Params.BevelSections;

    // 计算倒角弧线的起点和终点半径
    float StartRadius, EndRadius;
    if (IsTop)
    {
        // 顶部倒角：从侧面边缘到顶面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Params.Height;
        const float Radius_Start = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Start);
        // 倒角不受BendAmount影响，保持原始半径
        StartRadius = Radius_Start;
        
        EndRadius = FMath::Max(0.0f, Params.TopRadius - Params.BevelRadius);
    }
    else
    {
        // 底部倒角：从侧面边缘到底面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Params.Height;
        const float Radius_Start = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Start);
        // 倒角不受BendAmount影响，保持原始半径
        StartRadius = Radius_Start;
        
        EndRadius = FMath::Max(0.0f, Params.BottomRadius - Params.BevelRadius);
    }

    // 生成倒角弧线的三角形
    for (int32 i = 0; i < BevelSections; ++i)
    {
        const float Alpha = static_cast<float>(i) / BevelSections;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, Alpha);
        const float CurrentZ = FMath::Lerp(Z1, Z2, Alpha);

        // 当前倒角弧线点
        const FVector ArcPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
        const int32 ArcVertex =
            GetOrAddVertex(ArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, CalculateHeightRatio(CurrentZ)));

        // 下一个倒角弧线点
        const float NextAlpha = static_cast<float>(i + 1) / BevelSections;
        const float NextRadius = FMath::Lerp(StartRadius, EndRadius, NextAlpha);
        const float NextZ = FMath::Lerp(Z1, Z2, NextAlpha);

        const FVector NextArcPos = FVector(NextRadius * FMath::Cos(Angle), NextRadius * FMath::Sin(Angle), NextZ);
        const int32 NextArcVertex =
            GetOrAddVertex(NextArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, CalculateHeightRatio(NextZ)));

        // 创建三角形：当前弧线点 -> 下一个弧线点 -> 中心点（0,0,0）
        AddTriangle(ArcVertex, NextArcVertex, CenterVertex);
    }

    // 连接最边缘的点到上/下底中心点
    // 最边缘的点是倒角弧线的起点（i=0）
    const float StartAlpha = 0.0f;
    const float StartRadius_Edge = FMath::Lerp(StartRadius, EndRadius, StartAlpha);
    const float StartZ_Edge = FMath::Lerp(Z1, Z2, StartAlpha);
    const FVector StartEdgePos = FVector(StartRadius_Edge * FMath::Cos(Angle), StartRadius_Edge * FMath::Sin(Angle), StartZ_Edge);
    const int32 StartEdgeVertex =
        GetOrAddVertex(StartEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, CalculateHeightRatio(StartZ_Edge)));

    // 最边缘的点是倒角弧线的终点（i=BevelSections）
    const float EndAlpha = 1.0f;
    const float EndRadius_Edge = FMath::Lerp(StartRadius, EndRadius, EndAlpha);
    const float EndZ_Edge = FMath::Lerp(Z1, Z2, EndAlpha);
    const FVector EndEdgePos = FVector(EndRadius_Edge * FMath::Cos(Angle), EndRadius_Edge * FMath::Sin(Angle), EndZ_Edge);
    const int32 EndEdgeVertex =
        GetOrAddVertex(EndEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, CalculateHeightRatio(EndZ_Edge)));

    // 创建三角形：最边缘点 -> 上/下底中心点 -> 中心点（0,0,0）    // 注意顶点顺序，确保法线方向正确
    if (IsTop)
    {
        // 顶部：StartEdgeVertex -> CapCenterVertex -> CenterVertex
        AddTriangle(StartEdgeVertex, CapCenterVertex, CenterVertex);
        // 顶部：EndEdgeVertex -> CapCenterVertex -> CenterVertex
        AddTriangle(EndEdgeVertex, CapCenterVertex, CenterVertex);
    }
    else
    {
        // 底部：StartEdgeVertex -> CenterVertex -> CapCenterVertex（修正顶点顺序）
        AddTriangle(StartEdgeVertex, CenterVertex, CapCenterVertex);
        // 底部：EndEdgeVertex -> CenterVertex -> CapCenterVertex（修正顶点顺序）
        AddTriangle(EndEdgeVertex, CenterVertex, CapCenterVertex);
    }
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
}

void FFrustumBuilder::GenerateBevelGeometry(bool bIsTop)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSections = Params.BevelSections;
    
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

// 新增：端面重构相关方法实现
void FFrustumBuilder::GenerateEndCapVertices(float Angle, const FVector& Normal, bool IsStart,
                                            TArray<int32>& OutOrderedVertices)
{
    const float HalfHeight = Params.GetHalfHeight();
    
    OutOrderedVertices.Empty();
    
    // 上底中心顶点
    const int32 TopCenterVertex = GetOrAddVertex(FVector(0, 0, HalfHeight), Normal, FVector2D(0.5f, 1.0f));
    OutOrderedVertices.Add(TopCenterVertex);

    // 先计算侧边在倒角连接点的位置
    float TopBevelHeight, BottomBevelHeight;
    CalculateEndCapBevelHeights(TopBevelHeight, BottomBevelHeight);
    
    float StartZ, EndZ;
    CalculateEndCapZRange(TopBevelHeight, BottomBevelHeight, StartZ, EndZ);
    
    // 保存侧边在倒角连接点的位置
    const float TopSideRadius = CalculateEndCapRadiusAtHeight(EndZ);
    const float BottomSideRadius = CalculateEndCapRadiusAtHeight(StartZ);
    
    // 上倒角弧线顶点
    if (Params.BevelRadius > 0.0f)
    {
        GenerateEndCapBevelVertices(Angle, Normal, IsStart, true, OutOrderedVertices, TopSideRadius);
    }

    // 侧边顶点
    GenerateEndCapSideVertices(Angle, Normal, IsStart, OutOrderedVertices, TopSideRadius, BottomSideRadius);

    // 下倒角弧线顶点
    if (Params.BevelRadius > 0.0f)
    {
        GenerateEndCapBevelVertices(Angle, Normal, IsStart, false, OutOrderedVertices, BottomSideRadius);
    }

    // 下底中心顶点
    const int32 BottomCenterVertex = GetOrAddVertex(FVector(0, 0, -HalfHeight), Normal, FVector2D(0.5f, 0.0f));
    OutOrderedVertices.Add(BottomCenterVertex);
}

void FFrustumBuilder::GenerateEndCapBevelVertices(float Angle, const FVector& Normal, bool IsStart,
                                                 bool bIsTopBevel, TArray<int32>& OutVertices, float SideRadius)
{
    const float HalfHeight = Params.GetHalfHeight();
    float TopBevelHeight, BottomBevelHeight;
    CalculateEndCapBevelHeights(TopBevelHeight, BottomBevelHeight);
    
    float StartZ, EndZ;
    CalculateEndCapZRange(TopBevelHeight, BottomBevelHeight, StartZ, EndZ);
    
    const int32 BevelSections = Params.BevelSections;
    
    // 确保覆盖完整的倒角弧线，并与侧边几何体精确对齐
    if (bIsTopBevel)
    {
        // 顶部倒角：从顶面边缘到侧边边缘
        const float TopRadius = FMath::Max(0.0f, Params.TopRadius - Params.BevelRadius);
        // 直接使用预计算的侧边半径，确保完美连接
        // const float SideRadius = CalculateBevelRadiusAtHeight(EndZ); // 已移除
        
        // 生成从顶面到侧边的倒角弧线
        for (int32 i = 0; i <= BevelSections; ++i)
        {
            const float Alpha = static_cast<float>(i) / BevelSections;
            const float CurrentZ = FMath::Lerp(HalfHeight, EndZ, Alpha);
            const float CurrentRadius = FMath::Lerp(TopRadius, SideRadius, Alpha);
            
            // 生成倒角顶点
            const FVector BevelPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
            const int32 BevelVertex = GetOrAddVertex(BevelPos, Normal, 
                FVector2D(IsStart ? 0.0f : 1.0f, CalculateHeightRatio(CurrentZ)));
            OutVertices.Add(BevelVertex);
            
        }
        
        // 记录倒角环的结束点（与侧边连接的点）
        const FVector BevelEndPos = FVector(SideRadius * FMath::Cos(Angle), SideRadius * FMath::Sin(Angle), EndZ);
    }
    else
    {
        // 底部倒角：从侧边边缘到底面边缘
        // 直接使用预计算的侧边半径，确保完美连接
        // const float SideRadius = CalculateBevelRadiusAtHeight(StartZ); // 已移除
        const float BottomRadius = FMath::Max(0.0f, Params.BottomRadius - Params.BevelRadius);
        
        // 生成从侧边到底面的倒角弧线
        for (int32 i = 0; i <= BevelSections; ++i)
        {
            const float Alpha = static_cast<float>(i) / BevelSections;
            const float CurrentZ = FMath::Lerp(StartZ, -HalfHeight, Alpha);
            const float CurrentRadius = FMath::Lerp(SideRadius, BottomRadius, Alpha);
            
            // 生成倒角顶点
            const FVector BevelPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
            const int32 BevelVertex = GetOrAddVertex(BevelPos, Normal, 
                FVector2D(IsStart ? 0.0f : 1.0f, CalculateHeightRatio(CurrentZ)));
            OutVertices.Add(BevelVertex);
            
        }
    }
}

void FFrustumBuilder::GenerateEndCapSideVertices(float Angle, const FVector& Normal, bool IsStart,
                                                TArray<int32>& OutVertices, float TopSideRadius, float BottomSideRadius)
{
    const float HalfHeight = Params.GetHalfHeight();
    float TopBevelHeight, BottomBevelHeight;
    CalculateEndCapBevelHeights(TopBevelHeight, BottomBevelHeight);
    
    float StartZ, EndZ;
    CalculateEndCapZRange(TopBevelHeight, BottomBevelHeight, StartZ, EndZ);
    
    // 侧边顶点
    for (int32 h = 0; h <= Params.HeightSegments; ++h)
    {
        const float Z = FMath::Lerp(EndZ, StartZ, static_cast<float>(h) / Params.HeightSegments);
        const float Alpha = (Z + HalfHeight) / Params.Height;

        // 计算当前高度对应的半径
        float BentRadius;
        if (h == 0)
        {
            // 第一个顶点（顶部）：使用预计算的顶部侧边半径
            BentRadius = TopSideRadius;
        }
        else if (h == Params.HeightSegments)
        {
            // 最后一个顶点（底部）：使用预计算的底部侧边半径
            BentRadius = BottomSideRadius;
        }
        else
        {
            // 中间顶点：正常计算弯曲半径
            const float Radius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha);
            BentRadius = CalculateBentRadius(Radius, Alpha);
        }

        const FVector EdgePos = FVector(BentRadius * FMath::Cos(Angle), BentRadius * FMath::Sin(Angle), Z);
        
        // 计算法线：从中心轴指向顶点
        FVector SideNormal = FVector(EdgePos.X, EdgePos.Y, 0.0f).GetSafeNormal();
        if (SideNormal.IsNearlyZero())
        {
            SideNormal = FVector(1.0f, 0.0f, 0.0f);
        }
        
        const int32 EdgeVertex = GetOrAddVertex(EdgePos, SideNormal, FVector2D(IsStart ? 0.0f : 1.0f, Alpha));
        OutVertices.Add(EdgeVertex);
        
    }
}

void FFrustumBuilder::GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, bool IsStart)
{
    // 使用Frustum的中心点（0,0,0）
    const int32 CenterVertex = GetOrAddVertex(FVector(0, 0, 0), FVector(1.0f, 0.0f, 0.0f), FVector2D(0.5f, 0.5f));

    // 如果有记录的连接点，使用它们来生成更精确的三角形
    {
        // 如果没有记录连接点，使用原来的方法
        for (int32 i = 0; i < OrderedVertices.Num() - 1; ++i)
        {
            const int32 V1 = OrderedVertices[i];
            const int32 V2 = OrderedVertices[i + 1];

            if (IsStart)
            {
                // 起始端面：确保法线方向正确（指向外部）
                AddTriangle(V1, V2, CenterVertex);
            }
            else
            {
                // 结束端面：翻转顶点顺序以保持法线方向一致（指向外部）
                AddTriangle(V2, V1, CenterVertex);
            }
        }
    }
}

void FFrustumBuilder::CalculateEndCapBevelHeights(float& OutTopBevelHeight, float& OutBottomBevelHeight) const
{
    OutTopBevelHeight = FMath::Min(Params.BevelRadius, Params.TopRadius);
    OutBottomBevelHeight = FMath::Min(Params.BevelRadius, Params.BottomRadius);
}

void FFrustumBuilder::CalculateEndCapZRange(float TopBevelHeight, float BottomBevelHeight, 
                                           float& OutStartZ, float& OutEndZ) const
{
    const float HalfHeight = Params.GetHalfHeight();
    
    // 调整主体几何体的范围，避免与倒角重叠
    OutStartZ = -HalfHeight + BottomBevelHeight;
    OutEndZ = HalfHeight - TopBevelHeight;
}

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

// 新增：专门用于倒角的半径计算方法，不受BendAmount和MinBendRadius影响
float FFrustumBuilder::CalculateBevelRadiusAtHeight(float Z) const
{
    const float HalfHeight = Params.GetHalfHeight();
    const float Alpha = (Z + HalfHeight) / Params.Height;
    
    // 计算当前高度对应的半径
    const float Radius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha);
    // 倒角不受BendAmount影响，保持原始半径
    const float BentRadius = Radius;
    
    // 倒角不受MinBendRadius影响，只确保不为负值
    return FMath::Max(BentRadius, KINDA_SMALL_NUMBER);
}
