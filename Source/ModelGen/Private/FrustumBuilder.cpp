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
	TopSideRing.Empty();
	BottomSideRing.Empty();
	TopCapRing.Empty();
	BottomCapRing.Empty();
}

bool FFrustumBuilder::Generate(FModelGenMeshData& OutMeshData)
{
	if (!Frustum.IsValid())
	{
		return false;
	}

	Clear();
	ReserveMemory();

	// 计算是否启用倒角
	bEnableBevel = Frustum.BevelRadius > 0.0f;

	CreateSideGeometry();

	if (bEnableBevel)
	{
		GenerateBevelGeometry(EHeightPosition::Top);
		GenerateBevelGeometry(EHeightPosition::Bottom);
	}

	GenerateCapGeometry(Frustum.GetHalfHeight(), Frustum.TopSides, Frustum.TopRadius, EHeightPosition::Top);
	GenerateCapGeometry(-Frustum.GetHalfHeight(), Frustum.BottomSides, Frustum.BottomRadius, EHeightPosition::Bottom);

	GenerateEndCaps();

	if (!ValidateGeneratedData())
	{
		return false;
	}

	MeshData.CalculateTangents();

	OutMeshData = MeshData;
	return true;
}

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
	const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);

	// 计算侧边棱的方向向量（从底部到顶部）
	const float RadiusDiff = Frustum.TopRadius - Frustum.BottomRadius;
	const float SideLength = FMath::Sqrt(RadiusDiff * RadiusDiff + Frustum.Height * Frustum.Height);

	// 计算沿着侧边棱移动后的新位置
	float TopBevelRadius, TopBevelStartZ;
	float BottomBevelRadius, BottomBevelStartZ;

	if (bEnableBevel && SideLength > KINDA_SMALL_NUMBER)
	{
		// 侧边方向向量的归一化分量
		const float RadiusDir = RadiusDiff / SideLength;
		const float HeightDir = Frustum.Height / SideLength;

		// 顶部：沿着棱向下移动
		TopBevelRadius = Frustum.TopRadius - TopBevelHeight * RadiusDir;
		TopBevelStartZ = HalfHeight - TopBevelHeight * HeightDir;

		// 底部：沿着棱向上移动
		BottomBevelRadius = Frustum.BottomRadius + BottomBevelHeight * RadiusDir;
		BottomBevelStartZ = -HalfHeight + BottomBevelHeight * HeightDir;
	}
	else
	{
		// 无倒角时，使用原始值
		TopBevelRadius = Frustum.TopRadius;
		TopBevelStartZ = HalfHeight;
		BottomBevelRadius = Frustum.BottomRadius;
		BottomBevelStartZ = -HalfHeight;
	}

	// 侧边UV映射：使用周长比例计算V值
	const float TotalHeight = Frustum.Height;
	const float SideHeight = TotalHeight - TopBevelHeight - BottomBevelHeight;

	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;

	const float BottomBevelVScale = BottomBevelHeight / AvgCircumference;
	const float SideVScale = SideHeight / AvgCircumference;
	const float TopBevelVScale = TopBevelHeight / AvgCircumference;

	const float SideVStart = (BottomBevelVScale > 0.0f) ? (BottomBevelVScale + 0.001f) : 0.0f;
	const float SideVEnd = SideVStart + SideVScale;

	TArray<int32> TopRing = GenerateVertexRing(
		TopBevelRadius,
		TopBevelStartZ,
		Frustum.TopSides,
		1.0f,
		FVector2D(0.25f, SideVStart),
		FVector2D(0.5f, SideVScale)
	);
	TopSideRing = TopRing;

	TArray<int32> BottomRing = GenerateVertexRing(
		BottomBevelRadius,
		BottomBevelStartZ,
		Frustum.BottomSides,
		SideVStart,
		FVector2D(0.25f, SideVStart),
		FVector2D(0.5f, SideVScale)
	);
	BottomSideRing = BottomRing;

	// 为插值计算原始（未弯曲）的顶点
	TArray<int32> TopRingOrigin = GenerateVertexRing(Frustum.TopRadius, HalfHeight, Frustum.TopSides);
	TArray<int32> BottomRingOrigin = GenerateVertexRing(Frustum.BottomRadius, -HalfHeight, Frustum.BottomSides);
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

	// 实际分段数 = HeightSegments + 1（0表示只有顶部和底部，1表示顶部+底部+0个中间分段）
	// 中间分段数 = HeightSegments
	if (Frustum.HeightSegments > 0)
	{
		// 实际分段数 = HeightSegments + 1，所以需要 HeightSegments 个中间分段
		const int32 ActualHeightSegments = Frustum.HeightSegments + 1;
		const float HeightStep = Frustum.Height / ActualHeightSegments;

		for (int32 h = Frustum.HeightSegments; h > 0; --h)
		{
			const float CurrentHeight = HalfHeight - h * HeightStep;
			// 当 BendAmount = 0 时，HeightRatio 应该基于实际高度位置计算，确保顶点环在原先的棱上等距分布
			// HeightRatio 从底部（-HalfHeight）到顶部（HalfHeight）的线性映射：[0, 1]
			const float HeightRatio = (CurrentHeight + HalfHeight) / Frustum.Height;

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

				// 将原点移动到最底面（底部Z=0）：所有顶点向上偏移HalfHeight
				const FVector InterpolatedPos(X, Y, CurrentHeight + HalfHeight);

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

				const float U = static_cast<float>(BottomIndex) / Frustum.BottomSides;
				const float V = SideVStart + HeightRatio * SideVScale;
				const FVector2D UV = FVector2D(0.25f + U * 0.5f, V);

				const int32 VertexIndex = GetOrAddVertex(InterpolatedPos, Normal, UV);
				CurrentRing.Add(VertexIndex);
			}

			VertexRings.Add(CurrentRing);
		}
	}
	VertexRings.Add(TopRing);

	// 收集端盖边界连接点
	for (int32 i = 0; i < VertexRings.Num(); i++)
	{
		if (VertexRings[i].Num() > 0)
		{
			RecordEndCapConnectionPoint(VertexRings[i][0]);
		}
	}

	for (int i = 0; i < VertexRings.Num() - 1; i++)
	{
		TArray<int32> CurrentRing = VertexRings[i];
		TArray<int32> NextRing = VertexRings[i + 1];

		for (int32 CurrentIndex = 0; CurrentIndex < CurrentRing.Num() - 1; ++CurrentIndex)
		{
			const int32 NextCurrentIndex = CurrentIndex + 1;

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

		FVector BaseNormal = FVector(FMath::Cos(Angle + PI / 2), FMath::Sin(Angle + PI / 2), 0.0f);
		FVector EndCapNormal = (EndCapType == EEndCapType::Start) ? -BaseNormal : BaseNormal;

		if (Frustum.BendAmount > KINDA_SMALL_NUMBER)
		{
			const float Z = EndCapPos.Z;
			// Z已经是偏移后的值（底部在0），所以HeightRatio = Z / Height
			const float HeightRatio = Z / Frustum.Height;
			const float BendInfluence = FMath::Sin(HeightRatio * PI);
			FVector BendNormal = FVector(0, 0, BendInfluence).GetSafeNormal();
			EndCapNormal = (EndCapNormal + BendNormal * Frustum.BendAmount).GetSafeNormal();
		}

		const float Radius = FMath::Sqrt(EndCapPos.X * EndCapPos.X + EndCapPos.Y * EndCapPos.Y);
		const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);
		const float U = FMath::Clamp(Radius / MaxRadius, 0.0f, 1.0f);
		// Z已经是偏移后的值（底部在0），所以V = Z / Height
		const float V = EndCapPos.Z / Frustum.Height;
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

	const int32 VertexCount = Sides + 1;

	// 将原点移动到最底面（底部Z=0）：所有顶点向上偏移HalfHeight
	const float HalfHeight = Frustum.GetHalfHeight();
	const float AdjustedZ = Z + HalfHeight;

	for (int32 i = 0; i < VertexCount; ++i)
	{
		const float Angle = StartAngle + (i * AngleStep);

		const float X = Radius * FMath::Cos(Angle);
		const float Y = Radius * FMath::Sin(Angle);
		const FVector Pos(X, Y, AdjustedZ);
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

	const int32 VertexCount = Sides + 1;

	// 将原点移动到最底面（底部Z=0）：所有顶点向上偏移HalfHeight
	const float HalfHeight = Frustum.GetHalfHeight();
	const float AdjustedZ = Z + HalfHeight;

	// 判断是否为倒角连接点，软边时使用平滑法线
	// 计算沿着侧边棱移动后的位置（与 CreateSideGeometry 中的计算一致）
	const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);
	const float RadiusDiff = Frustum.TopRadius - Frustum.BottomRadius;
	const float SideLength = FMath::Sqrt(RadiusDiff * RadiusDiff + Frustum.Height * Frustum.Height);

	float TopBevelStartZ, BottomBevelStartZ;
	if (bEnableBevel && SideLength > KINDA_SMALL_NUMBER)
	{
		const float HeightDir = Frustum.Height / SideLength;
		TopBevelStartZ = HalfHeight - TopBevelHeight * HeightDir;
		BottomBevelStartZ = -HalfHeight + BottomBevelHeight * HeightDir;
	}
	else
	{
		TopBevelStartZ = HalfHeight;
		BottomBevelStartZ = -HalfHeight;
	}

	const bool bIsBevelConnection = (FMath::Abs(Z - TopBevelStartZ) < KINDA_SMALL_NUMBER) ||
		(FMath::Abs(Z - BottomBevelStartZ) < KINDA_SMALL_NUMBER);
	const bool bIsTopBevel = FMath::Abs(Z - TopBevelStartZ) < KINDA_SMALL_NUMBER;

	for (int32 i = 0; i < VertexCount; ++i)
	{
		const float Angle = StartAngle + i * AngleStep;
		const FVector Pos(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), AdjustedZ);
		FVector Normal = FVector(Pos.X, Pos.Y, 0).GetSafeNormal();

		if (bIsBevelConnection && bEnableBevel)
		{
			FVector SideNormal = Normal;
			FVector CapNormal = FVector(0.0f, 0.0f, bIsTopBevel ? 1.0f : -1.0f);
			Normal = (SideNormal + CapNormal).GetSafeNormal();
		}

		const float U = static_cast<float>(i) / Sides;
		const FVector2D UV = UVOffset + FVector2D(U * UVScale.X, VCoord * UVScale.Y);
		VertexRing.Add(GetOrAddVertex(Pos, Normal, UV));
	}

	return VertexRing;
}


void FFrustumBuilder::GenerateCapGeometry(float Z, int32 Sides, float Radius, EHeightPosition HeightPosition)
{
	TArray<int32>& CapRing = (HeightPosition == EHeightPosition::Top) ? TopCapRing : BottomCapRing;
	
	if (bEnableBevel && CapRing.Num() > 0)
	{
		GenerateCapTrianglesFromRing(CapRing, HeightPosition);
		return;
	}
	
	const float HalfHeight = Frustum.GetHalfHeight();
	const float AdjustedZ = Z + HalfHeight;

	FVector Normal(0.0f, 0.0f, (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f);

	const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);
	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;
	const float CapEdgeCircumference = 2.0f * PI * Radius;
	const float CapEdgeVScale = CapEdgeCircumference / AvgCircumference;
	const float EdgeUVRadius = CapEdgeVScale / (2.0f * PI);

	const FVector2D UVOffset = (HeightPosition == EHeightPosition::Top) ?
		FVector2D(0.5f, 0.5f) : FVector2D(0.5f, 0.0f);

	const float VScale = FMath::Clamp(CapEdgeVScale, 0.1f, 0.5f);
	const FVector2D UVScale(0.5f, VScale);
	const float ScaleRatio = (CapEdgeVScale > KINDA_SMALL_NUMBER) ? (VScale / CapEdgeVScale) : 1.0f;
	const float ActualEdgeUVRadius = EdgeUVRadius * ScaleRatio;

	const float CenterU = UVOffset.X + 0.5f * UVScale.X;
	const float CenterV = UVOffset.Y + 0.5f * UVScale.Y;
	const FVector2D CenterUV(CenterU, CenterV);

	const FVector CenterPos(0.0f, 0.0f, AdjustedZ);
	const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, CenterUV);

	const float AngleStep = CalculateAngleStep(Sides);
	TArray<int32> CapEdgeRing;
	CapEdgeRing.Reserve(Sides + 1);

	for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
	{
		const float CurrentAngle = StartAngle + (SideIndex * AngleStep);
		const float NextAngle = StartAngle + ((SideIndex + 1) * AngleStep);

		const FVector CurrentPos(CapRadius * FMath::Cos(CurrentAngle), CapRadius * FMath::Sin(CurrentAngle), AdjustedZ);
		const FVector NextPos(CapRadius * FMath::Cos(NextAngle), CapRadius * FMath::Sin(NextAngle), AdjustedZ);

		const float RadiusRatio = (Radius > KINDA_SMALL_NUMBER) ?
			FMath::Clamp(CapRadius / Radius, 0.0f, 1.0f) : 0.0f;
		const float UVRadius = RadiusRatio * ActualEdgeUVRadius;

		const FVector2D CurrentUV = CenterUV + FVector2D(
			FMath::Cos(CurrentAngle) * UVRadius,
			FMath::Sin(CurrentAngle) * UVRadius
		);
		const FVector2D NextUV = CenterUV + FVector2D(
			FMath::Cos(NextAngle) * UVRadius,
			FMath::Sin(NextAngle) * UVRadius
		);

		FVector CurrentVertexNormal = Normal;
		FVector NextVertexNormal = Normal;
		if (bEnableBevel)
		{
			FVector CurrentSideNormal = FVector(CurrentPos.X, CurrentPos.Y, 0.0f).GetSafeNormal();
			if (CurrentSideNormal.IsNearlyZero())
			{
				CurrentSideNormal = FVector(FMath::Cos(CurrentAngle), FMath::Sin(CurrentAngle), 0.0f);
			}
			CurrentVertexNormal = (Normal + CurrentSideNormal).GetSafeNormal();

			FVector NextSideNormal = FVector(NextPos.X, NextPos.Y, 0.0f).GetSafeNormal();
			if (NextSideNormal.IsNearlyZero())
			{
				NextSideNormal = FVector(FMath::Cos(NextAngle), FMath::Sin(NextAngle), 0.0f);
			}
			NextVertexNormal = (Normal + NextSideNormal).GetSafeNormal();
		}

		const int32 V1 = GetOrAddVertex(CurrentPos, CurrentVertexNormal, CurrentUV);
		const int32 V2 = GetOrAddVertex(NextPos, NextVertexNormal, NextUV);

		if (SideIndex == 0)
		{
			CapEdgeRing.Add(V1);
			RecordEndCapConnectionPoint(V1);
		}
		CapEdgeRing.Add(V2);

		if (HeightPosition == EHeightPosition::Top)
		{
			AddTriangle(CenterVertex, V2, V1);
		}
		else
		{
			AddTriangle(CenterVertex, V1, V2);
		}
	}

	if (HeightPosition == EHeightPosition::Top)
	{
		TopCapRing = CapEdgeRing;
	}
	else
	{
		BottomCapRing = CapEdgeRing;
	}
}

void FFrustumBuilder::GenerateCapTrianglesFromRing(const TArray<int32>& CapRing, EHeightPosition HeightPosition)
{
	if (CapRing.Num() < 2)
	{
		return;
	}

	const float CapZ = GetPosByIndex(CapRing[0]).Z;
	
	TArray<FVector> CapPositions;
	CapPositions.Reserve(CapRing.Num() + 1);
	
	const FVector CenterPos(0.0f, 0.0f, CapZ);
	CapPositions.Add(CenterPos);
	
	for (int32 VertexIdx : CapRing)
	{
		CapPositions.Add(GetPosByIndex(VertexIdx));
	}
	
	float MinX = CapPositions[0].X;
	float MaxX = CapPositions[0].X;
	float MinY = CapPositions[0].Y;
	float MaxY = CapPositions[0].Y;
	
	for (int32 i = 1; i < CapPositions.Num(); ++i)
	{
		MinX = FMath::Min(MinX, CapPositions[i].X);
		MaxX = FMath::Max(MaxX, CapPositions[i].X);
		MinY = FMath::Min(MinY, CapPositions[i].Y);
		MaxY = FMath::Max(MaxY, CapPositions[i].Y);
	}
	
	const float RangeX = MaxX - MinX;
	const float RangeY = MaxY - MinY;
	
	FVector Normal(0.0f, 0.0f, (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f);
	
	float CenterU = (RangeX > KINDA_SMALL_NUMBER) ? ((CenterPos.X - MinX) / RangeX) : 0.5f;
	float CenterV = (RangeY > KINDA_SMALL_NUMBER) ? ((CenterPos.Y - MinY) / RangeY) : 0.5f;
	const FVector2D CenterUV(CenterU, CenterV);
	const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, CenterUV);
	
	TMap<int32, int32> VertexIndexMap;
	
	for (int32 VertexIdx : CapRing)
	{
		const FVector Pos = GetPosByIndex(VertexIdx);
		
		float U = (RangeX > KINDA_SMALL_NUMBER) ? ((Pos.X - MinX) / RangeX) : 0.5f;
		float V = (RangeY > KINDA_SMALL_NUMBER) ? ((Pos.Y - MinY) / RangeY) : 0.5f;
		
		const FVector2D EdgeUV(U, V);
		const FVector EdgeNormal = MeshData.Normals[VertexIdx];
		const int32 NewVertexIdx = GetOrAddVertex(Pos, EdgeNormal, EdgeUV);
		VertexIndexMap.Add(VertexIdx, NewVertexIdx);
	}
	
	for (int32 i = 0; i < CapRing.Num() - 1; ++i)
	{
		const int32 V1Idx = VertexIndexMap[CapRing[i]];
		const int32 V2Idx = VertexIndexMap[CapRing[i + 1]];
		
		if (HeightPosition == EHeightPosition::Top)
		{
			AddTriangle(CenterVertex, V2Idx, V1Idx);
		}
		else
		{
			AddTriangle(CenterVertex, V1Idx, V2Idx);
		}
	}
}

void FFrustumBuilder::CalculateBevelPosition(bool bIsTop, int32 RingIndex, int32 BevelSections, float Theta,
	float CenterZ, float CenterRadius, float BevelRadius, int32 SideIndex, float AngleStep,
	const TArray<int32>& SideRing, FVector& OutPosition)
{
	if (RingIndex == 0)
	{
		if (bIsTop)
		{
			float CurrentZ = CenterZ + BevelRadius * FMath::Cos(Theta);
			float CurrentRadius = CenterRadius + BevelRadius * FMath::Sin(Theta);
			CurrentRadius = FMath::Max(0.0f, CurrentRadius);
			
			const float angle = StartAngle + (SideIndex * AngleStep);
			OutPosition = FVector(CurrentRadius * FMath::Cos(angle), CurrentRadius * FMath::Sin(angle), CurrentZ);
		}
		else
		{
			if (SideIndex < SideRing.Num())
			{
				OutPosition = GetPosByIndex(SideRing[SideIndex]);
			}
			else
			{
				float CurrentZ = CenterZ - BevelRadius * FMath::Sin(Theta);
				float CurrentRadius = CenterRadius - BevelRadius * FMath::Sin(Theta);
				
				const float angle = StartAngle + (SideIndex * AngleStep);
				OutPosition = FVector(CurrentRadius * FMath::Cos(angle), CurrentRadius * FMath::Sin(angle), CurrentZ);
			}
		}
	}
	else if (RingIndex == BevelSections)
	{
		if (bIsTop)
		{
			if (SideIndex < SideRing.Num())
			{
				OutPosition = GetPosByIndex(SideRing[SideIndex]);
			}
			else
			{
				float CurrentZ = CenterZ + BevelRadius * FMath::Cos(Theta);
				float CurrentRadius = CenterRadius + BevelRadius * FMath::Sin(Theta);
				
				const float angle = StartAngle + (SideIndex * AngleStep);
				OutPosition = FVector(CurrentRadius * FMath::Cos(angle), CurrentRadius * FMath::Sin(angle), CurrentZ);
			}
		}
		else
		{
			float CurrentZ = CenterZ - BevelRadius * FMath::Sin(Theta);
			float CurrentRadius = CenterRadius - BevelRadius * FMath::Sin(Theta);
			CurrentRadius = FMath::Max(0.0f, CurrentRadius);
			
			const float angle = StartAngle + (SideIndex * AngleStep);
			OutPosition = FVector(CurrentRadius * FMath::Cos(angle), CurrentRadius * FMath::Sin(angle), CurrentZ);
		}
	}
	else
	{
		float CurrentZ, CurrentRadius;
		
		if (bIsTop)
		{
			CurrentZ = CenterZ + BevelRadius * FMath::Cos(Theta);
			CurrentRadius = CenterRadius + BevelRadius * FMath::Sin(Theta);
		}
		else
		{
			CurrentZ = CenterZ - BevelRadius * FMath::Sin(Theta);
			CurrentRadius = CenterRadius - BevelRadius * FMath::Sin(Theta);
		}
		
		const float angle = StartAngle + (SideIndex * AngleStep);
		OutPosition = FVector(CurrentRadius * FMath::Cos(angle), CurrentRadius * FMath::Sin(angle), CurrentZ);
	}
}

FVector FFrustumBuilder::CalculateBevelNormal(bool bIsTop, int32 RingIndex, int32 BevelSections,
	const FVector& Position, float CenterZ, float CenterRadius, int32 SideIndex, float AngleStep,
	const TArray<int32>& SideRing)
{
	if ((bIsTop && RingIndex == BevelSections && SideIndex < SideRing.Num()) ||
		(!bIsTop && RingIndex == 0 && SideIndex < SideRing.Num()))
	{
		return MeshData.Normals[SideRing[SideIndex]];
	}
	
	if (RingIndex == 0)
	{
		if (bIsTop)
		{
			FVector CapNormal(0.0f, 0.0f, 1.0f);
			FVector SideNormal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
			if (SideNormal.IsNearlyZero())
			{
				SideNormal = FVector(FMath::Cos(StartAngle + SideIndex * AngleStep), FMath::Sin(StartAngle + SideIndex * AngleStep), 0.0f);
			}
			return (CapNormal + SideNormal).GetSafeNormal();
		}
		else
		{
			FVector Normal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
			return Normal.IsNearlyZero() ? FVector(1.0f, 0.0f, 0.0f) : Normal;
		}
	}
	
	if (RingIndex == BevelSections)
	{
		if (bIsTop)
		{
			FVector Normal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
			return Normal.IsNearlyZero() ? FVector(1.0f, 0.0f, 0.0f) : Normal;
		}
		else
		{
			FVector CapNormal(0.0f, 0.0f, -1.0f);
			FVector SideNormal = FVector(Position.X, Position.Y, 0.0f).GetSafeNormal();
			if (SideNormal.IsNearlyZero())
			{
				SideNormal = FVector(FMath::Cos(StartAngle + SideIndex * AngleStep), FMath::Sin(StartAngle + SideIndex * AngleStep), 0.0f);
			}
			return (CapNormal + SideNormal).GetSafeNormal();
		}
	}
	
	const float angle = StartAngle + (SideIndex * AngleStep);
	const FVector CenterPos(CenterRadius * FMath::Cos(angle), CenterRadius * FMath::Sin(angle), CenterZ);
	FVector Normal = (Position - CenterPos).GetSafeNormal();
	return Normal.IsNearlyZero() ? FVector(0.0f, 0.0f, bIsTop ? 1.0f : -1.0f) : Normal;
}

int32 FFrustumBuilder::CreateBevelVertex(bool bIsTop, int32 RingIndex, int32 BevelSections,
	const FVector& Position, const FVector& Normal, float Alpha, float Radius, float HalfHeight,
	EHeightPosition HeightPosition, const TArray<int32>& SideRing, int32 SideIndex)
{
	if ((bIsTop && RingIndex == BevelSections && SideIndex < SideRing.Num()) ||
		(!bIsTop && RingIndex == 0 && SideIndex < SideRing.Num()))
	{
		return SideRing[SideIndex];
	}
	
	const float Angle = StartAngle + (SideIndex * CalculateAngleStep(bIsTop ? Frustum.TopSides : Frustum.BottomSides));
	const float AlphaForUV = bIsTop ? Alpha : (1.0f - Alpha);
	const FVector2D UV = CalculateBevelUV(Angle, AlphaForUV, HeightPosition, Radius, Position.Z - HalfHeight);
	
	const int32 VertexIndex = GetOrAddVertex(Position, Normal, UV);
	
	if ((bIsTop && RingIndex == 0) || (!bIsTop && RingIndex == BevelSections))
	{
		TArray<int32>& CapRing = bIsTop ? TopCapRing : BottomCapRing;
		CapRing.Add(VertexIndex);
	}
	
	return VertexIndex;
}

void FFrustumBuilder::GenerateBevelGeometry(EHeightPosition HeightPosition)
{
	const bool bIsTop = (HeightPosition == EHeightPosition::Top);
	const float HalfHeight = Frustum.GetHalfHeight();
	const float BevelRadius = Frustum.BevelRadius;
	const int32 BevelSections = Frustum.BevelSegments;
	
	if (BevelRadius <= 0.0f || BevelSections <= 0)
	{
		return;
	}

	const float Radius = bIsTop ? Frustum.TopRadius : Frustum.BottomRadius;
	const int32 Sides = bIsTop ? Frustum.TopSides : Frustum.BottomSides;
	const TArray<int32>& SideRing = bIsTop ? TopSideRing : BottomSideRing;
	
	const float CenterZ_Unshifted = bIsTop ? (HalfHeight - BevelRadius) : (-HalfHeight + BevelRadius);
	const float CenterZ = CenterZ_Unshifted + HalfHeight;
	const float CenterRadius = bIsTop ? (Radius - BevelRadius) : Radius;

	TArray<int32> PrevRing;
	const float AngleStep = CalculateAngleStep(Sides);
	TArray<int32> StartRingForEndCap;

	for (int32 i = 0; i <= BevelSections; ++i)
	{
		const float Alpha = static_cast<float>(i) / BevelSections;
		const float Theta = Alpha * (PI / 2.0f);

		TArray<int32> CurrentRing;
		CurrentRing.Reserve(Sides + 1);

		const int32 VertexCount = Sides + 1;

		for (int32 s = 0; s < VertexCount; ++s)
		{
			FVector Position;
			CalculateBevelPosition(bIsTop, i, BevelSections, Theta, CenterZ, CenterRadius, BevelRadius, s, AngleStep, SideRing, Position);
			
			FVector Normal = CalculateBevelNormal(bIsTop, i, BevelSections, Position, CenterZ, CenterRadius, s, AngleStep, SideRing);
			
			const int32 VertexIndex = CreateBevelVertex(bIsTop, i, BevelSections, Position, Normal, Alpha, Radius, HalfHeight, HeightPosition, SideRing, s);
			
			CurrentRing.Add(VertexIndex);
		}

		if (i > 0 && PrevRing.Num() > 0)
		{
			for (int32 s = 0; s < Sides; ++s)
			{
				const int32 V00 = PrevRing[s];
				const int32 V10 = CurrentRing[s];
				const int32 V01 = PrevRing[s + 1];
				const int32 V11 = CurrentRing[s + 1];

				AddQuad(V00, V01, V11, V10);
			}
		}
		PrevRing = CurrentRing;
		StartRingForEndCap.Add(CurrentRing[0]);
	}

	if (bIsTop)
	{
		for (int32 i = StartRingForEndCap.Num() - 1; i >= 0; --i)
		{
			RecordEndCapConnectionPoint(StartRingForEndCap[i]);
		}
	}
	else
	{
		for (int32 i = 0; i < StartRingForEndCap.Num(); ++i)
		{
			RecordEndCapConnectionPoint(StartRingForEndCap[i]);
		}
	}
}

FVector2D FFrustumBuilder::CalculateWallUV(float Angle, float Z) const
{
	const float HalfHeight = Frustum.GetHalfHeight();
	const float AdjustedZ = Z + HalfHeight;
	
	const float U = (ArcAngleRadians > KINDA_SMALL_NUMBER) ?
		((Angle - StartAngle) / ArcAngleRadians) : 0.5f;
	const float NormalizedU = FMath::Clamp(U, 0.0f, 1.0f);
	
	const float TotalHeight = Frustum.Height;
	const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);
	const float SideHeight = TotalHeight - TopBevelHeight - BottomBevelHeight;
	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;
	
	const float BottomBevelVScale = BottomBevelHeight / AvgCircumference;
	const float SideVScale = SideHeight / AvgCircumference;
	const float TopBevelVScale = TopBevelHeight / AvgCircumference;
	
	const float SideVStart = (BottomBevelVScale > 0.0f) ? (BottomBevelVScale + 0.001f) : 0.0f;
	const float SideVEnd = SideVStart + SideVScale;
	
	const float HeightRatio = (TotalHeight > KINDA_SMALL_NUMBER) ? (AdjustedZ / TotalHeight) : 0.5f;
	const float V = SideVStart + HeightRatio * (SideVEnd - SideVStart);
	
	return FVector2D(0.25f + NormalizedU * 0.5f, V);
}

FVector2D FFrustumBuilder::CalculateCapUV(float Angle, float Radius, EHeightPosition HeightPosition) const
{
	const float U = (ArcAngleRadians > KINDA_SMALL_NUMBER) ?
		((Angle - StartAngle) / ArcAngleRadians) : 0.5f;
	const float NormalizedU = FMath::Clamp(U, 0.0f, 1.0f);
	
	const float CapRadius = (HeightPosition == EHeightPosition::Top) ? Frustum.TopRadius : Frustum.BottomRadius;
	const float V = (CapRadius > KINDA_SMALL_NUMBER) ?
		FMath::Clamp(Radius / CapRadius, 0.0f, 1.0f) : 0.0f;
	const float NormalizedV = FMath::Clamp(V, 0.0f, 1.0f);
	
	const float TotalHeight = Frustum.Height;
	const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);
	const float WallHeight = TotalHeight - TopBevelHeight - BottomBevelHeight;
	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;
	const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);
	
	const float WallVScale = WallHeight / AvgCircumference;
	const float BevelVScale = (TopBevelHeight + BottomBevelHeight) / AvgCircumference;
	const float CapVScale = MaxRadius / AvgCircumference;
	
	const float WallVOffset = 0.0f;
	const float TopBevelVOffset = WallVOffset + WallVScale;
	const float TopCapVOffset = TopBevelVOffset + BevelVScale;
	const float BottomBevelVOffset = TopCapVOffset + CapVScale;
	const float BottomCapVOffset = BottomBevelVOffset + BevelVScale;
	
	const float V_Start = (HeightPosition == EHeightPosition::Top) ? TopCapVOffset : BottomCapVOffset;
	const float V_End = V_Start + CapVScale;
	const float ActualV = V_Start + NormalizedV * (V_End - V_Start);
	
	return FVector2D(NormalizedU, ActualV);
}

FVector2D FFrustumBuilder::CalculateBevelUV(float Angle, float Alpha, EHeightPosition HeightPosition, float Radius, float Z) const
{
	const float AlphaThreshold = 0.5f;
	
	if (Alpha <= AlphaThreshold)
	{
		if (HeightPosition == EHeightPosition::Top)
		{
			return CalculateWallUV(Angle, Z);
		}
		else
		{
			return CalculateCapUV(Angle, Radius, EHeightPosition::Bottom);
		}
	}
	else
	{
		if (HeightPosition == EHeightPosition::Top)
		{
			return CalculateCapUV(Angle, Radius, EHeightPosition::Top);
		}
		else
		{
			return CalculateWallUV(Angle, Z);
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

float FFrustumBuilder::CalculateBevelHeight(float Radius) const
{
	return FMath::Min(Frustum.BevelRadius, Radius);
}

float FFrustumBuilder::CalculateHeightRatio(float Z)
{
	// Z已经是偏移后的值（底部在0），所以HeightRatio = Z / Height
	return Z / Frustum.Height;
}

float FFrustumBuilder::CalculateAngleStep(int32 Sides)
{
	if (Sides == 0) return 0.0f;
	return ArcAngleRadians / Sides;
}

void FFrustumBuilder::GenerateEndCapTrianglesFromVertices(const TArray<int32>& OrderedVertices, EEndCapType EndCapType, float Angle)
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

	const FVector BaseNormal = FVector(FMath::Cos(Angle + PI / 2), FMath::Sin(Angle + PI / 2), 0.0f);
	const FVector EndCapNormal = (EndCapType == EEndCapType::Start) ? -BaseNormal : BaseNormal;
	const FVector CenterLineNormal = EndCapNormal;

	// 端盖UV设置：计算完整的V值范围（倒角+侧面）
	const float TotalHeight = Frustum.Height;
	const float TopBevelHeight = CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelHeight = CalculateBevelHeight(Frustum.BottomRadius);
	const float SideHeight = TotalHeight - TopBevelHeight - BottomBevelHeight;
	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;
	const float BottomBevelVScale = BottomBevelHeight / AvgCircumference;
	const float SideVScale = SideHeight / AvgCircumference;
	const float TopBevelVScale = TopBevelHeight / AvgCircumference;
	const float TotalVScale = BottomBevelVScale + 0.001f + SideVScale + TopBevelVScale;
	const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);
	const FVector2D UVOffset = (EndCapType == EEndCapType::Start) ? FVector2D(0.0f, 0.0f) : FVector2D(0.75f, 0.0f);
	const FVector2D UVScale(0.25f, TotalVScale);

	if (SortedVertices.Num() == 2)
	{
		const int32 V1 = SortedVertices[0];
		const int32 V2 = SortedVertices[1];

		FVector Pos1 = GetPosByIndex(V1);
		FVector Pos2 = GetPosByIndex(V2);

		const float Radius1 = FMath::Sqrt(Pos1.X * Pos1.X + Pos1.Y * Pos1.Y);
		const float Radius2 = FMath::Sqrt(Pos2.X * Pos2.X + Pos2.Y * Pos2.Y);
		const float U1 = FMath::Clamp(Radius1 / MaxRadius, 0.0f, 1.0f);
		const float U2 = FMath::Clamp(Radius2 / MaxRadius, 0.0f, 1.0f);
		// Z已经是偏移后的值（底部在0），所以V = Z / Height
		const float V1_UV = Pos1.Z / Frustum.Height;
		const float V2_UV = Pos2.Z / Frustum.Height;

		const FVector2D UV1 = UVOffset + FVector2D(U1 * UVScale.X, V1_UV * UVScale.Y);
		const FVector2D UV2 = UVOffset + FVector2D(U2 * UVScale.X, V2_UV * UVScale.Y);
		const FVector2D CenterUV1 = UVOffset + FVector2D(0.5f * UVScale.X, V1_UV * UVScale.Y);
		const FVector2D CenterUV2 = UVOffset + FVector2D(0.5f * UVScale.X, V2_UV * UVScale.Y);

		const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), CenterLineNormal, CenterUV1);
		const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), CenterLineNormal, CenterUV2);
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

			const float Radius1 = FMath::Sqrt(Pos1.X * Pos1.X + Pos1.Y * Pos1.Y);
			const float Radius2 = FMath::Sqrt(Pos2.X * Pos2.X + Pos2.Y * Pos2.Y);
			const float U1 = FMath::Clamp(Radius1 / MaxRadius, 0.0f, 1.0f);
			const float U2 = FMath::Clamp(Radius2 / MaxRadius, 0.0f, 1.0f);
			// Z已经是偏移后的值（底部在0），所以V = Z / Height
			const float V1_UV = Pos1.Z / Frustum.Height;
			const float V2_UV = Pos2.Z / Frustum.Height;

			const FVector2D UV1 = UVOffset + FVector2D(U1 * UVScale.X, V1_UV * UVScale.Y);
			const FVector2D UV2 = UVOffset + FVector2D(U2 * UVScale.X, V2_UV * UVScale.Y);
			const FVector2D CenterUV1 = UVOffset + FVector2D(0.5f * UVScale.X, V1_UV * UVScale.Y);
			const FVector2D CenterUV2 = UVOffset + FVector2D(0.5f * UVScale.X, V2_UV * UVScale.Y);

			const int32 CenterV1 = GetOrAddVertex(FVector(0, 0, Pos1.Z), CenterLineNormal, CenterUV1);
			const int32 CenterV2 = GetOrAddVertex(FVector(0, 0, Pos2.Z), CenterLineNormal, CenterUV2);
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

void FFrustumBuilder::CalculateAngles()
{
	ArcAngleRadians = FMath::DegreesToRadians(Frustum.ArcAngle);
	StartAngle = -ArcAngleRadians / 2.0f;
	EndAngle = ArcAngleRadians / 2.0f;
}
