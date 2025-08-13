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
    
    // 清除所有连接点
    ClearAllConnectionPoints();
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
    GenerateTopGeometry();
	GenerateTopBevelGeometry();
	GenerateBottomBevelGeometry();
    GenerateBottomGeometry();

	GenerateEndCaps();
}

void FFrustumBuilder::CreateSideGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();

    // 生成上下两个环的顶点 - 使用通用方法
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

    for (int32 i = VertexRings.Num() - 1; i >= 0; i--)
    {
        // 记录中间环起始点的顶点索引到侧面连接点
        RecordSideConnectionPoint(VertexRings[i][0]);
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
    if (IsStart)
    {
        GenerateStartEndCap();
    }
    else
    {
        GenerateEndEndCap(Angle);
    }
}

void FFrustumBuilder::GenerateStartEndCap()
{
    // 起始端盖的实现
    const FVector TopCenterPos(0.0f, 0.0f, Params.GetHalfHeight());
    const FVector BottomCenterPos(0.0f, 0.0f, -Params.GetHalfHeight());
    const int32 TopCenterVertex = GetOrAddVertex(TopCenterPos, {0.0f, 0.0f, 1.0f}, FVector2D(0.5f, 0.5f));
    const int32 BottomCenterVertex = GetOrAddVertex(BottomCenterPos, {0.0f, 0.0f, -1.0f}, FVector2D(0.5f, 0.5f));
    
    const int32 CenterVertex = GetOrAddVertex(FVector(0, 0, 0), FVector(1.0f, 0.0f, 0.0f), FVector2D(0.5f, 0.5f));

    // 上倒角的弧
    for (int32 i = 0; i < TopArcConnectionPoints.Num() - 1; ++i)
    {
        const int32 V1 = TopArcConnectionPoints[i];
        const int32 V2 = TopArcConnectionPoints[i + 1];

        AddTriangle(V1, V2, TopCenterVertex);
    }
    AddTriangle(SideConnectionPoints[0], CenterVertex, TopCenterVertex);

    // 侧边
    for (int32 i = 0; i < SideConnectionPoints.Num() - 1; ++i)
    {
        const int32 V1 = SideConnectionPoints[i];
        const int32 V2 = SideConnectionPoints[i + 1];

        AddTriangle(V1, V2, CenterVertex);
    }

    AddTriangle(SideConnectionPoints[SideConnectionPoints.Num() - 1], BottomCenterVertex, CenterVertex);

    // 下倒角的弧
    for (int32 i = 0; i < BottomArcConnectionPoints.Num() - 1; ++i)
    {
        const int32 V1 = BottomArcConnectionPoints[i];
        const int32 V2 = BottomArcConnectionPoints[i + 1];

        AddTriangle(V1, V2, BottomCenterVertex);
    }
}

void FFrustumBuilder::GenerateEndEndCap(float Angle)
{
    // 结束端盖的实现
    // 根据角度生成对应的端面连接点
    TArray<int32> RotatedTopArcPoints;
    TArray<int32> RotatedSidePoints;
    TArray<int32> RotatedBottomArcPoints;
    
    // 计算旋转矩阵
    const FQuat Rotation = FQuat(FVector::UpVector, Angle);
    
    // 旋转上弧连接点
    for (int32 i = 0; i < TopArcConnectionPoints.Num(); ++i)
    {
        int32 VertexIndex = TopArcConnectionPoints[i];
        FVector OriginalPos = GetPosByIndex(VertexIndex);
        FVector RotatedPos = Rotation.RotateVector(OriginalPos);
        FVector Normal = FVector(RotatedPos.X, RotatedPos.Y, 0.0f).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector(1.0f, 0.0f, 0.0f);
        }
        
        // 计算正确的UV坐标：基于原始位置在弧线上的位置，与起始端盖保持一致
        float U = static_cast<float>(i) / FMath::Max(1, TopArcConnectionPoints.Num() - 1);
        float V = 0.0f; // 上弧的V坐标
        FVector2D UV(U, V);
        
        int32 NewVertexIndex = GetOrAddVertex(RotatedPos, Normal, UV);
        RotatedTopArcPoints.Add(NewVertexIndex);
    }
    
    // 旋转侧面连接点
    for (int32 i = 0; i < SideConnectionPoints.Num(); ++i)
    {
        int32 VertexIndex = SideConnectionPoints[i];
        FVector OriginalPos = GetPosByIndex(VertexIndex);
        FVector RotatedPos = Rotation.RotateVector(OriginalPos);
        FVector Normal = FVector(RotatedPos.X, RotatedPos.Y, 0.0f).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector(1.0f, 0.0f, 0.0f);
        }
        
        // 计算正确的UV坐标：基于高度比例，与起始端盖保持一致
        float HeightRatio = (OriginalPos.Z + Params.GetHalfHeight()) / Params.Height;
        float U = 0.0f; // 侧面的U坐标
        float V = HeightRatio; // 侧面的V坐标基于高度
        FVector2D UV(U, V);
        
        int32 NewVertexIndex = GetOrAddVertex(RotatedPos, Normal, UV);
        RotatedSidePoints.Add(NewVertexIndex);
    }
    
    // 旋转下弧连接点
    for (int32 i = 0; i < BottomArcConnectionPoints.Num(); ++i)
    {
        int32 VertexIndex = BottomArcConnectionPoints[i];
        FVector OriginalPos = GetPosByIndex(VertexIndex);
        FVector RotatedPos = Rotation.RotateVector(OriginalPos);
        FVector Normal = FVector(RotatedPos.X, RotatedPos.Y, 0.0f).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector(1.0f, 0.0f, 0.0f);
        }
        
        // 计算正确的UV坐标：基于原始位置在弧线上的位置，与起始端盖保持一致
        float U = static_cast<float>(i) / FMath::Min(1, BottomArcConnectionPoints.Num() - 1);
        float V = 1.0f; // 下弧的V坐标
        FVector2D UV(U, V);
        
        int32 NewVertexIndex = GetOrAddVertex(RotatedPos, Normal, UV);
        RotatedBottomArcPoints.Add(NewVertexIndex);
    }
    
    // 生成结束端盖的三角形（反转渲染方向）
    const FVector TopCenterPos(0.0f, 0.0f, Params.GetHalfHeight());
    const FVector BottomCenterPos(0.0f, 0.0f, -Params.GetHalfHeight());
    const int32 TopCenterVertex = GetOrAddVertex(TopCenterPos, {0.0f, 0.0f, 1.0f}, FVector2D(0.5f, 0.5f));
    const int32 BottomCenterVertex = GetOrAddVertex(BottomCenterPos, {0.0f, 0.0f, -1.0f}, FVector2D(0.5f, 0.5f));
    
    const int32 CenterVertex = GetOrAddVertex(FVector(0, 0, 0), FVector(1.0f, 0.0f, 0.0f), FVector2D(0.5f, 0.5f));

    // 上倒角的弧（反转方向）
    for (int32 i = 0; i < RotatedTopArcPoints.Num() - 1; ++i)
    {
        const int32 V1 = RotatedTopArcPoints[i];
        const int32 V2 = RotatedTopArcPoints[i + 1];

        AddTriangle(V2, V1, TopCenterVertex); // 反转顶点顺序
    }
    AddTriangle(TopCenterVertex, CenterVertex, RotatedSidePoints[0]); // 反转顶点顺序

    // 侧边（反转方向）
    for (int32 i = 0; i < RotatedSidePoints.Num() - 1; ++i)
    {
        const int32 V1 = RotatedSidePoints[i];
        const int32 V2 = RotatedSidePoints[i + 1];

        AddTriangle(V2, V1, CenterVertex); // 反转顶点顺序
    }

    AddTriangle(CenterVertex, BottomCenterVertex, RotatedSidePoints[RotatedSidePoints.Num() - 1]); // 反转顶点顺序

    // 下倒角的弧（反转方向）
    for (int32 i = 0; i < RotatedBottomArcPoints.Num() - 1; ++i)
    {
        const int32 V1 = RotatedBottomArcPoints[i];
        const int32 V2 = RotatedBottomArcPoints[i + 1];

        AddTriangle(V2, V1, BottomCenterVertex); // 反转顶点顺序
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

        if (SideIndex == 0)
        {
            if (bIsTop)
            {
                RecordTopArcConnectionPoint(V1);
            }
            else
            {
                RecordBottomArcConnectionPoint(V1);
            }
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

}

void FFrustumBuilder::GenerateBevelGeometry(bool bIsTop)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSegments = Params.BevelSegments;
    
    if (BevelRadius <= 0.0f || BevelSegments <= 0)
    {
        return;
    }

    // 根据是顶部还是底部选择参数
    const float Radius = bIsTop ? Params.TopRadius : Params.BottomRadius;
    const int32 Sides = bIsTop ? Params.TopSides : Params.BottomSides;
    const TArray<int32>& SideRing = bIsTop ? SideTopRing : SideBottomRing;
    
    // 修复上倒角位置计算：直接使用侧边环的实际位置，确保与侧边环完全一致
    const float StartZ = GetPosByIndex(SideRing[0]).Z;
    const float EndZ = bIsTop ? HalfHeight : -HalfHeight;

    TArray<int32> PrevRing;
    const float AngleStep = CalculateAngleStep(Sides);
    TArray<int32> StartRingForEndCap;

    for (int32 i = 0; i <= BevelSegments; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSegments;

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
            RecordTopArcConnectionPoint(StartRingForEndCap[i]);
        }
    }
    else
    {
        for (int i = 0; i < StartRingForEndCap.Num(); i++)
        {
            RecordBottomArcConnectionPoint(StartRingForEndCap[i]);
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
void FFrustumBuilder::RecordTopArcConnectionPoint(int32 VertexIndex)
{
    TopArcConnectionPoints.Add(VertexIndex);
    
    UE_LOG(LogTemp, Log, TEXT("RecordTopArcConnectionPoint - 记录上弧连接点: 顶点索引=%d"), VertexIndex);
}

void FFrustumBuilder::RecordSideConnectionPoint(int32 VertexIndex)
{
    SideConnectionPoints.Add(VertexIndex);
    
    UE_LOG(LogTemp, Log, TEXT("RecordSideConnectionPoint - 记录侧面连接点: 顶点索引=%d"), VertexIndex);
}

void FFrustumBuilder::RecordBottomArcConnectionPoint(int32 VertexIndex)
{
    BottomArcConnectionPoints.Add(VertexIndex);
    
    UE_LOG(LogTemp, Log, TEXT("RecordBottomArcConnectionPoint - 记录下弧连接点: 顶点索引=%d"), VertexIndex);
}

const TArray<int32>& FFrustumBuilder::GetTopArcConnectionPoints() const
{
    return TopArcConnectionPoints;
}

const TArray<int32>& FFrustumBuilder::GetSideConnectionPoints() const
{
    return SideConnectionPoints;
}

const TArray<int32>& FFrustumBuilder::GetBottomArcConnectionPoints() const
{
    return BottomArcConnectionPoints;
}

void FFrustumBuilder::ClearAllConnectionPoints()
{
    TopArcConnectionPoints.Empty();
    SideConnectionPoints.Empty();
    BottomArcConnectionPoints.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("ClearAllConnectionPoints - 已清除所有连接点"));
}
