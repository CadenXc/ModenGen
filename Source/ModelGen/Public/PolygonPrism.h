#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h" 
#include "PolygonPrism.generated.h"

// 前向声明
struct FPolygonPrismGeometry;
class FPolygonPrismBuilder;

/**
 * 通用多边形棱柱生成器
 * 可以生成任意边数的多边形棱柱，并支持倒角
 */
UCLASS()
class MODELGEN_API APolygonPrism : public AActor
{
	GENERATED_BODY()

public:
	APolygonPrism();

	// 棱柱参数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonPrism Parameters", meta = (ClampMin = "3", ClampMax = "32"))
		int32 Sides = 4; // 多边形边数

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonPrism Parameters", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
		float Radius = 100.0f; // 多边形半径

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonPrism Parameters", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
		float Height = 200.0f; // 棱柱高度

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonPrism Parameters", meta = (ClampMin = "0.0"))
		float ChamferSize = 10.0f; // 倒角大小

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonPrism Parameters", meta = (ClampMin = "1", ClampMax = "10"))
		int32 ChamferSections = 3; // 倒角分段数

	// 材质设置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonPrism Materials")
		UMaterialInterface* Material;

	// 碰撞设置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonPrism Collision")
		bool bGenerateCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PolygonPrism Collision")
		bool bUseAsyncCooking = true;

	void GeneratePolygonPrism(int32 InSides, float InRadius, float InHeight, float InChamferSize, int32 InChamferSections);

	// 重新生成网格
	UFUNCTION(BlueprintCallable, Category = "PolygonPrism")
		void RegenerateMesh();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PolygonPrism Components", meta = (AllowPrivateAccess = "true"))
		UProceduralMeshComponent* ProceduralMesh;

	TUniquePtr<class FPolygonPrismBuilder> GeometryBuilder;

	void InitializeComponents();
	void SetupCollision();
	void ApplyMaterial();
	bool GenerateMeshInternal();
};

// ============================================================================
// 几何数据结构
// ============================================================================

/**
 * 多边形棱柱几何数据结构
 */
struct MODELGEN_API FPolygonPrismGeometry
{
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
 * 多边形棱柱几何生成器
 */
class MODELGEN_API FPolygonPrismBuilder
{
public:
	// 生成参数
	struct FBuildParameters
	{
		int32 Sides = 4;
		float Radius = 100.0f;
		float Height = 200.0f;
		float ChamferSize = 10.0f;
		int32 ChamferSections = 3;
		
		// 验证参数有效性
		bool IsValid() const;
		
		// 获取计算后的参数
		float GetHalfHeight() const { return Height * 0.5f; }
		float GetInnerRadius() const { return Radius - ChamferSize; }
	};

	// 构造函数
	FPolygonPrismBuilder(const FBuildParameters& InParams);

	// 生成几何体
	bool Generate(FPolygonPrismGeometry& OutGeometry);

private:
	FBuildParameters Params;
	TMap<FVector, int32> UniqueVerticesMap;

	// 核心点计算
	TArray<FVector> CalculateCorePoints() const;
	
	// 顶点管理
	int32 GetOrAddVertex(FPolygonPrismGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
	int32 AddVertex(FPolygonPrismGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV);
	
	// 几何生成
	void GenerateMainFaces(FPolygonPrismGeometry& Geometry);
	void GenerateCornerChamfers(FPolygonPrismGeometry& Geometry, const TArray<FVector>& CorePoints);
	
	// 辅助函数
	void AddQuad(FPolygonPrismGeometry& Geometry, int32 V1, int32 V2, int32 V3, int32 V4);
	void AddTriangle(FPolygonPrismGeometry& Geometry, int32 V1, int32 V2, int32 V3);
	
	// 几何生成辅助函数
	TArray<FVector> GeneratePolygonVertices(float Radius, float Z, int32 NumSides) const;
	void GeneratePolygonSides(FPolygonPrismGeometry& Geometry, const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal, float UVOffsetY, float UVScaleY);
	void GeneratePolygonSidesWithChamfer(FPolygonPrismGeometry& Geometry, float HalfHeight, float ChamferSize);
	void GeneratePolygonFacesWithChamfer(FPolygonPrismGeometry& Geometry, float HalfHeight, float ChamferSize);
	void GeneratePolygonFace(FPolygonPrismGeometry& Geometry, const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder, float UVOffsetZ);
	
	// 倒角生成辅助函数
	TArray<FVector> GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const;
	void GenerateCornerTriangles(FPolygonPrismGeometry& Geometry, const TArray<TArray<int32>>& CornerVerticesGrid, int32 Lat, int32 Lon, bool bSpecialOrder);
	
	// 边缘倒角生成
	void GenerateEdgeChamfers(FPolygonPrismGeometry& Geometry, float HalfHeight, float ChamferSize);
	void GenerateEdgeChamfer(FPolygonPrismGeometry& Geometry, const FVector& StartPoint, const FVector& EndPoint, 
		const FVector& RadialDir, const FVector& VerticalDir, bool bIsTopEdge);
	
	// 法线计算
	FVector CalculateTangent(const FVector& Normal) const;
};
