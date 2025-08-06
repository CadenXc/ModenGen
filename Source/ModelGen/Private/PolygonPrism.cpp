#include "PolygonPrism.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h" 

// ============================================================================
// FPolygonPrismGeometry 实现
// ============================================================================

void FPolygonPrismGeometry::Clear()
{
	Vertices.Empty();
	Triangles.Empty();
	Normals.Empty();
	UV0.Empty();
	VertexColors.Empty();
	Tangents.Empty();
}

bool FPolygonPrismGeometry::IsValid() const
{
	return Vertices.Num() > 0 && 
		   Triangles.Num() > 0 && 
		   Triangles.Num() % 3 == 0 &&
		   Normals.Num() == Vertices.Num() &&
		   UV0.Num() == Vertices.Num() &&
		   Tangents.Num() == Vertices.Num();
}

// ============================================================================
// FPolygonPrismBuilder::FBuildParameters 实现
// ============================================================================

bool FPolygonPrismBuilder::FBuildParameters::IsValid() const
{
	return Sides >= 3 && Sides <= 32 &&
		   Radius > 0.0f && 
		   Height > 0.0f && 
		   ChamferSize >= 0.0f && 
		   ChamferSize < Radius &&
		   ChamferSections >= 1 && 
		   ChamferSections <= 10;
}

// ============================================================================
// FPolygonPrismBuilder 实现
// ============================================================================

FPolygonPrismBuilder::FPolygonPrismBuilder(const FBuildParameters& InParams)
	: Params(InParams)
{
}

bool FPolygonPrismBuilder::Generate(FPolygonPrismGeometry& OutGeometry)
{
	if (!Params.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid build parameters for PolygonPrism"));
		return false;
	}

	// 清除之前的几何数据
	OutGeometry.Clear();
	UniqueVerticesMap.Empty();

	// 计算核心点（如果有倒角的话）
	TArray<FVector> CorePoints;
	if (Params.ChamferSize > 0.0f)
	{
		CorePoints = CalculateCorePoints();
	}

	// 生成各个几何部分
	GenerateMainFaces(OutGeometry);
	
	// 如果有倒角，生成倒角部分
	if (Params.ChamferSize > 0.0f && CorePoints.Num() > 0)
	{
		GenerateCornerChamfers(OutGeometry, CorePoints);
	}

	return OutGeometry.IsValid();
}

TArray<FVector> FPolygonPrismBuilder::CalculateCorePoints() const
{
	const float InnerRadius = Params.GetInnerRadius();
	const float HalfHeight = Params.GetHalfHeight();
	const float ChamferSize = Params.ChamferSize;
	
	TArray<FVector> CorePoints;
	CorePoints.Reserve(Params.Sides * 2); // 上下底面各Sides个点
	
	// 生成下底面的核心点
	for (int32 i = 0; i < Params.Sides; ++i)
	{
		float Angle = 2 * PI * i / Params.Sides;
		CorePoints.Add(FVector(
			InnerRadius * FMath::Cos(Angle),  // 使用内半径
			InnerRadius * FMath::Sin(Angle),  // 使用内半径
			-HalfHeight + ChamferSize  // 与侧面下顶点对齐
		));
	}
	
	// 生成上底面的核心点
	for (int32 i = 0; i < Params.Sides; ++i)
	{
		float Angle = 2 * PI * i / Params.Sides;
		CorePoints.Add(FVector(
			InnerRadius * FMath::Cos(Angle),  // 使用内半径
			InnerRadius * FMath::Sin(Angle),  // 使用内半径
			HalfHeight - ChamferSize  // 与侧面上顶点对齐
		));
	}
	
	return CorePoints;
}

int32 FPolygonPrismBuilder::GetOrAddVertex(FPolygonPrismGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
	int32* FoundIndex = UniqueVerticesMap.Find(Pos);
	if (FoundIndex)
	{
		return *FoundIndex;
	}

	int32 NewIndex = AddVertex(Geometry, Pos, Normal, UV);
	UniqueVerticesMap.Add(Pos, NewIndex);
	return NewIndex;
}

int32 FPolygonPrismBuilder::AddVertex(FPolygonPrismGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
	Geometry.Vertices.Add(Pos);
	Geometry.Normals.Add(Normal);
	Geometry.UV0.Add(UV);
	Geometry.VertexColors.Add(FLinearColor::White);
	Geometry.Tangents.Add(FProcMeshTangent(CalculateTangent(Normal), false));

	return Geometry.Vertices.Num() - 1;
}

FVector FPolygonPrismBuilder::CalculateTangent(const FVector& Normal) const
{
	FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
	if (TangentDirection.IsNearlyZero())
	{
		TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
	}
	TangentDirection.Normalize();
	return TangentDirection;
}

void FPolygonPrismBuilder::AddQuad(FPolygonPrismGeometry& Geometry, int32 V1, int32 V2, int32 V3, int32 V4)
{
	AddTriangle(Geometry, V1, V2, V3);
	AddTriangle(Geometry, V1, V3, V4);
}

void FPolygonPrismBuilder::AddTriangle(FPolygonPrismGeometry& Geometry, int32 V1, int32 V2, int32 V3)
{
	Geometry.Triangles.Add(V1);
	Geometry.Triangles.Add(V2);
	Geometry.Triangles.Add(V3);
}

void FPolygonPrismBuilder::GenerateMainFaces(FPolygonPrismGeometry& Geometry)
{
	const float HalfHeight = Params.GetHalfHeight();
	const float ChamferSize = Params.ChamferSize;
	const float InnerRadius = Params.GetInnerRadius();

	// 有倒角时：使用内半径生成上下底面
	TArray<FVector> BottomVerts = GeneratePolygonVertices(InnerRadius, -HalfHeight, Params.Sides);
	TArray<FVector> TopVerts = GeneratePolygonVertices(InnerRadius, HalfHeight, Params.Sides);
	
	// 生成底面
	GeneratePolygonFace(Geometry, BottomVerts, FVector(0, 0, -1), false, 0.0f);
	// 生成顶面（需要反转顺序以确保法线朝外）
	GeneratePolygonFace(Geometry, TopVerts, FVector(0, 0, 1), true, 0.0f);
	// 生成侧面 - 使用标准方法，不考虑倒角
	const float Radius = Params.Radius;
	TArray<FVector> OBottomVerts = GeneratePolygonVertices(Radius, -HalfHeight + ChamferSize, Params.Sides);
	TArray<FVector> OTopVerts = GeneratePolygonVertices(Radius, HalfHeight - ChamferSize, Params.Sides);
	GeneratePolygonSides(Geometry, OBottomVerts, OTopVerts, false, 0.0f, 1.0f);
	
	// 生成边缘倒角
	GenerateEdgeChamfers(Geometry, HalfHeight, ChamferSize);
}

TArray<FVector> FPolygonPrismBuilder::GeneratePolygonVertices(float Radius, float Z, int32 NumSides) const
{
	TArray<FVector> Vertices;
	Vertices.Reserve(NumSides);

	for (int32 i = 0; i < NumSides; i++)
	{
		float Angle = 2 * PI * i / NumSides;
		Vertices.Add(FVector(
			Radius * FMath::Cos(Angle),
			Radius * FMath::Sin(Angle),
			Z
		));
	}
	return Vertices;
}

void FPolygonPrismBuilder::GeneratePolygonSides(FPolygonPrismGeometry& Geometry, const TArray<FVector>& BottomVerts, const TArray<FVector>& TopVerts, bool bReverseNormal, float UVOffsetY, float UVScaleY)
{
	const int32 NumSides = BottomVerts.Num();
	for (int32 i = 0; i < NumSides; i++)
	{
		int32 NextIndex = (i + 1) % NumSides;

		// 计算法线
		FVector Edge1 = BottomVerts[NextIndex] - BottomVerts[i];
		FVector Edge2 = TopVerts[i] - BottomVerts[i];
		FVector Normal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
		if (bReverseNormal) Normal = -Normal;

		// 添加四边形（两个三角形）
		int32 V0 = GetOrAddVertex(Geometry, BottomVerts[i], Normal,
			FVector2D(static_cast<float>(i) / NumSides, UVOffsetY));
		int32 V1 = GetOrAddVertex(Geometry, BottomVerts[NextIndex], Normal,
			FVector2D(static_cast<float>(i + 1) / NumSides, UVOffsetY));
		int32 V2 = GetOrAddVertex(Geometry, TopVerts[NextIndex], Normal,
			FVector2D(static_cast<float>(i + 1) / NumSides, UVOffsetY + UVScaleY));
		int32 V3 = GetOrAddVertex(Geometry, TopVerts[i], Normal,
			FVector2D(static_cast<float>(i) / NumSides, UVOffsetY + UVScaleY));

		if (bReverseNormal)
		{
			AddTriangle(Geometry, V0, V2, V3);
			AddTriangle(Geometry, V0, V1, V2);
		}
		else
		{
			AddTriangle(Geometry, V0, V2, V1);
			AddTriangle(Geometry, V0, V3, V2);
		}
	}
}

void FPolygonPrismBuilder::GeneratePolygonSidesWithChamfer(FPolygonPrismGeometry& Geometry, float HalfHeight, float ChamferSize)
{
	const int32 NumSides = Params.Sides;
	

	
	// 有倒角时：使用内半径生成侧面，与上下底面保持一致
	const float InnerRadius = Params.GetInnerRadius();
	const float Radius = Params.Radius;
	
	// 为每个侧面生成向内收缩的矩形
	for (int32 i = 0; i < NumSides; ++i)
	{
		// 计算当前侧面的角度
		float Angle = 2 * PI * i / NumSides;
		float NextAngle = 2 * PI * (i + 1) / NumSides;
		float MidAngle = (Angle + NextAngle) * 0.5f;
		
		// 计算侧面的中心点（在内半径上，与上下底面保持一致）
		FVector Center(
			InnerRadius * FMath::Cos(MidAngle),
			InnerRadius * FMath::Sin(MidAngle),
			0.0f
		);
		
		// 计算侧面的法线方向（从中心指向侧面）
		FVector Normal = Center.GetSafeNormal();
		
		// 计算侧面的两个方向向量
		// SizeX: 沿着多边形边缘的方向（切向）
		float SideWidth = InnerRadius * FMath::Sin(PI / NumSides) - ChamferSize;
		FVector SizeX = FVector(
			-FMath::Sin(MidAngle),
			FMath::Cos(MidAngle),
			0.0f
		) * SideWidth;
		
		// SizeY: 垂直方向（高度方向）
		FVector SizeY(0.0f, 0.0f, HalfHeight);
		
		// 生成侧面的四个顶点（类似ChamferCube的矩形）
		TArray<FVector> SideVerts;
		SideVerts.Add(Center - SizeX - SizeY); // 左下
		SideVerts.Add(Center - SizeX + SizeY); // 左上
		SideVerts.Add(Center + SizeX + SizeY); // 右上
		SideVerts.Add(Center + SizeX - SizeY); // 右下
		
		// 生成侧面的UV坐标
		TArray<FVector2D> UVs = {
			FVector2D(0, 0), // 左下
			FVector2D(0, 1), // 左上
			FVector2D(1, 1), // 右上
			FVector2D(1, 0)  // 右下
		};
		
		// 添加四个顶点
		int32 V0 = GetOrAddVertex(Geometry, SideVerts[0], Normal, UVs[0]);
		int32 V1 = GetOrAddVertex(Geometry, SideVerts[1], Normal, UVs[1]);
		int32 V2 = GetOrAddVertex(Geometry, SideVerts[2], Normal, UVs[2]);
		int32 V3 = GetOrAddVertex(Geometry, SideVerts[3], Normal, UVs[3]);
		
		// 添加四边形（两个三角形）
		AddQuad(Geometry, V0, V1, V2, V3);
	}
}

void FPolygonPrismBuilder::GeneratePolygonFace(FPolygonPrismGeometry& Geometry, const TArray<FVector>& PolygonVerts, const FVector& Normal, bool bReverseOrder, float UVOffsetZ)
{
	const int32 NumSides = PolygonVerts.Num();
	const FVector Center = FVector(0, 0, PolygonVerts[0].Z);
	
	int32 CenterIndex = GetOrAddVertex(Geometry, Center, Normal, FVector2D(0.5f, 0.5f));

	for (int32 i = 0; i < NumSides; i++)
	{
		int32 NextIndex = (i + 1) % NumSides;
		int32 V0 = GetOrAddVertex(Geometry, PolygonVerts[i], Normal, FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * i / NumSides),
				0.5f + 0.5f * FMath::Sin(2 * PI * i / NumSides)));
		int32 V1 = GetOrAddVertex(Geometry, PolygonVerts[NextIndex], Normal, FVector2D(0.5f + 0.5f * FMath::Cos(2 * PI * NextIndex / NumSides),
				0.5f + 0.5f * FMath::Sin(2 * PI * NextIndex / NumSides)));

		if (bReverseOrder)
		{
			AddTriangle(Geometry, CenterIndex, V1, V0);
		}
		else
		{
			AddTriangle(Geometry, CenterIndex, V0, V1);
		}
	}
}

// ============================================================================
// FPolygonPrismBuilder 倒角辅助函数实现
// ============================================================================







TArray<FVector> FPolygonPrismBuilder::GenerateCornerVertices(const FVector& CorePoint, const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ, int32 Lat, int32 Lon) const
{
	TArray<FVector> Vertices;
	Vertices.Reserve(1);
	
	const float LatAlpha = static_cast<float>(Lat) / Params.ChamferSections;
	const float LonAlpha = static_cast<float>(Lon) / Params.ChamferSections;

	// 使用三轴插值方法（参考 ChamferCube）
	// 计算法线方向
	FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
		AxisY * LatAlpha +
		AxisZ * LonAlpha);
	CurrentNormal.Normalize();

	// 从核心点沿法线方向扩展
	FVector CurrentPos = CorePoint + CurrentNormal * Params.ChamferSize;
	Vertices.Add(CurrentPos);
	
	return Vertices;
}

void FPolygonPrismBuilder::GenerateCornerTriangles(FPolygonPrismGeometry& Geometry, const TArray<TArray<int32>>& CornerVerticesGrid, int32 Lat, int32 Lon, bool bSpecialOrder)
{
	// 检查边界条件
	if (Lat + 1 >= CornerVerticesGrid.Num() || Lon + 1 >= CornerVerticesGrid[Lat].Num())
	{
		return; // 超出边界，不生成三角形
	}

	const int32 V00 = CornerVerticesGrid[Lat][Lon];
	const int32 V10 = CornerVerticesGrid[Lat + 1][Lon];
	const int32 V01 = CornerVerticesGrid[Lat][Lon + 1];

	// 根据特殊顺序标志决定三角形渲染顺序
	if (bSpecialOrder)
	{
		// 特殊顺序：用于顶面角落
		AddTriangle(Geometry, V00, V01, V10);
	}
	else
	{
		// 标准顺序：用于底面角落
		AddTriangle(Geometry, V00, V10, V01);
	}

	// 检查第二个三角形的边界条件
	if (Lon + 1 < CornerVerticesGrid[Lat + 1].Num())
	{
		const int32 V11 = CornerVerticesGrid[Lat + 1][Lon + 1];

		if (bSpecialOrder)
		{
			// 特殊顺序：用于顶面角落
			AddTriangle(Geometry, V10, V01, V11);
		}
		else
		{
			// 标准顺序：用于底面角落
			AddTriangle(Geometry, V10, V11, V01);
		}
	}
}

void FPolygonPrismBuilder::GenerateCornerChamfers(FPolygonPrismGeometry& Geometry, const TArray<FVector>& CorePoints)
{
	const int32 NumSides = Params.Sides;
	
	// 为每个角落生成倒角
	for (int32 CornerIndex = 0; CornerIndex < NumSides * 2; ++CornerIndex)
	{
		const FVector& CurrentCorePoint = CorePoints[CornerIndex];
		
		// 根据角落位置确定渲染顺序
		bool bSpecialCornerRenderingOrder = (CornerIndex >= NumSides); // 顶面角落使用特殊顺序

		// 计算角落的轴向 - 基于角落的实际位置
		const float Angle = 2 * PI * (CornerIndex % NumSides) / NumSides;
		const float CosAngle = FMath::Cos(Angle);
		const float SinAngle = FMath::Sin(Angle);
		
		// 计算三个正交方向作为轴向
		// 径向方向：从中心指向角落
		FVector RadialDir(CosAngle, -SinAngle, 0.0f);
		// 切向方向：垂直于径向，在水平面内
		FVector TangentialDir(-SinAngle, CosAngle, 0.0f);
		// 垂直方向：向上或向下
		FVector VerticalDir(0.0f, 0.0f, 1.0f);
		
		// 根据角落是底面还是顶面调整垂直方向
		if (CornerIndex < NumSides)
		{
			// 底面角落
			VerticalDir = FVector(0.0f, 0.0f, -1.0f);
		}
		
		// 确保三个轴向是正交的
		RadialDir.Normalize();
		TangentialDir.Normalize();
		VerticalDir.Normalize();
		
		// 使用这三个正交方向作为轴向
		// 注意：这里我们需要确保轴向的顺序与ChamferCube一致
		// 对于角落倒角，我们需要确保轴向指向正确的方向
		// 轴向应该指向角落的三个相邻面
		const FVector AxisX = RadialDir;    // 径向方向（从中心指向角落）
		const FVector AxisY = TangentialDir; // 切向方向（垂直于径向）
		const FVector AxisZ = VerticalDir;   // 垂直方向（向上或向下）

		// 创建顶点网格
		TArray<TArray<int32>> CornerVerticesGrid;
		CornerVerticesGrid.SetNum(Params.ChamferSections + 1);

		for (int32 Lat = 0; Lat <= Params.ChamferSections; ++Lat)
		{
			CornerVerticesGrid[Lat].SetNum(Params.ChamferSections + 1 - Lat);
		}

		// 生成四分之一球体的顶点
		for (int32 Lat = 0; Lat <= Params.ChamferSections; ++Lat)
		{
			for (int32 Lon = 0; Lon <= Params.ChamferSections - Lat; ++Lon)
			{
				const float LonAlpha = static_cast<float>(Lon) / Params.ChamferSections;
				const float LatAlpha = static_cast<float>(Lat) / Params.ChamferSections;

				// 使用辅助函数生成顶点
				TArray<FVector> Vertices = GenerateCornerVertices(CurrentCorePoint, AxisX, AxisY, AxisZ, Lat, Lon);
				if (Vertices.Num() > 0)
				{
					// 计算法线 - 使用三轴插值（与顶点生成一致）
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
		for (int32 Lat = 0; Lat < Params.ChamferSections; ++Lat)
		{
			for (int32 Lon = 0; Lon < Params.ChamferSections - Lat; ++Lon)
			{
				GenerateCornerTriangles(Geometry, CornerVerticesGrid, Lat, Lon, bSpecialCornerRenderingOrder);
			}
		}
	}
}

// ============================================================================
// APolygonPrism 实现
// ============================================================================

APolygonPrism::APolygonPrism()
{
	PrimaryActorTick.bCanEverTick = false;
	InitializeComponents();
}

void APolygonPrism::InitializeComponents()
{
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	RootComponent = ProceduralMesh;

	SetupCollision();
	ApplyMaterial();
}

void APolygonPrism::SetupCollision()
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

void APolygonPrism::ApplyMaterial()
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

void APolygonPrism::BeginPlay()
{
	Super::BeginPlay();
	RegenerateMesh();
}

void APolygonPrism::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RegenerateMesh();
}

void APolygonPrism::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APolygonPrism::GeneratePolygonPrism(int32 InSides, float InRadius, float InHeight, float InChamferSize, int32 InChamferSections)
{
	Sides = InSides;
	Radius = InRadius;
	Height = InHeight;
	ChamferSize = InChamferSize;
	ChamferSections = InChamferSections;
	
	RegenerateMesh();
}

void APolygonPrism::RegenerateMesh()
{
	if (!ProceduralMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
		return;
	}

	// 清除之前的网格数据
	ProceduralMesh->ClearAllMeshSections();

	// 创建几何生成器
	FPolygonPrismBuilder::FBuildParameters BuildParams;
	BuildParams.Sides = Sides;
	BuildParams.Radius = Radius;
	BuildParams.Height = Height;
	BuildParams.ChamferSize = ChamferSize;
	BuildParams.ChamferSections = ChamferSections;

	GeometryBuilder = MakeUnique<FPolygonPrismBuilder>(BuildParams);

	// 生成几何体
	FPolygonPrismGeometry Geometry;
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

		UE_LOG(LogTemp, Log, TEXT("PolygonPrism generated successfully: %d vertices, %d triangles"), 
			Geometry.GetVertexCount(), Geometry.GetTriangleCount());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to generate PolygonPrism geometry"));
	}
}

// ============================================================================
// 边缘倒角生成函数
// ============================================================================

void FPolygonPrismBuilder::GenerateEdgeChamfers(FPolygonPrismGeometry& Geometry, float HalfHeight, float ChamferSize)
{
	const int32 NumSides = Params.Sides;
	const float Radius = Params.Radius;
	
	// 为每个边缘生成倒角
	for (int32 i = 0; i < NumSides; ++i)
	{
		// 计算当前边缘的角度
		float Angle = 2 * PI * i / NumSides;
		float CosAngle = FMath::Cos(Angle);
		float SinAngle = FMath::Sin(Angle);
		
		// 生成上边缘倒角
		GenerateEdgeChamfer(Geometry, 
			FVector(Radius * CosAngle, Radius * SinAngle, HalfHeight - ChamferSize), // 上边缘起点
			FVector(Radius * CosAngle, Radius * SinAngle, HalfHeight), // 上边缘终点
			FVector(CosAngle, SinAngle, 0.0f), // 径向方向
			FVector(0.0f, 0.0f, 1.0f), // 垂直向上
			true); // 上边缘
		
		// 生成下边缘倒角
		GenerateEdgeChamfer(Geometry, 
			FVector(Radius * CosAngle, Radius * SinAngle, -HalfHeight), // 下边缘起点
			FVector(Radius * CosAngle, Radius * SinAngle, -HalfHeight + ChamferSize), // 下边缘终点
			FVector(CosAngle, SinAngle, 0.0f), // 径向方向
			FVector(0.0f, 0.0f, -1.0f), // 垂直向下
			false); // 下边缘
	}
}

void FPolygonPrismBuilder::GenerateEdgeChamfer(FPolygonPrismGeometry& Geometry, 
	const FVector& StartPoint, const FVector& EndPoint, 
	const FVector& RadialDir, const FVector& VerticalDir, bool bIsTopEdge)
{
	const int32 Sections = Params.ChamferSections;
	
	// 创建顶点网格
	TArray<TArray<int32>> EdgeVerticesGrid;
	EdgeVerticesGrid.SetNum(Sections + 1);
	
	for (int32 i = 0; i <= Sections; ++i)
	{
		EdgeVerticesGrid[i].SetNum(Sections + 1 - i);
	}
	
	// 生成边缘倒角的顶点
	for (int32 Lat = 0; Lat <= Sections; ++Lat)
	{
		for (int32 Lon = 0; Lon <= Sections - Lat; ++Lon)
		{
			const float LatAlpha = static_cast<float>(Lat) / Sections;
			const float LonAlpha = static_cast<float>(Lon) / Sections;
			
			// 计算插值位置
			FVector InterpolatedPos = FMath::Lerp(StartPoint, EndPoint, LatAlpha);
			
			// 计算法线方向（径向和垂直方向的插值）
			FVector CurrentNormal = (RadialDir * (1.0f - LatAlpha - LonAlpha) +
				RadialDir * LatAlpha +
				VerticalDir * LonAlpha);
			CurrentNormal.Normalize();
			
			// 从插值位置沿法线方向扩展
			FVector CurrentPos = InterpolatedPos + CurrentNormal * Params.ChamferSize;
			
			FVector2D UV(LonAlpha, LatAlpha);
			EdgeVerticesGrid[Lat][Lon] = GetOrAddVertex(Geometry, CurrentPos, CurrentNormal, UV);
		}
	}
	
	// 生成边缘倒角的三角形
	for (int32 Lat = 0; Lat < Sections; ++Lat)
	{
		for (int32 Lon = 0; Lon < Sections - Lat; ++Lon)
		{
			GenerateCornerTriangles(Geometry, EdgeVerticesGrid, Lat, Lon, bIsTopEdge);
		}
	}
}
