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

void FPolygonTorusBuilder::GenerateVertices() {
    // 直接计算圆环参数
    const float MajorRad = PolygonTorus.MajorRadius;
    const float MinorRad = PolygonTorus.MinorRadius;
    const int32 MajorSegs = PolygonTorus.MajorSegments;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const float AngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);
    
    const float StartAngle = -AngleRad / 2.0f;
    const float MajorAngleStep = AngleRad / MajorSegs;
    const float MinorAngleStep = 2.0f * PI / MinorSegs;
    
    // 生成主环和次环顶点
    // 主环：沿圆环中心线分布
    // 次环：每个主环位置上的圆形截面
    for (int32 MajorIndex = 0; MajorIndex <= MajorSegs; ++MajorIndex) {
        const float MajorAngle = StartAngle + MajorIndex * MajorAngleStep;
        const float MajorCos = FMath::Cos(MajorAngle);
        const float MajorSin = FMath::Sin(MajorAngle);
        
        // 为每个主环位置生成次环顶点
        for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex) {
            // 计算次环角度，确保第一个面平行于水平面且在模型最下面
            // 调整角度使第一个点和第二个点有相同的Z值（水平面）
            // 使用数学公式计算正确的角度偏移，使水平面在最下面
            const float AngleOffset = -(PI / 2.0f) - (MinorAngleStep / 2.0f);
            const float MinorAngle = MinorIndex * MinorAngleStep + AngleOffset;
            const float MinorCos = FMath::Cos(MinorAngle);
            const float MinorSin = FMath::Sin(MinorAngle);
            
            // 计算次环在局部坐标系中的位置
            // MinorCos对应径向方向，MinorSin对应Z轴方向
            const float RadialOffset = MinorCos * MinorRad;
            const float ZOffset = MinorSin * MinorRad;
            
            // 转换到世界坐标系
            const FVector VertexPosition(
                (MajorRad + RadialOffset) * MajorCos,
                (MajorRad + RadialOffset) * MajorSin,
                ZOffset
            );
            
            // 计算顶点法线
            const FVector VertexNormal = FVector(MinorCos * MajorCos, MinorCos * MajorSin, MinorSin).GetSafeNormal();
            
            GetOrAddVertexWithDualUV(VertexPosition, VertexNormal);
        }
    }
}

void FPolygonTorusBuilder::GenerateTriangles() {
    const int32 MajorSegs = PolygonTorus.MajorSegments;
    const int32 MinorSegs = PolygonTorus.MinorSegments;
    const bool bIsFullCircle = FMath::IsNearlyEqual(PolygonTorus.TorusAngle, 360.0f);

    for (int32 MajorIndex = 0; MajorIndex < MajorSegs; ++MajorIndex) {
        for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex) {
            const int32 NextMajor = bIsFullCircle ? (MajorIndex + 1) % MajorSegs : MajorIndex + 1;
            const int32 NextMinor = (MinorIndex + 1) % MinorSegs;

            const int32 V0 = MajorIndex * MinorSegs + MinorIndex;
            const int32 V1 = MajorIndex * MinorSegs + NextMinor;
            const int32 V2 = NextMajor * MinorSegs + NextMinor;
            const int32 V3 = NextMajor * MinorSegs + MinorIndex;

            AddTriangle(V0, V1, V2);
            AddTriangle(V0, V2, V3);
        }
    }
}

void FPolygonTorusBuilder::GenerateEndCaps() {
  GenerateAdvancedEndCaps();
}

void FPolygonTorusBuilder::GenerateAdvancedEndCaps() {
  if (FMath::IsNearlyEqual(PolygonTorus.TorusAngle, 360.0f)) {
    return;
  }

  const float MajorRad = PolygonTorus.MajorRadius;
  const float MinorRad = PolygonTorus.MinorRadius;
  const int32 MinorSegs = PolygonTorus.MinorSegments;
  const float AngleRad = FMath::DegreesToRadians(PolygonTorus.TorusAngle);

  const float StartAngle = -AngleRad / 2.0f;
  const float EndAngle = AngleRad / 2.0f;

  const FVector StartCenter(FMath::Cos(StartAngle) * MajorRad,
                            FMath::Sin(StartAngle) * MajorRad, 0.0f);

  const FVector EndCenter(FMath::Cos(EndAngle) * MajorRad,
                          FMath::Sin(EndAngle) * MajorRad, 0.0f);

  const FVector StartNormal = FVector(-FMath::Sin(StartAngle), FMath::Cos(StartAngle), 0.0f);
  const FVector EndNormal = FVector(-FMath::Sin(EndAngle), FMath::Cos(EndAngle), 0.0f);

  const int32 StartCenterIndex = GetOrAddVertexWithDualUV(StartCenter, StartNormal);
  const int32 EndCenterIndex = GetOrAddVertexWithDualUV(EndCenter, EndNormal);

  for (int32 i = 0; i < MinorSegs; ++i) {
    const int32 NextI = (i + 1) % MinorSegs;

    const int32 V0 = StartCenterIndex;
    const int32 V1 = NextI;
    const int32 V2 = i;
    AddTriangle(V0, V1, V2);
  }

  const int32 LastSectionStart = PolygonTorus.MajorSegments * MinorSegs;
  for (int32 i = 0; i < MinorSegs; ++i) {
    const int32 NextI = (i + 1) % MinorSegs;

    const int32 V0 = EndCenterIndex;
    const int32 V1 = LastSectionStart + i;
    const int32 V2 = LastSectionStart + NextI;
    AddTriangle(V0, V1, V2);
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

void FPolygonTorusBuilder::ApplySmoothing() {
  if (PolygonTorus.bSmoothCrossSection) {
    ApplyHorizontalSmoothing();
  }

  if (PolygonTorus.bSmoothVerticalSection) {
    ApplyVerticalSmoothing();
  }
}

void FPolygonTorusBuilder::ApplyHorizontalSmoothing() {
  const int32 MajorSegs = PolygonTorus.MajorSegments;
  const int32 MinorSegs = PolygonTorus.MinorSegments;
  const int32 TotalVertices = MeshData.GetVertexCount();

  for (int32 MajorIndex = 0; MajorIndex <= MajorSegs; ++MajorIndex) {
    const int32 SectionStartIndex = MajorIndex * MinorSegs;

    for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex) {
      const int32 CurrentVertexIndex = SectionStartIndex + MinorIndex;

      if (CurrentVertexIndex >= TotalVertices)
        continue;

      FVector CurrentPos = MeshData.Vertices[CurrentVertexIndex];
      FVector CurrentNormal = MeshData.Normals[CurrentVertexIndex];

      FVector LeftPos, RightPos;
      int32 LeftIndex = SectionStartIndex + ((MinorIndex - 1 + MinorSegs) % MinorSegs);
      int32 RightIndex = SectionStartIndex + ((MinorIndex + 1) % MinorSegs);

      if (LeftIndex < TotalVertices && RightIndex < TotalVertices) {
        LeftPos = MeshData.Vertices[LeftIndex];
        RightPos = MeshData.Vertices[RightIndex];

        FVector RadialVector = (RightPos - LeftPos).GetSafeNormal();
        FVector SmoothNormal = FVector::CrossProduct(RadialVector, FVector::UpVector).GetSafeNormal();

        if (FVector::DotProduct(SmoothNormal, CurrentNormal) < 0.0f) {
          SmoothNormal = -SmoothNormal;
        }

        const float SmoothingStrength = 0.3f;
        FVector FinalNormal = FMath::Lerp(CurrentNormal, SmoothNormal, SmoothingStrength).GetSafeNormal();

        MeshData.Normals[CurrentVertexIndex] = FinalNormal;
      }
    }
  }
}

void FPolygonTorusBuilder::ApplyVerticalSmoothing() {
  const int32 MajorSegs = PolygonTorus.MajorSegments;
  const int32 MinorSegs = PolygonTorus.MinorSegments;
  const int32 TotalVertices = MeshData.GetVertexCount();

  for (int32 MinorIndex = 0; MinorIndex < MinorSegs; ++MinorIndex) {
    for (int32 MajorIndex = 0; MajorIndex <= MajorSegs; ++MajorIndex) {
      const int32 CurrentVertexIndex = MajorIndex * MinorSegs + MinorIndex;

      if (CurrentVertexIndex >= TotalVertices)
        continue;

      FVector CurrentPos = MeshData.Vertices[CurrentVertexIndex];
      FVector CurrentNormal = MeshData.Normals[CurrentVertexIndex];

      FVector PrevPos, NextPos;
      int32 PrevIndex = ((MajorIndex - 1 + (MajorSegs + 1)) % (MajorSegs + 1)) * MinorSegs + MinorIndex;
      int32 NextIndex = (MajorIndex + 1) % (MajorSegs + 1) * MinorSegs + MinorIndex;

      if (PrevIndex < TotalVertices && NextIndex < TotalVertices) {
        PrevPos = MeshData.Vertices[PrevIndex];
        NextPos = MeshData.Vertices[NextIndex];

        FVector AxialVector = (NextPos - PrevPos).GetSafeNormal();
        FVector SmoothNormal = FVector::CrossProduct(AxialVector, FVector::RightVector).GetSafeNormal();

        if (FVector::DotProduct(SmoothNormal, CurrentNormal) < 0.0f) {
          SmoothNormal = -SmoothNormal;
        }

        const float SmoothingStrength = 0.3f;
        FVector FinalNormal = FMath::Lerp(CurrentNormal, SmoothNormal, SmoothingStrength).GetSafeNormal();

        MeshData.Normals[CurrentVertexIndex] = FinalNormal;
      }
    }
  }
}

FVector2D FPolygonTorusBuilder::GenerateStableUVCustom(const FVector& Position, const FVector& Normal) const {
  float X = Position.X;
  float Y = Position.Y;
  float Z = Position.Z;

  float MajorAngle = FMath::Atan2(Y, X);
  if (MajorAngle < 0)
    MajorAngle += 2.0f * PI;

  float DistanceFromCenter = FMath::Sqrt(X * X + Y * Y);
  float MinorRadius = DistanceFromCenter - PolygonTorus.MajorRadius;

  float MinorAngle = FMath::Asin(Z / PolygonTorus.MinorRadius);
  if (MinorRadius < 0)
    MinorAngle = PI - MinorAngle;

  float U = MajorAngle / (2.0f * PI);
  float V = (MinorAngle + PI) / (2.0f * PI);

  return FVector2D(U, V);
}

FVector2D FPolygonTorusBuilder::GenerateSecondaryUV(const FVector& Position, const FVector& Normal) const {
  float X = Position.X;
  float Y = Position.Y;
  float Z = Position.Z;

  float MajorAngle = FMath::Atan2(Y, X);
  if (MajorAngle < 0)
    MajorAngle += 2.0f * PI;

  float DistanceFromCenter = FMath::Sqrt(X * X + Y * Y);
  float MinorRadius = DistanceFromCenter - PolygonTorus.MajorRadius;

  float MinorAngle = FMath::Asin(Z / PolygonTorus.MinorRadius);
  if (MinorRadius < 0)
    MinorAngle = PI - MinorAngle;

  float U = MajorAngle / (2.0f * PI);
  float V = 0.2f + (MinorAngle + PI) / (2.0f * PI) * 0.6f;

  return FVector2D(U, V);
}

int32 FPolygonTorusBuilder::GetOrAddVertexWithDualUV(const FVector& Pos, const FVector& Normal) {
  FVector2D MainUV = GenerateStableUVCustom(Pos, Normal);
  FVector2D SecondaryUV = GenerateSecondaryUV(Pos, Normal);

  return FModelGenMeshBuilder::GetOrAddVertexWithDualUV(Pos, Normal, MainUV, SecondaryUV);
}

