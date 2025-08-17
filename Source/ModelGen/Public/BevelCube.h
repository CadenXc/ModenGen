// Copyright (c) 2024. All rights reserved.

/**
 * @file BevelCube.h
 * @brief 可配置的程序化圆角立方体生成器
 */

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "BevelCube.generated.h"

class FBevelCubeBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Bevel Cube"))
class MODELGEN_API ABevelCube : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    ABevelCube();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "Size"))
    float Size = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Bevel Size"))
    float BevelRadius = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters", 
        meta = (ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10", 
        DisplayName = "Bevel Sections"))
    int32 BevelSegments = 3;



public:
    float GetHalfSize() const { return Size * 0.5f; }
    float GetInnerOffset() const { return GetHalfSize() - BevelRadius; }
    int32 GetVertexCount() const;
    int32 GetTriangleCount() const;

protected:
    // 实现父类的纯虚函数
    virtual void GenerateMesh() override;

public:
    // 实现父类的纯虚函数
    virtual bool IsValid() const override;
    
    UFUNCTION(BlueprintCallable, Category = "BevelCube|Generation")
    void GenerateBeveledCube(float InSize, float InBevelSize, int32 InSections);
};