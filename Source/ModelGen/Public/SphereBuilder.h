#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshBuilder.h"

class ASphere;

class MODELGEN_API FSphereBuilder : public FModelGenMeshBuilder
{
public:
    explicit FSphereBuilder(const ASphere& InSphere);

    virtual bool Generate(FModelGenMeshData& OutMeshData) override;
    virtual int32 CalculateVertexCountEstimate() const override;
    virtual int32 CalculateTriangleCountEstimate() const override;

    void Clear();

private:
    const ASphere& Sphere;

    float Radius;
    int32 Sides;
    float HorizontalCut;
    float VerticalCut;
    float ZOffset;

    void GenerateSphereMesh();
    void GenerateCaps();
    void GenerateHorizontalCap(float Phi, bool bIsBottom);
    void GenerateVerticalCap(float Theta, bool bIsStart);

    FVector GetSpherePoint(float Theta, float Phi) const;
    FVector GetSphereNormal(float Theta, float Phi) const;

    void SafeAddTriangle(int32 V0, int32 V1, int32 V2);
    void SafeAddQuad(int32 V0, int32 V1, int32 V2, int32 V3);
};