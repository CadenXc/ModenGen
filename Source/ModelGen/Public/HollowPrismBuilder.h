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
  
  // 端盖顶点索引记录 - 拆分为4个独立数组
  TArray<int32> StartOuterCapIndices;  // 起始端盖外壁顶点
  TArray<int32> StartInnerCapIndices;  // 起始端盖内壁顶点
  TArray<int32> EndOuterCapIndices;    // 结束端盖外壁顶点
  TArray<int32> EndInnerCapIndices;    // 结束端盖内壁顶点
  
  // 倒角连接顶点记录
  TArray<int32> TopInnerBevelVertices;   // 顶部内环倒角连接顶点
  TArray<int32> TopOuterBevelVertices;   // 顶部外环倒角连接顶点
  TArray<int32> BottomInnerBevelVertices; // 底部内环倒角连接顶点
  TArray<int32> BottomOuterBevelVertices; // 底部外环倒角连接顶点
  
  //~ Begin Wall Generation
  void CreateInnerWalls();
  void CreateOuterWalls();
  void CreateWalls(float Radius, int32 Sides, bool bIsInner);

  //~ Begin Cap Generation
  void CreateTopCap();
  void CreateBottomCap();
  void CreateCapVertices(TArray<int32>& OutInnerVertices, 
                        TArray<int32>& OutOuterVertices, 
                        bool bIsTopCap);
  void CreateCapTriangles(const TArray<int32>& InnerVertices,
                         const TArray<int32>& OuterVertices,
                         bool bIsTopCap);

  //~ Begin Bevel Generation
  void CreateTopBevel();
  void CreateBottomBevel();
  void CreateBevelGeometry(bool bIsTop, bool bIsInner);
  void CreateBevelRing(TArray<int32>& OutCurrentRing,
                      bool bIsTop,
                      bool bIsInner,
                      int32 RingIndex,
                      int32 TotalRings);
  void ConnectBevelRings(const TArray<int32>& PrevRing,
                        const TArray<int32>& CurrentRing,
                        bool bIsInner,
                        bool bIsTop);

  //~ Begin End Cap Generation (for partial circles)
  void CreateEndCaps();
  void CreateAdvancedEndCaps();
  void CreateEndCap(float Angle, bool bIsStart);
  void CreateEndCapColumn(float Angle,
                         const FVector& Normal,
                         TArray<int32>& OutOrderedVertices);
  void CreateEndCapTriangles(const TArray<int32>& OrderedVertices,
                            bool bIsStart);
  void CreateEndCapFromVertices(const TArray<int32>& RecordedVertices,
                               bool bIsStart);
  void CreateEndCapFromSeparateArrays(const TArray<int32>& OuterVertices,
                                     const TArray<int32>& InnerVertices,
                                     bool bIsStart);
  void CreateEndCapWithBevel(bool bIsStart);
  void CreateEndCapFromSimpleVertices(const TArray<int32>& RecordedVertices,
                                     bool bIsStart);
  void CreateEndCapWithBevelVertices(const TArray<int32>& RecordedVertices,
                                    bool bIsStart);
  //~ Begin Utility Functions
  float CalculateStartAngle() const;
  float CalculateAngleStep(int32 Sides) const;
  FVector CalculateVertexPosition(float Radius, float Angle, float Z) const;
  FVector CalculateBevelNormal(float Angle, float Alpha, bool bIsInner, bool bIsTop) const;
  
  //~ Begin Vertex Management
  int32 GetOrAddVertex(const FVector& Pos, const FVector& Normal);
};