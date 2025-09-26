// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"
#include "Frustum.h"
#include "ModelGenMeshData.h"

FFrustumBuilder::FFrustumBuilder(const AFrustum& InFrustum)
    : Frustum(InFrustum)
{
    Clear();
    CalculateAngles();
}

void FFrustumBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    ClearEndCapConnectionPoints();
    // [REVERTED] 不再需要清理顶点缓存
}

bool FFrustumBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Frustum.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    CreateSideGeometry();

    // 生成顺序可以保持不变，逻辑上依然清晰
    // 内联 GenerateTopBevelGeometry
    if (Frustum.BevelRadius > 0.0f)
    {
        GenerateBevelGeometry(EHeightPosition::Top);
    }
    
    // 内联 GenerateBottomBevelGeometry
    if (Frustum.BevelRadius > 0.0f)
    {
        GenerateBevelGeometry(EHeightPosition::Bottom);
    }

    // 内联 GenerateTopGeometry
    GenerateCapGeometry(Frustum.GetHalfHeight(), Frustum.TopSides, Frustum.TopRadius, EHeightPosition::Top);
    
    // 内联 GenerateBottomGeometry
    GenerateCapGeometry(-Frustum.GetHalfHeight(), Frustum.BottomSides, Frustum.BottomRadius, EHeightPosition::Bottom);

    GenerateEndCaps();

    if (!ValidateGeneratedData())
    {
        return false;
    }

    // 计算正确的切线（用于法线贴图等）
    MeshData.CalculateTangents();

    OutMeshData = MeshData;
    return true;
}

// ... (其他未修改的函数保持不变) ...

int32 FFrustumBuilder::CalculateVertexCountEstimate() const
{
	return Frustum.CalculateVertexCountEstimate();
}

int32 FFrustumBuilder::CalculateTriangleCountEstimate() const
{
	return Frustum.CalculateTriangleCountEstimate();
}

void FFrustumBuilder::CreateSideGeometry()
{
	const float HalfHeight = Frustum.GetHalfHeight();
	const float TopBevelStartZ = HalfHeight - CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelStartZ = -HalfHeight + CalculateBevelHeight(Frustum.BottomRadius);

	// 侧边UV映射，使用周长比例计算V值
	// 计算各部分高度和周长
	const float TotalHeight = Frustum.Height;
	const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);
	const float SideHeight = TotalHeight - TopBevelHeight - BottomBevelHeight;
	
	// 计算平均周长用于V值计算
	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;
	
	// 计算V值占比（基于周长比例）
	const float BottomBevelVScale = BottomBevelHeight / AvgCircumference;
	const float SideVScale = SideHeight / AvgCircumference;
	const float TopBevelVScale = TopBevelHeight / AvgCircumference;
	
	// 计算周长比例，用于U值调整
	const float BottomCircumference = 2.0f * PI * Frustum.BottomRadius;
	const float CircumferenceRatio = AvgCircumference / TotalHeight;
	
	// 侧面UV区域：[0.25, 0.75] x [动态V范围] - 根据高度比例动态调整
	// 当没有倒角时，侧面从V=0开始；有倒角时，从倒角结束后开始
	const float SideVStart = (BottomBevelVScale > 0.0f) ? (BottomBevelVScale + 0.001f) : 0.0f;
	const float SideVEnd = SideVStart + SideVScale;
	
	TArray<int32> TopRing = GenerateVertexRing(
		Frustum.TopRadius,
		TopBevelStartZ,
		Frustum.TopSides,
		1.0f, // V坐标 - 使用1.0确保在UV图边缘
		FVector2D(0.25f, SideVStart), // UVOffset - 动态V范围
		FVector2D(0.5f, SideVScale)  // UVScale - 动态V范围
	);

	TArray<int32> BottomRing = GenerateVertexRing(
		Frustum.BottomRadius,
		BottomBevelStartZ,
		Frustum.BottomSides,
		SideVStart, // V坐标 - 连接下倒角
		FVector2D(0.25f, SideVStart), // UVOffset - 动态V范围
		FVector2D(0.5f, SideVScale)  // UVScale - 动态V范围
	);


	// 为插值计算原始（未弯曲）的顶点
	// 中间环的顶点数量应该与下底环相同，但位置是下底到上底的插值
	TArray<int32> TopRingOrigin = GenerateVertexRing(Frustum.TopRadius, HalfHeight, Frustum.TopSides); // 使用上底边数
	TArray<int32> BottomRingOrigin = GenerateVertexRing(Frustum.BottomRadius, -HalfHeight, Frustum.BottomSides);

	// 映射下底环的每个顶点到上底环的对应顶点
	TArray<int32> BottomToTopMapping;
	BottomToTopMapping.SetNum(BottomRingOrigin.Num());
	for (int32 i = 0; i < BottomRingOrigin.Num(); ++i)
	{
		const float BottomRatio = static_cast<float>(i) / (BottomRingOrigin.Num() - 1);
		const int32 TopIndex = FMath::Clamp(FMath::RoundToInt(BottomRatio * (TopRingOrigin.Num() - 1)), 0, TopRingOrigin.Num() - 1);
		BottomToTopMapping[i] = TopIndex;
	}

	TArray<TArray<int32>> VertexRings;
	VertexRings.Add(BottomRing);

	if (Frustum.HeightSegments > 1)
	{
		const float HeightStep = Frustum.Height / Frustum.HeightSegments;

		for (int32 h = Frustum.HeightSegments - 1; h > 0; --h)
		{
			const float CurrentHeight = HalfHeight - h * HeightStep;
			const float HeightRatio = static_cast<float>(Frustum.HeightSegments - h) / Frustum.HeightSegments;

			TArray<int32> CurrentRing;

			for (int32 BottomIndex = 0; BottomIndex < BottomRingOrigin.Num(); ++BottomIndex)
			{
				const int32 TopIndex = BottomToTopMapping[BottomIndex];

				const float XR = FMath::Lerp(
					GetPosByIndex(BottomRingOrigin[BottomIndex]).X, GetPosByIndex(TopRingOrigin[TopIndex]).X, HeightRatio);
				const float YR = FMath::Lerp(
					GetPosByIndex(BottomRingOrigin[BottomIndex]).Y, GetPosByIndex(TopRingOrigin[TopIndex]).Y, HeightRatio);

				const float BaseRadius = FMath::Lerp(Frustum.BottomRadius, Frustum.TopRadius, HeightRatio);
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

				if (Frustum.BendAmount > KINDA_SMALL_NUMBER)
				{
					const float BendFactor = FMath::Sin(HeightRatio * PI);
					const float NormalZ = Frustum.BendAmount * FMath::Cos(HeightRatio * PI); // 移除负号，让法线向Z轴正方向弯曲
					Normal = (Normal + FVector(0, 0, NormalZ)).GetSafeNormal();
				}

				// 计算侧边UV坐标，使用动态V值占比
				const float U = static_cast<float>(BottomIndex) / Frustum.BottomSides;
				const float V = SideVStart + HeightRatio * SideVScale; // 使用动态V值占比
				const FVector2D UV = FVector2D(0.25f + U * 0.5f, V); // 侧边使用动态V范围

				const int32 VertexIndex = GetOrAddVertex(InterpolatedPos, Normal, UV);
				CurrentRing.Add(VertexIndex);
			}

			VertexRings.Add(CurrentRing);
		}
	}
	VertexRings.Add(TopRing);

	// 只收集端盖边界连接点，确保正确的端盖几何
	// 从低到高收集，确保上底顶点在UV边缘
	for (int32 i = 0; i < VertexRings.Num(); i++)
	{
		if (VertexRings[i].Num() > 0)
		{
			// 只收集每个环的第一个顶点，形成端盖边界
			RecordEndCapConnectionPoint(VertexRings[i][0]);
		}
	}

	for (int i = 0; i < VertexRings.Num() - 1; i++)
	{
		TArray<int32> CurrentRing = VertexRings[i];
		TArray<int32> NextRing = VertexRings[i + 1];

		// 对于相邻环之间的连接，需要根据顶点数量进行映射
		for (int32 CurrentIndex = 0; CurrentIndex < CurrentRing.Num() - 1; ++CurrentIndex)
		{
			const int32 NextCurrentIndex = CurrentIndex + 1;

			// 计算在下一个环中对应的顶点索引
			// 使用比例映射来确保正确的连接
			const float CurrentRatio = static_cast<float>(CurrentIndex) / (CurrentRing.Num() - 1);
			const float NextCurrentRatio = static_cast<float>(NextCurrentIndex) / (CurrentRing.Num() - 1);

			const int32 NextRingIndex = FMath::Clamp(FMath::RoundToInt(CurrentRatio * (NextRing.Num() - 1)), 0, NextRing.Num() - 1);
			const int32 NextRingNextIndex = FMath::Clamp(FMath::RoundToInt(NextCurrentRatio * (NextRing.Num() - 1)), 0, NextRing.Num() - 1);

			AddQuad(CurrentRing[CurrentIndex], NextRing[NextRingIndex], NextRing[NextRingNextIndex], CurrentRing[NextCurrentIndex]);
		}
	}
}


void FFrustumBuilder::GenerateEndCaps()
{
	if (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 端面法线应该根据角度计算
	GenerateEndCap(StartAngle, EEndCapType::Start);
	GenerateEndCap(EndAngle, EEndCapType::End);
}

void FFrustumBuilder::GenerateEndCap(float Angle, EEndCapType EndCapType)
{
	if (EndCapConnectionPoints.Num() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateEndCap - %s端面连接点不足，无法生成端面"),
			(EndCapType == EEndCapType::Start) ? TEXT("起始") : TEXT("结束"));
		return;
	}

	TArray<int32> RotatedConnectionPoints;
	RotatedConnectionPoints.Reserve(EndCapConnectionPoints.Num());

	// 重新分配端盖UV区域：
	// - 起始端盖：使用纹理左半部分 [0.0, 0.5] x [0.0, 1.0]
	// - 结束端盖：使用纹理右半部分 [0.5, 1.0] x [0.0, 1.0]
	const FVector2D UVOffset = (EndCapType == EEndCapType::Start) ?
		FVector2D(0.0f, 0.0f) : FVector2D(0.5f, 0.0f);
	const FVector2D UVScale(0.5f, 1.0f);

	for (int32 i = 0; i < EndCapConnectionPoints.Num(); ++i)
	{
		const int32 VertexIndex = EndCapConnectionPoints[i];
		FVector OriginalPos = GetPosByIndex(VertexIndex);

		FVector EndCapPos;
		if (EndCapType == EEndCapType::Start)
		{
			EndCapPos = OriginalPos;
		}
		else
		{
			const float Radius = FMath::Sqrt(OriginalPos.X * OriginalPos.X + OriginalPos.Y * OriginalPos.Y);
			const float CurrentAngle = FMath::Atan2(OriginalPos.Y, OriginalPos.X);
			const float RotationAngle = EndAngle - StartAngle;
			const float NewAngle = CurrentAngle + RotationAngle;
			EndCapPos = FVector(Radius * FMath::Cos(NewAngle), Radius * FMath::Sin(NewAngle), OriginalPos.Z);
		}

		// 端面法线
		FVector BaseNormal = FVector(FMath::Cos(Angle + PI / 2), FMath::Sin(Angle + PI / 2), 0.0f);
		FVector EndCapNormal = (EndCapType == EEndCapType::Start) ? -BaseNormal : BaseNormal;

		if (Frustum.BendAmount > KINDA_SMALL_NUMBER)
		{
			const float Z = EndCapPos.Z;
			const float HeightRatio = (Z + Frustum.GetHalfHeight()) / Frustum.Height;
			const float BendInfluence = FMath::Sin(HeightRatio * PI);
			FVector BendNormal = FVector(0, 0, BendInfluence).GetSafeNormal();
			EndCapNormal = (EndCapNormal + BendNormal * Frustum.BendAmount).GetSafeNormal();
		}

		// U坐标基于半径，V坐标基于高度
		const float Radius = FMath::Sqrt(EndCapPos.X * EndCapPos.X + EndCapPos.Y * EndCapPos.Y);
		const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);
		const float U = FMath::Clamp(Radius / MaxRadius, 0.0f, 1.0f);
		
		// 使用简单的线性映射，基于端盖的实际Z值范围
		// 直接使用Z值相对于Frustum高度的比例

		float V = (EndCapPos.Z + Frustum.GetHalfHeight()) / Frustum.Height;


		const FVector2D UV = UVOffset + FVector2D(U * UVScale.X, V * UVScale.Y);

		int32 NewVertexIndex = GetOrAddVertex(EndCapPos, EndCapNormal, UV);
		RotatedConnectionPoints.Add(NewVertexIndex);
	}

	GenerateEndCapTrianglesFromVertices(RotatedConnectionPoints, EndCapType, Angle);
}

TArray<int32> FFrustumBuilder::GenerateVertexRing(float Radius, float Z, int32 Sides)
{
	TArray<int32> VertexRing;
	const float AngleStep = CalculateAngleStep(Sides);

	const int32 VertexCount = Sides + 1; // 总是生成 Sides+1 个顶点

	for (int32 i = 0; i < VertexCount; ++i)
	{
		const float Angle = StartAngle + (i * AngleStep);

		const float X = Radius * FMath::Cos(Angle);
		const float Y = Radius * FMath::Sin(Angle);
		const FVector Pos(X, Y, Z);
		FVector Normal = FVector(X, Y, 0.0f).GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			Normal = FVector(1.0f, 0.0f, 0.0f);
		}

		const FVector2D UV = FVector2D(0.0f, 0.0f);
		const int32 VertexIndex = GetOrAddVertex(Pos, Normal, UV);
		VertexRing.Add(VertexIndex);
	}

	return VertexRing;
}

TArray<int32> FFrustumBuilder::GenerateVertexRing(
	float Radius, float Z, int32 Sides, float VCoord,
	const FVector2D& UVOffset, const FVector2D& UVScale)
{
	TArray<int32> VertexRing;

	// 角度步进
	const float AngleStep = CalculateAngleStep(Sides);

	// 侧面生成与ArcAngle无关：总是生成 Sides+1 个点
	const int32 VertexCount = Sides + 1;

	for (int32 i = 0; i < VertexCount; ++i)
	{
		const float Angle = StartAngle + i * AngleStep;

		const FVector Pos(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), Z);
		const FVector Normal = FVector(Pos.X, Pos.Y, 0).GetSafeNormal();

		// UV计算：使用线性分布，与ArcAngle无关
		const float U = static_cast<float>(i) / Sides;
		const FVector2D UV = UVOffset + FVector2D(U * UVScale.X, VCoord * UVScale.Y);

		VertexRing.Add(GetOrAddVertex(Pos, Normal, UV));
	}

	// 侧面不需要闭合处理，保持与ArcAngle无关
	return VertexRing;
}


// [MODIFIED] 恢复为简单逻辑，总是独立创建自己的顶点
void FFrustumBuilder::GenerateCapGeometry(float Z, int32 Sides, float Radius, EHeightPosition HeightPosition)
{
	FVector Normal(0.0f, 0.0f, (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f);

	// 计算实际半径（考虑倒角）
	const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);

	// 重新分配UV区域，避免重叠：
	// - 上底：使用纹理上半部分 [0.5, 1.0] x [0.5, 1.0]
	// - 下底：使用纹理下半部分 [0.5, 1.0] x [0.0, 0.5]
	const FVector2D UVOffset = (HeightPosition == EHeightPosition::Top) ?
		FVector2D(0.5f, 0.5f) : FVector2D(0.5f, 0.0f);
	const FVector2D UVScale(0.5f, 0.5f);

	// 中心点UV
	const FVector2D CenterUV = UVOffset + FVector2D(0.5f * UVScale.X, 0.5f * UVScale.Y);

	const FVector CenterPos(0.0f, 0.0f, Z);
	const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, CenterUV);

	const float AngleStep = CalculateAngleStep(Sides);

	for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
	{
		const float CurrentAngle = StartAngle + (SideIndex * AngleStep);
		const float NextAngle = StartAngle + ((SideIndex + 1) * AngleStep);

		const FVector CurrentPos(CapRadius * FMath::Cos(CurrentAngle), CapRadius * FMath::Sin(CurrentAngle), Z);
		const FVector NextPos(CapRadius * FMath::Cos(NextAngle), CapRadius * FMath::Sin(NextAngle), Z);

		// 改进的UV计算：使用圆形映射
		const float CurrentAngleNormalized = (CurrentAngle - StartAngle) / (EndAngle - StartAngle);
		const float NextAngleNormalized = (NextAngle - StartAngle) / (EndAngle - StartAngle);

		// 将角度映射到圆形UV
		const FVector2D CurrentCircleUV = FVector2D(
			0.5f + 0.5f * FMath::Cos(CurrentAngle) * (CapRadius / Radius),
			0.5f + 0.5f * FMath::Sin(CurrentAngle) * (CapRadius / Radius)
		);
		const FVector2D NextCircleUV = FVector2D(
			0.5f + 0.5f * FMath::Cos(NextAngle) * (CapRadius / Radius),
			0.5f + 0.5f * FMath::Sin(NextAngle) * (CapRadius / Radius)
		);

		const FVector2D CurrentUV = UVOffset + CurrentCircleUV * UVScale;
		const FVector2D NextUV = UVOffset + NextCircleUV * UVScale;

		const int32 V1 = GetOrAddVertex(CurrentPos, Normal, CurrentUV);
		const int32 V2 = GetOrAddVertex(NextPos, Normal, NextUV);

		// 记录端盖连接点
		if (SideIndex == 0)
		{
			RecordEndCapConnectionPoint(V1);
		}

		if (HeightPosition == EHeightPosition::Top)
		{
			AddTriangle(CenterVertex, V2, V1);
		}
		else
		{
			AddTriangle(CenterVertex, V1, V2);
		}
	}
}

// [MODIFIED] 移除顶点缓存逻辑，只负责生成倒角面本身
void FFrustumBuilder::GenerateBevelGeometry(EHeightPosition HeightPosition) {
	const float HalfHeight = Frustum.GetHalfHeight();
	const float BevelRadius = Frustum.BevelRadius;

	if (BevelRadius <= 0.0f)
	{
		return;
	}

	const float Radius = (HeightPosition == EHeightPosition::Top) ? Frustum.TopRadius : Frustum.BottomRadius;
	const int32 Sides = (HeightPosition == EHeightPosition::Top) ? Frustum.TopSides : Frustum.BottomSides;

	// 倒角UV映射，使用周长比例计算V值
	// 计算各部分高度和周长
	const float TotalHeight = Frustum.Height;
	const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);
	const float SideHeight = TotalHeight - TopBevelHeight - BottomBevelHeight;
	
	// 计算平均周长用于V值计算
	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;
	
	// 计算V值占比（基于周长比例）
	const float BottomBevelVScale = BottomBevelHeight / AvgCircumference;
	const float SideVScale = SideHeight / AvgCircumference;
	const float TopBevelVScale = TopBevelHeight / AvgCircumference;
	
	// 倒角UV区域：[0.25, 0.75] x [动态V范围] - 一头连接边界，一头连接侧面
	// 计算与侧面相同的V值范围，避免重叠
	// 当没有倒角时，侧面从V=0开始；有倒角时，从倒角结束后开始
	const float SideVStart = (BottomBevelVScale > 0.0f) ? (BottomBevelVScale + 0.001f) : 0.0f;
	const float SideVEnd = SideVStart + SideVScale;
	
	// 倒角UV区域设置
	FVector2D UVOffset, UVScale;
	if (HeightPosition == EHeightPosition::Top)
	{
		// 上倒角：从侧面顶部开始，到边界结束
		UVOffset = FVector2D(0.25f, SideVEnd);
		UVScale = FVector2D(0.5f, TopBevelVScale);
	}
	else
	{
		// 下倒角：从边界开始，到侧面底部结束
		// 确保下倒角结束位置与侧面开始位置不重叠
		UVOffset = FVector2D(0.25f, 0.0f);
		UVScale = FVector2D(0.5f, BottomBevelVScale);
	}

	const float EndZ = (HeightPosition == EHeightPosition::Top) ? HalfHeight : -HalfHeight;
	const float StartZ = (HeightPosition == EHeightPosition::Top) ? (HalfHeight - BevelRadius) : (-HalfHeight + BevelRadius);
	const float AngleStep = CalculateAngleStep(Sides);
	const int32 RingSize = Sides + 1; // 与ArcAngle无关

	TArray<int32> StartRing;
	TArray<int32> EndRing;
	StartRing.Reserve(RingSize);
	EndRing.Reserve(RingSize);

	for (int32 s = 0; s < RingSize; ++s)
	{
		const float angle = StartAngle + (s * AngleStep);

		// 直接计算倒角顶点位置，不依赖侧边顶点环
		const FVector SidePos(Radius * FMath::Cos(angle), Radius * FMath::Sin(angle), StartZ);

		FVector SideNormal = FVector(SidePos.X, SidePos.Y, 0.0f).GetSafeNormal();
		if (SideNormal.IsNearlyZero())
		{
			SideNormal = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.f);
		}
		FVector CapNormal = FVector(0.0f, 0.0f, (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f);
		FVector BevelNormal = (SideNormal + CapNormal).GetSafeNormal();

		// 计算倒角UV坐标，一头连接边界，一头连接侧面
		const float U = static_cast<float>(s) / Sides;
		// 下倒角：V_Side = 0.0f (连接边界), V_Cap = 1.0f (连接侧面底部)
		// 上倒角：V_Side = 0.0f (连接侧面顶部), V_Cap = 1.0f (连接边界)
		// 倒角UV坐标计算 - 一头连接边界，一头连接侧面
		// 根据倒角位置计算V坐标，确保与侧面无缝连接
		float V_Side, V_Cap;
		if (HeightPosition == EHeightPosition::Top)
		{
			// 上倒角：侧边连接到侧面顶部，端盖连接到边界
			V_Side = 0.0f; // 侧边部分（连接到侧面顶部）
			V_Cap = 1.0f;  // 端盖部分（连接到边界）
		}
		else
		{
			// 下倒角：侧边连接到边界，端盖连接到侧面底部
			V_Side = 0.0f; // 侧边部分（连接到边界）
			V_Cap = 1.0f;  // 端盖部分（连接到侧面底部）
		}

		// 计算实际的V坐标
		const float V_Side_Actual = UVOffset.Y + V_Side * UVScale.Y;
		const float V_Cap_Actual = UVOffset.Y + V_Cap * UVScale.Y;
		
		const FVector2D UV_Side = FVector2D(UVOffset.X + U * UVScale.X, V_Side_Actual);
		const FVector2D UV_Cap = FVector2D(UVOffset.X + U * UVScale.X, V_Cap_Actual);

		// 添加顶点到StartRing - 重新生成顶点，不复用侧边顶点
		StartRing.Add(GetOrAddVertex(SidePos, BevelNormal, UV_Side));

		const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);
		const FVector CapPos(CapRadius * FMath::Cos(angle), CapRadius * FMath::Sin(angle), EndZ);
		EndRing.Add(GetOrAddVertex(CapPos, BevelNormal, UV_Cap));
	}

	// 倒角面生成与ArcAngle无关，总是生成倒角面
	if (EndRing.Num() > 0)
	{

		// 连接内外环形成倒角面 - 与ArcAngle无关
		for (int32 s = 0; s < Sides; ++s)
		{
			const int32 V00 = StartRing[s];
			const int32 V10 = EndRing[s];
			const int32 V01 = StartRing[s + 1];
			const int32 V11 = EndRing[s + 1];

			if ((HeightPosition == EHeightPosition::Top)) {
				AddQuad(V00, V10, V11, V01);
			}
			else {
				AddQuad(V00, V01, V11, V10);
			}
		}
	}
}

	float FFrustumBuilder::CalculateBentRadius(float BaseRadius, float HeightRatio)
	{
		const float BendFactor = FMath::Sin(HeightRatio * PI);
		const float BentRadius = BaseRadius - Frustum.BendAmount * BendFactor * BaseRadius; // 改为减法，让半径向内收缩

		if (Frustum.MinBendRadius > KINDA_SMALL_NUMBER)
		{
			return FMath::Max(BentRadius, Frustum.MinBendRadius);
		}

		return FMath::Max(BentRadius, KINDA_SMALL_NUMBER);
	}

	float FFrustumBuilder::CalculateBevelHeight(float Radius)
	{
		return FMath::Min(Frustum.BevelRadius, Radius);
	}

	float FFrustumBuilder::CalculateHeightRatio(float Z)
	{
		const float HalfHeight = Frustum.GetHalfHeight();
		return (Z + HalfHeight) / Frustum.Height;
	}

	float FFrustumBuilder::CalculateAngleStep(int32 Sides)
	{
		if (Sides == 0) return 0.0f;
		return ArcAngleRadians / Sides;
	}
	void FFrustumBuilder::GenerateEndCapTrianglesFromVertices(const TArray<int32>&OrderedVertices, EEndCapType EndCapType, float Angle)
	{
		if (OrderedVertices.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateEndCapTrianglesFromVertices - 顶点数量不足，无法生成面"));
			return;
		}

		TArray<int32> SortedVertices = OrderedVertices;
		SortedVertices.Sort([this](const int32& A, const int32& B) -> bool
			{
				FVector PosA = GetPosByIndex(A);
				FVector PosB = GetPosByIndex(B);
				return PosA.Z > PosB.Z;
			});

		// 计算端面的实际法线方向
		// 端面法线应该垂直于端面平面
		// 根据是否为开始端面来确定法线方向
		const FVector BaseNormal = FVector(FMath::Cos(Angle + PI / 2), FMath::Sin(Angle + PI / 2), 0.0f);
		const FVector EndCapNormal = (EndCapType == EEndCapType::Start) ? -BaseNormal : BaseNormal;

		// 中心线法线应该与端面法线一致，确保渲染方向正确
		const FVector CenterLineNormal = EndCapNormal;

		// 端盖UV设置 - 放在侧边左右两侧
		// 重新计算侧边V值范围，与CreateSideGeometry保持一致
		const float TotalHeight = Frustum.Height;
		const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
		const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);
		const float SideHeight = TotalHeight - TopBevelHeight - BottomBevelHeight;
		const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
		const float AvgCircumference = 2.0f * PI * AvgRadius;
		const float BottomBevelVScale = BottomBevelHeight / AvgCircumference;
		const float SideVScale = SideHeight / AvgCircumference;
		const float SideVStart = (BottomBevelVScale > 0.0f) ? (BottomBevelVScale + 0.001f) : 0.0f;
		
		// 计算端盖半径比例，用于UV坐标调整
		const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);
		
		// 端盖UV区域：侧边左右两侧
		// 当有倒角时，端盖V值应该包含倒角+侧面的完整V范围
		const float TopBevelVScale = TopBevelHeight / AvgCircumference;
		const float TotalVScale = BottomBevelVScale + 0.001f + SideVScale + TopBevelVScale; // 下倒角+偏移+侧面+上倒角的完整V范围
		const float TotalVStart = 0.0f; // 从V=0开始
		
		// 起始端盖：[0, 0.25] x [完整V范围] - 左侧
		// 结束端盖：[0.75, 1.0] x [完整V范围] - 右侧
		const FVector2D UVOffset = (EndCapType == EEndCapType::Start) ? FVector2D(0.0f, TotalVStart) : FVector2D(0.75f, TotalVStart);
		const FVector2D UVScale(0.25f, TotalVScale);


		if (SortedVertices.Num() == 2)
		{
			const int32 V1 = SortedVertices[0];
			const int32 V2 = SortedVertices[1];

			FVector Pos1 = GetPosByIndex(V1);
			FVector Pos2 = GetPosByIndex(V2);

			// 计算UV坐标，使用与GenerateEndCap相同的逻辑
			const float Radius1 = FMath::Sqrt(Pos1.X * Pos1.X + Pos1.Y * Pos1.Y);
			const float Radius2 = FMath::Sqrt(Pos2.X * Pos2.X + Pos2.Y * Pos2.Y);
			
			// U坐标基于半径，与GenerateEndCap保持一致
			const float U1 = FMath::Clamp(Radius1 / MaxRadius, 0.0f, 1.0f);
			const float U2 = FMath::Clamp(Radius2 / MaxRadius, 0.0f, 1.0f);

			// 在GenerateEndCapTrianglesFromVertices函数中，修改UV计算部分：

			// 使用简单的线性映射，基于端盖的实际Z值范围
			const float V1_UV = (Pos1.Z + Frustum.GetHalfHeight()) / Frustum.Height;
			const float V2_UV = (Pos2.Z + Frustum.GetHalfHeight()) / Frustum.Height;

			const FVector2D UV1 = UVOffset + FVector2D(U1 * UVScale.X, V1_UV * UVScale.Y);
			const FVector2D UV2 = UVOffset + FVector2D(U2 * UVScale.X, V2_UV * UVScale.Y);

			// 中心点的UV坐标（基于高度）
			const FVector2D CenterUV1 = UVOffset + FVector2D(0.5f * UVScale.X, V1_UV * UVScale.Y);
			const FVector2D CenterUV2 = UVOffset + FVector2D(0.5f * UVScale.X, V2_UV * UVScale.Y);

			// 创建中心顶点
			const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), CenterLineNormal, CenterUV1);
			const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), CenterLineNormal, CenterUV2);

			// 重新创建边缘顶点以更新UV坐标
			const int32 V1_New = GetOrAddVertex(Pos1, EndCapNormal, UV1);
			const int32 V2_New = GetOrAddVertex(Pos2, EndCapNormal, UV2);

			if ((EndCapType == EEndCapType::Start))
			{
				AddTriangle(V1_New, V2_New, CenterV1);
				AddTriangle(V2_New, CenterV2, CenterV1);
			}
			else
			{
				AddTriangle(V2_New, V1_New, CenterV1);
				AddTriangle(CenterV1, CenterV2, V2_New);
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

				// 计算UV坐标，使用与GenerateEndCap相同的逻辑
				const float Radius1 = FMath::Sqrt(Pos1.X * Pos1.X + Pos1.Y * Pos1.Y);
				const float Radius2 = FMath::Sqrt(Pos2.X * Pos2.X + Pos2.Y * Pos2.Y);
				
				// U坐标基于半径，与GenerateEndCap保持一致
				const float U1 = FMath::Clamp(Radius1 / MaxRadius, 0.0f, 1.0f);
				const float U2 = FMath::Clamp(Radius2 / MaxRadius, 0.0f, 1.0f);

				// V坐标基于高度，与GenerateEndCap保持一致
				const float V1_UV = (Pos1.Z + Frustum.GetHalfHeight()) / Frustum.Height;
				const float V2_UV = (Pos2.Z + Frustum.GetHalfHeight()) / Frustum.Height;

				const FVector2D UV1 = UVOffset + FVector2D(U1 * UVScale.X, V1_UV * UVScale.Y);
				const FVector2D UV2 = UVOffset + FVector2D(U2 * UVScale.X, V2_UV * UVScale.Y);

				// 中心点的UV坐标
				const FVector2D CenterUV1 = UVOffset + FVector2D(0.5f * UVScale.X, V1_UV * UVScale.Y);
				const FVector2D CenterUV2 = UVOffset + FVector2D(0.5f * UVScale.X, V2_UV * UVScale.Y);

				const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), CenterLineNormal, CenterUV1);
				const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), CenterLineNormal, CenterUV2);

				// 重新创建边缘顶点以更新UV坐标
				const int32 V1_New = GetOrAddVertex(Pos1, EndCapNormal, UV1);
				const int32 V2_New = GetOrAddVertex(Pos2, EndCapNormal, UV2);

				if ((EndCapType == EEndCapType::Start))
				{
					AddTriangle(V1_New, V2_New, CenterV1);
					AddTriangle(V2_New, CenterV2, CenterV1);
				}
				else
				{
					AddTriangle(V2_New, V1_New, CenterV1);
					AddTriangle(CenterV1, CenterV2, V2_New);
				}
			}
		}
	}
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

	// UV生成已移除 - 让UE4自动处理UV生成

	void FFrustumBuilder::CalculateAngles()
	{
		ArcAngleRadians = FMath::DegreesToRadians(Frustum.ArcAngle);
		StartAngle = -ArcAngleRadians / 2.0f;
		EndAngle = ArcAngleRadians / 2.0f;
	}
