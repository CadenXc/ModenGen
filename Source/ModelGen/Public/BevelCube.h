// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"

#include "BevelCube.generated.h"

class FBevelCubeBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta = (DisplayName = "Bevel Cube"))
class MODELGEN_API ABevelCube : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    ABevelCube();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters",
        meta = (MakeEditWidget = true, DisplayName = "Size"))
        FVector Size = FVector(100.0f, 100.0f, 100.0f);

    UFUNCTION(BlueprintCallable, Category = "BevelCube|Parameters")
        void SetSize(FVector NewSize);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters",
        meta = (ClampMin = "0.0", ClampMax = "500", UIMin = "0.0", UIMax = "500", DisplayName = "Bevel Radius"))
        float BevelRadius = 10.0f;

    UFUNCTION(BlueprintCallable, Category = "BevelCube|Parameters")
        void SetBevelRadius(float NewBevelRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BevelCube|Parameters",
        meta = (ClampMin = "0", ClampMax = "10", UIMin = "0", UIMax = "10",
            DisplayName = "Bevel Sections", ToolTip = "倒角分段数。0表示不启用倒角"))
        int32 BevelSegments = 3;

    UFUNCTION(BlueprintCallable, Category = "BevelCube|Parameters")
        void SetBevelSegments(int32 NewBevelSegments);

    virtual void GenerateMesh() override;

private:
    bool TryGenerateMeshInternal();

public:
    FVector GetHalfSize() const { return Size * 0.5f; }

    FVector GetInnerOffset() const { return GetHalfSize() - FVector(BevelRadius); }

    int32 GetVertexCount() const;
    int32 GetTriangleCount() const;

public:
    virtual bool IsValid() const override;
};
