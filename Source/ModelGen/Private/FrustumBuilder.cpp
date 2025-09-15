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

	// 侧边UV区域：[0.2, 0.7] x [0, 1] - 扩大侧边区域
	TArray<int32> TopRing = GenerateVertexRing(
		Frustum.TopRadius,
		TopBevelStartZ,
		Frustum.TopSides,
		1.0f, // V坐标
		FVector2D(0.2f, 0.0f), // UVOffset
		FVector2D(0.5f, 1.0f)  // UVScale
	);

	TArray<int32> BottomRing = GenerateVertexRing(
		Frustum.BottomRadius,
		BottomBevelStartZ,
		Frustum.BottomSides,
		0.0f, // V坐标
		FVector2D(0.2f, 0.0f), // UVOffset
		FVector2D(0.5f, 1.0f)  // UVScale
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
					const float NormalZ = -Frustum.BendAmount * FMath::Cos(HeightRatio * PI);
					Normal = (Normal + FVector(0, 0, NormalZ)).GetSafeNormal();
				}

				// 计算侧边UV坐标 - 使用下底边数作为基准
				const float U = static_cast<float>(BottomIndex) / Frustum.BottomSides;
				const float V = HeightRatio;
				const FVector2D UV = FVector2D(0.2f + U * 0.5f, V); // 侧边使用[0.2, 0.7] x [0, 1]区域

				const int32 VertexIndex = GetOrAddVertex(InterpolatedPos, Normal, UV);
				CurrentRing.Add(VertexIndex);
			}

			VertexRings.Add(CurrentRing);
		}
	}
	VertexRings.Add(TopRing);

	for (int32 i = VertexRings.Num() - 1; i >= 0; i--)
	{
		RecordEndCapConnectionPoint(VertexRings[i][0]);
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
		UE_LOG(LogTemp, Warning, TEXT("GenerateEndCap - %s端面连接点不足，无法生成端面"), (EndCapType == EEndCapType::Start) ? TEXT("起始") : TEXT("结束"));
		return;
	}

	TArray<int32> RotatedConnectionPoints;
	RotatedConnectionPoints.Reserve(EndCapConnectionPoints.Num());

	// 端盖UV设置 - 放在左下角，垂直对齐，优化空间利用
	// 端盖UV区域：[0, 0.2] x [0.4, 1]
	// 起始端盖：[0, 0.2] x [0.4, 0.7]
	// 结束端盖：[0, 0.2] x [0.7, 1]
	const FVector2D UVOffset = (EndCapType == EEndCapType::Start) ? FVector2D(0.0f, 0.4f) : FVector2D(0.0f, 0.7f);
	const FVector2D UVScale(0.2f, 0.3f);

	for (int32 i = 0; i < EndCapConnectionPoints.Num(); ++i)
	{
		const int32 VertexIndex = EndCapConnectionPoints[i];
		FVector OriginalPos = GetPosByIndex(VertexIndex);

		FVector EndCapPos;
		if ((EndCapType == EEndCapType::Start))
		{
			EndCapPos = OriginalPos;
		}
		else
		{
			const float Radius = FMath::Sqrt(OriginalPos.X * OriginalPos.X + OriginalPos.Y * OriginalPos.Y);
			const float CurrentAngle = FMath::Atan2(OriginalPos.Y, OriginalPos.X);

			const float RotationAngle = EndAngle - StartAngle;
			const float NewAngle = CurrentAngle + RotationAngle;

			const float NewX = Radius * FMath::Cos(NewAngle);
			const float NewY = Radius * FMath::Sin(NewAngle);
			EndCapPos = FVector(NewX, NewY, OriginalPos.Z);
		}

		// 端面法线应该垂直于端面平面
		// 根据是否为开始端面来确定法线方向
		FVector BaseNormal = FVector(FMath::Cos(Angle + PI / 2), FMath::Sin(Angle + PI / 2), 0.0f);
		FVector EndCapNormal = (EndCapType == EEndCapType::Start) ? -BaseNormal : BaseNormal;

		if (Frustum.BendAmount > KINDA_SMALL_NUMBER)
		{
			const float Z = EndCapPos.Z;
			const float HeightRatio = (Z + Frustum.GetHalfHeight()) / Frustum.Height;
			const float BendInfluence = FMath::Sin(HeightRatio * PI);

			FVector BendNormal = FVector(0, 0, -BendInfluence).GetSafeNormal();
			EndCapNormal = (EndCapNormal + BendNormal * Frustum.BendAmount).GetSafeNormal();
		}

		// 计算端盖UV坐标
		const float Radius = FMath::Sqrt(EndCapPos.X * EndCapPos.X + EndCapPos.Y * EndCapPos.Y);
		const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);
		const float NormalizedRadius = FMath::Clamp(Radius / MaxRadius, 0.0f, 1.0f);
		const float PosAngle = FMath::Atan2(EndCapPos.Y, EndCapPos.X);

		// 将角度映射到[0,1]范围 - 修复U值分布问题
		float NormalizedAngle;
		if ((EndCapType == EEndCapType::Start))
		{
			NormalizedAngle = (PosAngle - StartAngle) / (EndAngle - StartAngle);
		}
		else
		{
			// 对于结束端盖，需要反转角度方向以确保UV正确分布
			NormalizedAngle = 1.0f - (PosAngle - StartAngle) / (EndAngle - StartAngle);
		}
		const float U = FMath::Clamp(NormalizedAngle, 0.0f, 1.0f);
		const float V = (EndCapPos.Z + Frustum.GetHalfHeight()) / Frustum.Height;

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

	// UV区域定义 - 上下底面，移到左上角，保持正方形
	// 上底：[0, 0.2] x [0, 0.2]
	// 下底：[0, 0.2] x [0.2, 0.4]
	const FVector2D UVOffset = (HeightPosition == EHeightPosition::Top) ? FVector2D(0.0f, 0.0f) : FVector2D(0.0f, 0.2f);
	const FVector2D UVScale(0.2f, 0.2f); // 正方形区域
	const FVector2D CenterUV = UVOffset + FVector2D(0.5f * UVScale.X, 0.5f * UVScale.Y);

	const FVector CenterPos(0.0f, 0.0f, Z);
	const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, CenterUV);

	// 如果没有倒角，顶/底面半径就是它本身；如果有，半径要向内收缩。
	const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);

	const float AngleStep = CalculateAngleStep(Sides);
	const int32 LoopCount = Sides; // 与ArcAngle无关

	for (int32 SideIndex = 0; SideIndex < LoopCount; ++SideIndex)
	{
		const float CurrentAngle = StartAngle + (SideIndex * AngleStep);
		const float NextAngle = StartAngle + ((SideIndex + 1) * AngleStep);

		const FVector CurrentPos(CapRadius * FMath::Cos(CurrentAngle), CapRadius * FMath::Sin(CurrentAngle), Z);
		const FVector NextPos(CapRadius * FMath::Cos(NextAngle), CapRadius * FMath::Sin(NextAngle), Z);

		// 计算UV坐标 - 使用标准圆形映射，完全不受ArcAngle影响
		// 将角度标准化到[0, 2π]范围，然后映射到圆形UV坐标
		const float FullCircleAngle = 2.0f * PI;
		const float NormalizedCurrentAngle = FMath::Fmod(CurrentAngle + 2.0f * PI, 2.0f * PI);
		const float NormalizedNextAngle = FMath::Fmod(NextAngle + 2.0f * PI, 2.0f * PI);

		// 使用标准圆形映射，确保UV分布均匀
		const float U_Current = 0.5f + 0.5f * FMath::Cos(NormalizedCurrentAngle);
		const float U_Next = 0.5f + 0.5f * FMath::Cos(NormalizedNextAngle);
		const float V_Current = 0.5f + 0.5f * FMath::Sin(NormalizedCurrentAngle);
		const float V_Next = 0.5f + 0.5f * FMath::Sin(NormalizedNextAngle);

		const FVector2D CurrentUV = UVOffset + FVector2D(U_Current, V_Current) * UVScale;
		const FVector2D NextUV = UVOffset + FVector2D(U_Next, V_Next) * UVScale;

		const int32 V1 = GetOrAddVertex(CurrentPos, Normal, CurrentUV);
		const int32 V2 = GetOrAddVertex(NextPos, Normal, NextUV);

		// 记录端盖连接点（只记录开始点，i=0的那个）
		if (SideIndex == 0)
		{
			RecordEndCapConnectionPoint(V1);
		}

		if ((HeightPosition == EHeightPosition::Top))
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

	if (BevelRadius <= 0.0f) {
		return;
	}

	const float Radius = (HeightPosition == EHeightPosition::Top) ? Frustum.TopRadius : Frustum.BottomRadius;
	const int32 Sides = (HeightPosition == EHeightPosition::Top) ? Frustum.TopSides : Frustum.BottomSides;

	// 倒角UV设置 - 调整倒角区域
	// 倒角UV区域：[0.7, 1] x [0, 1]
	// 上倒角：[0.7, 1] x [0.5, 1]
	// 下倒角：[0.7, 1] x [0, 0.5]
	const FVector2D UVOffset = (HeightPosition == EHeightPosition::Top) ? FVector2D(0.7f, 0.5f) : FVector2D(0.7f, 0.0f);
	const FVector2D UVScale(0.3f, 0.5f);

	const float EndZ = (HeightPosition == EHeightPosition::Top) ? HalfHeight : -HalfHeight;
	const float StartZ = (HeightPosition == EHeightPosition::Top) ? (HalfHeight - BevelRadius) : (-HalfHeight + BevelRadius);
	const float AngleStep = CalculateAngleStep(Sides);
	const int32 RingSize = Sides + 1; // 与ArcAngle无关

	TArray<int32> StartRing;
	TArray<int32> EndRing;
	StartRing.Reserve(RingSize);
	EndRing.Reserve(RingSize);

	for (int32 s = 0; s < RingSize; ++s) {
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

		// 计算倒角UV坐标 - 所有顶点都有不同的UV，不重合
		const float U = static_cast<float>(s) / Sides;
		const float V_Side = 0.0f; // 侧边部分使用V=0
		const float V_Cap = 1.0f;  // 端盖部分使用V=1

		const FVector2D UV_Side = UVOffset + FVector2D(U * UVScale.X, V_Side * UVScale.Y);
		const FVector2D UV_Cap = UVOffset + FVector2D(U * UVScale.X, V_Cap * UVScale.Y);

		// 添加顶点到StartRing - 重新生成顶点，不复用侧边顶点
		StartRing.Add(GetOrAddVertex(SidePos, BevelNormal, UV_Side));

		const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);
		const FVector CapPos(CapRadius * FMath::Cos(angle), CapRadius * FMath::Sin(angle), EndZ);
		EndRing.Add(GetOrAddVertex(CapPos, BevelNormal, UV_Cap));
	}

	// 倒角面生成与ArcAngle无关，总是生成倒角面
	if (EndRing.Num() > 0) {
		// 记录倒角开始点到端盖连接点（只记录开始点，i=0的那个）
		// 倒角有自己的顶点，位置相同但顶点不同
		if (StartRing.Num() > 0)
		{
			RecordEndCapConnectionPoint(StartRing[0]);
		}

		// 连接内外环形成倒角面 - 与ArcAngle无关
		for (int32 s = 0; s < Sides; ++s) {
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
		const float BentRadius = BaseRadius + Frustum.BendAmount * BendFactor * BaseRadius;

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

		// 端盖UV设置 - 放在左下角，垂直对齐，优化空间利用
		// 端盖UV区域：[0, 0.2] x [0.4, 1]
		// 起始端盖：[0, 0.2] x [0.4, 0.7]
		// 结束端盖：[0, 0.2] x [0.7, 1]
		const FVector2D UVOffset = (EndCapType == EEndCapType::Start) ? FVector2D(0.0f, 0.4f) : FVector2D(0.0f, 0.7f);
		const FVector2D UVScale(0.2f, 0.3f);

		if (SortedVertices.Num() == 2)
		{
			const int32 V1 = SortedVertices[0];
			const int32 V2 = SortedVertices[1];

			FVector Pos1 = GetPosByIndex(V1);
			FVector Pos2 = GetPosByIndex(V2);

			// 计算UV坐标
			const float Radius1 = FMath::Sqrt(Pos1.X * Pos1.X + Pos1.Y * Pos1.Y);
			const float Radius2 = FMath::Sqrt(Pos2.X * Pos2.X + Pos2.Y * Pos2.Y);
			const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);

			const float Angle1 = FMath::Atan2(Pos1.Y, Pos1.X);
			const float Angle2 = FMath::Atan2(Pos2.Y, Pos2.X);

			// 将角度映射到[0,1]范围 - 修复U值分布问题
			float U1, U2;
			if ((EndCapType == EEndCapType::Start))
			{
				U1 = FMath::Clamp((Angle1 - StartAngle) / (EndAngle - StartAngle), 0.0f, 1.0f);
				U2 = FMath::Clamp((Angle2 - StartAngle) / (EndAngle - StartAngle), 0.0f, 1.0f);
			}
			else
			{
				// 对于结束端盖，反转角度方向
				U1 = 1.0f - FMath::Clamp((Angle1 - StartAngle) / (EndAngle - StartAngle), 0.0f, 1.0f);
				U2 = 1.0f - FMath::Clamp((Angle2 - StartAngle) / (EndAngle - StartAngle), 0.0f, 1.0f);
			}

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
		else
		{
			for (int32 i = 0; i < SortedVertices.Num() - 1; ++i)
			{
				const int32 V1 = SortedVertices[i];
				const int32 V2 = SortedVertices[i + 1];

				FVector Pos1 = GetPosByIndex(V1);
				FVector Pos2 = GetPosByIndex(V2);

				// 计算UV坐标
				const float Radius1 = FMath::Sqrt(Pos1.X * Pos1.X + Pos1.Y * Pos1.Y);
				const float Radius2 = FMath::Sqrt(Pos2.X * Pos2.X + Pos2.Y * Pos2.Y);
				const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);

				const float Angle1 = FMath::Atan2(Pos1.Y, Pos1.X);
				const float Angle2 = FMath::Atan2(Pos2.Y, Pos2.X);

				// 将角度映射到[0,1]范围 - 修复U值分布问题
				float U1, U2;
				if ((EndCapType == EEndCapType::Start))
				{
					U1 = FMath::Clamp((Angle1 - StartAngle) / (EndAngle - StartAngle), 0.0f, 1.0f);
					U2 = FMath::Clamp((Angle2 - StartAngle) / (EndAngle - StartAngle), 0.0f, 1.0f);
				}
				else
				{
					// 对于结束端盖，反转角度方向
					U1 = 1.0f - FMath::Clamp((Angle1 - StartAngle) / (EndAngle - StartAngle), 0.0f, 1.0f);
					U2 = 1.0f - FMath::Clamp((Angle2 - StartAngle) / (EndAngle - StartAngle), 0.0f, 1.0f);
				}

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
