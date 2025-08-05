#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PolygonTorus.generated.h"

UCLASS()
class MODELGEN_API APolygonTorus : public AActor
{
    GENERATED_BODY()

public:
    APolygonTorus();

    // 圆环参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        float MajorRadius = 100.0f;  // 圆环中心到截面中心的距离

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        float MinorRadius = 25.0f;   // 截面半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        int32 MajorSegments = 8;     // 圆环分段数（多边形边数）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        int32 MinorSegments = 4;     // 截面分段数（多边形边数）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        float TorusAngle = 360.0f;   // 圆环角度（度数）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        bool bSmoothCrossSection = true;  // 横切面光滑

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torus Parameters")
        bool bSmoothVerticalSection = true;  // 竖面光滑

        // 网格生成函数
    UFUNCTION(BlueprintCallable, Category = "Torus")
        void GeneratePolygonTorus(float MajorRad, float MinorRad, int32 MajorSegs, int32 MinorSegs, float Angle, bool bSmoothCross, bool bSmoothVertical);

    // 引擎事件
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere)
        UProceduralMeshComponent* ProceduralMesh;

    // 辅助函数
    void GeneratePolygonVertices(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        const FVector& Center,
        const FVector& Direction,
        const FVector& UpVector,
        float Radius,
        int32 Segments,
        float UOffset
    );

    void AddQuad(
        TArray<int32>& Triangles,
        int32 V0, int32 V1, int32 V2, int32 V3
    );

    // 重构后的辅助函数
    void ValidateParameters(float& MajorRad, float& MinorRad, int32& MajorSegs, int32& MinorSegs, float& Angle);
    
    void GenerateSectionVertices(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& SectionStartIndices,
        float MajorRad,
        float MinorRad,
        int32 MajorSegs,
        int32 MinorSegs,
        float AngleStep
    );
    
    void GenerateSideTriangles(
        TArray<int32>& Triangles,
        const TArray<int32>& SectionStartIndices,
        int32 MajorSegs,
        int32 MinorSegs
    );
    
    void GenerateEndCaps(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FProcMeshTangent>& Tangents,
        TArray<int32>& Triangles,
        const TArray<int32>& SectionStartIndices,
        float MajorRad,
        float AngleRad,
        int32 MajorSegs,
        int32 MinorSegs
    );
    
    void CalculateSmoothNormals(
        TArray<FVector>& Normals,
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        const TArray<int32>& SectionStartIndices,
        float MajorRad,
        float AngleStep,
        int32 MajorSegs,
        int32 MinorSegs,
        bool bSmoothCross,
        bool bSmoothVertical
    );
    
    // 调试函数：验证法线方向
    void ValidateNormalDirections(
        const TArray<FVector>& Vertices,
        const TArray<FVector>& Normals,
        const TArray<int32>& SectionStartIndices,
        float MajorRad,
        int32 MajorSegs,
        int32 MinorSegs
    );
};
