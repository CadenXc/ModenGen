// Frustum.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Frustum.generated.h"

// 前向声明
class FFrustumGeometry;
class FFrustumBuilder;

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

    // 验证参数有效性
    bool IsValid() const;
    
    // 获取预估的顶点和三角形数量
    void GetEstimatedCounts(int32& OutVertexCount, int32& OutTriangleCount) const;
};

/**
 * 截锥体几何数据结构
 */
class FFrustumGeometry
{
public:
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    // 清空数据
    void Clear();
    
    // 预分配内存
    void Reserve(int32 VertexCount, int32 TriangleCount);
    
    // 验证几何数据有效性
    bool IsValid() const;
    
    // 获取几何统计信息
    void GetStats(int32& OutVertexCount, int32& OutTriangleCount) const;
    
    // 获取顶点数量
    int32 GetVertexCount() const { return Vertices.Num(); }
    
    // 获取三角形数量
    int32 GetTriangleCount() const { return Triangles.Num() / 3; }
};

/**
 * 截锥体几何生成器
 */
class FFrustumBuilder
{
public:
    // 生成参数
    struct FBuildParameters
    {
        float TopRadius = 50.0f;
        float BottomRadius = 100.0f;
        float Height = 200.0f;
        int32 Sides = 16;
        int32 HeightSegments = 4;
        float ChamferRadius = 5.0f;
        int32 ChamferSections = 4;
        float BendAmount = 0.0f;
        float MinBendRadius = 1.0f;
        float ArcAngle = 360.0f;
        float CapThickness = 10.0f;
        
        // 验证参数有效性
        bool IsValid() const;
        
        // 获取计算后的参数
        float GetHalfHeight() const { return Height * 0.5f; }
        float GetTopChamferHeight() const { return FMath::Min(ChamferRadius, TopRadius); }
        float GetBottomChamferHeight() const { return FMath::Min(ChamferRadius, BottomRadius); }
        float GetSideHeight() const { return Height - GetTopChamferHeight() - GetBottomChamferHeight(); }
        float GetTopInnerRadius() const { return FMath::Max(0.0f, TopRadius - ChamferRadius); }
        float GetBottomInnerRadius() const { return FMath::Max(0.0f, BottomRadius - ChamferRadius); }
    };

    // 构造函数
    FFrustumBuilder(const FBuildParameters& InParams);

    // 生成几何体
    bool Generate(FFrustumGeometry& OutGeometry);

private:
    FBuildParameters Params;
    TMap<FVector, int32> UniqueVerticesMap;

    // 顶点管理
    int32 GetOrAddVertex(FFrustumGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    int32 AddVertex(FFrustumGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
    
    // 顶点环管理
    struct FVertexRings
    {
        TArray<TArray<int32>> SideRings;          // 侧面顶点环
        TArray<int32> TopRing;                    // 顶部顶点环
        TArray<int32> BottomRing;                 // 底部顶点环
        TArray<int32> TopInnerRing;               // 顶部内圆环（倒角用）
        TArray<int32> BottomInnerRing;            // 底部内圆环（倒角用）
    };
    
    // 几何生成
    void GenerateAllVertexRings(FFrustumGeometry& Geometry, FVertexRings& Rings);
    void GenerateSideGeometry(FFrustumGeometry& Geometry, const FVertexRings& Rings);
    void GenerateTopGeometry(FFrustumGeometry& Geometry, const FVertexRings& Rings);
    void GenerateBottomGeometry(FFrustumGeometry& Geometry, const FVertexRings& Rings);
    void GenerateTopChamferGeometry(FFrustumGeometry& Geometry, FVertexRings& Rings);
    void GenerateBottomChamferGeometry(FFrustumGeometry& Geometry, FVertexRings& Rings);
    void GenerateEndCaps(FFrustumGeometry& Geometry, const FVertexRings& Rings);
    
    // 辅助函数
    void AddQuad(FFrustumGeometry& Geometry, int32 V1, int32 V2, int32 V3, int32 V4);
    void AddTriangle(FFrustumGeometry& Geometry, int32 V1, int32 V2, int32 V3);
    
    // 法线计算
    FVector CalculateTangent(const FVector& Normal) const;
    
    // 弯曲计算
    float CalculateBentRadius(float BaseRadius, float Alpha) const;
    FVector CalculateBentNormal(const FVector& BaseNormal, float Alpha) const;
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

    // 材质设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Materials")
    UMaterialInterface* Material;

    // 碰撞设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Collision")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frustum Collision")
    bool bUseAsyncCooking = true;

    // 蓝图可调用函数
    UFUNCTION(BlueprintCallable, Category = "Frustum")
    void Regenerate();

    UFUNCTION(BlueprintCallable, Category = "Frustum")
    void RegenerateMesh();

    UFUNCTION(BlueprintPure, Category = "Frustum")
    UProceduralMeshComponent* GetProceduralMesh() const { return MeshComponent; }

    UFUNCTION(BlueprintPure, Category = "Frustum")
    bool IsGeometryValid() const;

    UFUNCTION(BlueprintPure, Category = "Frustum")
    void GetGeometryStats(int32& OutVertexCount, int32& OutTriangleCount) const;

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    #if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    #endif

    virtual void Tick(float DeltaTime) override;

private:
    // 网格组件
    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* MeshComponent;

    // 内部几何生成器
    TUniquePtr<FFrustumBuilder> GeometryBuilder;

    // 当前几何数据
    FFrustumGeometry CurrentGeometry;

    // 内部状态
    bool bGeometryDirty = true;

    // 几何生成
    void GenerateGeometry();

    // 材质管理
    void ApplyMaterial();
    
    // 设置碰撞
    void SetupCollision();

    // 参数验证
    bool ValidateParameters() const;

    // 错误处理
    void LogError(const FString& ErrorMessage);
};