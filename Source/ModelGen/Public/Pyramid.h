#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Pyramid.generated.h"

UCLASS()
class MODELGEN_API APyramid : public AActor
{
    GENERATED_BODY()

public:
    APyramid();

    // 可调节参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters")
        float BaseRadius = 100.0f; // 底面半径

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters")
        float Height = 200.0f; // 高度

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters", meta = (ClampMin = 3, UIMin = 3))
        int32 Sides = 4; // 底面边数（最小为3）

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pyramid Parameters")
        bool bCreateBottom = true; // 是否创建底面

    UFUNCTION(BlueprintCallable, Category = "Pyramid")
        void GeneratePyramid(float InBaseRadius, float InHeight, int32 InSides, bool bInCreateBottom);

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere)
        UProceduralMeshComponent* ProceduralMesh;

    // 辅助函数：添加顶点并返回索引
    int32 AddVertex(
        TArray<FVector>& Vertices,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UV0,
        TArray<FProcMeshTangent>& Tangents,
        const FVector& Position,
        const FVector& Normal,
        const FVector2D& UV
    );

    // 添加三角形
    void AddTriangle(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3);
};