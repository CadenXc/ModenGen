// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"

// 前向声明
class UProceduralMeshComponent;
class UBodySetup;

/**
 * V-HACD凸包分解工具类（运行时可用的版本）
 * 基于UE内置的VHACD库实现
 */
class MODELGEN_API FModelGenVHACD
{
public:
    /**
     * 从ProceduralMeshComponent生成多个凸包碰撞
     * @param ProceduralMeshComponent 输入的ProceduralMesh组件
     * @param BodySetup 输出的BodySetup（将添加凸包元素）
     * @param HullCount 目标凸包数量（1-64，默认8）
     * @param MaxHullVerts 每个凸包的最大顶点数（6-32，默认16）
     * @param HullPrecision 精度/体素数量（10000-1000000，默认100000）
     * @return 是否成功生成凸包
     */
    static bool GenerateConvexHulls(
        UProceduralMeshComponent* ProceduralMeshComponent,
        UBodySetup* BodySetup,
        int32 HullCount = 8,
        int32 MaxHullVerts = 16,
        uint32 HullPrecision = 100000);
};

