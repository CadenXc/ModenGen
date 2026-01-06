#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "Sphere.generated.h"

class FSphereBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Sphere"))
class MODELGEN_API ASphere : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    ASphere();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere|Geometry", 
        meta = (ClampMin = "4", ClampMax = "64", UIMin = "4", UIMax = "64", DisplayName = "边数"))
    int32 Sides = 16;

    UFUNCTION(BlueprintCallable, Category = "Sphere|Parameters")
    void SetSides(int32 NewSides);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere|Geometry", 
        meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "横截断"))
    float HorizontalCut = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Sphere|Parameters")
    void SetHorizontalCut(float NewHorizontalCut);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere|Geometry", 
        meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "竖截断"))
    float VerticalCut = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "Sphere|Parameters")
    void SetVerticalCut(float NewVerticalCut);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sphere|Geometry", 
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "Radius"))
    float Radius = 50.0f;

    UFUNCTION(BlueprintCallable, Category = "Sphere|Parameters")
    void SetRadius(float NewRadius);

    virtual void GenerateMesh() override;

private:
    bool TryGenerateMeshInternal();

public:
    virtual bool IsValid() const override;
    
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
};

