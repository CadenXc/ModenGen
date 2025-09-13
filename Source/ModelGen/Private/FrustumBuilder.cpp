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
	 GenerateTopBevelGeometry();
	 GenerateBottomBevelGeometry();

	GenerateTopGeometry();
	GenerateBottomGeometry();

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

	// UV区域定义
	const FVector2D SideUVOffset(0.0f, 0.0f);
	const FVector2D SideUVScale(0.25f, 1.0f);

	TArray<int32> TopRing = GenerateVertexRing(
		Frustum.TopRadius,
		TopBevelStartZ,
		Frustum.TopSides,
		1.0f,
		SideUVOffset,
		SideUVScale
	);

	TArray<int32> BottomRing = GenerateVertexRing(
		Frustum.BottomRadius,
		BottomBevelStartZ,
		Frustum.BottomSides,
		0.0f,
		SideUVOffset,
		SideUVScale
	);

	SideTopRing = TopRing;
	SideBottomRing = BottomRing;

	// 为插值计算原始（未弯曲）的顶点
	TArray<int32> TopRingOrigin = GenerateVertexRing(Frustum.TopRadius, HalfHeight, Frustum.TopSides, 1.0f, SideUVOffset, SideUVScale);
	TArray<int32> BottomRingOrigin = GenerateVertexRing(Frustum.BottomRadius, -HalfHeight, Frustum.BottomSides, 0.0f, SideUVOffset, SideUVScale);

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

				// 计算UV坐标
				const float U = static_cast<float>(BottomIndex) / BottomRingOrigin.Num();
				const float V = HeightRatio;
				const FVector2D UV = SideUVOffset + FVector2D(U * SideUVScale.X, V * SideUVScale.Y);

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

		// 对于非完整圆形，需要特殊处理最后一个顶点
		const int32 MaxIndex = (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? CurrentRing.Num() : CurrentRing.Num() - 1;
		
		for (int32 CurrentIndex = 0; CurrentIndex < MaxIndex; ++CurrentIndex)
		{
			const int32 NextCurrentIndex =
				(Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? (CurrentIndex + 1) % CurrentRing.Num() : (CurrentIndex + 1);

			// 确保NextCurrentIndex在有效范围内
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
	GenerateCapGeometry(Frustum.GetHalfHeight(), Frustum.TopSides, Frustum.TopRadius, true);
}

void FFrustumBuilder::GenerateBottomGeometry()
{
	GenerateCapGeometry(-Frustum.GetHalfHeight(), Frustum.BottomSides, Frustum.BottomRadius, false);
}

void FFrustumBuilder::GenerateTopBevelGeometry()
{
	if (Frustum.BevelRadius <= 0.0f)
	{
		return;
	}
	GenerateBevelGeometry(true);
}

void FFrustumBuilder::GenerateBottomBevelGeometry()
{
	if (Frustum.BevelRadius <= 0.0f)
	{
		return;
	}
	GenerateBevelGeometry(false);
}

void FFrustumBuilder::GenerateEndCaps()
{
	if (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 端面法线应该根据角度计算
	GenerateEndCap(StartAngle, true);
	GenerateEndCap(EndAngle, false);
}

void FFrustumBuilder::GenerateEndCap(float Angle, bool IsStart)
{
	if (EndCapConnectionPoints.Num() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateEndCap - %s端面连接点不足，无法生成端面"), IsStart ? TEXT("起始") : TEXT("结束"));
		return;
	}

	// UV区域定义
	const FVector2D UVOffset = IsStart ? FVector2D(0.75f, 0.0f) : FVector2D(0.75f, 0.5f);
	const FVector2D UVScale(0.25f, 0.5f);

	TArray<int32> RotatedConnectionPoints;
	RotatedConnectionPoints.Reserve(EndCapConnectionPoints.Num());

	for (int32 i = 0; i < EndCapConnectionPoints.Num(); ++i)
	{
		const int32 VertexIndex = EndCapConnectionPoints[i];
		FVector OriginalPos = GetPosByIndex(VertexIndex);

		FVector EndCapPos;
		if (IsStart)
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
		FVector BaseNormal = FVector(FMath::Cos(Angle + PI/2), FMath::Sin(Angle + PI/2), 0.0f);
		FVector EndCapNormal = IsStart ? -BaseNormal : BaseNormal;

		if (Frustum.BendAmount > KINDA_SMALL_NUMBER)
		{
			const float Z = EndCapPos.Z;
			const float HeightRatio = (Z + Frustum.GetHalfHeight()) / Frustum.Height;
			const float BendInfluence = FMath::Sin(HeightRatio * PI);

			FVector BendNormal = FVector(0, 0, -BendInfluence).GetSafeNormal();
			EndCapNormal = (EndCapNormal + BendNormal * Frustum.BendAmount).GetSafeNormal();
		}

		// 计算UV坐标
		const float U = static_cast<float>(i) / (EndCapConnectionPoints.Num() - 1);
		const float V = (EndCapPos.Z + Frustum.GetHalfHeight()) / Frustum.Height;
		const FVector2D UV = UVOffset + FVector2D(U * UVScale.X, V * UVScale.Y);

		int32 NewVertexIndex = GetOrAddVertex(EndCapPos, EndCapNormal, UV);
		RotatedConnectionPoints.Add(NewVertexIndex);
	}

	GenerateEndCapTrianglesFromVertices(RotatedConnectionPoints, IsStart, Angle);
}

TArray<int32> FFrustumBuilder::GenerateVertexRing(float Radius, float Z, int32 Sides)
{
	TArray<int32> VertexRing;
	const float AngleStep = CalculateAngleStep(Sides);

	const int32 VertexCount = (Frustum.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides + 1; // 末尾会与首点合并

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

		const int32 VertexIndex = GetOrAddVertex(Pos, Normal);
		VertexRing.Add(VertexIndex);
	}
	// 满圆时闭合最后一个点到第一个
	if (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER && VertexRing.Num() > 0)
	{
		VertexRing.Last() = VertexRing[0];
	}

	return VertexRing;
}

TArray<int32> FFrustumBuilder::GenerateVertexRing(float Radius, float Z, int32 Sides, float VCoord, const FVector2D& UVOffset, const FVector2D& UVScale)
{
	TArray<int32> VertexRing;
	const float AngleStep = CalculateAngleStep(Sides);
	const int32 VertexCount = (Frustum.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides;

	for (int32 i = 0; i < VertexCount; ++i)
	{
		const float Angle = StartAngle + i * AngleStep;
		const FVector Pos(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), Z);
		const FVector Normal = FVector(Pos.X, Pos.Y, 0).GetSafeNormal();
		
		const float U = static_cast<float>(i) / Sides;
		const FVector2D UV = UVOffset + FVector2D(U * UVScale.X, VCoord * UVScale.Y);

		VertexRing.Add(GetOrAddVertex(Pos, Normal, UV));
	}
	return VertexRing;
}


// [MODIFIED] 恢复为简单逻辑，总是独立创建自己的顶点
void FFrustumBuilder::GenerateCapGeometry(float Z, int32 Sides, float Radius, bool bIsTop)
{
	FVector Normal(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);

	// UV区域定义
	const FVector2D UVOffset = bIsTop ? FVector2D(0.25f, 0.5f) : FVector2D(0.25f, 0.0f);
	const FVector2D UVScale(0.25f, 0.5f);
	const FVector2D CenterUV = UVOffset + FVector2D(0.5f * UVScale.X, 0.5f * UVScale.Y);

	const FVector CenterPos(0.0f, 0.0f, Z);
	const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, CenterUV);

	// 如果没有倒角，顶/底面半径就是它本身；如果有，半径要向内收缩。
	const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);

	const float AngleStep = CalculateAngleStep(Sides);
	const int32 LoopCount = (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? Sides : Sides;

	for (int32 SideIndex = 0; SideIndex < LoopCount; ++SideIndex)
	{
		const float CurrentAngle = StartAngle + (SideIndex * AngleStep);
		const float NextAngle = StartAngle + ((SideIndex + 1) * AngleStep);

		const FVector CurrentPos(CapRadius * FMath::Cos(CurrentAngle), CapRadius * FMath::Sin(CurrentAngle), Z);
		const FVector NextPos(CapRadius * FMath::Cos(NextAngle), CapRadius * FMath::Sin(NextAngle), Z);

		// 计算UV坐标
		const FVector2D CurrentUV = UVOffset + FVector2D((1.0f + FMath::Cos(CurrentAngle)) * 0.5f * UVScale.X, (1.0f + FMath::Sin(CurrentAngle)) * 0.5f * UVScale.Y);
		const FVector2D NextUV = UVOffset + FVector2D((1.0f + FMath::Cos(NextAngle)) * 0.5f * UVScale.X, (1.0f + FMath::Sin(NextAngle)) * 0.5f * UVScale.Y);

		const int32 V1 = GetOrAddVertex(CurrentPos, Normal, CurrentUV);
		const int32 V2 = GetOrAddVertex(NextPos, Normal, NextUV);

		// 记录端盖连接点（只记录开始点，i=0的那个）
		if (SideIndex == 0)
		{
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

// [MODIFIED] 移除顶点缓存逻辑，只负责生成倒角面本身
void FFrustumBuilder::GenerateBevelGeometry(bool bIsTop) {
	const float HalfHeight = Frustum.GetHalfHeight();
	const float BevelRadius = Frustum.BevelRadius;

	if (BevelRadius <= 0.0f) {
		return;
	}

	const float Radius = bIsTop ? Frustum.TopRadius : Frustum.BottomRadius;
	const int32 Sides = bIsTop ? Frustum.TopSides : Frustum.BottomSides;
	const TArray<int32>& SideRing = bIsTop ? SideTopRing : SideBottomRing;

	if (SideRing.Num() == 0) {
		return;
	}

	// UV区域定义
	const FVector2D UVOffset = bIsTop ? FVector2D(0.5f, 0.5f) : FVector2D(0.5f, 0.0f);
	const FVector2D UVScale(0.25f, 0.5f);

	const float EndZ = bIsTop ? HalfHeight : -HalfHeight;
	const float AngleStep = CalculateAngleStep(Sides);
	const int32 RingSize = (Frustum.ArcAngle < 360.0f - KINDA_SMALL_NUMBER) ? Sides + 1 : Sides;

	TArray<int32> StartRing = SideRing;
	TArray<int32> EndRing;
	EndRing.Reserve(RingSize);

	for (int32 s = 0; s < RingSize; ++s) {
		const float angle = StartAngle + (s * AngleStep);
		const FVector SidePos = GetPosByIndex(SideRing[s]);

		FVector SideNormal = FVector(SidePos.X, SidePos.Y, 0.0f).GetSafeNormal();
		if (SideNormal.IsNearlyZero())
		{
			SideNormal = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.f);
		}
		FVector CapNormal = FVector(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f);
		FVector BevelNormal = (SideNormal + CapNormal).GetSafeNormal();

		// 计算UV坐标
		const float U = static_cast<float>(s) / Sides;
		const FVector2D UV_Side = UVOffset + FVector2D(U * UVScale.X, 0.0f);
		const FVector2D UV_Cap = UVOffset + FVector2D(U * UVScale.X, UVScale.Y);

		StartRing[s] = GetOrAddVertex(SidePos, BevelNormal, UV_Side);

		const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);
		const FVector CapPos(CapRadius * FMath::Cos(angle), CapRadius * FMath::Sin(angle), EndZ);
		EndRing.Add(GetOrAddVertex(CapPos, BevelNormal, UV_Cap));
	}

	if (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER && EndRing.Num() > 0) {
		EndRing.Last() = EndRing[0];
	}

	// 记录倒角开始点到端盖连接点（只记录开始点，i=0的那个）
	if (StartRing.Num() > 0)
	{
		RecordEndCapConnectionPoint(StartRing[0]);
	}

	// 连接内外环形成倒角面
	const int32 ConnectLoopCount = (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? Sides : Sides;
	for (int32 s = 0; s < ConnectLoopCount; ++s) {
		const int32 V00 = StartRing[s];
		const int32 V10 = EndRing[s];
		
		// 对于非完整圆形，最后一个面需要特殊处理
		const int32 NextS = (Frustum.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER) ? (s + 1) % Sides : (s + 1);
		if (NextS >= StartRing.Num() || NextS >= EndRing.Num())
			continue;
			
		const int32 V01 = StartRing[NextS];
		const int32 V11 = EndRing[NextS];

		if (bIsTop) {
			AddQuad(V00, V10, V11, V01);
		}
		else {
			AddQuad(V00, V01, V11, V10);
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

void FFrustumBuilder::GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, bool IsStart, float Angle)
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
	const FVector BaseNormal = FVector(FMath::Cos(Angle + PI/2), FMath::Sin(Angle + PI/2), 0.0f);
	const FVector EndCapNormal = IsStart ? -BaseNormal : BaseNormal;
	
	// 中心线法线应该与端面法线一致，确保渲染方向正确
	const FVector CenterLineNormal = EndCapNormal;

	if (SortedVertices.Num() == 2)
	{
		const int32 V1 = SortedVertices[0];
		const int32 V2 = SortedVertices[1];

		FVector Pos1 = GetPosByIndex(V1);
		FVector Pos2 = GetPosByIndex(V2);

		// 计算UV坐标
		const FVector2D UVOffset = IsStart ? FVector2D(0.75f, 0.0f) : FVector2D(0.75f, 0.5f);
		const FVector2D UVScale(0.25f, 0.5f);
		
		// 中心点的UV坐标 - 使用固定的U坐标
		const FVector2D CenterUV1 = UVOffset + FVector2D(0.5f * UVScale.X, (Pos1.Z + Frustum.GetHalfHeight()) / Frustum.Height * UVScale.Y);
		const FVector2D CenterUV2 = UVOffset + FVector2D(0.5f * UVScale.X, (Pos2.Z + Frustum.GetHalfHeight()) / Frustum.Height * UVScale.Y);
		
		// 边缘顶点的UV坐标 - 基于角度和高度
		const float Angle1 = FMath::Atan2(Pos1.Y, Pos1.X);
		const float Angle2 = FMath::Atan2(Pos2.Y, Pos2.X);
		const float U1 = 0.5f + 0.5f * FMath::Cos(Angle1);
		const float U2 = 0.5f + 0.5f * FMath::Cos(Angle2);
		const FVector2D EdgeUV1 = UVOffset + FVector2D(U1 * UVScale.X, (Pos1.Z + Frustum.GetHalfHeight()) / Frustum.Height * UVScale.Y);
		const FVector2D EdgeUV2 = UVOffset + FVector2D(U2 * UVScale.X, (Pos2.Z + Frustum.GetHalfHeight()) / Frustum.Height * UVScale.Y);

		const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), CenterLineNormal, CenterUV1);
		const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), CenterLineNormal, CenterUV2);
		
		// 重新创建边缘顶点以更新UV坐标
		const int32 V1_New = GetOrAddVertex(Pos1, EndCapNormal, EdgeUV1);
		const int32 V2_New = GetOrAddVertex(Pos2, EndCapNormal, EdgeUV2);

		if (IsStart)
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
			const FVector2D UVOffset = IsStart ? FVector2D(0.75f, 0.0f) : FVector2D(0.75f, 0.5f);
			const FVector2D UVScale(0.25f, 0.5f);
			
			// 中心点的UV坐标 - 使用固定的U坐标
			const FVector2D CenterUV1 = UVOffset + FVector2D(0.5f * UVScale.X, (Pos1.Z + Frustum.GetHalfHeight()) / Frustum.Height * UVScale.Y);
			const FVector2D CenterUV2 = UVOffset + FVector2D(0.5f * UVScale.X, (Pos2.Z + Frustum.GetHalfHeight()) / Frustum.Height * UVScale.Y);
			
			// 边缘顶点的UV坐标 - 基于角度和高度
			const float Angle1 = FMath::Atan2(Pos1.Y, Pos1.X);
			const float Angle2 = FMath::Atan2(Pos2.Y, Pos2.X);
			const float U1 = 0.5f + 0.5f * FMath::Cos(Angle1);
			const float U2 = 0.5f + 0.5f * FMath::Cos(Angle2);
			const FVector2D EdgeUV1 = UVOffset + FVector2D(U1 * UVScale.X, (Pos1.Z + Frustum.GetHalfHeight()) / Frustum.Height * UVScale.Y);
			const FVector2D EdgeUV2 = UVOffset + FVector2D(U2 * UVScale.X, (Pos2.Z + Frustum.GetHalfHeight()) / Frustum.Height * UVScale.Y);

			const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), CenterLineNormal, CenterUV1);
			const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), CenterLineNormal, CenterUV2);
			
			// 重新创建边缘顶点以更新UV坐标
			const int32 V1_New = GetOrAddVertex(Pos1, EndCapNormal, EdgeUV1);
			const int32 V2_New = GetOrAddVertex(Pos2, EndCapNormal, EdgeUV2);

			if (IsStart)
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