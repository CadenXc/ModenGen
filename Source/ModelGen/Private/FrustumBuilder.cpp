// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"
#include "ModelGenMeshData.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"

FFrustumBuilder::FFrustumBuilder(const FFrustumParameters& InParams)
    : Params(InParams)
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
    UE_LOG(LogTemp, Log, TEXT("FFrustumBuilder::Generate - Generated %d vertices, %d triangles"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
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
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    
    // 计算倒角高度
    const float TopBevelHeight = FMath::Min(BevelRadius, Params.TopRadius);
    const float BottomBevelHeight = FMath::Min(BevelRadius, Params.BottomRadius);
    
    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomBevelHeight;
    const float EndZ = HalfHeight - TopBevelHeight;
    
    // 生成几何部件 - 确保不重叠
    if (EndZ > StartZ) // 只有当主体高度大于0时才生成主体
    {
        // 生成侧边几何体 - 使用完整高度范围以正确连接顶底面
        //GenerateSideGeometry(-HalfHeight, HalfHeight);
        CreateSideGeometry();
    }
    
    // 生成倒角几何体
    if (Params.BevelRadius > 0.0f)
    {
        GenerateTopBevelGeometry(EndZ);
        GenerateBottomBevelGeometry(StartZ);
    }
    
    // 生成顶底面几何体
    GenerateTopGeometry(HalfHeight);
    GenerateBottomGeometry(-HalfHeight);
    
    // 生成端面（如果不是完整的360度）
    if (Params.ArcAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        GenerateEndCaps();
    }
}

void FFrustumBuilder::CreateSideGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();
    
    // 生成上下两个环的顶点
    TArray<int32> TopRing;
    TArray<int32> BottomRing;
    
    // 存储下底顶点和上底顶点的对应关系
    TArray<int32> BottomToTopMapping;

    // 生成顶环顶点
    const float TopRadius = Params.TopRadius - Params.BevelRadius;
    const float TopAngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.TopSides;
    for (int32 i = 0; i < Params.TopSides; ++i)
    {
        const float LocalTopRadius = FMath::Max(TopRadius, Params.MinBendRadius);
        const float Angle = i * TopAngleStep;
        const float X = LocalTopRadius * FMath::Cos(Angle);
        const float Y = LocalTopRadius * FMath::Sin(Angle);
        const FVector Pos(X, Y, HalfHeight);
        const FVector Normal(0.0f, 0.0f, 1.0f);
        const FVector2D UV(static_cast<float>(i) / Params.TopSides, 0.0f);
        
        const int32 VertexIndex = GetOrAddVertex(Pos, Normal, UV);
        TopRing.Add(VertexIndex);
    }
    
    // 生成底环顶点
    const float BottomRadius = Params.BottomRadius - Params.BevelRadius;
    const float BottomAngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.BottomSides;
    for (int32 i = 0; i < Params.BottomSides; ++i)
    {
        const float LocalBottomRadius = FMath::Max(BottomRadius, Params.MinBendRadius);
        const float Angle = i * BottomAngleStep;
        const float X = LocalBottomRadius * FMath::Cos(Angle);
        const float Y = LocalBottomRadius * FMath::Sin(Angle);
        const FVector Pos(X, Y, -HalfHeight);
        const FVector Normal(0.0f, 0.0f, -1.0f);
        const FVector2D UV(static_cast<float>(i) / Params.BottomSides, 1.0f);

        const int32 VertexIndex = GetOrAddVertex(Pos, Normal, UV);
        BottomRing.Add(VertexIndex);
    }

    // 不受Params.MinBendRadius影响的上下环，为了计算中间值。
    TArray<int32> TopRingOrigin;
    TArray<int32> BottomRingOrigin;
    for (int32 i = 0; i < Params.TopSides; ++i)
    {
        const float Angle = i * TopAngleStep;
        const float X = TopRadius * FMath::Cos(Angle);
        const float Y = TopRadius * FMath::Sin(Angle);
        const FVector Pos(X, Y, HalfHeight);
        const FVector Normal(0.0f, 0.0f, 1.0f);
        const FVector2D UV(static_cast<float>(i) / Params.TopSides, 0.0f);
        
        const int32 VertexIndex = GetOrAddVertex(Pos, Normal, UV);
        TopRingOrigin.Add(VertexIndex);
    }
    
    // 生成底环顶点
    for (int32 i = 0; i < Params.BottomSides; ++i)
    {
        const float Angle = i * BottomAngleStep;
        const float X = BottomRadius * FMath::Cos(Angle);
        const float Y = BottomRadius * FMath::Sin(Angle);
        const FVector Pos(X, Y, -HalfHeight);
        const FVector Normal(0.0f, 0.0f, -1.0f);
        const FVector2D UV(static_cast<float>(i) / Params.BottomSides, 1.0f);

        const int32 VertexIndex = GetOrAddVertex(Pos, Normal, UV);
        BottomRingOrigin.Add(VertexIndex);
    }
    
    // 计算并存储下底顶点和上底顶点的对应关系
    BottomToTopMapping.SetNum(BottomRingOrigin.Num());
    for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
    {
        const float BottomRatio = static_cast<float>(BottomIndex) / BottomRingOrigin.Num();
        const int32 TopIndex = FMath::Clamp(FMath::RoundToInt(BottomRatio * TopRingOrigin.Num()), 0, TopRingOrigin.Num() - 1);
        BottomToTopMapping[BottomIndex] = TopIndex;
    }

    TArray<TArray<int32>> VertexRings;
    VertexRings.Add(BottomRing);
    //生成中间环
// 中间环顶点生成
    if (Params.HeightSegments > 1)
    {
        const float HeightStep = Params.Height / Params.HeightSegments;

        for (int32 h = 1; h < Params.HeightSegments; ++h)
        {
            const float CurrentHeight = -HalfHeight + h * HeightStep;
            const float HeightRatio = static_cast<float>(h) / Params.HeightSegments;
            const float BendFactor = 1 + Params.BendAmount * FMath::Sin(HeightRatio * PI);

            TArray<int32> CurrentRing;

            // 为每个下底顶点生成对应的中间顶点
            for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
            {
                const int32 TopIndex = BottomToTopMapping[BottomIndex];

                // 获取对应的上下顶点位置
                const FVector& TopPos = GetPosByIndex(TopRingOrigin[TopIndex]);
                const FVector& BottomPos = GetPosByIndex(BottomRingOrigin[BottomIndex]);


                // 根据Z值对X和Y进行线性插值
                const float XR = FMath::Lerp(BottomPos.X, TopPos.X, HeightRatio) * BendFactor;
                const float YR = FMath::Lerp(BottomPos.Y, TopPos.Y, HeightRatio) * BendFactor;
                float X = 0;
                float Y = 0;
                const FVector CenterPoint(0.0f, 0.0f, CurrentHeight);

                // 计算弯曲后的顶点位置
                FVector BentPos(XR, YR, CurrentHeight);

                // 计算顶点到中心点的距离
                const float DistanceToCenter = FMath::Sqrt(FMath::Pow(XR, 2) + FMath::Pow(YR, 2));

                // 如果距离小于最小弯曲半径，则调整到最小距离
                if (DistanceToCenter < Params.MinBendRadius)
                {
                    // 计算从中心点到当前顶点的方向向量
                    FVector Direction = (BentPos - CenterPoint).GetSafeNormal();

                    // 将顶点调整到最小弯曲半径的距离
                    BentPos = CenterPoint + Direction * Params.MinBendRadius;

                    // 更新X和Y坐标
                    X = BentPos.X;
                    Y = BentPos.Y;
                }
                else
                {
                    // 距离足够，直接使用弯曲后的坐标
                    X = XR;
                    Y = YR;
                }


                // 生成插值后的顶点位置
                const FVector InterpolatedPos(X, Y, CurrentHeight);

                // 计算法线（指向外侧）
                const FVector Normal = InterpolatedPos.GetSafeNormal();

                // 计算UV坐标
                const FVector2D UV(
                    static_cast<float>(BottomIndex) / Params.BottomSides,
                    HeightRatio
                );

                // 添加顶点到MeshData并获取索引
                const int32 VertexIndex = GetOrAddVertex(InterpolatedPos, Normal, UV);
                CurrentRing.Add(VertexIndex);
            }

            VertexRings.Add(CurrentRing);
        }
    }
    VertexRings.Add(TopRingOrigin);


    // 连接三角形
    for (int i = 0; i < VertexRings.Num() - 1; i++)
    {
        TArray<int32> CurrentRing = VertexRings[i];
        TArray<int32> NextRing = VertexRings[i + 1];

		for (int32 CurrentIndex = 0; CurrentIndex < CurrentRing.Num(); ++CurrentIndex)
		{
			const int32 NextTopIndex = (CurrentIndex + 1) % CurrentRing.Num();
			
			// 计算对应的下顶点索引
			const float TopRatio = static_cast<float>(CurrentIndex) / CurrentRing.Num();
			const float NextTopRatio = static_cast<float>(NextTopIndex) / CurrentRing.Num();
			
			const int32 BottomIndex = FMath::Clamp(FMath::RoundToInt(TopRatio * NextRing.Num()), 0, NextRing.Num() - 1);
			const int32 NextBottomIndex = FMath::Clamp(FMath::RoundToInt(NextTopRatio * NextRing.Num()), 0, NextRing.Num() - 1);
			
			// 添加三角形：上顶点 -> 下顶点 -> 下一个上顶点
			AddTriangle(CurrentRing[CurrentIndex], NextRing[BottomIndex], CurrentRing[NextTopIndex]);
			
			// 如果下顶点不同，添加第二个三角形形成四边形
			if (BottomIndex != NextBottomIndex)
			{
				AddTriangle(CurrentRing[NextTopIndex], NextRing[BottomIndex], NextRing[NextBottomIndex]);
			}
		}

		for (int32 NextIndex = 0; NextIndex < NextRing.Num(); ++NextIndex)
		{
			const int32 NextBottomIndex = (NextIndex + 1) % NextRing.Num();
			
            const float NextRatio = static_cast<float>(NextIndex) / NextRing.Num();
            const float NextBottomRatio = static_cast<float>(NextBottomIndex) / NextRing.Num();

            const int32 TopIndex = FMath::Clamp(FMath::RoundToInt(NextRatio * CurrentRing.Num()), 0, CurrentRing.Num() - 1);
            const int32 NextTopIndex = FMath::Clamp(FMath::RoundToInt(NextBottomRatio * CurrentRing.Num()), 0, CurrentRing.Num() - 1);

			
			// 添加三角形：下顶点 -> 上顶点 -> 下一个下顶点
			AddTriangle(NextRing[NextIndex], CurrentRing[TopIndex], NextRing[NextBottomIndex]);
			
			// 如果上顶点不同，添加第二个三角形形成四边形
			if (TopIndex != NextTopIndex)
			{
				AddTriangle(NextRing[NextBottomIndex], CurrentRing[TopIndex], CurrentRing[NextTopIndex]);
			}
		}
    }
}

void FFrustumBuilder::GenerateTopGeometry(float Z)
{
    const float HalfHeight = Params.GetHalfHeight();
    
    // 顶面法线向上
    FVector Normal(0.0f, 0.0f, 1.0f);
    if (Params.bFlipNormals)
    {
        Normal = -Normal;
    }
    
    // 添加中心顶点
    const FVector CenterPos(0.0f, 0.0f, Z);
    const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, FVector2D(0.5f, 0.5f));
    
    // 使用顶面自己的边数
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.TopSides;
    
    for (int32 SideIndex = 0; SideIndex < Params.TopSides; ++SideIndex)
    {
        const float CurrentAngle = SideIndex * AngleStep;
        const float NextAngle = (SideIndex + 1) * AngleStep;
        
        // 顶面使用固定半径，不受弯曲影响
        const float CurrentRadius = Params.TopRadius - Params.BevelRadius;
        
        // 计算顶点位置
        const FVector CurrentPos(CurrentRadius * FMath::Cos(CurrentAngle), 
                               CurrentRadius * FMath::Sin(CurrentAngle), Z);
        const FVector NextPos(CurrentRadius * FMath::Cos(NextAngle), 
                            CurrentRadius * FMath::Sin(NextAngle), Z);
        
        // 计算UV坐标
        const FVector2D UV1(static_cast<float>(SideIndex) / Params.TopSides, 0.0f);
        const FVector2D UV2(static_cast<float>(SideIndex + 1) / Params.TopSides, 0.0f);
        
        // 添加顶点
        const int32 V1 = GetOrAddVertex(CurrentPos, Normal, UV1);
        const int32 V2 = GetOrAddVertex(NextPos, Normal, UV2);
        
        // 添加三角形 - 确保正确的顶点顺序（逆时针）
        AddTriangle(CenterVertex, V2, V1);
    }
}

void FFrustumBuilder::GenerateBottomGeometry(float Z)
{
    const float HalfHeight = Params.GetHalfHeight();
    
    // 底面法线向下
    FVector Normal(0.0f, 0.0f, -1.0f);
    if (Params.bFlipNormals)
    {
        Normal = -Normal;
    }
    
    // 添加中心顶点
    const FVector CenterPos(0.0f, 0.0f, Z);
    const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, FVector2D(0.5f, 0.5f));
    
    // 使用底面自己的边数
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.BottomSides;
    
    for (int32 SideIndex = 0; SideIndex < Params.BottomSides; ++SideIndex)
    {
        const float CurrentAngle = SideIndex * AngleStep;
        const float NextAngle = (SideIndex + 1) * AngleStep;
        
        // 底面使用固定半径，不受弯曲影响
        const float CurrentRadius = Params.BottomRadius - Params.BevelRadius;
        
        // 计算顶点位置
        const FVector CurrentPos(CurrentRadius * FMath::Cos(CurrentAngle), 
                               CurrentRadius * FMath::Sin(CurrentAngle), Z);
        const FVector NextPos(CurrentRadius * FMath::Cos(NextAngle), 
                            CurrentRadius * FMath::Sin(NextAngle), Z);
        
        // 计算UV坐标
        const FVector2D UV1(static_cast<float>(SideIndex) / Params.BottomSides, 0.0f);
        const FVector2D UV2(static_cast<float>(SideIndex + 1) / Params.BottomSides, 0.0f);
        
        // 添加顶点
        const int32 V1 = GetOrAddVertex(CurrentPos, Normal, UV1);
        const int32 V2 = GetOrAddVertex(NextPos, Normal, UV2);
        
        // 添加三角形 - 确保正确的顶点顺序（顺时针，因为底面法线朝下）
        AddTriangle(CenterVertex, V1, V2);
    }
}

void FFrustumBuilder::GenerateTopBevelGeometry(float StartZ)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSections = Params.BevelSections;
    
    // 使用顶面自己的边数
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.TopSides;

    if (BevelRadius <= 0.0f || BevelSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= BevelSections; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSections; // 沿倒角弧线的进度

        // 倒角起点：主侧面几何体的最顶层边缘顶点
        const float Alpha_Height = (StartZ + HalfHeight) / Params.Height;
        const float RadiusAtZ = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Height);
        const float BendFactor = FMath::Sin(Alpha_Height * PI);
        const float BentRadius = FMath::Max(
            RadiusAtZ + Params.BendAmount * BendFactor * RadiusAtZ,
            Params.MinBendRadius
        );

        // 倒角终点：在顶面上，半径减去倒角半径
        const float TopRadius = FMath::Max(0.0f, Params.TopRadius - Params.BevelRadius);

        // 使用线性插值而不是贝塞尔曲线
        const float CurrentRadius = FMath::Lerp(BentRadius, TopRadius, alpha);
        const float CurrentZ = FMath::Lerp(StartZ, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.TopSides + 1);

        for (int32 s = 0; s <= Params.TopSides; ++s)
        {
            const float angle = s * AngleStep; // 圆周旋转角度

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 改进的法线计算：使用连续线性插值，确保法线平滑过渡
            const FVector SideNormal = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector TopNormal = FVector(0, 0, 1.0f);
            FVector Normal = FMath::Lerp(SideNormal, TopNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            FVector ToCenter = FVector(Position.X, Position.Y, 0).GetSafeNormal();
            if (FVector::DotProduct(Normal, ToCenter) < 0)
            {
                Normal = -Normal;
            }

            // 确保法线方向正确
            if (Normal.Z < 0.0f)
            {
                Normal = -Normal;
            }

            // 如果启用了法线反转选项
            if (Params.bFlipNormals)
            {
                Normal = -Normal;
            }

            // 计算 UV - 使用与主体一致的UV映射
            const float U = static_cast<float>(s) / Params.TopSides;
            const float V = (Position.Z + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 只在有前一个环时才生成四边形，避免重复
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.TopSides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                // 顶部倒角：确保法线朝外，弧线方向正确
                // 使用一致的顶点顺序，避免重叠
                AddQuad(V00, V10, V11, V01);
            }
        }
        PrevRing = CurrentRing;
    }
}

void FFrustumBuilder::GenerateBottomBevelGeometry(float StartZ)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSections = Params.BevelSections;
    
    // 使用底面自己的边数
    const float AngleStep = FMath::DegreesToRadians(Params.ArcAngle) / Params.BottomSides;

    if (BevelRadius <= 0.0f || BevelSections <= 0) return;

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= BevelSections; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSections;

        // 倒角起点：主侧面几何体的最底层边缘顶点
        const float Alpha_Height = (StartZ + HalfHeight) / Params.Height;
        const float RadiusAtZ = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Height);
        const float BendFactor = FMath::Sin(Alpha_Height * PI);
        const float BentRadius = FMath::Max(
            RadiusAtZ + Params.BendAmount * BendFactor * RadiusAtZ,
            Params.MinBendRadius
        );

        // 倒角终点：在底面上，半径减去倒角半径
        const float BottomRadius = FMath::Max(0.0f, Params.BottomRadius - Params.BevelRadius);

        // 使用线性插值而不是贝塞尔曲线
        const float CurrentRadius = FMath::Lerp(BentRadius, BottomRadius, alpha);
        const float CurrentZ = FMath::Lerp(StartZ, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.BottomSides + 1);

        for (int32 s = 0; s <= Params.BottomSides; ++s)
        {
            const float angle = s * AngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 改进的法线计算：使用连续线性插值，确保法线平滑过渡
            const FVector SideNormal = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector BottomNormal = FVector(0, 0, -1.0f);
            FVector Normal = FMath::Lerp(SideNormal, BottomNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            FVector ToCenter = FVector(Position.X, Position.Y, 0).GetSafeNormal();
            if (FVector::DotProduct(Normal, ToCenter) < 0)
            {
                Normal = -Normal;
            }

            // 确保法线方向正确
            if (Normal.Z > 0.0f)
            {
                Normal = -Normal;
            }

            // 如果启用了法线反转选项
            if (Params.bFlipNormals)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Params.BottomSides;
            const float V = (Position.Z + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 只在有前一个环时才生成四边形，避免重复
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.BottomSides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];

                // 底部倒角需要不同的顶点顺序，因为法线朝下
                // 使用一致的顶点顺序，避免重叠
                AddQuad(V00, V01, V11, V10);
            }
        }
        PrevRing = CurrentRing;
    }
}

void FFrustumBuilder::GenerateEndCaps()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float StartAngle = 0.0f;
    const float EndAngle = FMath::DegreesToRadians(Params.ArcAngle);
    const float BevelRadius = Params.BevelRadius;

    // 起始端面法线：指向圆弧外部（远离中心）
    const FVector StartNormal = FVector(FMath::Sin(StartAngle), -FMath::Cos(StartAngle), 0.0f);

    // 结束端面法线：指向圆弧外部（远离中心）
    const FVector EndNormal = FVector(FMath::Sin(EndAngle), -FMath::Cos(EndAngle), 0.0f);

    // 计算倒角高度
    const float TopBevelHeight = FMath::Min(BevelRadius, Params.TopRadius);
    const float BottomBevelHeight = FMath::Min(BevelRadius, Params.BottomRadius);

    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomBevelHeight;
    const float EndZ = HalfHeight - TopBevelHeight;

    // 创建端面几何体 - 使用三角形连接到中心点
    GenerateEndCapTriangles(StartAngle, StartNormal, true);  // 起始端面
    GenerateEndCapTriangles(EndAngle, EndNormal, false);     // 结束端面
}

void FFrustumBuilder::GenerateEndCapTriangles(float Angle, const FVector& Normal, bool IsStart)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    
    // 计算倒角高度
    const float TopBevelHeight = FMath::Min(BevelRadius, Params.TopRadius);
    const float BottomBevelHeight = FMath::Min(BevelRadius, Params.BottomRadius);
    
    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomBevelHeight;
    const float EndZ = HalfHeight - TopBevelHeight;

    // 中心点（0,0,0）
    const int32 CenterVertex = GetOrAddVertex(
        FVector(0, 0, 0),
        Normal,
        FVector2D(0.5f, 0.5f)
    );

    // 按照指定顺序存储顶点：上底中心 -> 上倒角圆弧 -> 侧边 -> 下倒角圆弧 -> 下底中心
    TArray<int32> OrderedVertices;
    
    // 1. 上底中心顶点
    const int32 TopCenterVertex = GetOrAddVertex(
        FVector(0, 0, HalfHeight),
        Normal,
        FVector2D(0.5f, 1.0f)
    );
    OrderedVertices.Add(TopCenterVertex);

    // 2. 上倒角弧线顶点（从顶面到侧边）
    if (Params.BevelRadius > 0.0f)
    {
        const int32 TopBevelSections = Params.BevelSections;
        for (int32 i = 0; i < TopBevelSections; ++i) // 从顶面到侧边
        {
            const float Alpha = static_cast<float>(i) / TopBevelSections;
            
            // 从顶面开始（HalfHeight位置）到侧边（EndZ位置）
            const float CurrentZ = FMath::Lerp(HalfHeight, EndZ, Alpha);
            
            // 计算侧边结束半径（在EndZ位置）
            const float Alpha_EndZ = (EndZ + HalfHeight) / Params.Height;
            const float Radius_EndZ = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_EndZ);
            const float BendFactor_EndZ = FMath::Sin(Alpha_EndZ * PI);
            const float BentRadius_EndZ = FMath::Max(
                Radius_EndZ + Params.BendAmount * BendFactor_EndZ * Radius_EndZ,
                Params.MinBendRadius
            );
            
            // 从顶面半径到侧边半径（减去倒角半径，与主体几何体保持一致）
            const float TopRadius = FMath::Max(0.0f, Params.TopRadius - Params.BevelRadius);
            const float CurrentRadius = FMath::Lerp(TopRadius, BentRadius_EndZ, Alpha);
            
            const FVector BevelPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
            const int32 BevelVertex = GetOrAddVertex(BevelPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(BevelVertex);
        }
    }

    // 3. 侧边顶点（从上到下）
    for (int32 h = 0; h <= Params.HeightSegments; ++h)
    {
        const float Z = FMath::Lerp(EndZ, StartZ, static_cast<float>(h) / Params.HeightSegments);
        const float Alpha = (Z + HalfHeight) / Params.Height;

        // 计算当前高度对应的半径
        const float Radius = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha);
        const float BendFactor = FMath::Sin(Alpha * PI);
        const float BentRadius = FMath::Max(
            Radius + Params.BendAmount * BendFactor * Radius,
            Params.MinBendRadius
        );

        const FVector EdgePos = FVector(BentRadius * FMath::Cos(Angle), BentRadius * FMath::Sin(Angle), Z);
        const int32 EdgeVertex = GetOrAddVertex(EdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, Alpha));
        OrderedVertices.Add(EdgeVertex);
    }

    // 4. 下倒角弧线顶点（从侧边到底面）
    if (Params.BevelRadius > 0.0f)
    {
        const int32 BottomBevelSections = Params.BevelSections;
        for (int32 i = 0; i < BottomBevelSections; ++i) // 从侧边到底面
        {
            const float Alpha = static_cast<float>(i) / BottomBevelSections;
            
            // 从侧边开始（StartZ位置）到底面（-HalfHeight位置）
            const float CurrentZ = FMath::Lerp(StartZ, -HalfHeight, Alpha);
            
            // 计算侧边起始半径（在StartZ位置）
            const float Alpha_StartZ = (StartZ + HalfHeight) / Params.Height;
            const float Radius_StartZ = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_StartZ);
            const float BendFactor_StartZ = FMath::Sin(Alpha_StartZ * PI);
            const float BentRadius_StartZ = FMath::Max(
                Radius_StartZ + Params.BendAmount * BendFactor_StartZ * Radius_StartZ,
                Params.MinBendRadius
            );
            
            // 从侧边半径到底面半径（减去倒角半径，与主体几何体保持一致）
            const float BottomRadius = FMath::Max(0.0f, Params.BottomRadius - Params.BevelRadius);
            const float CurrentRadius = FMath::Lerp(BentRadius_StartZ, BottomRadius, Alpha);
            
            const FVector BevelPos = FVector(CurrentRadius * FMath::Cos(Angle), CurrentRadius * FMath::Sin(Angle), CurrentZ);
            const int32 BevelVertex = GetOrAddVertex(BevelPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(BevelVertex);
        }
    }

    // 5. 下底中心顶点
    const int32 BottomCenterVertex = GetOrAddVertex(
        FVector(0, 0, -HalfHeight),
        Normal,
        FVector2D(0.5f, 0.0f)
    );
    OrderedVertices.Add(BottomCenterVertex);

    // 生成三角形：每两个相邻顶点与中心点形成三角形
    for (int32 i = 0; i < OrderedVertices.Num() - 1; ++i)
    {
        const int32 V1 = OrderedVertices[i];
        const int32 V2 = OrderedVertices[i + 1];
        
        // 创建三角形：V1 -> V2 -> 中心点
        if (IsStart)
        {
            AddTriangle(V1, V2, CenterVertex);
        }
        else
        {
            AddTriangle(V2, V1, CenterVertex);
        }
    }
}

void FFrustumBuilder::GenerateBevelArcTriangles(float Angle, const FVector& Normal, bool IsStart, 
                                                  float Z1, float Z2, bool IsTop)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadius = Params.BevelRadius;
    const int32 BevelSections = Params.BevelSections;
    
    // 中心点（0,0,0）
    const int32 CenterVertex = GetOrAddVertex(
        FVector(0, 0, 0),
        Normal,
        FVector2D(0.5f, 0.5f)
    );

    // 计算倒角弧线的起点和终点半径
    float StartRadius, EndRadius;
    if (IsTop)
    {
        // 顶部倒角：从侧面边缘到顶面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Params.Height;
        const float Radius_Start = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Start);
        const float BendFactor_Start = FMath::Sin(Alpha_Start * PI);
        StartRadius = FMath::Max(
            Radius_Start + Params.BendAmount * BendFactor_Start * Radius_Start,
            Params.MinBendRadius
        );
        EndRadius = FMath::Max(0.0f, Params.TopRadius - Params.BevelRadius);
    }
    else
    {
        // 底部倒角：从侧面边缘到底面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Params.Height;
        const float Radius_Start = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Start);
        const float BendFactor_Start = FMath::Sin(Alpha_Start * PI);
        StartRadius = FMath::Max(
            Radius_Start + Params.BendAmount * BendFactor_Start * Radius_Start,
            Params.MinBendRadius
        );
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
        const int32 ArcVertex = GetOrAddVertex(ArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));

        // 下一个倒角弧线点
        const float NextAlpha = static_cast<float>(i + 1) / BevelSections;
        const float NextRadius = FMath::Lerp(StartRadius, EndRadius, NextAlpha);
        const float NextZ = FMath::Lerp(Z1, Z2, NextAlpha);

        const FVector NextArcPos = FVector(NextRadius * FMath::Cos(Angle), NextRadius * FMath::Sin(Angle), NextZ);
        const int32 NextArcVertex = GetOrAddVertex(NextArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (NextZ + HalfHeight) / Params.Height));

        // 创建三角形：当前弧线点 -> 下一个弧线点 -> 中心点
        AddTriangle(ArcVertex, NextArcVertex, CenterVertex);
    }
}

void FFrustumBuilder::GenerateBevelArcTrianglesWithCaps(float Angle, const FVector& Normal, bool IsStart, 
                                                          float Z1, float Z2, bool IsTop, int32 CenterVertex, 
                                                          int32 CapCenterVertex)
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
        const float BendFactor_Start = FMath::Sin(Alpha_Start * PI);
        StartRadius = FMath::Max(
            Radius_Start + Params.BendAmount * BendFactor_Start * Radius_Start,
            Params.MinBendRadius
        );
        EndRadius = FMath::Max(0.0f, Params.TopRadius - Params.BevelRadius);
    }
    else
    {
        // 底部倒角：从侧面边缘到底面边缘
        const float Alpha_Start = (Z1 + HalfHeight) / Params.Height;
        const float Radius_Start = FMath::Lerp(Params.BottomRadius, Params.TopRadius, Alpha_Start);
        const float BendFactor_Start = FMath::Sin(Alpha_Start * PI);
        StartRadius = FMath::Max(
            Radius_Start + Params.BendAmount * BendFactor_Start * Radius_Start,
            Params.MinBendRadius
        );
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
        const int32 ArcVertex = GetOrAddVertex(ArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));

        // 下一个倒角弧线点
        const float NextAlpha = static_cast<float>(i + 1) / BevelSections;
        const float NextRadius = FMath::Lerp(StartRadius, EndRadius, NextAlpha);
        const float NextZ = FMath::Lerp(Z1, Z2, NextAlpha);

        const FVector NextArcPos = FVector(NextRadius * FMath::Cos(Angle), NextRadius * FMath::Sin(Angle), NextZ);
        const int32 NextArcVertex = GetOrAddVertex(NextArcPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (NextZ + HalfHeight) / Params.Height));

        // 创建三角形：当前弧线点 -> 下一个弧线点 -> 中心点（0,0,0）
        AddTriangle(ArcVertex, NextArcVertex, CenterVertex);
    }

    // 连接最边缘的点到上/下底中心点
    // 最边缘的点是倒角弧线的起点（i=0）
    const float StartAlpha = 0.0f;
    const float StartRadius_Edge = FMath::Lerp(StartRadius, EndRadius, StartAlpha);
    const float StartZ_Edge = FMath::Lerp(Z1, Z2, StartAlpha);
    const FVector StartEdgePos = FVector(StartRadius_Edge * FMath::Cos(Angle), StartRadius_Edge * FMath::Sin(Angle), StartZ_Edge);
    const int32 StartEdgeVertex = GetOrAddVertex(StartEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (StartZ_Edge + HalfHeight) / Params.Height));

    // 最边缘的点是倒角弧线的终点（i=BevelSections）
    const float EndAlpha = 1.0f;
    const float EndRadius_Edge = FMath::Lerp(StartRadius, EndRadius, EndAlpha);
    const float EndZ_Edge = FMath::Lerp(Z1, Z2, EndAlpha);
    const FVector EndEdgePos = FVector(EndRadius_Edge * FMath::Cos(Angle), EndRadius_Edge * FMath::Sin(Angle), EndZ_Edge);
    const int32 EndEdgeVertex = GetOrAddVertex(EndEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (EndZ_Edge + HalfHeight) / Params.Height));

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

FFrustumBuilder::FBevelArcControlPoints FFrustumBuilder::CalculateBevelControlPoints(const FVector& SideVertex, 
                                                                                        const FVector& TopBottomVertex)
{
    FBevelArcControlPoints ControlPoints;
    ControlPoints.StartPoint = SideVertex;
    ControlPoints.EndPoint = TopBottomVertex;
    ControlPoints.ControlPoint = (SideVertex + TopBottomVertex) * 0.5f;
    return ControlPoints;
}

FVector FFrustumBuilder::CalculateBevelArcPoint(const FBevelArcControlPoints& ControlPoints, float t)
{
    // 二次贝塞尔曲线计算
    const float OneMinusT = 1.0f - t;
    return OneMinusT * OneMinusT * ControlPoints.StartPoint + 
           2.0f * OneMinusT * t * ControlPoints.ControlPoint + 
           t * t * ControlPoints.EndPoint;
}

FVector FFrustumBuilder::CalculateBevelArcTangent(const FBevelArcControlPoints& ControlPoints, float t)
{
    // 二次贝塞尔曲线切线计算
    const float OneMinusT = 1.0f - t;
    return 2.0f * OneMinusT * (ControlPoints.ControlPoint - ControlPoints.StartPoint) + 
           2.0f * t * (ControlPoints.EndPoint - ControlPoints.ControlPoint);
}
