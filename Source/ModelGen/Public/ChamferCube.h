#pragma once

// Engine includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h" 

// Generated header must be the last include
#include "ChamferCube.generated.h"

// Forward declarations
class FChamferCubeGeometry;
class FChamferCubeBuilder;

/**
 * 倒角立方体生成器
 * 使用程序化网格生成倒角立方体几何体，支持可配置的大小、倒角和分段数
 */
UCLASS(BlueprintType, meta=(DisplayName = "Chamfer Cube"))
class MODELGEN_API AChamferCube : public AActor
{
	GENERATED_BODY()

public:
	//~ Begin Constructor Section
	AChamferCube();

	//~ Begin Shape Parameters
	/** 立方体的基础大小 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube|Parameters", 
		meta = (ClampMin = "1.0", ClampMax = "1000.0", UIMin = "1.0", UIMax = "1000.0", DisplayName = "Cube Size"))
	float CubeSize = 100.0f;

	/** 倒角的大小 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube|Parameters", 
		meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Chamfer Size"))
	float CubeChamferSize = 10.0f;

	/** 倒角的分段数，影响倒角的平滑度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube|Parameters", 
		meta = (ClampMin = "1", ClampMax = "10", UIMin = "1", UIMax = "10", DisplayName = "Chamfer Sections"))
	int32 ChamferSections = 3;

	//~ Begin Material Section
	/** 应用于立方体的材质 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube|Materials")
	UMaterialInterface* Material;

	//~ Begin Collision Section
	/** 是否生成碰撞体 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube|Collision")
	bool bGenerateCollision = true;

	/** 是否使用异步碰撞体烘焙 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChamferCube|Collision")
	bool bUseAsyncCooking = true;

	//~ Begin Public Methods
	/**
	 * 使用指定参数生成倒角立方体
	 * @param Size - 立方体大小
	 * @param ChamferSize - 倒角大小
	 * @param Sections - 倒角分段数
	 */
	UFUNCTION(BlueprintCallable, Category = "ChamferCube|Generation")
	void GenerateChamferedCube(float Size, float ChamferSize, int32 Sections);

	/** 重新生成网格 */
	UFUNCTION(BlueprintCallable, Category = "ChamferCube|Generation")
	void RegenerateMesh();

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

private:
	//~ Begin Components
	/** 程序化网格组件 */
	UPROPERTY(VisibleAnywhere, Category = "ChamferCube|Components")
	UProceduralMeshComponent* ProceduralMesh;

	/** 几何体生成器 */
	TUniquePtr<FChamferCubeBuilder> GeometryBuilder;

	//~ Begin Private Methods
	/** 初始化组件和默认值 */
	void InitializeComponents();
	
	/** 应用材质到网格 */
	void ApplyMaterial();
	
	/** 设置碰撞相关参数 */
	void SetupCollision();
};

/**
 * 倒角立方体几何数据结构
 * 存储生成的网格数据，包括顶点、三角形、法线等
 */
class FChamferCubeGeometry
{
public:
	//~ Begin Mesh Data
	/** 顶点位置数组 */
	TArray<FVector> Vertices;
	
	/** 三角形索引数组 */
	TArray<int32> Triangles;
	
	/** 顶点法线数组 */
	TArray<FVector> Normals;
	
	/** UV坐标数组 */
	TArray<FVector2D> UV0;
	
	/** 顶点颜色数组 */
	TArray<FLinearColor> VertexColors;
	
	/** 切线数组 */
	TArray<FProcMeshTangent> Tangents;

	//~ Begin Public Methods
	/** 清除所有几何数据 */
	void Clear();
	
	/** 验证几何数据的完整性和有效性 */
	bool IsValid() const;
	
	/** 获取顶点总数 */
	int32 GetVertexCount() const { return Vertices.Num(); }
	
	/** 获取三角形总数 */
	int32 GetTriangleCount() const { return Triangles.Num() / 3; }
};

/**
 * 倒角立方体几何生成器
 * 负责生成倒角立方体的几何数据，包括顶点、三角形、UV等
 */
class FChamferCubeBuilder
{
public:
	/**
	 * 生成参数结构体
	 * 包含控制立方体生成的所有参数
	 */
	struct FBuildParameters
	{
		/** 立方体的基础大小 */
		float Size = 100.0f;
		
		/** 倒角的大小 */
		float ChamferSize = 10.0f;
		
		/** 倒角的分段数 */
		int32 Sections = 1;
		
		/** 验证参数的有效性 */
		bool IsValid() const;
		
		/** 获取立方体的半边长 */
		float GetHalfSize() const { return Size * 0.5f; }
		
		/** 获取内部偏移量（从中心到倒角起始点的距离） */
		float GetInnerOffset() const { return GetHalfSize() - ChamferSize; }
	};

	//~ Begin Constructor
	explicit FChamferCubeBuilder(const FBuildParameters& InParams);

	//~ Begin Public Methods
	/** 生成倒角立方体的几何体 */
	bool Generate(FChamferCubeGeometry& OutGeometry);

private:
	//~ Begin Private Members
	/** 生成参数 */
	FBuildParameters Params;
	
	/** 用于顶点去重的映射表 */
	TMap<FVector, int32> UniqueVerticesMap;

	//~ Begin Core Generation Methods
	/** 计算立方体的核心点（倒角起始点） */
	TArray<FVector> CalculateCorePoints() const;
	
	/** 生成主要面 */
	void GenerateMainFaces(FChamferCubeGeometry& Geometry);
	
	/** 生成边缘倒角 */
	void GenerateEdgeChamfers(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints);
	
	/** 生成角落倒角 */
	void GenerateCornerChamfers(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints);

	//~ Begin Vertex Management
	/** 获取或添加顶点，确保顶点不重复 */
	int32 GetOrAddVertex(FChamferCubeGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
	
	/** 添加新顶点到几何体 */
	int32 AddVertex(FChamferCubeGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);

	//~ Begin Geometry Helper Methods
	/** 添加四边形（两个三角形） */
	void AddQuad(FChamferCubeGeometry& Geometry, int32 V1, int32 V2, int32 V3, int32 V4);
	
	/** 添加三角形 */
	void AddTriangle(FChamferCubeGeometry& Geometry, int32 V1, int32 V2, int32 V3);
	
	/** 生成矩形的顶点 */
	TArray<FVector> GenerateRectangleVertices(const FVector& Center, const FVector& SizeX, const FVector& SizeY) const;
	
	/** 生成四边形面 */
	void GenerateQuadSides(FChamferCubeGeometry& Geometry, const TArray<FVector>& Verts, const FVector& Normal, const TArray<FVector2D>& UVs);

	//~ Begin Chamfer Generation Methods
	/** 生成边缘倒角的顶点 */
	TArray<FVector> GenerateEdgeVertices(const FVector& CorePoint1, const FVector& CorePoint2, const FVector& Normal1, const FVector& Normal2, float Alpha) const;
	
	/** 生成边缘倒角的条带 */
	void GenerateEdgeStrip(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints, int32 Core1Idx, int32 Core2Idx, const FVector& Normal1, const FVector& Normal2);
	
	/** 生成角落倒角的顶点 */
	TArray<FVector> GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const;
	
	/** 生成角落倒角的三角形 */
	void GenerateCornerTriangles(FChamferCubeGeometry& Geometry, const TArray<TArray<int32>>& CornerVerticesGrid, int32 Lat, int32 Lon, bool bSpecialOrder);

	//~ Begin Utility Methods
	/** 计算切线向量 */
	FVector CalculateTangent(const FVector& Normal) const;
};