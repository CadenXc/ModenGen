#include "ChamferCube.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h" 

AChamferCube::AChamferCube()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;

    // 启用异步碰撞网格生成，提高性能
    ProceduralMesh->bUseAsyncCooking = true; 
    // 启用碰撞查询和物理
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    // 默认不模拟物理
    ProceduralMesh->SetSimulatePhysics(false);

    GenerateChamferedCube(CubeSize, CubeChamferSize, ChamferSections);

    // 设置材质
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

void AChamferCube::BeginPlay()
{
    Super::BeginPlay();

    GenerateChamferedCube(CubeSize, CubeChamferSize, ChamferSections);

}

void AChamferCube::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerateChamferedCube(CubeSize, CubeChamferSize, ChamferSections);

}
void AChamferCube::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// 辅助函数：添加顶点，并返回其在顶点数组中的索引
// 参数：/
//   Vertices: 顶点位置数组
//   Normals: 顶点法线数组
//   UV0: 顶点UV坐标数组（第0套UV）
//   VertexColors: 顶点颜色数组
//   Tangents: 顶点切线数组
//   Pos: 顶点位置
//   Normal: 顶点法线
//   UV: 顶点UV坐标
int32 AChamferCube::AddVertexInternal(TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents,
    const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    Vertices.Add(Pos);
    Normals.Add(Normal);
    UV0.Add(UV);
    //VertexColors.Add(FLinearColor::White);

    // 计算切线：通常切线垂直于法线和UpVector。
    // 如果法线和UpVector共线（例如法线朝上或朝下），则使用RightVector作为备用。
    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    if (TangentDirection.IsNearlyZero()) // 如果叉积结果接近零向量，说明法线和UpVector平行或反平行
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector); // 尝试与RightVector叉积
    }
    TangentDirection.Normalize(); // 归一化切线向量
    Tangents.Add(FProcMeshTangent(TangentDirection, false)); // 添加切线，false表示切线方向未翻转

    return Vertices.Num() - 1; // 返回新添加顶点的索引
}

// 辅助函数：添加一个四边形（由两个三角形组成）
// 此函数按照 V1-V2-V3-V4 的顺序创建四边形，以确保外向面的逆时针缠绕顺序。
// 四边形示意图：
// V1 -- V4
// |    |
// V2 -- V3
void AChamferCube::AddQuadInternal(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3, int32 V4)
{
    Triangles.Add(V1); Triangles.Add(V2); Triangles.Add(V3);
    Triangles.Add(V1); Triangles.Add(V3); Triangles.Add(V4);
}

int32 AChamferCube::GetOrAddVertex(TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
	int32* FoundIndex = UniqueVerticesMap.Find(Pos);
	if (FoundIndex)
	{
		return *FoundIndex; 
	}

	int32 NewIndex = AddVertexInternal(Vertices, Normals, UV0, VertexColors, Tangents, Pos, Normal, UV);
	UniqueVerticesMap.Add(Pos, NewIndex); 
	return NewIndex; 
}

void AChamferCube::GenerateMainFaces(TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents, TArray<int32>& Triangles, float HalfSize, float InnerOffset)
{
    // --- 1. 生成主面（6个）的顶点和三角形 ---
    // 这些是立方体的大平面表面。它们的顶点是倒角开始的地方。
    // 每个主面由4个顶点定义，这些顶点位于主平面和倒角半径的交点处。

    // +X 面 (法线: (1,0,0))
    // 这些顶点定义了 +X 面在倒角起点处的四个角点
    int32 PxPyPz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(HalfSize, InnerOffset, InnerOffset), FVector(1, 0, 0), FVector2D(0, 1));
    int32 PxPyNz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(HalfSize, InnerOffset, -InnerOffset), FVector(1, 0, 0), FVector2D(0, 0));
    int32 PxNyNz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(HalfSize, -InnerOffset, -InnerOffset), FVector(1, 0, 0), FVector2D(1, 0));
    int32 PxNyPz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(HalfSize, -InnerOffset, InnerOffset), FVector(1, 0, 0), FVector2D(1, 1));
    AddQuadInternal(Triangles, PxNyNz, PxNyPz, PxPyPz, PxPyNz);


    // -X 面 (法线: (-1,0,0))
    int32 NxPyPz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-HalfSize, InnerOffset, InnerOffset), FVector(-1, 0, 0), FVector2D(1, 1));
    int32 NxPyNz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-HalfSize, InnerOffset, -InnerOffset), FVector(-1, 0, 0), FVector2D(1, 0));
    int32 NxNyNz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-HalfSize, -InnerOffset, -InnerOffset), FVector(-1, 0, 0), FVector2D(0, 0));
    int32 NxNyPz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-HalfSize, -InnerOffset, InnerOffset), FVector(-1, 0, 0), FVector2D(0, 1));
    AddQuadInternal(Triangles, NxNyNz, NxPyNz, NxPyPz, NxNyPz);

    // +Y 面 (法线: (0,1,0))
    int32 PyPxPz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(InnerOffset, HalfSize, InnerOffset), FVector(0, 1, 0), FVector2D(0, 1));
    int32 PyPxNz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(InnerOffset, HalfSize, -InnerOffset), FVector(0, 1, 0), FVector2D(0, 0));
    int32 PyNxNz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-InnerOffset, HalfSize, -InnerOffset), FVector(0, 1, 0), FVector2D(1, 0));
    int32 PyNxPz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-InnerOffset, HalfSize, InnerOffset), FVector(0, 1, 0), FVector2D(1, 1));
    AddQuadInternal(Triangles, PyNxNz, PyPxNz, PyPxPz, PyNxPz);

    // -Y 面 (法线: (0,-1,0))
    int32 NyPxPz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(InnerOffset, -HalfSize, InnerOffset), FVector(0, -1, 0), FVector2D(1, 1));
    int32 NyPxNz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(InnerOffset, -HalfSize, -InnerOffset), FVector(0, -1, 0), FVector2D(1, 0));
    int32 NyNxNz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-InnerOffset, -HalfSize, -InnerOffset), FVector(0, -1, 0), FVector2D(0, 0));
    int32 NyNxPz = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-InnerOffset, -HalfSize, InnerOffset), FVector(0, -1, 0), FVector2D(0, 1));
    AddQuadInternal(Triangles, NyNxNz, NyNxPz, NyPxPz, NyPxNz);

    // +Z 面 (法线: (0,0,1))
    int32 PzPxPy = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(InnerOffset, InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(1, 1));
    int32 PzNxPy = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-InnerOffset, InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(0, 1));
    int32 PzNxNy = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-InnerOffset, -InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(0, 0));
    int32 PzPxNy = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(InnerOffset, -InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(1, 0));
    AddQuadInternal(Triangles, PzNxNy, PzNxPy, PzPxPy, PzPxNy);

    // -Z 面 (法线: (0,0,-1))
    int32 NzPxPy = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(InnerOffset, InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(1, 0));
    int32 NzNxPy = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-InnerOffset, InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(0, 0));
    int32 NzNxNy = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(-InnerOffset, -InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(0, 1));
    int32 NzPxNy = GetOrAddVertex(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents,
        FVector(InnerOffset, -InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(1, 1));
    AddQuadInternal(Triangles, NzNxNy, NzPxNy, NzPxPy, NzNxPy);

}

void AChamferCube::GenerateEdgeChamfers(TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents, TArray<int32>& Triangles, const TArray<FVector>& CorePoints, float ChamferSize, int32 Sections)
{
    // 定义边缘倒角数据结构
    struct FEdgeChamferDef
    {
        int32 Core1Idx;         // 第一个 CorePoint 的索引
        int32 Core2Idx;         // 第二个 CorePoint 的索引
        FVector AxisDirection;  // 边缘的方向
        FVector Normal1;        // 起始法线
        FVector Normal2;        // 结束法线
    };

    TArray<FEdgeChamferDef> EdgeDefs;

    // 填充所有12条边的定义
    // +X 方向的边缘 (平行于 X 轴)
    EdgeDefs.Add({ 0, 1, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1) }); // -Y/-Z 边缘
    EdgeDefs.Add({ 2, 3, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0) });  // +Y/-Z 边缘
    EdgeDefs.Add({ 4, 5, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0) });  // -Y/+Z 边缘
    EdgeDefs.Add({ 6, 7, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1) });   // +Y/+Z 边缘

    // +Y 方向的边缘 (平行于 Y 轴)
    EdgeDefs.Add({ 0, 2, FVector(0,1,0), FVector(0,0,-1), FVector(-1,0,0) }); // -X/-Z 边缘
    EdgeDefs.Add({ 1, 3, FVector(0,1,0), FVector(1,0,0), FVector(0,0,-1) });  // +X/-Z 边缘
    EdgeDefs.Add({ 4, 6, FVector(0,1,0), FVector(-1,0,0), FVector(0,0,1) });  // -X/+Z 边缘
    EdgeDefs.Add({ 5, 7, FVector(0,1,0), FVector(0,0,1), FVector(1,0,0) });   // +X/+Z 边缘

    // +Z 方向的边缘 (平行于 Z 轴)
    EdgeDefs.Add({ 0, 4, FVector(0,0,1), FVector(-1,0,0), FVector(0,-1,0) }); // -X/-Y 边缘
    EdgeDefs.Add({ 1, 5, FVector(0,0,1), FVector(0,-1,0), FVector(1,0,0) });  // +X/-Y 边缘
    EdgeDefs.Add({ 2, 6, FVector(0,0,1), FVector(0,1,0), FVector(-1,0,0) });  // -X/+Y 边缘
    EdgeDefs.Add({ 3, 7, FVector(0,0,1), FVector(1,0,0), FVector(0,1,0) });   // +X/+Y 边缘

    // 为每条边生成倒角
    for (const FEdgeChamferDef& EdgeDef : EdgeDefs)
    {
        TArray<int32> PrevStripStartIndices; // 存储上一段弧线起点的顶点索引
        TArray<int32> PrevStripEndIndices;   // 存储上一段弧线终点的顶点索引

        // 沿边缘分段生成顶点和三角形
        for (int32 s = 0; s <= Sections; ++s)
        {
            const float Alpha = static_cast<float>(s) / Sections; // 当前分段位置比例

            // 插值计算当前法线
            FVector CurrentNormal = FMath::Lerp(EdgeDef.Normal1, EdgeDef.Normal2, Alpha).GetSafeNormal();

            // 计算当前分段的位置
            FVector PosStart = CorePoints[EdgeDef.Core1Idx] + CurrentNormal * ChamferSize;
            FVector PosEnd = CorePoints[EdgeDef.Core2Idx] + CurrentNormal * ChamferSize;

            // 设置UV坐标
            FVector2D UV1(Alpha, 0.0f); // 起点UV
            FVector2D UV2(Alpha, 1.0f); // 终点UV

            // 添加或获取顶点
            int32 VtxStart = GetOrAddVertex(
                UniqueVerticesMap,
                Vertices, Normals, UV0, VertexColors, Tangents,
                PosStart, CurrentNormal, UV1
            );

            int32 VtxEnd = GetOrAddVertex(
                UniqueVerticesMap,
                Vertices, Normals, UV0, VertexColors, Tangents,
                PosEnd, CurrentNormal, UV2
            );

            // 从第二段开始生成四边形
            if (s > 0)
            {
                // 确保有足够的顶点形成四边形
                if (PrevStripStartIndices.Num() > 0 && PrevStripEndIndices.Num() > 0)
                {
                    // 添加四边形（两个三角形）
                    // 顶点顺序：上一个起点 -> 上一个终点 -> 当前终点 -> 当前起点
                    AddQuadInternal(
                        Triangles,
                        PrevStripStartIndices[0],
                        PrevStripEndIndices[0],
                        VtxEnd,
                        VtxStart
                    );
                }
            }

            // 保存当前分段的顶点供下一分段使用
            PrevStripStartIndices = { VtxStart };
            PrevStripEndIndices = { VtxEnd };
        }
    }
}
void AChamferCube::GenerateCornerChamfers(TMap<FVector, int32>& UniqueVerticesMap, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents, TArray<int32>& Triangles, const TArray<FVector>& CorePoints, float ChamferSize, int32 Sections)
{
    // 确保核心点数量正确
    // 硬编码了
    if (CorePoints.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid CorePoints array size. Expected 8 elements."));
        return;
    }

    // 处理每个角落（8个）
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const FVector& CurrentCorePoint = CorePoints[CornerIndex];

        // 确定此角落是否需要特殊三角形缠绕顺序
        // 原始代码中指定索引为4,7,2,1的角落需要特殊处理
        bool bSpecialCornerRenderingOrder = (CornerIndex == 4 || CornerIndex == 7 || CornerIndex == 2 || CornerIndex == 1);

        // 根据角落位置确定三个轴方向（基于坐标符号）
        const float SignX = FMath::Sign(CurrentCorePoint.X);
        const float SignY = FMath::Sign(CurrentCorePoint.Y);
        const float SignZ = FMath::Sign(CurrentCorePoint.Z);

        const FVector AxisX(SignX, 0.0f, 0.0f); // X轴方向
        const FVector AxisY(0.0f, SignY, 0.0f); // Y轴方向
        const FVector AxisZ(0.0f, 0.0f, SignZ); // Z轴方向

        // 创建顶点网格（锥形结构）
        TArray<TArray<int32>> CornerVerticesGrid;
        CornerVerticesGrid.SetNum(Sections + 1);

        // 初始化网格结构（每行顶点数递减）
        for (int32 Lat = 0; Lat <= Sections; ++Lat)
        {
            CornerVerticesGrid[Lat].SetNum(Sections + 1 - Lat);
        }

        // 生成四分之一球体的顶点
        for (int32 Lat = 0; Lat <= Sections; ++Lat)
        {
            const float LatAlpha = static_cast<float>(Lat) / Sections;

            for (int32 Lon = 0; Lon <= Sections - Lat; ++Lon)
            {
                const float LonAlpha = static_cast<float>(Lon) / Sections;

                // 计算当前法线方向（三轴插值）
                FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) +
                    AxisY * LatAlpha +
                    AxisZ * LonAlpha);
                CurrentNormal.Normalize();

                // 计算顶点位置（从核心点沿法线方向偏移）
                FVector CurrentPos = CurrentCorePoint + CurrentNormal * ChamferSize;

                // 设置UV坐标
                FVector2D UV(LonAlpha, LatAlpha);

                // 特殊角落的UV调整（如果需要）
                // 原始代码中有注释，但未实际使用
                /*
                if (bSpecialCornerRenderingOrder)
                {
                    UV.X = 1.0f - UV.X; // 翻转U坐标
                }
                */

                // 添加顶点并存储索引
                CornerVerticesGrid[Lat][Lon] = GetOrAddVertex(
                    UniqueVerticesMap,
                    Vertices, Normals, UV0, VertexColors, Tangents,
                    CurrentPos, CurrentNormal, UV
                );
            }
        }

        // 生成四分之一球体的三角形
        for (int32 Lat = 0; Lat < Sections; ++Lat)
        {
            for (int32 Lon = 0; Lon < Sections - Lat; ++Lon)
            {
                // 获取当前网格单元的四个顶点
                const int32 V00 = CornerVerticesGrid[Lat][Lon];      // 当前点
                const int32 V10 = CornerVerticesGrid[Lat + 1][Lon];  // 下方点
                const int32 V01 = CornerVerticesGrid[Lat][Lon + 1];  // 右侧点

                // 添加第一个三角形
                if (bSpecialCornerRenderingOrder)
                {
                    // 特殊角落：V00 -> V01 -> V10
                    Triangles.Add(V00);
                    Triangles.Add(V01);
                    Triangles.Add(V10);
                }
                else
                {
                    // 标准角落：V00 -> V10 -> V01
                    Triangles.Add(V00);
                    Triangles.Add(V10);
                    Triangles.Add(V01);
                }

                // 检查是否可以添加第二个三角形（形成四边形）
                if (Lon + 1 < CornerVerticesGrid[Lat + 1].Num())
                {
                    const int32 V11 = CornerVerticesGrid[Lat + 1][Lon + 1]; // 右下点

                    if (bSpecialCornerRenderingOrder)
                    {
                        // 特殊角落：V10 -> V01 -> V11
                        Triangles.Add(V10);
                        Triangles.Add(V01);
                        Triangles.Add(V11);
                    }
                    else
                    {
                        // 标准角落：V10 -> V11 -> V01
                        Triangles.Add(V10);
                        Triangles.Add(V11);
                        Triangles.Add(V01);
                    }
                }
            }
        }
    }

}


// 主要的网格生成函数
void AChamferCube::GenerateChamferedCube(float Size, float ChamferSize, int32 Sections)
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // 清除之前可能存在的网格数据
    ProceduralMesh->ClearAllMeshSections();

    // 初始化网格数据数组，这些是后面 ProceduralMesh->CreateMeshSection_LinearColor() 所需要的参数
    TArray<FVector> Vertices;      
    TArray<int32> Triangles;       
    TArray<FVector> Normals;      
    TArray<FVector2D> UV0;       
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    // 限制倒角大小，防止其过大导致几何体自相交或无效
    // 倒角大小必须小于立方体边长的一半，并留一个很小的余量 KINDA_SMALL_NUMBER
    ChamferSize = FMath::Clamp(ChamferSize, 0.0f, (Size / 2.0f) - KINDA_SMALL_NUMBER);
	// 确保至少有一个分段，避免除零或空几何体
    Sections = FMath::Max(1, Sections);

	// 立方体半长
    float HalfSize = Size / 2.0f; 
    // InnerOffset 是从中心到面内部（倒角开始处）的距离
    float InnerOffset = HalfSize - ChamferSize;

    // 使用 TMap 存储唯一的顶点及其索引。
    // 在复杂场景中，可能需要自定义 FVector 的哈希函数或对位置进行四舍五入。
    TMap<FVector, int32> UniqueVerticesMap;

    // 定义8个角落的“核心点”（球形倒角的中心）
    TArray<FVector> CorePoints;
    CorePoints.Add(FVector(-InnerOffset, -InnerOffset, -InnerOffset)); 
    CorePoints.Add(FVector(InnerOffset, -InnerOffset, -InnerOffset)); 
    CorePoints.Add(FVector(-InnerOffset, InnerOffset, -InnerOffset));
    CorePoints.Add(FVector(InnerOffset, InnerOffset, -InnerOffset));
    CorePoints.Add(FVector(-InnerOffset, -InnerOffset, InnerOffset));
    CorePoints.Add(FVector(InnerOffset, -InnerOffset, InnerOffset));
    CorePoints.Add(FVector(-InnerOffset, InnerOffset, InnerOffset));
    CorePoints.Add(FVector(InnerOffset, InnerOffset, InnerOffset));

    GenerateMainFaces( UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents, Triangles, HalfSize, InnerOffset );

    GenerateEdgeChamfers( UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents, Triangles, CorePoints, ChamferSize, Sections );

    GenerateCornerChamfers(UniqueVerticesMap, Vertices, Normals, UV0, VertexColors, Tangents, Triangles, CorePoints, ChamferSize, Sections);

    ProceduralMesh->CreateMeshSection_LinearColor( 0, Vertices, Triangles,Normals, UV0, VertexColors, Tangents, true);
}
