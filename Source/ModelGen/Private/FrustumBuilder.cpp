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

    CreateSideGeometry();

    if (Frustum.BevelRadius > 0.0f)
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
	const float TopBevelStartZ = HalfHeight - CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelStartZ = -HalfHeight + CalculateBevelHeight(Frustum.BottomRadius);

	// 侧边UV映射：使用周长比例计算V值
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
	
	TArray<int32> TopRing = GenerateVertexRing(
		Frustum.TopRadius,
		TopBevelStartZ,
		Frustum.TopSides,
		1.0f,
		FVector2D(0.25f, SideVStart),
		FVector2D(0.5f, SideVScale)
	);
	TopSideRing = TopRing;

	TArray<int32> BottomRing = GenerateVertexRing(
		Frustum.BottomRadius,
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

	// 判断是否为倒角连接点，软边时使用平滑法线（使用原始Z值进行判断）
	const float TopBevelStartZ = HalfHeight - CalculateBevelHeight(Frustum.TopRadius);
	const float BottomBevelStartZ = -HalfHeight + CalculateBevelHeight(Frustum.BottomRadius);
	const bool bIsBevelConnection = (FMath::Abs(Z - TopBevelStartZ) < KINDA_SMALL_NUMBER) || 
	                                 (FMath::Abs(Z - BottomBevelStartZ) < KINDA_SMALL_NUMBER);
	const bool bIsTopBevel = FMath::Abs(Z - TopBevelStartZ) < KINDA_SMALL_NUMBER;
	const bool bHasBevel = Frustum.BevelRadius > 0.0f;

	for (int32 i = 0; i < VertexCount; ++i)
	{
		const float Angle = StartAngle + i * AngleStep;
		const FVector Pos(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), AdjustedZ);
		FVector Normal = FVector(Pos.X, Pos.Y, 0).GetSafeNormal();
		
		if (bIsBevelConnection && bHasBevel)
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
	// 将原点移动到最底面（底部Z=0）：所有顶点向上偏移HalfHeight
	const float HalfHeight = Frustum.GetHalfHeight();
	const float AdjustedZ = Z + HalfHeight;

	FVector Normal(0.0f, 0.0f, (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f);

	// 计算实际半径（考虑倒角）
	const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);

	// 计算与侧边对齐的V值比例
	const float AvgRadius = (Frustum.TopRadius + Frustum.BottomRadius) * 0.5f;
	const float AvgCircumference = 2.0f * PI * AvgRadius;
	const float CapEdgeCircumference = 2.0f * PI * Radius;
	const float CapEdgeVScale = CapEdgeCircumference / AvgCircumference;

	// 圆形UV映射：边缘周长在UV图上的长度 = 2π * EdgeUVRadius = CapEdgeVScale
	const float EdgeUVRadius = CapEdgeVScale / (2.0f * PI);
	
	const FVector2D UVOffset = (HeightPosition == EHeightPosition::Top) ?
		FVector2D(0.5f, 0.5f) : FVector2D(0.5f, 0.0f);
	
	const float VScale = FMath::Clamp(CapEdgeVScale, 0.1f, 0.5f);
	const FVector2D UVScale(0.5f, VScale);
	
	// 如果VScale被限制，相应缩放EdgeUVRadius以保持在UV区域内
	const float ScaleRatio = (CapEdgeVScale > KINDA_SMALL_NUMBER) ? (VScale / CapEdgeVScale) : 1.0f;
	const float ActualEdgeUVRadius = EdgeUVRadius * ScaleRatio;

	const float CenterU = UVOffset.X + 0.5f * UVScale.X;
	const float CenterV = UVOffset.Y + 0.5f * UVScale.Y;
	const FVector2D CenterUV(CenterU, CenterV);

	const FVector CenterPos(0.0f, 0.0f, AdjustedZ);
	const int32 CenterVertex = GetOrAddVertex(CenterPos, Normal, CenterUV);

	const float AngleStep = CalculateAngleStep(Sides);
	
	// 保存端盖边缘顶点环索引，供倒角复用
	TArray<int32> CapEdgeRing;
	CapEdgeRing.Reserve(Sides + 1);

	for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
	{
		const float CurrentAngle = StartAngle + (SideIndex * AngleStep);
		const float NextAngle = StartAngle + ((SideIndex + 1) * AngleStep);

		const FVector CurrentPos(CapRadius * FMath::Cos(CurrentAngle), CapRadius * FMath::Sin(CurrentAngle), AdjustedZ);
		const FVector NextPos(CapRadius * FMath::Cos(NextAngle), CapRadius * FMath::Sin(NextAngle), AdjustedZ);

		// 圆形UV映射：计算从中心到边缘的UV半径（基于半径比例）
		const float CurrentRadiusRatio = (Radius > KINDA_SMALL_NUMBER) ? 
			FMath::Clamp(CapRadius / Radius, 0.0f, 1.0f) : 0.0f;
		const float NextRadiusRatio = (Radius > KINDA_SMALL_NUMBER) ?
			FMath::Clamp(CapRadius / Radius, 0.0f, 1.0f) : 0.0f;
		
		const float CurrentUVRadius = CurrentRadiusRatio * ActualEdgeUVRadius;
		const float NextUVRadius = NextRadiusRatio * ActualEdgeUVRadius;
		
		const FVector2D CurrentUV = CenterUV + FVector2D(
			FMath::Cos(CurrentAngle) * CurrentUVRadius,
			FMath::Sin(CurrentAngle) * CurrentUVRadius
		);
		const FVector2D NextUV = CenterUV + FVector2D(
			FMath::Cos(NextAngle) * NextUVRadius,
			FMath::Sin(NextAngle) * NextUVRadius
		);

		// 法线计算：软边时使用平滑法线（端盖法线 + 侧边法线的归一化和）
		FVector CurrentVertexNormal = Normal;
		FVector NextVertexNormal = Normal;
		if (Frustum.BevelRadius > 0.0f)
		{
			FVector CapNormal = Normal;
			
			FVector CurrentSideNormal = FVector(CurrentPos.X, CurrentPos.Y, 0.0f).GetSafeNormal();
			if (CurrentSideNormal.IsNearlyZero())
			{
				CurrentSideNormal = FVector(FMath::Cos(CurrentAngle), FMath::Sin(CurrentAngle), 0.f);
			}
			CurrentVertexNormal = (CapNormal + CurrentSideNormal).GetSafeNormal();
			
			FVector NextSideNormal = FVector(NextPos.X, NextPos.Y, 0.0f).GetSafeNormal();
			if (NextSideNormal.IsNearlyZero())
			{
				NextSideNormal = FVector(FMath::Cos(NextAngle), FMath::Sin(NextAngle), 0.f);
			}
			NextVertexNormal = (CapNormal + NextSideNormal).GetSafeNormal();
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
	
	// 保存端盖边缘环索引
	if (HeightPosition == EHeightPosition::Top)
	{
		TopCapRing = CapEdgeRing;
	}
	else
	{
		BottomCapRing = CapEdgeRing;
	}
}

void FFrustumBuilder::GenerateBevelGeometry(EHeightPosition HeightPosition)
{
	const float HalfHeight = Frustum.GetHalfHeight();
	const float BevelRadius = Frustum.BevelRadius;

	if (BevelRadius <= 0.0f)
	{
		return;
	}

	const float Radius = (HeightPosition == EHeightPosition::Top) ? Frustum.TopRadius : Frustum.BottomRadius;
	const int32 Sides = (HeightPosition == EHeightPosition::Top) ? Frustum.TopSides : Frustum.BottomSides;

	// 倒角UV映射：使用周长比例计算V值
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
	
	FVector2D UVOffset, UVScale;
	if (HeightPosition == EHeightPosition::Top)
	{
		UVOffset = FVector2D(0.25f, SideVEnd);
		UVScale = FVector2D(0.5f, TopBevelVScale);
	}
	else
	{
		UVOffset = FVector2D(0.25f, 0.0f);
		UVScale = FVector2D(0.5f, BottomBevelVScale);
	}

	// 将原点移动到最底面（底部Z=0）：所有顶点向上偏移HalfHeight
	const float EndZ = (HeightPosition == EHeightPosition::Top) ? HalfHeight : -HalfHeight;
	const float StartZ = (HeightPosition == EHeightPosition::Top) ? (HalfHeight - BevelRadius) : (-HalfHeight + BevelRadius);
	const float AdjustedEndZ = EndZ + HalfHeight;
	const float AdjustedStartZ = StartZ + HalfHeight;
	const float AngleStep = CalculateAngleStep(Sides);
	const int32 RingSize = Sides + 1;

	const TArray<int32>& SideRing = (HeightPosition == EHeightPosition::Top) ? TopSideRing : BottomSideRing;
	const TArray<int32>& CapRing = (HeightPosition == EHeightPosition::Top) ? TopCapRing : BottomCapRing;

	TArray<int32> StartRing;
	TArray<int32> EndRing;
	StartRing.Reserve(RingSize);
	EndRing.Reserve(RingSize);

	for (int32 s = 0; s < RingSize; ++s)
	{
		const float angle = StartAngle + (s * AngleStep);

		// StartRing：软边时复用侧边环顶点，避免重复
		const FVector SidePos(Radius * FMath::Cos(angle), Radius * FMath::Sin(angle), AdjustedStartZ);

		FVector SideNormal = FVector(SidePos.X, SidePos.Y, 0.0f).GetSafeNormal();
		if (SideNormal.IsNearlyZero())
		{
			SideNormal = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.f);
		}
		FVector CapNormal = FVector(0.0f, 0.0f, (HeightPosition == EHeightPosition::Top) ? 1.0f : -1.0f);
		
		// 法线计算：软边使用平滑法线（侧边和端盖法线的归一化和），参考BevelCube的实现
		FVector StartRingNormal, EndRingNormal;
		if (BevelRadius > 0.0f)
		{
			FVector SmoothNormal = (SideNormal + CapNormal).GetSafeNormal();
			StartRingNormal = SmoothNormal;
			EndRingNormal = SmoothNormal;
		}
		else
		{
			StartRingNormal = SideNormal;
			EndRingNormal = CapNormal;
		}
		int32 StartRingVertexIndex;
		if (BevelRadius > 0.0f && SideRing.Num() > 0)
		{
			const float SideRingRatio = static_cast<float>(s) / (RingSize - 1);
			const int32 SideRingIndex = FMath::Clamp(FMath::RoundToInt(SideRingRatio * (SideRing.Num() - 1)), 0, SideRing.Num() - 1);
			StartRingVertexIndex = SideRing[SideRingIndex];
		}
		else
		{
			const float U = static_cast<float>(s) / Sides;
			const FVector2D UV_Side = FVector2D(UVOffset.X + U * UVScale.X, UVOffset.Y);
			StartRingVertexIndex = GetOrAddVertex(SidePos, StartRingNormal, UV_Side);
		}

		StartRing.Add(StartRingVertexIndex);

		// EndRing：软边时复用端盖边缘环顶点，避免重复
		const float CapRadius = FMath::Max(0.0f, Radius - Frustum.BevelRadius);
		const FVector CapPos(CapRadius * FMath::Cos(angle), CapRadius * FMath::Sin(angle), AdjustedEndZ);
		
		int32 EndRingVertexIndex;
		if (BevelRadius > 0.0f && CapRing.Num() > 0)
		{
			const float CapRingRatio = static_cast<float>(s) / (RingSize - 1);
			const int32 CapRingIndex = FMath::Clamp(FMath::RoundToInt(CapRingRatio * (CapRing.Num() - 1)), 0, CapRing.Num() - 1);
			EndRingVertexIndex = CapRing[CapRingIndex];
		}
		else
		{
			const float U = static_cast<float>(s) / Sides;
			const float V_Cap_Actual = UVOffset.Y + UVScale.Y;
			const FVector2D UV_Cap = FVector2D(UVOffset.X + U * UVScale.X, V_Cap_Actual);
			EndRingVertexIndex = GetOrAddVertex(CapPos, EndRingNormal, UV_Cap);
		}
		
		EndRing.Add(EndRingVertexIndex);
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
