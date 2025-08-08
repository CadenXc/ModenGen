// Copyright (c) 2024. All rights reserved.

/**
 * @file PolygonTorus.h
 * @brief 可配置的多边形圆环生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的多边形圆环生成器。
 * 支持以下特性：
 * - 可配置的主半径和次半径
 * - 可调节的主次分段数
 * - 多种光滑模式
 * - 多种UV映射模式
 * - 端盖生成选项
 * - 实时预览和更新
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PolygonTorusParameters.h"
#include "ModelGenMeshData.h"

// Generated header must be the last include
#include "PolygonTorus.generated.h"



/**
 * 程序化生成的多边形圆环Actor
 * 支持实时参数调整和预览
 */
UCLASS(BlueprintType, meta=(DisplayName = "Polygon Torus"))
class MODELGEN_API APolygonTorus : public AActor
{
    GENERATED_BODY()

public:
    //~ Begin Constructor Section
    APolygonTorus();

    //~ Begin Parameters Section
    /** 圆环生成参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonTorus|Parameters",
        meta = (DisplayName = "Parameters", ToolTip = "Torus generation parameters"))
    FPolygonTorusParameters Parameters;

    //~ Begin Public Methods Section
    /**
     * 重新生成网格
     */
    UFUNCTION(BlueprintCallable, Category = "PolygonTorus|Generation", 
        meta = (DisplayName = "Regenerate Mesh"))
    void RegenerateMesh();

protected:
    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    //~ Begin Components Section
    /** 程序化网格组件 */
    UPROPERTY(VisibleAnywhere, Category = "PolygonTorus|Components")
    UProceduralMeshComponent* ProceduralMesh;
};
