#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"
#include "EditableSurface.h"

class AEditableSurface;
class USplineComponent;

struct FSurfaceSamplePoint
{
    FVector Location;
    FVector RightVector;
    FVector Normal;
    FVector Tangent;
    float Distance;
    float Alpha;
    float InterpolatedWidth;
};

struct FCornerData
{
    FVector MiterVector;
    float MiterScale;
    bool bIsConvexLeft;
    float LeftSlopeMiterScale;
    float RightSlopeMiterScale;
    float TurnRadius;
    FVector RotationCenter;
    
    FCornerData()
        : MiterVector(FVector::ZeroVector)
        , MiterScale(1.0f)
        , bIsConvexLeft(false)
        , LeftSlopeMiterScale(1.0f)
        , RightSlopeMiterScale(1.0f)
        , TurnRadius(FLT_MAX)
        , RotationCenter(FVector::ZeroVector)
    {
    }
};

struct FRailPoint
{
    FVector Position;
    FVector Normal;
    FVector Tangent;
    FVector2D UV;
    float DistanceAlongRail;

    FRailPoint() 
        : Position(FVector::ZeroVector), Normal(FVector::UpVector), Tangent(FVector::ForwardVector), UV(FVector2D::ZeroVector), DistanceAlongRail(0.f) 
    {}
};

class MODELGEN_API FEditableSurfaceBuilder : public FModelGenMeshBuilder
{
public:
    explicit FEditableSurfaceBuilder(const AEditableSurface& InSurface);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

    void Clear();

private:
    const AEditableSurface& Surface;

    USplineComponent* SplineComponent;
    float SurfaceWidth;
    float SplineSampleStep;
    float LoopRemovalThreshold;
    bool bEnableThickness;
    float ThicknessValue;
    int32 SideSmoothness;
    float RightSlopeLength;
    float RightSlopeGradient;
    float LeftSlopeLength;
    float LeftSlopeGradient;
    ESurfaceTextureMapping TextureMapping;
	float CachedMaxProfileLength;

    TArray<FSurfaceSamplePoint> SampledPath;
    TArray<FCornerData> PathCornerData;

    void SampleSplinePath();
    void CalculateCornerGeometry();

    TArray<FRailPoint> LeftRailRaw;
    TArray<FRailPoint> RightRailRaw;
    TArray<FRailPoint> LeftRailResampled;
    TArray<FRailPoint> RightRailResampled;
    
    TArray<FRailPoint> FinalLeftRail;
    TArray<FRailPoint> FinalRightRail;
    
    TArray<int32> TopStartIndices;
    TArray<int32> TopEndIndices;

    void GenerateRoadSurface();
    void GenerateSlopes(int32 LeftRoadStartIdx, int32 RightRoadStartIdx);
    void GenerateThickness();
    
    FVector CalculateRawPointPosition(const FSurfaceSamplePoint& Sample, const FCornerData& Corner, float HalfWidth, bool bIsRightSide);
    FVector CalculateSurfaceNormal(const FRailPoint& Pt, bool bIsRightSide) const;
    void GenerateSingleSideSlope(const TArray<FRailPoint>& BaseRail, float Length, float Gradient, int32 StartIndex, bool bIsRightSide);
    void BuildSideWall(const TArray<FRailPoint>& Rail, bool bIsRightSide);
    void BuildCap(const TArray<int32>& Indices, bool bIsStartCap, const FVector& OverrideNormal);
    
    void BuildRawRails();
    void StitchRailsInternal(int32 LeftStartIdx, int32 RightStartIdx, int32 LeftCount, int32 RightCount, bool bReverseWinding = false);
    void BuildNextSlopeRail(const TArray<FRailPoint>& ReferenceRail, TArray<FRailPoint>& OutSlopeRail, bool bIsRightSide, float OffsetH, float OffsetV);
    void ResampleSingleRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float SegmentLength);
    void SimplifyRail(const TArray<FRailPoint>& InPoints, TArray<FRailPoint>& OutPoints, float MergeThreshold);
    void RemoveGeometricLoops(TArray<FRailPoint>& InPoints, float Threshold);
    void ApplyMonotonicityConstraint(FVector& Pos, const FVector& LastValidPos, const FVector& Tangent);

    void CorrectRailUVs(TArray<FRailPoint>& Rail, float TargetTotalLength);
};