#include "ChamferCube.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h" 

// ============================================================================
// FChamferCubeGeometry 实现
// ============================================================================

void FChamferCubeGeometry::Clear()
{
	Vertices.Empty();
	Triangles.Empty();
	Normals.Empty();
	UV0.Empty();
	VertexColors.Empty();
	Tangents.Empty();
}

bool FChamferCubeGeometry::IsValid() const
{
	return Vertices.Num() > 0 && 
		   Triangles.Num() > 0 && 
		   Triangles.Num() % 3 == 0 &&
		   Normals.Num() == Vertices.Num() &&
		   UV0.Num() == Vertices.Num() &&
		   Tangents.Num() == Vertices.Num();
}

// ============================================================================
// FChamferCubeBuilder::FBuildParameters 实现
// ============================================================================

bool FChamferCubeBuilder::FBuildParameters::IsValid() const
{
	return Size > 0.0f && 
		   ChamferSize >= 0.0f && 
		   ChamferSize < GetHalfSize() &&
		   Sections >= 1 && 
		   Sections <= 10;
}

// ============================================================================
// FChamferCubeBuilder 实现
// ============================================================================

FChamferCubeBuilder::FChamferCubeBuilder(const FBuildParameters& InParams)
	: Params(InParams)
{
}

bool FChamferCubeBuilder::Generate(FChamferCubeGeometry& OutGeometry)
{
	if (!Params.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid build parameters for ChamferCube"));
		return false;
	}

	// 清除之前的几何数据
	OutGeometry.Clear();
	UniqueVerticesMap.Empty();

	// 计算核心点
	TArray<FVector> CorePoints = CalculateCorePoints();

	// 生成各个几何部分
	GenerateMainFaces(OutGeometry);
	GenerateEdgeChamfers(OutGeometry, CorePoints);
	GenerateCornerChamfers(OutGeometry, CorePoints);

	return OutGeometry.IsValid();
}

TArray<FVector> FChamferCubeBuilder::CalculateCorePoints() const
{
	const float InnerOffset = Params.GetInnerOffset();
	
	TArray<FVector> CorePoints;
	CorePoints.Reserve(8);
	
	// 8个角落的核心点
	CorePoints.Add(FVector(-InnerOffset, -InnerOffset, -InnerOffset)); 
	CorePoints.Add(FVector(InnerOffset, -InnerOffset, -InnerOffset)); 
	CorePoints.Add(FVector(-InnerOffset, InnerOffset, -InnerOffset));
	CorePoints.Add(FVector(InnerOffset, InnerOffset, -InnerOffset));
	CorePoints.Add(FVector(-InnerOffset, -InnerOffset, InnerOffset));
	CorePoints.Add(FVector(InnerOffset, -InnerOffset, InnerOffset));
	CorePoints.Add(FVector(-InnerOffset, InnerOffset, InnerOffset));
	CorePoints.Add(FVector(InnerOffset, InnerOffset, InnerOffset));
	
	return CorePoints;
}

int32 FChamferCubeBuilder::GetOrAddVertex(FChamferCubeGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
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

int32 FChamferCubeBuilder::AddVertex(FChamferCubeGeometry& Geometry, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
	Geometry.Vertices.Add(Pos);
	Geometry.Normals.Add(Normal);
	Geometry.UV0.Add(UV);
	Geometry.VertexColors.Add(FLinearColor::White);
	Geometry.Tangents.Add(FProcMeshTangent(CalculateTangent(Normal), false));

	return Geometry.Vertices.Num() - 1;
}

FVector FChamferCubeBuilder::CalculateTangent(const FVector& Normal) const
{
	FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
	if (TangentDirection.IsNearlyZero())
	{
		TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
	}
	TangentDirection.Normalize();
	return TangentDirection;
}

void FChamferCubeBuilder::AddQuad(FChamferCubeGeometry& Geometry, int32 V1, int32 V2, int32 V3, int32 V4)
{
	AddTriangle(Geometry, V1, V2, V3);
	AddTriangle(Geometry, V1, V3, V4);
}

void FChamferCubeBuilder::AddTriangle(FChamferCubeGeometry& Geometry, int32 V1, int32 V2, int32 V3)
{
	Geometry.Triangles.Add(V1);
	Geometry.Triangles.Add(V2);
	Geometry.Triangles.Add(V3);
}

void FChamferCubeBuilder::GenerateMainFaces(FChamferCubeGeometry& Geometry)
{
	const float HalfSize = Params.GetHalfSize();
	const float InnerOffset = Params.GetInnerOffset();

	// +X 面 (法线: (1,0,0))
	int32 PxPyPz = GetOrAddVertex(Geometry, FVector(HalfSize, InnerOffset, InnerOffset), FVector(1, 0, 0), FVector2D(0, 1));
	int32 PxPyNz = GetOrAddVertex(Geometry, FVector(HalfSize, InnerOffset, -InnerOffset), FVector(1, 0, 0), FVector2D(0, 0));
	int32 PxNyNz = GetOrAddVertex(Geometry, FVector(HalfSize, -InnerOffset, -InnerOffset), FVector(1, 0, 0), FVector2D(1, 0));
	int32 PxNyPz = GetOrAddVertex(Geometry, FVector(HalfSize, -InnerOffset, InnerOffset), FVector(1, 0, 0), FVector2D(1, 1));
	AddQuad(Geometry, PxNyNz, PxNyPz, PxPyPz, PxPyNz);

	// -X 面 (法线: (-1,0,0))
	int32 NxPyPz = GetOrAddVertex(Geometry, FVector(-HalfSize, InnerOffset, InnerOffset), FVector(-1, 0, 0), FVector2D(1, 1));
	int32 NxPyNz = GetOrAddVertex(Geometry, FVector(-HalfSize, InnerOffset, -InnerOffset), FVector(-1, 0, 0), FVector2D(1, 0));
	int32 NxNyNz = GetOrAddVertex(Geometry, FVector(-HalfSize, -InnerOffset, -InnerOffset), FVector(-1, 0, 0), FVector2D(0, 0));
	int32 NxNyPz = GetOrAddVertex(Geometry, FVector(-HalfSize, -InnerOffset, InnerOffset), FVector(-1, 0, 0), FVector2D(0, 1));
	AddQuad(Geometry, NxNyNz, NxPyNz, NxPyPz, NxNyPz);

	// +Y 面 (法线: (0,1,0))
	int32 PyPxPz = GetOrAddVertex(Geometry, FVector(InnerOffset, HalfSize, InnerOffset), FVector(0, 1, 0), FVector2D(0, 1));
	int32 PyPxNz = GetOrAddVertex(Geometry, FVector(InnerOffset, HalfSize, -InnerOffset), FVector(0, 1, 0), FVector2D(0, 0));
	int32 PyNxNz = GetOrAddVertex(Geometry, FVector(-InnerOffset, HalfSize, -InnerOffset), FVector(0, 1, 0), FVector2D(1, 0));
	int32 PyNxPz = GetOrAddVertex(Geometry, FVector(-InnerOffset, HalfSize, InnerOffset), FVector(0, 1, 0), FVector2D(1, 1));
	AddQuad(Geometry, PyNxNz, PyPxNz, PyPxPz, PyNxPz);

	// -Y 面 (法线: (0,-1,0))
	int32 NyPxPz = GetOrAddVertex(Geometry, FVector(InnerOffset, -HalfSize, InnerOffset), FVector(0, -1, 0), FVector2D(1, 1));
	int32 NyPxNz = GetOrAddVertex(Geometry, FVector(InnerOffset, -HalfSize, -InnerOffset), FVector(0, -1, 0), FVector2D(1, 0));
	int32 NyNxNz = GetOrAddVertex(Geometry, FVector(-InnerOffset, -HalfSize, -InnerOffset), FVector(0, -1, 0), FVector2D(0, 0));
	int32 NyNxPz = GetOrAddVertex(Geometry, FVector(-InnerOffset, -HalfSize, InnerOffset), FVector(0, -1, 0), FVector2D(0, 1));
	AddQuad(Geometry, NyNxNz, NyNxPz, NyPxPz, NyPxNz);

	// +Z 面 (法线: (0,0,1))
	int32 PzPxPy = GetOrAddVertex(Geometry, FVector(InnerOffset, InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(1, 1));
	int32 PzNxPy = GetOrAddVertex(Geometry, FVector(-InnerOffset, InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(0, 1));
	int32 PzNxNy = GetOrAddVertex(Geometry, FVector(-InnerOffset, -InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(0, 0));
	int32 PzPxNy = GetOrAddVertex(Geometry, FVector(InnerOffset, -InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(1, 0));
	AddQuad(Geometry, PzNxNy, PzNxPy, PzPxPy, PzPxNy);

	// -Z 面 (法线: (0,0,-1))
	int32 NzPxPy = GetOrAddVertex(Geometry, FVector(InnerOffset, InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(1, 0));
	int32 NzNxPy = GetOrAddVertex(Geometry, FVector(-InnerOffset, InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(0, 0));
	int32 NzNxNy = GetOrAddVertex(Geometry, FVector(-InnerOffset, -InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(0, 1));
	int32 NzPxNy = GetOrAddVertex(Geometry, FVector(InnerOffset, -InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(1, 1));
	AddQuad(Geometry, NzNxNy, NzPxNy, NzPxPy, NzNxPy);
}

void FChamferCubeBuilder::GenerateEdgeChamfers(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints)
{
	// 边缘倒角定义结构
	struct FEdgeChamferDef
	{
		int32 Core1Idx;
		int32 Core2Idx;
		FVector AxisDirection;
		FVector Normal1;
		FVector Normal2;
	};

	TArray<FEdgeChamferDef> EdgeDefs;
	EdgeDefs.Reserve(12);

	// +X 方向的边缘
	EdgeDefs.Add({ 0, 1, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1) });
	EdgeDefs.Add({ 2, 3, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0) });
	EdgeDefs.Add({ 4, 5, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0) });
	EdgeDefs.Add({ 6, 7, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1) });

	// +Y 方向的边缘
	EdgeDefs.Add({ 0, 2, FVector(0,1,0), FVector(0,0,-1), FVector(-1,0,0) });
	EdgeDefs.Add({ 1, 3, FVector(0,1,0), FVector(1,0,0), FVector(0,0,-1) });
	EdgeDefs.Add({ 4, 6, FVector(0,1,0), FVector(-1,0,0), FVector(0,0,1) });
	EdgeDefs.Add({ 5, 7, FVector(0,1,0), FVector(0,0,1), FVector(1,0,0) });

	// +Z 方向的边缘
	EdgeDefs.Add({ 0, 4, FVector(0,0,1), FVector(-1,0,0), FVector(0,-1,0) });
	EdgeDefs.Add({ 1, 5, FVector(0,0,1), FVector(0,-1,0), FVector(1,0,0) });
	EdgeDefs.Add({ 2, 6, FVector(0,0,1), FVector(0,1,0), FVector(-1,0,0) });
	EdgeDefs.Add({ 3, 7, FVector(0,0,1), FVector(1,0,0), FVector(0,1,0) });

	// 为每条边生成倒角
	for (const FEdgeChamferDef& EdgeDef : EdgeDefs)
	{
		TArray<int32> PrevStripStartIndices;
		TArray<int32> PrevStripEndIndices;

		for (int32 s = 0; s <= Params.Sections; ++s)
		{
			const float Alpha = static_cast<float>(s) / Params.Sections;
			FVector CurrentNormal = FMath::Lerp(EdgeDef.Normal1, EdgeDef.Normal2, Alpha).GetSafeNormal();

			FVector PosStart = CorePoints[EdgeDef.Core1Idx] + CurrentNormal * Params.ChamferSize;
			FVector PosEnd = CorePoints[EdgeDef.Core2Idx] + CurrentNormal * Params.ChamferSize;

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
}

void FChamferCubeBuilder::GenerateCornerChamfers(FChamferCubeGeometry& Geometry, const TArray<FVector>& CorePoints)
{
	if (CorePoints.Num() < 8)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid CorePoints array size. Expected 8 elements."));
		return;
	}

	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector& CurrentCorePoint = CorePoints[CornerIndex];
		bool bSpecialCornerRenderingOrder = (CornerIndex == 4 || CornerIndex == 7 || CornerIndex == 2 || CornerIndex == 1);

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
			const float LatAlpha = static_cast<float>(Lat) / Params.Sections;

			for (int32 Lon = 0; Lon <= Params.Sections - Lat; ++Lon)
			{
				const float LonAlpha = static_cast<float>(Lon) / Params.Sections;

				FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
					AxisY * LatAlpha +
					AxisZ * LonAlpha);
				CurrentNormal.Normalize();

				FVector CurrentPos = CurrentCorePoint + CurrentNormal * Params.ChamferSize;
				FVector2D UV(LonAlpha, LatAlpha);

				CornerVerticesGrid[Lat][Lon] = GetOrAddVertex(Geometry, CurrentPos, CurrentNormal, UV);
			}
		}

		// 生成四分之一球体的三角形
		for (int32 Lat = 0; Lat < Params.Sections; ++Lat)
		{
			for (int32 Lon = 0; Lon < Params.Sections - Lat; ++Lon)
			{
				const int32 V00 = CornerVerticesGrid[Lat][Lon];
				const int32 V10 = CornerVerticesGrid[Lat + 1][Lon];
				const int32 V01 = CornerVerticesGrid[Lat][Lon + 1];

				if (bSpecialCornerRenderingOrder)
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

					if (bSpecialCornerRenderingOrder)
					{
						AddTriangle(Geometry, V10, V01, V11);
					}
					else
					{
						AddTriangle(Geometry, V10, V11, V01);
					}
				}
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
