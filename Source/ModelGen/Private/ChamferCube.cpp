#include "ChamferCube.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h" 

// ============================================================================
// FChamferCubeGeometry Implementation
// 几何数据结构的实现
// ============================================================================

/**
 * 清除所有几何数据
 * 在重新生成几何体或清理资源时调用
 */
void FChamferCubeGeometry::Clear()
{
	Vertices.Empty();
	Triangles.Empty();
	Normals.Empty();
	UV0.Empty();
	VertexColors.Empty();
	Tangents.Empty();
}

/**
 * 验证几何数据的完整性
 * 
 * @return bool - 如果所有必要的几何数据都存在且数量匹配则返回true
 */
bool FChamferCubeGeometry::IsValid() const
{
	// 检查是否有基本的顶点和三角形数据
	const bool bHasBasicGeometry = Vertices.Num() > 0 && Triangles.Num() > 0;
	
	// 检查三角形索引是否为3的倍数（每个三角形3个顶点）
	const bool bValidTriangleCount = Triangles.Num() % 3 == 0;
	
	// 检查所有顶点属性数组的大小是否匹配
	const bool bMatchingArraySizes = Normals.Num() == Vertices.Num() &&
								   UV0.Num() == Vertices.Num() &&
								   Tangents.Num() == Vertices.Num();
	
	return bHasBasicGeometry && bValidTriangleCount && bMatchingArraySizes;
}

// ============================================================================
// FChamferCubeBuilder::FBuildParameters Implementation
// 生成参数结构体的实现
// ============================================================================

/**
 * 验证生成参数的有效性
 * 
 * @return bool - 如果所有参数都在有效范围内则返回true
 */
bool FChamferCubeBuilder::FBuildParameters::IsValid() const
{
	// 检查基础尺寸是否有效
	const bool bValidSize = Size > 0.0f;
	
	// 检查倒角尺寸是否在有效范围内
	const bool bValidChamferSize = ChamferSize >= 0.0f && ChamferSize < GetHalfSize();
	
	// 检查分段数是否在有效范围内
	const bool bValidSections = Sections >= 1 && Sections <= 10;
	
	return bValidSize && bValidChamferSize && bValidSections;
}

// ============================================================================
// FChamferCubeBuilder Implementation
// 几何体生成器的实现
// ============================================================================

/**
 * 构造函数
 * @param InParams - 初始化生成器的参数
 */
FChamferCubeBuilder::FChamferCubeBuilder(const FBuildParameters& InParams)
	: Params(InParams)
{
}

/**
 * 生成倒角立方体的几何体
 * 
 * @param OutGeometry - 输出参数，用于存储生成的几何数据
 * @return bool - 如果生成成功则返回true
 */
bool FChamferCubeBuilder::Generate(FChamferCubeGeometry& OutGeometry)
{
	// 验证生成参数
	if (!Params.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("无效的倒角立方体生成参数"));
		return false;
	}

	// 清除之前的几何数据
	OutGeometry.Clear();
	UniqueVerticesMap.Empty();

	// 计算立方体的核心点（倒角起始点）
	TArray<FVector> CorePoints = CalculateCorePoints();

	// 按照以下顺序生成几何体：
	// 1. 生成主要面（6个面）
	// 2. 生成边缘倒角（12条边）
	// 3. 生成角落倒角（8个角）
	GenerateMainFaces(OutGeometry);
	GenerateEdgeChamfers(OutGeometry, CorePoints);
	GenerateCornerChamfers(OutGeometry, CorePoints);

	return OutGeometry.IsValid();
}

/**
 * 计算立方体的8个核心点（倒角起始点）
 * 这些点位于立方体的8个角落，但向内偏移了倒角的距离
 * 
 * @return TArray<FVector> - 包含8个核心点的数组
 */
TArray<FVector> FChamferCubeBuilder::CalculateCorePoints() const
{
	const float InnerOffset = Params.GetInnerOffset();
	
	TArray<FVector> CorePoints;
	CorePoints.Reserve(8);
	
	// 按照以下顺序添加8个角落的核心点：
	// 下面四个点（Z轴负方向）
	CorePoints.Add(FVector(-InnerOffset, -InnerOffset, -InnerOffset)); // 左后下
	CorePoints.Add(FVector(InnerOffset, -InnerOffset, -InnerOffset));  // 右后下
	CorePoints.Add(FVector(-InnerOffset, InnerOffset, -InnerOffset));  // 左前下
	CorePoints.Add(FVector(InnerOffset, InnerOffset, -InnerOffset));   // 右前下
	// 上面四个点（Z轴正方向）
	CorePoints.Add(FVector(-InnerOffset, -InnerOffset, InnerOffset));  // 左后上
	CorePoints.Add(FVector(InnerOffset, -InnerOffset, InnerOffset));   // 右后上
	CorePoints.Add(FVector(-InnerOffset, InnerOffset, InnerOffset));   // 左前上
	CorePoints.Add(FVector(InnerOffset, InnerOffset, InnerOffset));    // 右前上
	
	return CorePoints;
}

/**
 * 获取或添加顶点到几何体中
 * 使用位置作为唯一标识，避免重复顶点
 * 
 * @param Geometry - 目标几何体
 * @param Pos - 顶点位置
 * @param Normal - 顶点法线
 * @param UV - 顶点UV坐标
 * @return int32 - 顶点在几何体中的索引
 */
int32 FChamferCubeBuilder::GetOrAddVertex(FChamferCubeGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
	// 尝试查找已存在的顶点
	if (int32* FoundIndex = UniqueVerticesMap.Find(Pos))
	{
		return *FoundIndex;
	}

	// 添加新顶点并记录其索引
	const int32 NewIndex = AddVertex(Geometry, Pos, Normal, UV);
	UniqueVerticesMap.Add(Pos, NewIndex);
	return NewIndex;
}

/**
 * 添加新顶点到几何体
 * 同时添加所有必要的顶点属性（法线、UV、颜色、切线）
 * 
 * @param Geometry - 目标几何体
 * @param Pos - 顶点位置
 * @param Normal - 顶点法线
 * @param UV - 顶点UV坐标
 * @return int32 - 新顶点的索引
 */
int32 FChamferCubeBuilder::AddVertex(FChamferCubeGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
	Geometry.Vertices.Add(Pos);
	Geometry.Normals.Add(Normal);
	Geometry.UV0.Add(UV);
	Geometry.VertexColors.Add(FLinearColor::White);
	Geometry.Tangents.Add(FProcMeshTangent(CalculateTangent(Normal), false));

	return Geometry.Vertices.Num() - 1;
}

/**
 * 计算顶点的切线向量
 * 切线向量用于法线贴图的计算
 * 
 * @param Normal - 顶点法线
 * @return FVector - 计算得到的切线向量
 */
FVector FChamferCubeBuilder::CalculateTangent(const FVector& Normal) const
{
	// 首先尝试使用上向量计算切线
	FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
	
	// 如果结果接近零向量，改用右向量计算
	if (TangentDirection.IsNearlyZero())
	{
		TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
	}
	
	TangentDirection.Normalize();
	return TangentDirection;
}

/**
 * 添加四边形到几何体
 * 将四边形分解为两个三角形
 * 
 * @param Geometry - 目标几何体
 * @param V1,V2,V3,V4 - 四边形的四个顶点索引（按逆时针顺序）
 */
void FChamferCubeBuilder::AddQuad(FChamferCubeGeometry& Geometry, int32 V1, int32 V2, int32 V3, int32 V4)
{
	// 将四边形分解为两个三角形
	AddTriangle(Geometry, V1, V2, V3);  // 第一个三角形
	AddTriangle(Geometry, V1, V3, V4);  // 第二个三角形
}

/**
 * 添加三角形到几何体
 * 
 * @param Geometry - 目标几何体
 * @param V1,V2,V3 - 三角形的三个顶点索引（按逆时针顺序）
 */
void FChamferCubeBuilder::AddTriangle(FChamferCubeGeometry& Geometry, int32 V1, int32 V2, int32 V3)
{
	// 按逆时针顺序添加顶点索引
	Geometry.Triangles.Add(V1);
	Geometry.Triangles.Add(V2);
	Geometry.Triangles.Add(V3);
}

/**
 * 生成立方体的六个主要面
 * 每个面都是一个矩形，考虑了倒角的影响
 * 
 * @param Geometry - 目标几何体
 */
void FChamferCubeBuilder::GenerateMainFaces(FChamferCubeGeometry& Geometry)
{
	const float HalfSize = Params.GetHalfSize();
	const float InnerOffset = Params.GetInnerOffset();

	/**
	 * 面数据结构，用于定义每个面的属性
	 */
	struct FaceData
	{
		FVector Center;   // 面的中心点
		FVector SizeX;    // 面的X方向尺寸向量
		FVector SizeY;    // 面的Y方向尺寸向量
		FVector Normal;   // 面的法线向量
	};

	// 定义立方体的六个面
	// 每个面的定义包括：
	// 1. 面的中心点位置
	// 2. 面的两个方向向量（考虑倒角后的实际尺寸）
	// 3. 面的法线方向
	TArray<FaceData> Faces = {
		// +X面（右面）：从+X方向看，Z轴向左(SizeX)，Y轴向上(SizeY)
		{
			FVector(HalfSize, 0, 0),                    // 中心点
			FVector(0, 0, -InnerOffset),                // X方向（实际是Z轴负方向）
			FVector(0, InnerOffset, 0),                 // Y方向
			FVector(1, 0, 0)                            // 法线
		},
		// -X面（左面）：从-X方向看，Z轴向右(SizeX)，Y轴向上(SizeY)
		{
			FVector(-HalfSize, 0, 0),                   // 中心点
			FVector(0, 0, InnerOffset),                 // X方向（实际是Z轴正方向）
			FVector(0, InnerOffset, 0),                 // Y方向
			FVector(-1, 0, 0)                           // 法线
		},
		// +Y面（前面）：从+Y方向看，X轴向左，Z轴向上
		{
			FVector(0, HalfSize, 0),                    // 中心点
			FVector(-InnerOffset, 0, 0),                // X方向
			FVector(0, 0, InnerOffset),                 // Y方向（实际是Z轴）
			FVector(0, 1, 0)                            // 法线
		},
		// -Y面（后面）：从-Y方向看，X轴向右，Z轴向上
		{
			FVector(0, -HalfSize, 0),                   // 中心点
			FVector(InnerOffset, 0, 0),                 // X方向
			FVector(0, 0, InnerOffset),                 // Y方向（实际是Z轴）
			FVector(0, -1, 0)                           // 法线
		},
		// +Z面（上面）：从+Z方向看，X轴向右，Y轴向上
		{
			FVector(0, 0, HalfSize),                    // 中心点
			FVector(InnerOffset, 0, 0),                 // X方向
			FVector(0, InnerOffset, 0),                 // Y方向
			FVector(0, 0, 1)                            // 法线
		},
		// -Z面（下面）：从-Z方向看，X轴向右，Y轴向下
		{
			FVector(0, 0, -HalfSize),                   // 中心点
			FVector(InnerOffset, 0, 0),                 // X方向
			FVector(0, -InnerOffset, 0),                // Y方向
			FVector(0, 0, -1)                           // 法线
		}
	};

	// 定义标准UV坐标
	// UV坐标按照顺时针顺序定义，确保纹理正确映射
	TArray<FVector2D> UVs = {
		FVector2D(0, 0),  // 左下角
		FVector2D(0, 1),  // 左上角
		FVector2D(1, 1),  // 右上角
		FVector2D(1, 0)   // 右下角
	};

	// 为每个面生成几何体
	for (const FaceData& Face : Faces)
	{
		// 生成面的四个顶点
		TArray<FVector> FaceVerts = GenerateRectangleVertices(Face.Center, Face.SizeX, Face.SizeY);
		
		// 使用这些顶点和UV创建面的几何体
		GenerateQuadSides(Geometry, FaceVerts, Face.Normal, UVs);
	}
}

void FChamferCubeBuilder::GenerateEdgeChamfers(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints)
{
	// 边缘倒角定义结构
	struct FEdgeChamferDef
	{
		int32 Core1Idx;
		int32 Core2Idx;
		FVector Normal1;
		FVector Normal2;
	};

	// 定义所有边缘的倒角数据
	TArray<FEdgeChamferDef> EdgeDefs = {
		// +X 方向的边缘
		{ 0, 1, FVector(0,-1,0), FVector(0,0,-1) },
		{ 2, 3, FVector(0,0,-1), FVector(0,1,0) },
		{ 4, 5, FVector(0,0,1), FVector(0,-1,0) },
		{ 6, 7, FVector(0,1,0), FVector(0,0,1) },

		// +Y 方向的边缘
		{ 0, 2, FVector(0,0,-1), FVector(-1,0,0) },
		{ 1, 3, FVector(1,0,0), FVector(0,0,-1) },
		{ 4, 6, FVector(-1,0,0), FVector(0,0,1) },
		{ 5, 7, FVector(0,0,1), FVector(1,0,0) },

		// +Z 方向的边缘
		{ 0, 4, FVector(-1,0,0), FVector(0,-1,0) },
		{ 1, 5, FVector(0,-1,0), FVector(1,0,0) },
		{ 2, 6, FVector(0,1,0), FVector(-1,0,0) },
		{ 3, 7, FVector(1,0,0), FVector(0,1,0) }
	};

	// 为每条边生成倒角
	for (const FEdgeChamferDef& EdgeDef : EdgeDefs)
	{
		GenerateEdgeStrip(Geometry, CorePoints, EdgeDef.Core1Idx, EdgeDef.Core2Idx, EdgeDef.Normal1, EdgeDef.Normal2);
	}
}

void FChamferCubeBuilder::GenerateCornerChamfers(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints)
{
	if (CorePoints.Num() < 8)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid CorePoints array size. Expected 8 elements."));
		return;
	}

	// 为每个角落生成倒角
	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector& CurrentCorePoint = CorePoints[CornerIndex];
		bool bSpecialCornerRenderingOrder = (CornerIndex == 4 || CornerIndex == 7 || CornerIndex == 2 || CornerIndex == 1);

		// 计算角落的轴向
		const float SignX = FMath::Sign(CurrentCorePoint.X);
		const float SignY = FMath::Sign(CurrentCorePoint.Y);
		const float SignZ = FMath::Sign(CurrentCorePoint.Z);

		const FVector AxisX(SignX, 0.0f, 0.0f);
		const FVector AxisY(0.0f, SignY, 0.0f);
		const FVector AxisZ(0.0f, 0.0f, SignZ);

		// 创建顶点网格
		TArray<TArray<int32>> CornerVerticesGrid;
		CornerVerticesGrid.SetNum(Params.Sections + 1);

		for (int32 Lat = 0; Lat <= Params.Sections; ++Lat)
		{
			CornerVerticesGrid[Lat].SetNum(Params.Sections + 1 - Lat);
		}

		// 生成四分之一球体的顶点
		for (int32 Lat = 0; Lat <= Params.Sections; ++Lat)
		{
			for (int32 Lon = 0; Lon <= Params.Sections - Lat; ++Lon)
			{
				const float LonAlpha = static_cast<float>(Lon) / Params.Sections;
				const float LatAlpha = static_cast<float>(Lat) / Params.Sections;

				// 使用辅助函数生成顶点
				TArray<FVector> Vertices = GenerateCornerVertices(CurrentCorePoint, AxisX, AxisY, AxisZ, Lat, Lon);
				if (Vertices.Num() > 0)
				{
					FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
						AxisY * LatAlpha +
						AxisZ * LonAlpha);
					CurrentNormal.Normalize();

					FVector2D UV(LonAlpha, LatAlpha);
					CornerVerticesGrid[Lat][Lon] = GetOrAddVertex(Geometry, Vertices[0], CurrentNormal, UV);
				}
			}
		}

		// 生成四分之一球体的三角形
		for (int32 Lat = 0; Lat < Params.Sections; ++Lat)
		{
			for (int32 Lon = 0; Lon < Params.Sections - Lat; ++Lon)
			{
				GenerateCornerTriangles(Geometry, CornerVerticesGrid, Lat, Lon, bSpecialCornerRenderingOrder);
			}
		}
	}
}

// ============================================================================
// AChamferCube 实现
// ============================================================================

AChamferCube::AChamferCube()
{
	PrimaryActorTick.bCanEverTick = false;
	InitializeComponents();
}

void AChamferCube::InitializeComponents()
{
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	RootComponent = ProceduralMesh;

	SetupCollision();
	ApplyMaterial();
}

void AChamferCube::SetupCollision()
{
	if (ProceduralMesh)
	{
		ProceduralMesh->bUseAsyncCooking = bUseAsyncCooking;
		
		if (bGenerateCollision)
		{
			ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		
		ProceduralMesh->SetSimulatePhysics(false);
	}
}

void AChamferCube::ApplyMaterial()
{
	if (ProceduralMesh)
	{
		if (Material)
		{
			ProceduralMesh->SetMaterial(0, Material);
		}
		else
		{
			// 使用默认材质
			static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
			if (MaterialFinder.Succeeded())
			{
				ProceduralMesh->SetMaterial(0, MaterialFinder.Object);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to find material. Make sure StarterContent is enabled or provide a valid path."));
			}
		}
	}
}

void AChamferCube::BeginPlay()
{
	Super::BeginPlay();
	RegenerateMesh();
}

void AChamferCube::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RegenerateMesh();
}

void AChamferCube::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AChamferCube::GenerateChamferedCube(float Size, float ChamferSize, int32 Sections)
{
	CubeSize = Size;
	CubeChamferSize = ChamferSize;
	ChamferSections = Sections;
	
	RegenerateMesh();
}

void AChamferCube::RegenerateMesh()
{
	if (!ProceduralMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
		return;
	}

	// 清除之前的网格数据
	ProceduralMesh->ClearAllMeshSections();

	// 创建几何生成器
	FChamferCubeBuilder::FBuildParameters BuildParams;
	BuildParams.Size = CubeSize;
	BuildParams.ChamferSize = CubeChamferSize;
	BuildParams.Sections = ChamferSections;

	GeometryBuilder = MakeUnique<FChamferCubeBuilder>(BuildParams);

	// 生成几何体
	FChamferCubeGeometry Geometry;
	if (GeometryBuilder->Generate(Geometry))
	{
		// 创建网格段
		ProceduralMesh->CreateMeshSection_LinearColor(
			0, 
			Geometry.Vertices, 
			Geometry.Triangles,
			Geometry.Normals, 
			Geometry.UV0, 
			Geometry.VertexColors, 
			Geometry.Tangents, 
			bGenerateCollision
		);

		UE_LOG(LogTemp, Log, TEXT("ChamferCube generated successfully: %d vertices, %d triangles"), 
			Geometry.GetVertexCount(), Geometry.GetTriangleCount());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to generate ChamferCube geometry"));
	}
}

// ============================================================================
// FChamferCubeBuilder 辅助函数实现（类似 Pyramid 的辅助函数）
// ============================================================================

TArray<FVector> FChamferCubeBuilder::GenerateRectangleVertices(const FVector& Center, const FVector& SizeX, const FVector& SizeY) const
{
	TArray<FVector> Vertices;
	Vertices.Reserve(4);
	
	// 生成矩形的四个顶点（按照右手法则：从法线方向看，顶点按逆时针排列）
	// 这样在UE5的左手坐标系中会正确显示
	Vertices.Add(Center - SizeX - SizeY); // 0: 左下
	Vertices.Add(Center - SizeX + SizeY); // 1: 左上  
	Vertices.Add(Center + SizeX + SizeY); // 2: 右上
	Vertices.Add(Center + SizeX - SizeY); // 3: 右下
	
	return Vertices;
}

void FChamferCubeBuilder::GenerateQuadSides(FChamferCubeGeometry& Geometry, const TArray<FVector>& Verts, const FVector& Normal, const TArray<FVector2D>& UVs)
{
	if (Verts.Num() != 4 || UVs.Num() != 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateQuadSides: 需要4个顶点和4个UV坐标"));
		return;
	}
	
	// 添加四个顶点，每个顶点都使用相同的法线
	int32 V0 = GetOrAddVertex(Geometry, Verts[0], Normal, UVs[0]);
	int32 V1 = GetOrAddVertex(Geometry, Verts[1], Normal, UVs[1]);
	int32 V2 = GetOrAddVertex(Geometry, Verts[2], Normal, UVs[2]);
	int32 V3 = GetOrAddVertex(Geometry, Verts[3], Normal, UVs[3]);
	
	// 添加四边形（两个三角形）
	AddQuad(Geometry, V0, V1, V2, V3);
}

// ============================================================================
// FChamferCubeBuilder 倒角辅助函数实现
// ============================================================================

TArray<FVector> FChamferCubeBuilder::GenerateEdgeVertices(const FVector& CorePoint1, const FVector& CorePoint2, const FVector& Normal1, const FVector& Normal2, float Alpha) const
{
	TArray<FVector> Vertices;
	Vertices.Reserve(2);
	
	// 计算插值法线
	FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();
	
	// 生成边缘的两个顶点
	FVector PosStart = CorePoint1 + CurrentNormal * Params.ChamferSize;
	FVector PosEnd = CorePoint2 + CurrentNormal * Params.ChamferSize;
	
	Vertices.Add(PosStart);
	Vertices.Add(PosEnd);
	
	return Vertices;
}

void FChamferCubeBuilder::GenerateEdgeStrip(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints, int32 Core1Idx, int32 Core2Idx, const FVector& Normal1, const FVector& Normal2)
{
	TArray<int32> PrevStripStartIndices;
	TArray<int32> PrevStripEndIndices;

	for (int32 s = 0; s <= Params.Sections; ++s)
	{
		const float Alpha = static_cast<float>(s) / Params.Sections;
		FVector CurrentNormal = FMath::Lerp(Normal1, Normal2, Alpha).GetSafeNormal();

		FVector PosStart = CorePoints[Core1Idx] + CurrentNormal * Params.ChamferSize;
		FVector PosEnd = CorePoints[Core2Idx] + CurrentNormal * Params.ChamferSize;

		FVector2D UV1(Alpha, 0.0f);
		FVector2D UV2(Alpha, 1.0f);

		int32 VtxStart = GetOrAddVertex(Geometry, PosStart, CurrentNormal, UV1);
		int32 VtxEnd = GetOrAddVertex(Geometry, PosEnd, CurrentNormal, UV2);

		if (s > 0 && PrevStripStartIndices.Num() > 0 && PrevStripEndIndices.Num() > 0)
		{
			AddQuad(Geometry, PrevStripStartIndices[0], PrevStripEndIndices[0], VtxEnd, VtxStart);
		}

		PrevStripStartIndices = { VtxStart };
		PrevStripEndIndices = { VtxEnd };
	}
}

TArray<FVector> FChamferCubeBuilder::GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const
{
	TArray<FVector> Vertices;
	Vertices.Reserve(1);
	
	const float LatAlpha = static_cast<float>(Lat) / Params.Sections;
	const float LonAlpha = static_cast<float>(Lon) / Params.Sections;

	// 计算法线（三轴插值）
	FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
		AxisY * LatAlpha +
		AxisZ * LonAlpha);
	CurrentNormal.Normalize();

	// 计算顶点位置
	FVector CurrentPos = CorePoint + CurrentNormal * Params.ChamferSize;
	Vertices.Add(CurrentPos);
	
	return Vertices;
}

void FChamferCubeBuilder::GenerateCornerTriangles(FChamferCubeGeometry& Geometry, const TArray<TArray<int32>>& CornerVerticesGrid, int32 Lat, int32 Lon, bool bSpecialOrder)
{
	const int32 V00 = CornerVerticesGrid[Lat][Lon];
	const int32 V10 = CornerVerticesGrid[Lat + 1][Lon];
	const int32 V01 = CornerVerticesGrid[Lat][Lon + 1];

	if (bSpecialOrder)
	{
		AddTriangle(Geometry, V00, V01, V10);
	}
	else
	{
		AddTriangle(Geometry, V00, V10, V01);
	}

	if (Lon + 1 < CornerVerticesGrid[Lat + 1].Num())
	{
		const int32 V11 = CornerVerticesGrid[Lat + 1][Lon + 1];

		if (bSpecialOrder)
		{
			AddTriangle(Geometry, V10, V01, V11);
		}
		else
		{
			AddTriangle(Geometry, V10, V11, V01);
		}
	}
}