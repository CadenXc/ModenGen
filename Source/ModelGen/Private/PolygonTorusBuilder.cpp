// Copyright (c) 2024. All rights reserved.

#include "PolygonTorusBuilder.h"
#include "PolygonTorus.h"
#include "ModelGenMeshData.h"
#include "CoreMinimal.h"

FPolygonTorusBuilder::FPolygonTorusBuilder(const APolygonTorus& InPolygonTorus)
    : PolygonTorus(InPolygonTorus) {
  Clear();
  ValidateAndClampParameters();
}

bool FPolygonTorusBuilder::Generate(FModelGenMeshData& OutMeshData) {
  if (!PolygonTorus.IsValid()) {
    return false;
  }

  Clear();
  ReserveMemory();

  GenerateVertices();
  GenerateTriangles();

  if (PolygonTorus.TorusAngle < 360.0f - KINDA_SMALL_NUMBER) {
    GenerateEndCaps();
  }

  ApplySmoothing();

  if (!ValidateGeneratedData()) {
    return false;
  }

  OutMeshData = MeshData;
  return true;
}

int32 FPolygonTorusBuilder::CalculateVertexCountEstimate() const {
  return PolygonTorus.CalculateVertexCountEstimate();
}

int32 FPolygonTorusBuilder::CalculateTriangleCountEstimate() const {
  return PolygonTorus.CalculateTriangleCountEstimate();
}

void FPolygonTorusBuilder::GenerateEndCaps() {
  GenerateAdvancedEndCaps();
}

// PolygonTorusBuilder.cpp

void FPolygonTorusBuilder::GenerateAdvancedEndCaps()
{
    if (FMath::IsNearlyEqual(PolygonTorus.TorusAngle, 360.0f) || StartCapIndices.Num() == 0 || EndCapIndices.Num() == 0)
    {
        return;
    }

    const float MajorRad = PolygonTorus.MajorRadius;
    const float AngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);

    // --- 起始封盖 (Start Cap) ---
    const float StartAngle = -AngleRad / 2.0f;
    const FVector StartCenter = FVector(FMath::Cos(StartAngle) * MajorRad, FMath::Sin(StartAngle) * MajorRad, 0.0f);
    // 修正法线：法线应指向圆弧的反方向，即切线的反方向
    const FVector StartNormal = FVector(FMath::Sin(StartAngle), -FMath::Cos(StartAngle), 0.0f);
    const int32 StartCenterIndex = GetOrAddVertex(StartCenter, StartNormal);

    for (int32 i = 0; i < StartCapIndices.Num(); ++i)
    {
        const int32 CurrentIndex = StartCapIndices[i];
        // 确保循环连接到第一个点
        const int32 NextIndex = StartCapIndices[(i + 1) % StartCapIndices.Num()];
        // 使用 (Center, Next, Current) 的顺序保证法线朝外
        AddTriangle(StartCenterIndex, NextIndex, CurrentIndex);
    }

    // --- 终止封盖 (End Cap) ---
    const float EndAngle = AngleRad / 2.0f;
    const FVector EndCenter = FVector(FMath::Cos(EndAngle) * MajorRad, FMath::Sin(EndAngle) * MajorRad, 0.0f);
    // 终止法线：法线应指向圆弧的前进方向，即切线方向
    const FVector EndNormal = FVector(-FMath::Sin(EndAngle), FMath::Cos(EndAngle), 0.0f);
    const int32 EndCenterIndex = GetOrAddVertex(EndCenter, EndNormal);

    for (int32 i = 0; i < EndCapIndices.Num(); ++i)
    {
        const int32 CurrentIndex = EndCapIndices[i];
        // 确保循环连接到第一个点
        const int32 NextIndex = EndCapIndices[(i + 1) % EndCapIndices.Num()];
        // 使用 (Center, Current, Next) 的顺序保证法线朝外
        AddTriangle(EndCenterIndex, CurrentIndex, NextIndex);
    }
}

void FPolygonTorusBuilder::ValidateAndClampParameters() {
  if (PolygonTorus.MajorSegments < 3 || PolygonTorus.MajorSegments > 256) {
    UE_LOG(LogTemp, Warning, TEXT("主分段数 %d 超出有效范围 [3, 256]，建议调整"), PolygonTorus.MajorSegments);
  }

  if (PolygonTorus.MinorSegments < 3 || PolygonTorus.MinorSegments > 256) {
    UE_LOG(LogTemp, Warning, TEXT("次分段数 %d 超出有效范围 [3, 256]，建议调整"), PolygonTorus.MinorSegments);
  }

  if (PolygonTorus.MajorRadius < 1.0f) {
    UE_LOG(LogTemp, Warning, TEXT("主半径 %f 小于最小值 1.0，建议调整"), PolygonTorus.MajorRadius);
  }

  if (PolygonTorus.MinorRadius < 1.0f) {
    UE_LOG(LogTemp, Warning, TEXT("次半径 %f 小于最小值 1.0，建议调整"), PolygonTorus.MinorRadius);
  }

  if (PolygonTorus.MinorRadius > PolygonTorus.MajorRadius * 0.9f) {
    UE_LOG(LogTemp, Warning, TEXT("次半径 %f 超过主半径的 90%% (%f)，建议调整"), 
           PolygonTorus.MinorRadius, PolygonTorus.MajorRadius * 0.9f);
  }

  if (PolygonTorus.TorusAngle < 1.0f || PolygonTorus.TorusAngle > 360.0f) {
    UE_LOG(LogTemp, Warning, TEXT("圆环角度 %f 超出有效范围 [1.0, 360.0]，建议调整"), PolygonTorus.TorusAngle);
  }
}

void FPolygonTorusBuilder::LogMeshStatistics() {
  UE_LOG(LogTemp, Log, TEXT("FPolygonTorusBuilder::LogMeshStatistics - Mesh statistics: %d vertices, %d triangles"), 
         MeshData.GetVertexCount(), MeshData.GetTriangleCount());
}

// UV生成已移除 - 让UE4自动处理UV生成

int32 FPolygonTorusBuilder::GetOrAddVertex(const FVector& Pos, const FVector& Normal) {
  // 不计算UV，让UE4自动生成
  return FModelGenMeshBuilder::GetOrAddVertex(Pos, Normal);
}

void FPolygonTorusBuilder::GenerateVertices()
{
    // 该函数将与GenerateTriangles协同工作，因此大部分逻辑移至主循环中。
    // 在这里我们只确保内存被预留。
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

    // 按四边形面片遍历并生成顶点和三角形
    for (int32 MajorIndex = 0; MajorIndex < MajorSegs; ++MajorIndex)
    {
        for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex)
        {
            // --- 计算四个角的位置 ---
            
            // 定义四边形的四个角的参数角度
            const float MajorAngles[2] = {
                StartAngle + MajorIndex * MajorAngleStep,
                StartAngle + (MajorIndex + 1) * MajorAngleStep
            };
            const float MinorAngles[2] = {
                (MinorIndex * MinorAngleStep) - (PI / 2.0f) - (MinorAngleStep / 2.0f),
                ((MinorIndex + 1) * MinorAngleStep) - (PI / 2.0f) - (MinorAngleStep / 2.0f)
            };

            // 存储四边形的四个顶点位置和法线
            FVector QuadPositions[4];
            FVector QuadNormals[4];
            
            // --- 计算每个角的法线 ---
            
            // 为了生成平坦的法线，计算面片中心的角度
            const float MajorAngle_FaceCenter = bSmoothVert ? 0.0f : (MajorAngles[0] + MajorAngles[1]) * 0.5f;
            const float MinorAngle_FaceCenter = bSmoothCross ? 0.0f : (MinorAngles[0] + MinorAngles[1]) * 0.5f;

            for (int i = 0; i < 2; ++i) // 对应 MajorIndex, MajorIndex + 1
            {
                for (int j = 0; j < 2; ++j) // 对应 MinorIndex, MinorIndex + 1
                {
                    const int CornerIndex = i * 2 + j;
                    
                    // 确定用于计算位置和法线的角度
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
                }
            }

            // --- 获取顶点索引并创建三角形 ---
            
            // 获取或创建四边形的四个顶点。如果法线不同，将会创建新顶点
            const int32 V0 = GetOrAddVertex(QuadPositions[0], QuadNormals[0]); // Major, Minor
            const int32 V1 = GetOrAddVertex(QuadPositions[1], QuadNormals[1]); // Major, Minor + 1
            const int32 V2 = GetOrAddVertex(QuadPositions[2], QuadNormals[2]); // Major + 1, Minor
            const int32 V3 = GetOrAddVertex(QuadPositions[3], QuadNormals[3]); // Major + 1, Minor + 1

            // 如果这是第一个主分段，记录其起始边（V0）的顶点索引
            if (MajorIndex == 0)
            {
                StartCapIndices.Add(V0);
            }
            // 如果这是最后一个主分段，记录其终止边（V2）的顶点索引
            if (MajorIndex == MajorSegs - 1)
            {
                EndCapIndices.Add(V2);
            }

            // 创建两个三角形
            AddTriangle(V0, V1, V2);
            AddTriangle(V1, V3, V2);
        }
    }
    
    // 如果是完整的圆环，需要将首尾两端的顶点进行缝合
    // 注意：上述循环已经通过生成独立的顶点来处理平滑，此处不需要手动缝合。
    // 如果bIsFullCircle为真且需要硬编码缝合，则需要不同的逻辑。
    // 当前的逻辑通过为每一面片生成顶点，自然地处理了平滑和锐边，包括在360度接缝处。
}

void FPolygonTorusBuilder::ApplySmoothing()
{
    // 这个函数不再需要，因为平滑效果已在顶点生成时处理。
    // if (PolygonTorus.bSmoothCrossSection) {
    //   ApplyHorizontalSmoothing();
    // }
    //
    // if (PolygonTorus.bSmoothVerticalSection) {
    //   ApplyVerticalSmoothing();
    // }
}

void FPolygonTorusBuilder::ApplyHorizontalSmoothing()
{
    // 保留为空，逻辑已移至 GenerateTriangles
}

void FPolygonTorusBuilder::ApplyVerticalSmoothing()
{
    // 保留为空，逻辑已移至 GenerateTriangles
}
