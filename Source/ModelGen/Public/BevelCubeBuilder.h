// Copyright (c) 2024. All rights reserved.

/**
 * @file BevelCubeBuilder.h
 * @brief 圆角立方体网格构建器
 * * 该类负责生成圆角立方体的几何数据，继承自FModelGenMeshBuilder。
 * * 经过重构，该构建器将每个带倒角的面作为一个整体生成，以创建连续的、可展开的UV坐标。
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

// Forward declarations
class ABevelCube;

/**
 * @struct FUnfoldedFace
 * @brief 描述一个可展开的、带倒角的面，包含其空间朝向和UV布局信息。
 */
struct FUnfoldedFace
{
    // 空间定义
    FVector Normal;      // 面的法线方向 (e.g., +X, -X, +Y...)
    FVector U_Axis;      // 面空间中的U轴（对应纹理的横向）
    FVector V_Axis;      // 面空间中的V轴（对应纹理的纵向）

    // UV布局定义
    FIntPoint UV_Grid_Offset; // 该面在展开图网格中的坐标 (e.g., {1, 1} 代表十字架的中心)

    FString Name;
};

class MODELGEN_API FBevelCubeBuilder : public FModelGenMeshBuilder
{
public:
    explicit FBevelCubeBuilder(const ABevelCube& InBevelCube);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

private:
    const ABevelCube& BevelCube;
    
    // 是否启用倒角：BevelSegments > 0 且 BevelRadius > 0 且 HalfSize > InnerOffset
    bool bEnableBevel;

    // 几何参数
    float HalfSize;
    float InnerOffset;
    float BevelRadius;
    int32 BevelSegments;

    /**
     * @brief 为单个展开的面生成完整的网格（主平面+边缘+角落）。
     * @param FaceDef 描述面的空间朝向和UV布局的结构体。
     */
    void GenerateUnfoldedFace(const FUnfoldedFace& FaceDef);
};