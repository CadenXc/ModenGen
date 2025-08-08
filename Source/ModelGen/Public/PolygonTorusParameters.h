// Copyright (c) 2024. All rights reserved.

/**
 * @file PolygonTorusParameters.h
 * @brief 圆环生成参数
 * 
 * 该文件定义了圆环生成器的所有参数。
 * 使用USTRUCT以便在蓝图中使用和编辑。
 */

#pragma once

#include "CoreMinimal.h"
#include "PolygonTorusParameters.generated.h"

/**
 * 圆环端面填充类型
 * 定义端盖的填充方式
 */
UENUM(BlueprintType)
enum class ETorusFillType : uint8
{
    /* 不填充 */
    None        UMETA(DisplayName = "None"),
    
    /* 多边形填充 */
    NGon        UMETA(DisplayName = "NGon"),
    
    /* 三角形填充 */
    Triangles   UMETA(DisplayName = "Triangles")
};

/**
 * 圆环UV映射模式
 * 定义纹理坐标的生成方式
 */
UENUM(BlueprintType)
enum class ETorusUVMode : uint8
{
    /* 标准UV映射 */
    Standard    UMETA(DisplayName = "Standard"),
    
    /* 柱面UV映射 */
    Cylindrical UMETA(DisplayName = "Cylindrical"),
    
    /* 球面UV映射 */
    Spherical   UMETA(DisplayName = "Spherical")
};

/**
 * 圆环光滑模式
 * 定义法线光滑的计算方式
 */
UENUM(BlueprintType)
enum class ETorusSmoothMode : uint8
{
    /* 不平滑 */
    None        UMETA(DisplayName = "None"),
    
    /* 仅横截面光滑 */
    Cross       UMETA(DisplayName = "Cross Section Only"),
    
    /* 仅垂直截面光滑 */
    Vertical    UMETA(DisplayName = "Vertical Section Only"),
    
    /* 两个方向都光滑 */
    Both        UMETA(DisplayName = "Both Sections"),
    
    /* 自动检测 */
    Auto        UMETA(DisplayName = "Auto Detect")
};

/**
 * 圆环生成参数
 */
USTRUCT(BlueprintType)
struct MODELGEN_API FPolygonTorusParameters
{
    GENERATED_BODY()

public:
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

    /** 光滑模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Mode", ToolTip = "Normal smoothing calculation method"))
    ETorusSmoothMode SmoothMode = ETorusSmoothMode::Both;

    /** 横切面光滑（向后兼容） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Cross Section", ToolTip = "Smooth cross sections"))
    bool bSmoothCrossSection = true;

    /** 竖面光滑（向后兼容） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Smooth Vertical Section", ToolTip = "Smooth vertical sections"))
    bool bSmoothVerticalSection = true;

    /** 生成光滑组 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Generate Smooth Groups", ToolTip = "Generate smooth groups"))
    bool bGenerateSmoothGroups = true;

    /** 生成硬边标记 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (DisplayName = "Generate Hard Edges", ToolTip = "Generate hard edge markers"))
    bool bGenerateHardEdges = true;

    /** 光滑角度阈值（度数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Smoothing", 
        meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "180.0", UIMax = "180.0", 
        DisplayName = "Smoothing Angle", ToolTip = "Normal smoothing angle threshold"))
    float SmoothingAngle = 30.0f;

    /** 端面填充类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "Fill Type", ToolTip = "End cap fill method"))
    ETorusFillType FillType = ETorusFillType::NGon;

    /** UV映射模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "UV Mode", ToolTip = "Texture coordinate generation method"))
    ETorusUVMode UVMode = ETorusUVMode::Standard;

    /** 是否生成UV坐标 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "Generate UVs", ToolTip = "Generate UV coordinates"))
    bool bGenerateUVs = true;

    /** 是否生成法线 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "Generate Normals", ToolTip = "Generate normal vectors"))
    bool bGenerateNormals = true;

    /** 是否生成切线 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Options", 
        meta = (DisplayName = "Generate Tangents", ToolTip = "Generate tangent vectors"))
    bool bGenerateTangents = true;

    /** 是否生成起始端盖 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|End Caps", 
        meta = (DisplayName = "Generate Start Cap", ToolTip = "Generate start end cap"))
    bool bGenerateStartCap = true;

    /** 是否生成结束端盖 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|End Caps", 
        meta = (DisplayName = "Generate End Cap", ToolTip = "Generate end end cap"))
    bool bGenerateEndCap = true;



    /** 是否使用圆形端盖 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|End Caps", 
        meta = (DisplayName = "Use Circular Caps", ToolTip = "Use circular end caps"))
    bool bUseCircularCaps = false;

public:
    /** 验证参数的有效性 */
    bool IsValid() const;
    
    /** 计算预估的顶点数量 */
    int32 CalculateVertexCountEstimate() const;
    
    /** 计算预估的三角形数量 */
    int32 CalculateTriangleCountEstimate() const;
};
