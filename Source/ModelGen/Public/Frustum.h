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
    int32 Sides = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (ClampMin = "1"))
    int32 HeightSegments = 4;

    // 倒角参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "0.0"))
    float ChamferRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (ClampMin = "1"))
    int32 ChamferSections = 4;

    // 倒角突出参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamfer", meta = (UIMin = "0.0", UIMax = "1.0"))
    float ChamferBulgeAmount = 0.2f;

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
    void CreateSideGeometry(FMeshSection& Section, float StartZ, float EndZ);
    void CreateTopGeometry(FMeshSection& Section, float Z);
    void CreateBottomGeometry(FMeshSection& Section, float Z);
    void CreateTopChamferGeometry(FMeshSection& Section, float StartZ);
    void CreateBottomChamferGeometry(FMeshSection& Section, float StartZ);
    void CreateEndCaps(FMeshSection& Section);

    // 顶点管理
    int32 AddVertex(FMeshSection& Section, const FVector& Position, const FVector& Normal, const FVector2D& UV);
    void AddQuad(FMeshSection& Section, int32 V1, int32 V2, int32 V3, int32 V4);
    void AddTriangle(FMeshSection& Section, int32 V1, int32 V2, int32 V3);

    // 材质管理
    void ApplyMaterial();

    // 内部状态
    bool bGeometryDirty = true;

    // 数学倒角圆弧相关结构与函数声明
    struct FChamferArcControlPoints
    {
        FVector StartPoint;      // 起点：侧边顶点
        FVector EndPoint;        // 终点：顶/底面顶点
        FVector ControlPoint;    // 控制点：原始顶点位置
    };

    // 修正后的函数签名，传入倒角起点和终点
    FChamferArcControlPoints CalculateChamferControlPoints(const FVector& SideVertex, const FVector& TopBottomVertex);
    FVector CalculateChamferArcPoint(const FChamferArcControlPoints& ControlPoints, float t);
    FVector CalculateChamferArcTangent(const FChamferArcControlPoints& ControlPoints, float t);
};