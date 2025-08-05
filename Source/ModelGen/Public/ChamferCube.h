#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h" 
#include "ChamferCube.generated.h"

// 前向声明
class FChamferCubeGeometry;
class FChamferCubeBuilder;

/**
 * 倒角立方体生成器
 * 使用程序化网格生成倒角立方体几何体
 */
UCLASS()
class MODELGEN_API AChamferCube : public AActor
{
	GENERATED_BODY()

public:
	AChamferCube();

	// 立方体参数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
		float CubeSize = 100.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters", meta = (ClampMin = "0.0"))
		float CubeChamferSize = 10.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Parameters", meta = (ClampMin = "1", ClampMax = "10"))
		int32 ChamferSections = 3;

	// 材质设置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Materials")
		UMaterialInterface* Material;

	// 碰撞设置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Collision")
		bool bGenerateCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube Collision")
		bool bUseAsyncCooking = true;

	// 蓝图可调用的生成函数
	UFUNCTION(BlueprintCallable, Category = "ChamferCube")
		void GenerateChamferedCube(float Size, float ChamferSize, int32 Sections);

	// 重新生成网格
	UFUNCTION(BlueprintCallable, Category = "ChamferCube")
		void RegenerateMesh();

	// 获取网格组件
	UFUNCTION(BlueprintPure, Category = "ChamferCube")
		UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMesh; }

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere)
		UProceduralMeshComponent* ProceduralMesh;

	// 内部几何生成器
	TUniquePtr<FChamferCubeBuilder> GeometryBuilder;

	// 初始化组件
	void InitializeComponents();
	
	// 应用材质
	void ApplyMaterial();
	
	// 设置碰撞
	void SetupCollision();
};

/**
 * 倒角立方体几何数据结构
 */
class FChamferCubeGeometry
{
public:
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	// 清除所有数据
	void Clear();
	
	// 验证几何数据有效性
	bool IsValid() const;
	
	// 获取顶点数量
	int32 GetVertexCount() const { return Vertices.Num(); }
	
	// 获取三角形数量
	int32 GetTriangleCount() const { return Triangles.Num() / 3; }
};

/**
 * 倒角立方体几何生成器
 */
class FChamferCubeBuilder
{
public:
	// 生成参数
	struct FBuildParameters
	{
		float Size = 100.0f;
		float ChamferSize = 10.0f;
		int32 Sections = 3;
		
		// 验证参数有效性
		bool IsValid() const;
		
		// 获取计算后的参数
		float GetHalfSize() const { return Size * 0.5f; }
		float GetInnerOffset() const { return GetHalfSize() - ChamferSize; }
	};

	// 构造函数
	FChamferCubeBuilder(const FBuildParameters& InParams);

	// 生成几何体
	bool Generate(FChamferCubeGeometry& OutGeometry);

private:
	FBuildParameters Params;
	TMap<FVector, int32> UniqueVerticesMap;

	// 核心点计算
	TArray<FVector> CalculateCorePoints() const;
	
	// 顶点管理
	int32 GetOrAddVertex(FChamferCubeGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
	int32 AddVertex(FChamferCubeGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
	
	// 几何生成
	void GenerateMainFaces(FChamferCubeGeometry& Geometry);
	void GenerateEdgeChamfers(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints);
	void GenerateCornerChamfers(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints);
	
	// 辅助函数
	void AddQuad(FChamferCubeGeometry& Geometry, int32 V1, int32 V2, int32 V3, int32 V4);
	void AddTriangle(FChamferCubeGeometry& Geometry, int32 V1, int32 V2, int32 V3);
	
	// 法线计算
	FVector CalculateTangent(const FVector& Normal) const;
};