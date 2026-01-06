// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelGenMeshData.h"

UENUM(BlueprintType)
enum class EEndCapType : uint8
{
    Start   UMETA(DisplayName = "起始端盖"),
    End     UMETA(DisplayName = "结束端盖")
};

UENUM(BlueprintType)
enum class EHeightPosition : uint8
{
    Top     UMETA(DisplayName = "顶部"),
    Bottom  UMETA(DisplayName = "底部")
};

UENUM(BlueprintType)
enum class EInnerOuter : uint8
{
    Inner   UMETA(DisplayName = "内侧"),
    Outer   UMETA(DisplayName = "外侧")
};

class MODELGEN_API FModelGenMeshBuilder
{
public:
    FModelGenMeshBuilder();
    virtual ~FModelGenMeshBuilder() = default;

    virtual bool Generate(FModelGenMeshData& OutMeshData) = 0;

    virtual int32 CalculateVertexCountEstimate() const = 0;
    virtual int32 CalculateTriangleCountEstimate() const = 0;

protected:
    FModelGenMeshData MeshData;

    TMap<FString, int32> UniqueVerticesMap;

    int32 GetOrAddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV);

    FVector GetPosByIndex(int32 index) const;

    int32 AddVertex(const FVector& Pos, const FVector& Normal, const FVector2D& UV);

    void AddTriangle(int32 V0, int32 V1, int32 V2);
    void AddQuad(int32 V0, int32 V1, int32 V2, int32 V3);

    FVector CalculateTangent(const FVector& Normal) const;

    void Clear();

    bool ValidateGeneratedData() const;

    void ReserveMemory();
};