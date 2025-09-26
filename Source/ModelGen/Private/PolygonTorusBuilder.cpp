// Copyright (c) 2024. All rights reserved.

#include "PolygonTorusBuilder.h"
#include "PolygonTorus.h"
#include "ModelGenMeshData.h"

FPolygonTorusBuilder::FPolygonTorusBuilder(const APolygonTorus& InPolygonTorus)
    : PolygonTorus(InPolygonTorus)
{
    Clear();
    ValidateAndClampParameters();
}

bool FPolygonTorusBuilder::Generate(FModelGenMeshData& OutMeshData) 
{
    if (!PolygonTorus.IsValid()) 
    {
        return false;
    }

    Clear();
    ReserveMemory();

    GenerateTriangles();

    if (PolygonTorus.TorusAngle < 360.0f - KINDA_SMALL_NUMBER) 
    {
        GenerateEndCaps();
    }

    if (!ValidateGeneratedData()) 
    {
        return false;
    }

    // 计算正确的切线（用于法线贴图等）
    MeshData.CalculateTangents();

    OutMeshData = MeshData;
    return true;
}

int32 FPolygonTorusBuilder::CalculateVertexCountEstimate() const 
{
    return PolygonTorus.CalculateVertexCountEstimate();
}

int32 FPolygonTorusBuilder::CalculateTriangleCountEstimate() const 
{
    return PolygonTorus.CalculateTriangleCountEstimate();
}

void FPolygonTorusBuilder::GenerateEndCaps() 
{
    GenerateAdvancedEndCaps();
}


void FPolygonTorusBuilder::GenerateAdvancedEndCaps()
{
    if (FMath::IsNearlyEqual(PolygonTorus.TorusAngle, 360.0f) || StartCapIndices.Num() == 0 || EndCapIndices.Num() == 0)
    {
        return;
    }

    const float MajorRad = PolygonTorus.MajorRadius;
    const float MinorRad = PolygonTorus.MinorRadius;
    const float AngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const float MinorAngleStep = 2.0f * PI / FMath::Max(1, MinorSegs);
    
    // 计算周长比例，用于端盖UV计算
    const float MajorCircumference = 2.0f * PI * MajorRad;
    const float MinorCircumference = 2.0f * PI * MinorRad;
    const float CircumferenceRatio = MajorCircumference / MinorCircumference;
    
    // 计算小圆环一段的长度，用于端盖UV半径计算
    const float MinorSegmentLength = MinorCircumference / MinorSegs;
    const float MajorSegmentLength = MajorCircumference / PolygonTorus.MajorSegments;

    // --- 起始封盖 (Start Cap) ---
    const float StartAngle = -AngleRad / 2.0f;
    const FVector StartCenter = FVector(FMath::Cos(StartAngle) * MajorRad, FMath::Sin(StartAngle) * MajorRad, 0.0f);
    const FVector StartNormal = FVector(FMath::Sin(StartAngle), -FMath::Cos(StartAngle), 0.0f);
    // 根据角度比例动态计算UV中心位置
    const float AngleRatio = AngleRad / (2.0f * PI);
    const FVector2D StartCenterUV(0.5f - AngleRatio * 0.4f, 0.5f);
    const int32 StartCenterIndex = GetOrAddVertex(StartCenter, StartNormal, StartCenterUV);

    TArray<int32> NewStartCapEdgeIndices;
    for (int32 i = 0; i < MinorSegs; ++i)
    {
        const FVector EdgePos = GetPosByIndex(StartCapIndices[i]);
        const float MinorAngle = (i * MinorAngleStep) - (PI / 2.0f) - (MinorAngleStep / 2.0f);
        // 端盖的半径应该与小圆环相同，所以UV半径也使用相同的比例
        // 小圆环半径与主圆环半径的比例
        const float UVRadius = MinorRad / MajorRad * 0.3f;
        const FVector2D EdgeUV = StartCenterUV + FVector2D(FMath::Cos(MinorAngle) * UVRadius, FMath::Sin(MinorAngle) * UVRadius);
        const int32 NewEdgeIndex = GetOrAddVertex(EdgePos, StartNormal, EdgeUV);
        NewStartCapEdgeIndices.Add(NewEdgeIndex);
    }

    for (int32 i = 0; i < NewStartCapEdgeIndices.Num(); ++i)
    {
        const int32 CurrentIndex = NewStartCapEdgeIndices[i];
        const int32 NextIndex = NewStartCapEdgeIndices[(i + 1) % NewStartCapEdgeIndices.Num()];
        AddTriangle(StartCenterIndex, NextIndex, CurrentIndex);
    }

    // --- 终止封盖 (End Cap) ---
    const float EndAngle = AngleRad / 2.0f;
    const FVector EndCenter = FVector(FMath::Cos(EndAngle) * MajorRad, FMath::Sin(EndAngle) * MajorRad, 0.0f);
    const FVector EndNormal = FVector(-FMath::Sin(EndAngle), FMath::Cos(EndAngle), 0.0f);
    // 根据角度比例动态计算UV中心位置
    const FVector2D EndCenterUV(0.5f + AngleRatio * 0.4f, 0.5f);
    const int32 EndCenterIndex = GetOrAddVertex(EndCenter, EndNormal, EndCenterUV);

    TArray<int32> NewEndCapEdgeIndices;
    for (int32 i = 0; i < MinorSegs; ++i)
    {
        const FVector EdgePos = GetPosByIndex(EndCapIndices[i]);
        const float MinorAngle = (i * MinorAngleStep) - (PI / 2.0f) - (MinorAngleStep / 2.0f);
        // 端盖的半径应该与小圆环相同，所以UV半径也使用相同的比例
        // 小圆环半径与主圆环半径的比例
        const float UVRadius = MinorRad / MajorRad * 0.3f;
        const FVector2D EdgeUV = EndCenterUV + FVector2D(FMath::Cos(MinorAngle) * UVRadius, FMath::Sin(MinorAngle) * UVRadius);
        const int32 NewEdgeIndex = GetOrAddVertex(EdgePos, EndNormal, EdgeUV);
        NewEndCapEdgeIndices.Add(NewEdgeIndex);
    }

    for (int32 i = 0; i < NewEndCapEdgeIndices.Num(); ++i)
    {
        const int32 CurrentIndex = NewEndCapEdgeIndices[i];
        const int32 NextIndex = NewEndCapEdgeIndices[(i + 1) % NewEndCapEdgeIndices.Num()];
        AddTriangle(EndCenterIndex, CurrentIndex, NextIndex);
    }
}

void FPolygonTorusBuilder::ValidateAndClampParameters()
{
    if (PolygonTorus.MajorSegments < 3 || PolygonTorus.MajorSegments > 256)
    {
        UE_LOG(LogTemp, Warning, TEXT("主分段数 %d 超出有效范围 [3, 256]，建议调整"), PolygonTorus.MajorSegments);
    }

    if (PolygonTorus.MinorSegments < 3 || PolygonTorus.MinorSegments > 256)
    {
        UE_LOG(LogTemp, Warning, TEXT("次分段数 %d 超出有效范围 [3, 256]，建议调整"), PolygonTorus.MinorSegments);
    }

    if (PolygonTorus.MajorRadius < 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("主半径 %f 小于最小值 1.0，建议调整"), PolygonTorus.MajorRadius);
    }

    if (PolygonTorus.MinorRadius < 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("次半径 %f 小于最小值 1.0，建议调整"), PolygonTorus.MinorRadius);
    }

    if (PolygonTorus.MinorRadius > PolygonTorus.MajorRadius * 0.9f)
    {
        UE_LOG(LogTemp, Warning, TEXT("次半径 %f 超过主半径的 90%% (%f)，建议调整"),
            PolygonTorus.MinorRadius, PolygonTorus.MajorRadius * 0.9f);
    }

    if (PolygonTorus.TorusAngle < 1.0f || PolygonTorus.TorusAngle > 360.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("圆环角度 %f 超出有效范围 [1.0, 360.0]，建议调整"), PolygonTorus.TorusAngle);
    }
}

void FPolygonTorusBuilder::GenerateTriangles()
{
    // 获取圆环参数
    const float MajorRad = PolygonTorus.MajorRadius;
    const float MinorRad = PolygonTorus.MinorRadius;
    const int32 MajorSegs = PolygonTorus.MajorSegments;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const float TorusAngle = PolygonTorus.TorusAngle;
    const float AngleRad = FMath::DegreesToRadians(TorusAngle);

    const bool bSmoothCross = PolygonTorus.bSmoothCrossSection;
    const bool bSmoothVert = PolygonTorus.bSmoothVerticalSection;

    const float StartAngle = -AngleRad / 2.0f;
    const float MajorAngleStep = AngleRad / FMath::Max(1, MajorSegs);
    const float MinorAngleStep = 2.0f * PI / FMath::Max(1, MinorSegs);

    const bool bIsFullCircle = FMath::IsNearlyEqual(TorusAngle, 360.0f);

    StartCapIndices.Empty(MinorSegs);
    EndCapIndices.Empty(MinorSegs);

    for (int32 MajorIndex = 0; MajorIndex < MajorSegs; ++MajorIndex)
    {
        for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex)
        {
            const float MajorAngles[2] = {
                StartAngle + MajorIndex * MajorAngleStep,
                StartAngle + (MajorIndex + 1) * MajorAngleStep
            };
            const float MinorAngles[2] = {
                (MinorIndex * MinorAngleStep) - (PI / 2.0f) - (MinorAngleStep / 2.0f),
                ((MinorIndex + 1) * MinorAngleStep) - (PI / 2.0f) - (MinorAngleStep / 2.0f)
            };

            FVector QuadPositions[4];
            FVector QuadNormals[4];
            FVector2D QuadUVs[4];

            const float MajorAngle_FaceCenter = bSmoothVert ? 0.0f : (MajorAngles[0] + MajorAngles[1]) * 0.5f;
            const float MinorAngle_FaceCenter = bSmoothCross ? 0.0f : (MinorAngles[0] + MinorAngles[1]) * 0.5f;

            for (int i = 0; i < 2; ++i) // Major
            {
                for (int j = 0; j < 2; ++j) // Minor
                {
                    const int CornerIndex = i * 2 + j;

                    const float CurrentMajorAngle = MajorAngles[i];
                    const float CurrentMinorAngle = MinorAngles[j];

                    const float NormalMajorAngle = bSmoothVert ? CurrentMajorAngle : MajorAngle_FaceCenter;
                    const float NormalMinorAngle = bSmoothCross ? CurrentMinorAngle : MinorAngle_FaceCenter;

                    // 计算位置
                    const float MajorCos_Pos = FMath::Cos(CurrentMajorAngle);
                    const float MajorSin_Pos = FMath::Sin(CurrentMajorAngle);
                    const float MinorCos_Pos = FMath::Cos(CurrentMinorAngle);
                    const float MinorSin_Pos = FMath::Sin(CurrentMinorAngle);

                    const float RadialOffset = MinorCos_Pos * MinorRad;
                    const float ZOffset = MinorSin_Pos * MinorRad;

                    QuadPositions[CornerIndex] = FVector(
                        (MajorRad + RadialOffset) * MajorCos_Pos,
                        (MajorRad + RadialOffset) * MajorSin_Pos,
                        ZOffset
                    );

                    // 计算法线
                    const float MajorCos_Norm = FMath::Cos(NormalMajorAngle);
                    const float MajorSin_Norm = FMath::Sin(NormalMajorAngle);
                    const float MinorCos_Norm = FMath::Cos(NormalMinorAngle);
                    const float MinorSin_Norm = FMath::Sin(NormalMinorAngle);

                    QuadNormals[CornerIndex] = FVector(
                        MinorCos_Norm * MajorCos_Norm,
                        MinorCos_Norm * MajorSin_Norm,
                        MinorSin_Norm
                    ).GetSafeNormal();

                    // 计算UV (根据周长比例动态分布，避免面拉伸)
                    // 计算大圆环和小圆环的周长比例
                    const float MajorCircumference = 2.0f * PI * MajorRad;
                    const float MinorCircumference = 2.0f * PI * MinorRad;
                    const float CircumferenceRatio = MajorCircumference / MinorCircumference;
                    
                    // U坐标：沿主环方向均匀分布
                    // V坐标：沿次环方向均匀分布，根据周长比例调整
                    const float UCoord = static_cast<float>(MajorIndex + i) / MajorSegs;
                    const float VCoord = (static_cast<float>(MinorIndex + j) / MinorSegs) / CircumferenceRatio;
                    QuadUVs[CornerIndex] = FVector2D(UCoord, VCoord);
                }
            }

            // 获取或创建四边形的四个顶点
            const int32 V0 = GetOrAddVertex(QuadPositions[0], QuadNormals[0], QuadUVs[0]);
            const int32 V1 = GetOrAddVertex(QuadPositions[1], QuadNormals[1], QuadUVs[1]);
            const int32 V2 = GetOrAddVertex(QuadPositions[2], QuadNormals[2], QuadUVs[2]);
            const int32 V3 = GetOrAddVertex(QuadPositions[3], QuadNormals[3], QuadUVs[3]);

            if (MajorIndex == 0)
            {
                StartCapIndices.Add(V0);
            }
            if (MajorIndex == MajorSegs - 1)
            {
                EndCapIndices.Add(V2);
            }

            AddTriangle(V0, V1, V2);
            AddTriangle(V1, V3, V2);

        }
    }
}
