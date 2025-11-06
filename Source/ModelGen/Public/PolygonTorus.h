// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "PolygonTorus.generated.h"

UCLASS(BlueprintType, meta=(DisplayName = "Polygon Torus"))
class MODELGEN_API APolygonTorus : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    APolygonTorus();

    //~ Begin Geometry Parameters
    /** 主半径（圆环中心到截面中心的距离） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = "1.0", ClampMax = "1000", UIMin = "1.0", UIMax = "1000", 
        DisplayName = "Major Radius", ToolTip = "Distance from torus center to section center"))
    float MajorRadius = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Parameters")
    void SetMajorRadius(float NewMajorRadius);

    /** 次半径（截面半径） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = "1.0", ClampMax = "1000", UIMin = "1.0", UIMax = "1000", 
        DisplayName = "Minor Radius", ToolTip = "Section radius"))
    float MinorRadius = 25.0f;

    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Parameters")
    void SetMinorRadius(float NewMinorRadius);

    /** 主分段数（圆环分段数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = 3, ClampMax = 25, UIMin = 3, UIMax = 25, 
        DisplayName = "Major Segments", ToolTip = "Number of torus segments"))
    int32 MajorSegments = 8;

    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Parameters")
    void SetMajorSegments(int32 NewMajorSegments);

    /** 次分段数（截面分段数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = 3, ClampMax = 25, UIMin = 3, UIMax = 25, 
        DisplayName = "Minor Segments", ToolTip = "Number of section segments"))
    int32 MinorSegments = 8;

    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Parameters")
    void SetMinorSegments(int32 NewMinorSegments);

    /** 圆环角度（度数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = "0.0", ClampMax = "360.0", UIMin = "0.0", UIMax = "360.0", 
        DisplayName = "Torus Angle", ToolTip = "Torus angle in degrees"))
    float TorusAngle = 360.0f;

    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Parameters")
    void SetTorusAngle(float NewTorusAngle);

    /** 横切面光滑 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Cross Section", ToolTip = "Smooth cross sections"))
    bool bSmoothCrossSection = false;

    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Parameters")
    void SetSmoothCrossSection(bool bNewSmoothCrossSection);

    /** 竖面光滑 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Vertical Section", ToolTip = "Smooth vertical sections"))
    bool bSmoothVerticalSection = false;

    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Parameters")
    void SetSmoothVerticalSection(bool bNewSmoothVerticalSection);

    virtual void GenerateMesh() override;

private:
    bool TryGenerateMeshInternal();

public:
    virtual bool IsValid() const override;

    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
};
