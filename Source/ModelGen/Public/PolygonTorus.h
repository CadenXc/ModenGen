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
        meta = (ClampMin = "1.0", UIMin = "1.0", 
        DisplayName = "Major Radius", ToolTip = "Distance from torus center to section center"))
    float MajorRadius = 100.0f;

    /** 次半径（截面半径） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = "1.0", UIMin = "1.0", 
        DisplayName = "Minor Radius", ToolTip = "Section radius"))
    float MinorRadius = 25.0f;

    /** 主分段数（圆环分段数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = 3, UIMin = 3, ClampMax = 256, UIMax = 100, 
        DisplayName = "Major Segments", ToolTip = "Number of torus segments"))
    int32 MajorSegments = 8;

    /** 次分段数（截面分段数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = 3, UIMin = 3, ClampMax = 256, UIMax = 100, 
        DisplayName = "Minor Segments", ToolTip = "Number of section segments"))
    int32 MinorSegments = 4;

    /** 圆环角度（度数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Geometry", 
        meta = (ClampMin = "1.0", UIMin = "1.0", ClampMax = "360.0", UIMax = "360.0", 
        DisplayName = "Torus Angle", ToolTip = "Torus angle in degrees"))
    float TorusAngle = 360.0f;

    /** 横切面光滑 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Cross Section", ToolTip = "Smooth cross sections"))
    bool bSmoothCrossSection = true;

    /** 竖面光滑 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Vertical Section", ToolTip = "Smooth vertical sections"))
    bool bSmoothVerticalSection = true;





    void SetMaterial(UMaterialInterface* NewMaterial);



protected:
    // 实现父类的纯虚函数
    virtual void GenerateMesh() override;

public:
    // 实现父类的纯虚函数
    virtual bool IsValid() const override;

public:
    // 参数验证和计算函数
    /** 计算预估的顶点数量 */
    int32 CalculateVertexCountEstimate() const;
    
    /** 计算预估的三角形数量 */
    int32 CalculateTriangleCountEstimate() const;
};
