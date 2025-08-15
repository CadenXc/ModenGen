// Copyright (c) 2024. All rights reserved.

/**
 * @file HollowPrism.h
 * @brief 可配置的空心棱柱生成器
 * 
 * 该类提供了一个可在编辑器中实时配置的空心棱柱生成器。
 * 支持以下特性：
 * - 可配置的内外半径和高度
 * - 可调节的边数和弧角
 * - 可选的倒角效果
 * - 支持部分弧段生成
 * - 实时预览和更新
 */

#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

// Generated header must be the last include
#include "HollowPrism.generated.h"

// Forward declarations
class UProceduralMeshComponent;
class FHollowPrismBuilder;
struct FModelGenMeshData;

/**
 * 程序化生成的空心棱柱Actor
 * 支持实时参数调整和预览
 */
UCLASS(BlueprintType, meta=(DisplayName = "Hollow Prism"))
class MODELGEN_API AHollowPrism : public AActor
{
    GENERATED_BODY()

public:
    //~ Begin Constructor Section
    AHollowPrism();

    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

    //~ Begin Geometry Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Inner Radius", ToolTip = "空心棱柱的内半径"))
    float InnerRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Outer Radius", ToolTip = "空心棱柱的外半径"))
    float OuterRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.01", UIMin = "0.01", 
        DisplayName = "Height", ToolTip = "空心棱柱的高度"))
    float Height = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", 
        DisplayName = "Sides", ToolTip = "空心棱柱的边数"))
    int32 Sides = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", 
        DisplayName = "Outer Sides", ToolTip = "空心棱柱的外边数"))
    int32 OuterSides = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "3", UIMin = "3", ClampMax = "100", UIMax = "50", 
        DisplayName = "Inner Sides", ToolTip = "空心棱柱的内边数"))
    int32 InnerSides = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Geometry", 
        meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "360.0", UIMax = "360.0", 
        DisplayName = "Arc Angle", ToolTip = "空心棱柱的弧角（度）"))
    float ArcAngle = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Bevel", 
        meta = (ClampMin = "0.0", UIMin = "0.0", 
        DisplayName = "Bevel Radius", ToolTip = "倒角的半径"))
    float BevelRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Bevel", 
        meta = (ClampMin = "1", UIMin = "1", ClampMax = "20", UIMax = "10", 
        DisplayName = "Bevel Sections", ToolTip = "倒角的分段数"))
    int32 BevelSegments = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Options", 
        meta = (DisplayName = "Use Triangle Method", ToolTip = "使用三角形方法生成顶底面"))
    bool bUseTriangleMethod = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Debug", 
        meta = (DisplayName = "Flip Normals", ToolTip = "反转所有法线方向"))
    bool bFlipNormals = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Debug", 
        meta = (DisplayName = "Disable Debounce", ToolTip = "禁用防抖功能，实现实时更新"))
    bool bDisableDebounce = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Materials")
    UMaterialInterface* Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HollowPrism|Collision")
    bool bUseAsyncCooking = true;

private:
    UPROPERTY(VisibleAnywhere, Category = "HollowPrism|Components")
    UProceduralMeshComponent* ProceduralMesh;

    void InitializeComponents();
    
    void ApplyMaterial();
    
    void SetupCollision();
    
    void RegenerateMesh();
    
    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Generation")
    void RegenerateMeshBlueprint();
    
    UFUNCTION(BlueprintCallable, Category = "HollowPrism|Materials")
    void SetMaterial(UMaterialInterface* NewMaterial);

public:
    // 参数验证和计算函数
    bool IsValid() const;
    
    float GetWallThickness() const;
    
    bool IsFullCircle() const;
    
    float GetHalfHeight() const { return Height * 0.5f; }
    
    int32 CalculateVertexCountEstimate() const;
    
    int32 CalculateTriangleCountEstimate() const;
    
    bool operator==(const AHollowPrism& Other) const;
    bool operator!=(const AHollowPrism& Other) const;
};