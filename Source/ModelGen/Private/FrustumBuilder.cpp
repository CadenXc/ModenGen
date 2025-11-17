// Copyright (c) 2024. All rights reserved.

#include "FrustumBuilder.h"
#include "Frustum.h"
#include "ModelGenMeshData.h"
#include "Math/UnrealMathUtility.h"

FFrustumBuilder::FFrustumBuilder(const AFrustum& InFrustum)
	: Frustum(InFrustum)
{
	Clear();
	CalculateAngles();
	BevelSegments = 4; // Default bevel segments; adjust or get from Frustum if available
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
	bEnableBevel = Frustum.BevelRadius > 0.0f && BevelSegments > 0;

	const float HalfHeight = Frustum.GetHalfHeight();
	const float BevelHeight = Frustum.BevelRadius;

	TopBevelHeight = bEnableBevel ? BevelHeight : 0.0f;
	BottomBevelHeight = bEnableBevel ? BevelHeight : 0.0f;

	TopBevelRadius = Frustum.TopRadius;
	TopBevelStartZ = HalfHeight - TopBevelHeight;

	BottomBevelRadius = Frustum.BottomRadius;
	BottomBevelStartZ = -HalfHeight + BottomBevelHeight;

	CreateSideGeometry();

	if (bEnableBevel)
	{
		GenerateBevelGeometry(EHeightPosition::Top);
		GenerateBevelGeometry(EHeightPosition::Bottom);
	}
	else
	{
		// 无倒角时，直接生成端盖作为侧边连接
		TopCapRing = GenerateVertexRing(Frustum.TopRadius, HalfHeight, Frustum.TopSides);
		BottomCapRing = GenerateVertexRing(Frustum.BottomRadius, -HalfHeight, Frustum.BottomSides);
	}

	GenerateCapGeometry(HalfHeight, Frustum.TopSides, Frustum.TopRadius, EHeightPosition::Top, TopCapRing);
	GenerateCapGeometry(-HalfHeight, Frustum.BottomSides, Frustum.BottomRadius, EHeightPosition::Bottom, BottomCapRing);

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

	// 侧边UV映射：使用周长比例计算V值
	const float TotalHeight = Frustum.Height;
	const float SideHeight = TotalHeight - TopBevelHeight - BottomBevelHeight;

	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;

	const float BottomBevelVScale = BottomBevelHeight / AvgCircumference;
	const float SideVScale = SideHeight / AvgCircumference;
	const float TopBevelVScale = TopBevelHeight / AvgCircumference;

	const float SideVStart = BottomBevelVScale + 0.001f;
	const float SideVEnd = SideVStart + SideVScale;

	TArray<int32> TopRing = GenerateVertexRing(
		TopBevelRadius,
		TopBevelStartZ,
		Frustum.TopSides,
		SideVEnd,
		FVector2D(0.25f, SideVEnd),
		FVector2D(0.5f, 0.0f) // V fixed for ring
	);
	TopSideRing = TopRing;

	TArray<int32> BottomRing = GenerateVertexRing(
		BottomBevelRadius,
		BottomBevelStartZ,
		Frustum.BottomSides,
		SideVStart,
		FVector2D(0.25f, SideVStart),
		FVector2D(0.5f, 0.0f)
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

	if (Frustum.HeightSegments > 0)
	{
		const int32 ActualHeightSegments = Frustum.HeightSegments + 1;
		const float HeightStep = Frustum.Height / ActualHeightSegments;

		for (int32 h = Frustum.HeightSegments; h > 0; --h)
		{
			const float CurrentHeight = HalfHeight - h * HeightStep;
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

				const FVector InterpolatedPos(X, Y, CurrentHeight + HalfHeight);

				FVector Normal = FVector(X, Y, 0.0f).GetSafeNormal();
				if (Normal.IsNearlyZero())
				{
					Normal = FVector(1.0f, 0.0f, 0.0f);
				}

				if (Frustum.BendAmount > KINDA_SMALL_NUMBER)
				{
					const float BendFactor = FMath::Sin(HeightRatio * PI);
					const float NormalZ = Frustum.BendAmount * FMath::Cos(HeightRatio * PI);
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

void FFrustumBuilder::GenerateBevelGeometry(EHeightPosition HeightPosition)
{
	const bool bTop = HeightPosition == EHeightPosition::Top;
	const float Radius = bTop ? Frustum.TopRadius : Frustum.BottomRadius;
	const float Z_cap = bTop ? Frustum.GetHalfHeight() : -Frustum.GetHalfHeight();
	const int32 Sides = bTop ? Frustum.TopSides : Frustum.BottomSides;
	const float AngleStep = CalculateAngleStep(Sides);
	const FVector CapNormal(0.0f, 0.0f, bTop ? 1.0f : -1.0f);
	const float sign_z = bTop ? -1.0f : 1.0f;
	const float BevelRadius = Frustum.BevelRadius;

	// UV for bevel
	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;
	const float BevelVScale = BevelRadius / AvgCircumference;
	const float CapV = bTop ? 1.0f : 0.0f;
	const float SideV = bTop ? CapV - BevelVScale : CapV + BevelVScale;

	// Bevel vertex grid: Sides x (BevelSegments + 1)
	TArray<TArray<int32>> BevelVertexGrid;
	BevelVertexGrid.SetNum(Sides + 1);

	for (int32 s = 0; s <= Sides; ++s)
	{
		const float angle = StartAngle + s * AngleStep;
		BevelVertexGrid[s].SetNum(BevelSegments + 1);

		const float cos_angle = FMath::Cos(angle);
		const float sin_angle = FMath::Sin(angle);

		for (int32 i = 0; i <= BevelSegments; ++i)
		{
			const float theta_ratio = static_cast<float>(i) / BevelSegments;
			const float theta = theta_ratio * (PI / 2.0f);

			const float dr = BevelRadius * FMath::Sin(theta);
			const float dz_offset = BevelRadius * FMath::Cos(theta);

			const float r = (Radius - BevelRadius) + dr;
			const float z = Z_cap + sign_z * (BevelRadius - dz_offset); // Adjust for direction

			const FVector Pos(r * cos_angle, r * sin_angle, z + Frustum.GetHalfHeight());

			// 计算倒角法线：倒角是一个圆角过渡，法线应该垂直于倒角表面并指向外部
			// 倒角的圆心在 (r_center, z_center)，法线应该从圆心指向顶点（指向外部）
			const float r_center = Radius - BevelRadius;
			const float z_center = Z_cap + sign_z * BevelRadius;
			const FVector CenterPos(r_center * cos_angle, r_center * sin_angle, z_center + Frustum.GetHalfHeight());

			// 计算倒角法线：倒角是一个圆角过渡，法线应该垂直于倒角表面并指向外部
			// 倒角的圆心在 (r_center, z_center)，法线应该从圆心指向顶点
			// 但由于倒角是向内凹的，法线需要反转以指向外部
			FVector Normal = (Pos - CenterPos).GetSafeNormal();
			
			// 反转法线方向，使其指向外部（倒角是向内凹的，所以法线需要反转）
			Normal = -Normal;

			// UV
			FVector2D UV;
			if (i == 0)
			{
				// For inner, use cap fixed V, angle U
				const float U = static_cast<float>(s % Sides) / Sides;
				UV = FVector2D(0.25f + U * 0.5f, CapV);
			}
			else
			{
				// For bevel, angle U, V based on theta
				const float U = static_cast<float>(s % Sides) / Sides;
				const float V = CapV + sign_z * (theta_ratio * BevelVScale); // sign for top/bottom
				UV = FVector2D(0.25f + U * 0.5f, V);
			}

			const int32 VertexIndex = GetOrAddVertex(Pos, Normal, UV);
			BevelVertexGrid[s][i] = VertexIndex;
		}
	}

	// Generate quads for bevel surface
	for (int32 s = 0; s < Sides; ++s)
	{
		for (int32 i = 0; i < BevelSegments; ++i)
		{
			const int32 V00 = BevelVertexGrid[s][i];
			const int32 V10 = BevelVertexGrid[s][i + 1];
			const int32 V01 = BevelVertexGrid[s + 1][i];
			const int32 V11 = BevelVertexGrid[s + 1][i + 1];

			// Order for normal outward - 反转顶点顺序以修复渲染方向
			if (bTop)
			{
				// 反转顺序：从 V00, V10, V11, V01 改为 V00, V01, V11, V10
				AddQuad(V00, V01, V11, V10);
			}
			else
			{
				// 反转顺序：从 V00, V01, V11, V10 改为 V00, V10, V11, V01
				AddQuad(V00, V10, V11, V01);
			}
		}
	}

	// Extract inner and outer rings
	TArray<int32> InnerRing;
	TArray<int32> OuterRing;
	for (int32 s = 0; s < Sides; ++s)
	{
		InnerRing.Add(BevelVertexGrid[s][0]);
		OuterRing.Add(BevelVertexGrid[s][BevelSegments]);
	}

	if (bTop)
	{
		TopCapRing = InnerRing;
		TopSideRing = OuterRing;
	}
	else
	{
		BottomCapRing = InnerRing;
		BottomSideRing = OuterRing;
	}
}

void FFrustumBuilder::GenerateCapGeometry(float Z, int32 Sides, float Radius, EHeightPosition HeightPosition, const TArray<int32>& EdgeRing)
{
	const bool bTop = HeightPosition == EHeightPosition::Top;
	const float CapV = bTop ? 1.0f : 0.0f;

	TArray<int32> LocalEdgeRing = EdgeRing;
	if (LocalEdgeRing.Num() == 0)
	{
		// Generate full ring for no bevel
		LocalEdgeRing = GenerateVertexRing(Radius, Z, Sides, CapV, FVector2D(0.25f, CapV), FVector2D(0.5f, 0.0f));
	}

	if (LocalEdgeRing.Num() < 3)
	{
		return;
	}

	const float AdjustedZ = Z + Frustum.GetHalfHeight();
	FVector Normal(0.0f, 0.0f, bTop ? 1.0f : -1.0f);

	const FVector CenterPos(0.0f, 0.0f, AdjustedZ);
	const FVector2D CenterUV(0.5f, CapV);
	const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, CenterUV);

	// Generate fan triangles
	for (int32 i = 0; i < Sides; ++i)
	{
		const int32 V1 = LocalEdgeRing[i];
		const int32 V2 = LocalEdgeRing[(i + 1) % Sides];

		if (bTop)
		{
			AddTriangle(CenterVertex, V2, V1);
		}
		else
		{
			AddTriangle(CenterVertex, V1, V2);
		}
	}

	// Save the ring
	if (bTop)
	{
		TopCapRing = LocalEdgeRing;
	}
	else
	{
		BottomCapRing = LocalEdgeRing;
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
			const float HeightRatio = EndCapPos.Z / Frustum.Height;
			const float BendInfluence = FMath::Sin(HeightRatio * PI);
			FVector BendNormal = FVector(0, 0, BendInfluence).GetSafeNormal();
			EndCapNormal = (EndCapNormal + BendNormal * Frustum.BendAmount).GetSafeNormal();
		}

		const float Radius_pos = FMath::Sqrt(EndCapPos.X * EndCapPos.X + EndCapPos.Y * EndCapPos.Y);
		const float MaxRadius = FMath::Max(Frustum.TopRadius, Frustum.BottomRadius);
		const float U = FMath::Clamp(Radius_pos / MaxRadius, 0.0f, 1.0f);
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

	const float AngleStep = CalculateAngleStep(Sides);

	const int32 VertexCount = Sides + 1;

	const float HalfHeight = Frustum.GetHalfHeight();
	const float AdjustedZ = Z + HalfHeight;

	const bool bIsTopBevel = bEnableBevel && FMath::Abs(Z - TopBevelStartZ) < KINDA_SMALL_NUMBER;
	const bool bIsBottomBevel = bEnableBevel && FMath::Abs(Z - BottomBevelStartZ) < KINDA_SMALL_NUMBER;

	for (int32 i = 0; i < VertexCount; ++i)
	{
		const float Angle = StartAngle + i * AngleStep;
		const FVector Pos(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), AdjustedZ);
		FVector Normal = FVector(Pos.X, Pos.Y, 0).GetSafeNormal();

		if (bIsTopBevel || bIsBottomBevel)
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

float FFrustumBuilder::CalculateBentRadius(float BaseRadius, float HeightRatio)
{
	const float BendFactor = FMath::Sin(HeightRatio * PI);
	const float BentRadius = BaseRadius - Frustum.BendAmount * BendFactor * BaseRadius;

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

	// 端盖UV设置
	const float TotalHeight = Frustum.Height;
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

		if (EndCapType == EEndCapType::Start)
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

			if (EndCapType == EEndCapType::Start)
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