// Copyright (c) 2024. All rights reserved.
/**
 * @file HollowPrismBuilder.h
 * @brief 空心棱柱网格构建器
 */
#pragma once
#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
// Forward declarations

class AHollowPrism;
class MODELGEN_API FHollowPrismBuilder : public FModelGenMeshBuilder

{
 public:
  //~ Begin Constructor

  explicit FHollowPrismBuilder(const AHollowPrism& InHollowPrism);
  //~ Begin FModelGenMeshBuilder Interface

  virtual bool Generate(FModelGenMeshData& OutMeshData) override;

  virtual int32 CalculateVertexCountEstimate() const override;

  virtual int32 CalculateTriangleCountEstimate() const override;

 private:
  const AHollowPrism& HollowPrism;
  // 墙体和顶/底盖

  void GenerateInnerWalls();

  void GenerateOuterWalls();

  void GenerateTopCapWithTriangles();

  void GenerateBottomCapWithTriangles();

  // 倒角

  void GenerateTopBevelGeometry();

  void GenerateBottomBevelGeometry();

  // 端盖 (非完整圆环时)

  void GenerateEndCaps();

  void GenerateEndCap(float Angle, bool IsStart);

  // 顶点和三角形生成辅助函数

  void GenerateCapTriangles(const TArray<int32>& InnerVertices,

                            const TArray<int32>& OuterVertices,

                            bool bIsTopCap);

  void GenerateCapVertices(TArray<int32>& OutInnerVertices,

                           TArray<int32>& OutOuterVertices,

                           bool bIsTopCap);

  void GenerateBevelGeometry(bool bIsTop, bool bIsInner);

  void GenerateBevelRing(TArray<int32>& OutCurrentRing,

                         bool bIsTop,
                         bool bIsInner,

                         int32 RingIndex,
                         int32 TotalRings);

  void ConnectBevelRings(const TArray<int32>& PrevRing,

                         const TArray<int32>& CurrentRing,

                         bool bIsInner,
                         bool bIsTop);
  // 新的、重构后的端盖生成函数

  void GenerateEndCapColumn(float Angle,
                            const FVector& Normal,
                            TArray<int32>& OutOrderedVertices);

  void GenerateEndCapTriangles(const TArray<int32>& OrderedVertices,
                               bool IsStart);
  // 计算函数

  float CalculateStartAngle() const;

  float CalculateAngleStep(int32 Sides) const;

  FVector CalculateVertexPosition(float Radius, float Angle, float Z) const;

  FVector CalculateBevelNormal(float Angle,
                               float Alpha,
                               bool bIsInner,
                               bool bIsTop) const;
  // UV 生成

  // UV生成已移除 - 让UE4自动处理UV生成

  int32 GetOrAddVertex(const FVector& Pos, const FVector& Normal);
};