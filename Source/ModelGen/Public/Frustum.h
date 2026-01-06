#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshActor.h"
#include "Frustum.generated.h"

class FFrustumBuilder;
struct FModelGenMeshData;

UCLASS(BlueprintType, meta=(DisplayName = "Frustum"))
class MODELGEN_API AFrustum : public AProceduralMeshActor
{
    GENERATED_BODY()

public:
    AFrustum();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "Top Radius"))
    float TopRadius = 30.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetTopRadius(float NewTopRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "Bottom Radius"))
    float BottomRadius = 50.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBottomRadius(float NewBottomRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Geometry", 
        meta = (ClampMin = "0.01", ClampMax = "1000", UIMin = "0.01", UIMax = "1000", DisplayName = "Height"))
    float Height = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetHeight(float NewHeight);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", ClampMax = "25", UIMin = "3", UIMax = "25", DisplayName = "Top Sides"))
    int32 TopSides = 8;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetTopSides(int32 NewTopSides);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "3", ClampMax = "25", UIMin = "3", UIMax = "25", DisplayName = "Bottom Sides"))
    int32 BottomSides = 12;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBottomSides(int32 NewBottomSides);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Tessellation", 
        meta = (ClampMin = "0", ClampMax = "12", UIMin = "0", UIMax = "12", DisplayName = "Height Segments", 
        ToolTip = "高度分段数。0表示只有顶部和底部两个分段，实际分段数 = 此值 + 1"))
    int32 HeightSegments = 0;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetHeightSegments(int32 NewHeightSegments);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "0.0", ClampMax = "250", UIMin = "0.0", UIMax = "250", DisplayName = "Bevel Radius"))
    float BevelRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bevel", 
        meta = (ClampMin = "0", ClampMax = "12", UIMin = "0", UIMax = "12", DisplayName = "Bevel Segments"))
    int32 BevelSegments = 2;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBevelRadius(float NewBevelRadius);

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBevelSegments(int32 NewBevelSegments);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (ClampMin = "-1.0", ClampMax = "1.0", DisplayName = "Bend Amount"))
    float BendAmount = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetBendAmount(float NewBendAmount);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Bending", 
        meta = (ClampMin = "0.0", ClampMax = "1000", UIMin = "0.0", UIMax = "1000", DisplayName = "Min Bend Radius"))
    float MinBendRadius = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetMinBendRadius(float NewMinBendRadius);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum|Angular", 
        meta = (ClampMin = "0.0", ClampMax = "360.0", UIMin = "0.0", UIMax = "360.0", DisplayName = "Arc Angle"))
    float ArcAngle = 360.0f;

    UFUNCTION(BlueprintCallable, Category = "Frustum|Parameters")
    void SetArcAngle(float NewArcAngle);

    virtual void GenerateMesh() override;

private:
    bool TryGenerateMeshInternal();

public:
    virtual bool IsValid() const override;
    
    float GetHalfHeight() const { return Height * 0.5f; }
    int32 CalculateVertexCountEstimate() const;
    int32 CalculateTriangleCountEstimate() const;
};