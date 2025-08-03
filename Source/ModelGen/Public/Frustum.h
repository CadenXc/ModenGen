// Frustum.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Frustum.generated.h"

USTRUCT(BlueprintType)
struct FFrustumParameters
{
    GENERATED_BODY()

        // 基础几何参数
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float TopRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float BottomRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (ClampMin = "0.01"))
        float Height = 200.0f;

    // 细分控制
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (ClampMin = "3"))
        int32 TopSides = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (ClampMin = "3"))
        int32 BottomSides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (ClampMin = "1"))
        int32 HeightSegments = 4;

    // 倒角参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "0.0"))
        float ChamferRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "1"))
        int32 ChamferSections = 2;

    // 弯曲参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bending", meta = (UIMin = "-1.0", UIMax = "1.0"))
        float BendAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bending", meta = (ClampMin = "0.0"))
        float MinBendRadius = 1.0f;

    // 角度控制
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angular", meta = (UIMin = "0.0", UIMax = "360.0"))
        float ArcAngle = 360.0f;

    // 端面控制
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caps", meta = (ClampMin = "0.0"))
        float CapThickness = 10.0f;

};

UCLASS()
class MODELGEN_API AFrustum : public AActor
{
    GENERATED_BODY()

public:
    AFrustum();

    // 主要参数集
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum")
        FFrustumParameters Parameters;

    // 蓝图可调用生成函数
    UFUNCTION(BlueprintCallable, Category = "Frustum")
        void Regenerate();

protected:
    virtual void BeginPlay() override;
    virtual void PostLoad() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    virtual void Tick(float DeltaTime) override;

private:
    // 网格组件
    UPROPERTY(VisibleAnywhere)
        UProceduralMeshComponent* MeshComponent;

    // 网格数据结构
    struct FMeshSection
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UVs;
        TArray<FLinearColor> VertexColors;
        TArray<FProcMeshTangent> Tangents;

        void Clear()
        {
            Vertices.Reset();
            Triangles.Reset();
            Normals.Reset();
            UVs.Reset();
            VertexColors.Reset();
            Tangents.Reset();
        }

        void Reserve(int32 VertexCount, int32 TriangleCount)
        {
            Vertices.Reserve(VertexCount);
            Triangles.Reserve(TriangleCount);
            Normals.Reserve(VertexCount);
            UVs.Reserve(VertexCount);
            VertexColors.Reserve(VertexCount);
            Tangents.Reserve(VertexCount);
        }
    };

    // 几何生成模块
    void GenerateGeometry();
    void CreateSideGeometry(FMeshSection& Section);
    void CreateTopGeometry(FMeshSection& Section);
    void CreateBottomGeometry(FMeshSection& Section);
    void CreateChamfers(FMeshSection& Section);
    void CreateEndCaps(FMeshSection& Section);

    TArray<int32> GenerateRingVertices(FMeshSection& Section, float Radius, float Z, int32 NumSides, float ArcAngleDeg, const FVector& NormalBase, float VValue, bool bCapUV);

    // 顶点管理
    int32 AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV);
    void AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4);
    void AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3);

    // 材质管理
    void ApplyMaterial();

    // 弯曲计算函数
    void ApplyBendEffect(FVector& Position, FVector& Normal, float Z) const;

    // 内部状态
    bool bGeometryDirty = true;
};