// Copyright (c) 2024. All rights reserved.

#include "BevelCubeBuilder.h"
#include "BevelCube.h"
#include "ModelGenMeshData.h"
#include "ModelGenConstants.h"

FBevelCubeBuilder::FBevelCubeBuilder(const ABevelCube & InBevelCube)
    : BevelCube(InBevelCube)
{
    Clear();
}

void FBevelCubeBuilder::Clear()
{
    FModelGenMeshBuilder::Clear();
    GridX.Empty();
    GridY.Empty();
    GridZ.Empty();
}

bool FBevelCubeBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    if (!BevelCube.IsValid())
    {
        return false;
    }

    Clear();
    ReserveMemory();

    HalfSize = BevelCube.GetHalfSize();
    InnerOffset = BevelCube.GetInnerOffset();

    BevelRadius = BevelCube.BevelRadius;
    BevelSegments = BevelCube.BevelSegments;

    bEnableBevel = (BevelSegments > 0) && (BevelRadius > KINDA_SMALL_NUMBER) &&
        (HalfSize.X > InnerOffset.X + KINDA_SMALL_NUMBER) &&
        (HalfSize.Y > InnerOffset.Y + KINDA_SMALL_NUMBER) &&
        (HalfSize.Z > InnerOffset.Z + KINDA_SMALL_NUMBER);

    PrecomputeGrids();

    GenerateUnfoldedFace({ FVector(0, 1, 0),  FVector(1, 0, 0), FVector(0, 0,-1), TEXT("Front") }, GridX, GridZ, 0, 2);
    GenerateUnfoldedFace({ FVector(0,-1, 0),  FVector(-1, 0, 0), FVector(0, 0,-1), TEXT("Back") }, GridX, GridZ, 0, 2);
    GenerateUnfoldedFace({ FVector(0, 0, 1),  FVector(1, 0, 0), FVector(0, 1, 0), TEXT("Top") }, GridX, GridY, 0, 1);
    GenerateUnfoldedFace({ FVector(0, 0,-1),  FVector(1, 0, 0), FVector(0,-1, 0), TEXT("Bottom") }, GridX, GridY, 0, 1);
    GenerateUnfoldedFace({ FVector(1, 0, 0),  FVector(0,-1, 0), FVector(0, 0,-1), TEXT("Right") }, GridY, GridZ, 1, 2);
    GenerateUnfoldedFace({ FVector(-1, 0, 0),  FVector(0, 1, 0), FVector(0, 0,-1), TEXT("Left") }, GridY, GridZ, 1, 2);

    if (!ValidateGeneratedData())
    {
        return false;
    }

    MeshData.CalculateTangents();
    OutMeshData = MeshData;
    return true;
}

int32 FBevelCubeBuilder::CalculateVertexCountEstimate() const
{
    return BevelCube.GetVertexCount();
}

int32 FBevelCubeBuilder::CalculateTriangleCountEstimate() const
{
    return BevelCube.GetTriangleCount();
}

void FBevelCubeBuilder::PrecomputeGrids()
{
    ComputeSingleAxisGrid(HalfSize.X, InnerOffset.X, GridX);
    ComputeSingleAxisGrid(HalfSize.Y, InnerOffset.Y, GridY);
    ComputeSingleAxisGrid(HalfSize.Z, InnerOffset.Z, GridZ);
}

void FBevelCubeBuilder::ComputeSingleAxisGrid(float AxisHalfSize, float AxisInnerOffset, TArray<float>& OutGrid)
{
    OutGrid.Empty();
    if (!bEnableBevel)
    {
        OutGrid = { -AxisHalfSize, AxisHalfSize };
    }
    else
    {
        const float Range = AxisHalfSize - AxisInnerOffset;
        const float Step = Range / BevelSegments;

        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            OutGrid.Add(-AxisHalfSize + i * Step);
        }

        for (int32 i = 0; i <= BevelSegments; ++i)
        {
            OutGrid.Add(AxisInnerOffset + i * Step);
        }
    }
}

void FBevelCubeBuilder::GenerateUnfoldedFace(const FUnfoldedFace& FaceDef, const TArray<float>& GridU, const TArray<float>& GridV, int32 AxisIndexU, int32 AxisIndexV)
{
    const int32 NumU = GridU.Num();
    const int32 NumV = GridV.Num();

    if (NumU < 2 || NumV < 2) return;

    TArray<int32> VertIndices;
    VertIndices.SetNumUninitialized(NumU * NumV);

    float OffsetU = InnerOffset[AxisIndexU];
    float OffsetV = InnerOffset[AxisIndexV];

    for (int32 v_idx = 0; v_idx < NumV; ++v_idx)
    {
        const float local_v = GridV[v_idx];
        const bool bInMainPlaneV = (FMath::Abs(local_v) <= OffsetV + KINDA_SMALL_NUMBER);

        for (int32 u_idx = 0; u_idx < NumU; ++u_idx)
        {
            const float local_u = GridU[u_idx];
            const bool bInMainPlaneU = (FMath::Abs(local_u) <= OffsetU + KINDA_SMALL_NUMBER);

            FVector VertexPos;
            FVector Normal;

            if (!bEnableBevel || (bInMainPlaneU && bInMainPlaneV))
            {
                Normal = FaceDef.Normal;
                VertexPos = FaceDef.Normal * HalfSize[0] * FMath::Abs(FaceDef.Normal.X) +
                    FaceDef.Normal * HalfSize[1] * FMath::Abs(FaceDef.Normal.Y) +
                    FaceDef.Normal * HalfSize[2] * FMath::Abs(FaceDef.Normal.Z) +
                    FaceDef.U_Axis * local_u +
                    FaceDef.V_Axis * local_v;
            }
            else
            {
                const float ClampedU = FMath::Clamp(local_u, -OffsetU, OffsetU);
                const float ClampedV = FMath::Clamp(local_v, -OffsetV, OffsetV);

                FVector NormalOffset = FaceDef.Normal * InnerOffset[0] * FMath::Abs(FaceDef.Normal.X) +
                    FaceDef.Normal * InnerOffset[1] * FMath::Abs(FaceDef.Normal.Y) +
                    FaceDef.Normal * InnerOffset[2] * FMath::Abs(FaceDef.Normal.Z);

                const FVector InnerPoint = NormalOffset +
                    FaceDef.U_Axis * ClampedU +
                    FaceDef.V_Axis * ClampedV;

                FVector OuterNormalOffset = FaceDef.Normal * HalfSize[0] * FMath::Abs(FaceDef.Normal.X) +
                    FaceDef.Normal * HalfSize[1] * FMath::Abs(FaceDef.Normal.Y) +
                    FaceDef.Normal * HalfSize[2] * FMath::Abs(FaceDef.Normal.Z);

                const FVector OuterPoint = OuterNormalOffset +
                    FaceDef.U_Axis * local_u +
                    FaceDef.V_Axis * local_v;

                FVector Dir = (OuterPoint - InnerPoint);
                if (!Dir.IsNearlyZero())
                {
                    Normal = Dir.GetSafeNormal();
                    VertexPos = InnerPoint + Normal * BevelRadius;
                }
                else
                {
                    Normal = FaceDef.Normal;
                    VertexPos = OuterPoint;
                }
            }

            VertexPos += FVector(0, 0, HalfSize.Z);

            float PhysicalU = (local_u + HalfSize[AxisIndexU]) * ModelGenConstants::GLOBAL_UV_SCALE;
            float PhysicalV = (local_v + HalfSize[AxisIndexV]) * ModelGenConstants::GLOBAL_UV_SCALE;

            FVector2D UV(PhysicalU, PhysicalV);

            VertIndices[v_idx * NumU + u_idx] = AddVertex(VertexPos, Normal, UV);
        }
    }

    for (int32 v = 0; v < NumV - 1; ++v)
    {
        for (int32 u = 0; u < NumU - 1; ++u)
        {
            int32 V00 = VertIndices[v * NumU + u];
            int32 V10 = VertIndices[v * NumU + (u + 1)];
            int32 V01 = VertIndices[(v + 1) * NumU + u];
            int32 V11 = VertIndices[(v + 1) * NumU + (u + 1)];

            AddQuad(V00, V01, V11, V10);
        }
    }
}
