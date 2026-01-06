#include "SphereBuilder.h"
#include "Sphere.h"
#include "ModelGenMeshData.h"

FSphereBuilder::FSphereBuilder(const ASphere& InSphere)
    : Sphere(InSphere)
{
    Clear();
}

void FSphereBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    Radius = 0.0f;
    Sides = 0;
    HorizontalCut = 0.0f;
    VerticalCut = 0.0f;
    ZOffset = 0.0f;
}

static bool IsTriangleDegenerate(int32 V0, int32 V1, int32 V2)
{
    return (V0 == V1) || (V1 == V2) || (V2 == V0);
}


bool FSphereBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!Sphere.IsValid())
    {
        return false;
    }

    Clear();

    // Cache settings
    Radius = Sphere.Radius;
    Sides = Sphere.Sides;
    HorizontalCut = Sphere.HorizontalCut;
    VerticalCut = Sphere.VerticalCut;

    if (HorizontalCut >= 1.0f - KINDA_SMALL_NUMBER)
    {
        HorizontalCut = 1.0f - KINDA_SMALL_NUMBER;
    }

    if (VerticalCut <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const float PhiRange = PI * (1.0f - HorizontalCut);
    const float ThetaRange = VerticalCut * 2.0f * PI;

    const float MinArcLength = 0.01f; // 0.01cm
    if (Radius * PhiRange < MinArcLength || Radius * ThetaRange < MinArcLength)
    {
        return false;
    }

    ReserveMemory();

    const float EndPhi = PhiRange; // StartPhi is 0
    const float CutPlaneZ = Radius * FMath::Cos(EndPhi);
    ZOffset = -CutPlaneZ;

    GenerateSphereMesh();
    GenerateCaps();

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;

    return true;
}

int32 FSphereBuilder::CalculateVertexCountEstimate() const
{
    return Sphere.CalculateVertexCountEstimate();
}

int32 FSphereBuilder::CalculateTriangleCountEstimate() const
{
    return Sphere.CalculateTriangleCountEstimate();
}

FVector FSphereBuilder::GetSpherePoint(float Theta, float Phi) const
{
    float SinPhi = FMath::Sin(Phi);
    float CosPhi = FMath::Cos(Phi);
    float SinTheta = FMath::Sin(Theta);
    float CosTheta = FMath::Cos(Theta);

    float X = Radius * SinPhi * CosTheta;
    float Y = Radius * SinPhi * SinTheta;
    
    float Z = Radius * CosPhi + ZOffset;

    return FVector(X, Y, Z);
}

FVector FSphereBuilder::GetSphereNormal(float Theta, float Phi) const
{
    return FVector(
        FMath::Sin(Phi) * FMath::Cos(Theta),
        FMath::Sin(Phi) * FMath::Sin(Theta),
        FMath::Cos(Phi)
    );
}

void FSphereBuilder::SafeAddTriangle(int32 V0, int32 V1, int32 V2)
{
    if (!IsTriangleDegenerate(V0, V1, V2))
    {
        AddTriangle(V0, V1, V2);
    }
}

void FSphereBuilder::SafeAddQuad(int32 V0, int32 V1, int32 V2, int32 V3)
{
    SafeAddTriangle(V0, V1, V3);
    SafeAddTriangle(V1, V2, V3);
}

void FSphereBuilder::GenerateSphereMesh()
{
    const int32 NumRings = FMath::Max(2, Sides / 2);
    const int32 NumSegments = Sides;

    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;

    if (PhiRange <= KINDA_SMALL_NUMBER) return;

    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;
    const float ThetaRange = EndTheta - StartTheta;

    TArray<TArray<int32>> GridIndices;
    GridIndices.SetNum(NumRings + 1);

    // 1. 生成顶点
    for (int32 v = 0; v <= NumRings; ++v)
    {
        const float VRatio = static_cast<float>(v) / NumRings;
        const float Phi = StartPhi + VRatio * PhiRange;

        GridIndices[v].Reserve(NumSegments + 1);

        for (int32 h = 0; h <= NumSegments; ++h)
        {
            const float HRatio = static_cast<float>(h) / NumSegments;
            const float Theta = StartTheta + HRatio * ThetaRange;

            FVector Pos = GetSpherePoint(Theta, Phi);
            FVector Normal = GetSphereNormal(Theta, Phi);
            FVector2D UV(HRatio, VRatio);

            GridIndices[v].Add(AddVertex(Pos, Normal, UV));
        }
    }

    for (int32 v = 0; v < NumRings; ++v)
    {
        for (int32 h = 0; h < NumSegments; ++h)
        {
            int32 V0 = GridIndices[v][h];       // Top-Left
            int32 V1 = GridIndices[v][h + 1];   // Top-Right
            int32 V2 = GridIndices[v + 1][h];   // Bottom-Left
            int32 V3 = GridIndices[v + 1][h + 1]; // Bottom-Right

            if (v == 0)
            {
                SafeAddTriangle(V0, V3, V2);
            }
            else if (v == NumRings - 1 && FMath::IsNearlyEqual(EndPhi, PI))
            {
                SafeAddTriangle(V0, V1, V2);
            }
            else
            {
                SafeAddQuad(V0, V1, V3, V2);
            }
        }
    }
}

void FSphereBuilder::GenerateCaps()
{
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;

    if (EndPhi < PI - KINDA_SMALL_NUMBER)
    {
        GenerateHorizontalCap(EndPhi, true);
    }

    if (VerticalCut < 1.0f - KINDA_SMALL_NUMBER)
    {
        GenerateVerticalCap(StartTheta, true);
        GenerateVerticalCap(EndTheta, false);
    }
}

void FSphereBuilder::GenerateHorizontalCap(float Phi, bool bIsBottom)
{
    const int32 Segments = Sides;
    const float StartTheta = 0.0f;
    const float EndTheta = VerticalCut * 2.0f * PI;
    const float ThetaRange = EndTheta - StartTheta;

    if (ThetaRange <= KINDA_SMALL_NUMBER) return;

    float CenterZ = Radius * FMath::Cos(Phi) + ZOffset;
    FVector CenterPos(0.0f, 0.0f, CenterZ);
    FVector Normal(0.0f, 0.0f, -1.0f);
    FVector2D CenterUV(0.5f, 0.5f);

    int32 CenterIndex = AddVertex(CenterPos, Normal, CenterUV);

    TArray<int32> RimIndices;
    RimIndices.Reserve(Segments + 1);

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Ratio = static_cast<float>(i) / Segments;
        float Theta = StartTheta + Ratio * ThetaRange;

        FVector Pos = GetSpherePoint(Theta, Phi);
        float U = (Pos.X / Radius) * 0.5f + 0.5f;
        float V = (Pos.Y / Radius) * 0.5f + 0.5f;

        RimIndices.Add(AddVertex(Pos, Normal, FVector2D(U, V)));
    }

    for (int32 i = 0; i < Segments; ++i)
    {
        SafeAddTriangle(CenterIndex, RimIndices[i], RimIndices[i + 1]);
    }
}

void FSphereBuilder::GenerateVerticalCap(float Theta, bool bIsStart)
{
    const float StartPhi = 0.0f;
    const float EndPhi = PI * (1.0f - HorizontalCut);
    const float PhiRange = EndPhi - StartPhi;

    if (PhiRange <= KINDA_SMALL_NUMBER) return;

    const int32 Segments = FMath::Max(2, Sides / 2);

    FVector Tangent(-FMath::Sin(Theta), FMath::Cos(Theta), 0.0f);
    FVector Normal = bIsStart ? -Tangent : Tangent;

    TArray<int32> ProfileIndices;
    TArray<int32> AxisIndices;

    for (int32 i = 0; i <= Segments; ++i)
    {
        float Ratio = static_cast<float>(i) / Segments;
        float Phi = StartPhi + Ratio * PhiRange;

        FVector Pos = GetSpherePoint(Theta, Phi);
        FVector2D UV_Prof(1.0f, Ratio);
        ProfileIndices.Add(AddVertex(Pos, Normal, UV_Prof));

        FVector AxisPos(0.0f, 0.0f, Pos.Z);
        FVector2D UV_Axis(0.0f, Ratio);
        AxisIndices.Add(AddVertex(AxisPos, Normal, UV_Axis));
    }

    for (int32 i = 0; i < Segments; ++i)
    {
        int32 AxisCurr = AxisIndices[i];
        int32 AxisNext = AxisIndices[i + 1];
        int32 ProfCurr = ProfileIndices[i];
        int32 ProfNext = ProfileIndices[i + 1];

        bool bTopDegenerate = (i == 0);
        bool bBottomDegenerate = (i == Segments - 1) && FMath::IsNearlyEqual(EndPhi, PI);

        if (bTopDegenerate)
        {
            if (bIsStart) SafeAddTriangle(AxisCurr, ProfNext, AxisNext);
            else          SafeAddTriangle(AxisCurr, AxisNext, ProfNext);
        }
        else if (bBottomDegenerate)
        {
            if (bIsStart) SafeAddTriangle(AxisCurr, ProfCurr, AxisNext);
            else          SafeAddTriangle(AxisCurr, AxisNext, ProfCurr);
        }
        else
        {
            if (bIsStart) SafeAddQuad(AxisCurr, ProfCurr, ProfNext, AxisNext);
            else          SafeAddQuad(AxisCurr, AxisNext, ProfNext, ProfCurr);
        }
    }
}