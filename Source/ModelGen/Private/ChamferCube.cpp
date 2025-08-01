#include "ChamferCube.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h" 

AChamferCube::AChamferCube()
{
    PrimaryActorTick.bCanEverTick = false; // 关闭 Tick 函数，因为网格是静态生成的

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh")); // 创建一个程序化网格组件
    RootComponent = ProceduralMesh; // 设置为根组件

    ProceduralMesh->bUseAsyncCooking = true; // 启用异步碰撞网格生成，提高性能
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // 启用碰撞查询和物理
    ProceduralMesh->SetSimulatePhysics(false); // 默认不模拟物理

    // 调用生成倒角立方体的函数
    GenerateChamferedCube(CubeSize, CubeChamferSize, ChamferSections);
}

void AChamferCube::BeginPlay()
{
    Super::BeginPlay();

    GenerateChamferedCube(CubeSize, CubeChamferSize, ChamferSections);
}

void AChamferCube::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// 辅助函数：添加顶点，并返回其在顶点数组中的索引
// 参数：
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
    Vertices.Add(Pos);         // 添加顶点位置
    Normals.Add(Normal);       // 添加顶点法线
    UV0.Add(UV);               // 添加顶点UV
    VertexColors.Add(FLinearColor::White); // 添加顶点颜色，默认为白色

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
    // 三角形1：V1 -> V2 -> V3 (逆时针缠绕，用于外向面)
    Triangles.Add(V1); Triangles.Add(V2); Triangles.Add(V3);
    // 三角形2：V1 -> V3 -> V4 (逆时针缠绕，用于外向面)
    Triangles.Add(V1); Triangles.Add(V3); Triangles.Add(V4);
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
    TArray<FVector> Vertices;       // 顶点位置
    TArray<int32> Triangles;        // 三角形索引
    TArray<FVector> Normals;        // 顶点法线
    TArray<FVector2D> UV0;          // 顶点UV坐标
    TArray<FLinearColor> VertexColors; // 顶点颜色
    TArray<FProcMeshTangent> Tangents; // 顶点切线

    // 限制倒角大小，防止其过大导致几何体自相交或无效
    // 倒角大小必须小于立方体边长的一半，并留一个很小的余量 KINDA_SMALL_NUMBER
    ChamferSize = FMath::Clamp(ChamferSize, 0.0f, (Size / 2.0f) - KINDA_SMALL_NUMBER);
    Sections = FMath::Max(1, Sections); // 确保至少有一个分段，避免除零或空几何体


    float HalfSize = Size / 2.0f; // 立方体半长
    // InnerOffset 是从中心到面内部（倒角开始处）的距离
    // 如果 ChamferSize 为 0，则 InnerOffset 等于 HalfSize，形成一个锐角立方体
    // 如果 ChamferSize 接近 HalfSize，则 InnerOffset 接近 0，形成一个球体
    float InnerOffset = HalfSize - ChamferSize;

    // 使用 TMap 存储唯一的顶点及其索引。
    // 这对于确保网格各个部分之间平滑连接至关重要，避免重复顶点。
    // 注意：将 FVector 直接用作 TMap 的键可能会因浮点精度问题而导致查找失败，
    // 在复杂场景中，可能需要自定义 FVector 的哈希函数或对位置进行四舍五入。
    TMap<FVector, int32> UniqueVerticesMap;

    // Lambda 表达式：获取现有顶点索引或添加新顶点
    // 检查给定位置的顶点是否已存在。如果存在，返回其索引；否则，添加新顶点并返回其索引。
    auto GetOrAddVertex = [&](const FVector& Pos, const FVector& Normal, const FVector2D& UV) -> int32
    {
        // 尝试在 UniqueVerticesMap 中查找当前位置的顶点
        int32* FoundIndex = UniqueVerticesMap.Find(Pos);
        if (FoundIndex)
        {
            return *FoundIndex; 
        }

        // 如果未找到，则添加新顶点
        int32 NewIndex = AddVertexInternal(Vertices, Normals, UV0, VertexColors, Tangents, Pos, Normal, UV);
        UniqueVerticesMap.Add(Pos, NewIndex); 
        return NewIndex; 
    };


    // 定义8个角落的“核心点”（球形倒角的中心）
    // 这些点定义了倒角立方体的内部边界。
    FVector CorePoints[8];
    CorePoints[0] = FVector(-InnerOffset, -InnerOffset, -InnerOffset); // --- (-X, -Y, -Z)
    CorePoints[1] = FVector(InnerOffset, -InnerOffset, -InnerOffset);  // +-- (+X, -Y, -Z)
    CorePoints[2] = FVector(-InnerOffset, InnerOffset, -InnerOffset);  // -+- (-X, +Y, -Z)
    CorePoints[3] = FVector(InnerOffset, InnerOffset, -InnerOffset);   // ++- (+X, +Y, -Z)
    CorePoints[4] = FVector(-InnerOffset, -InnerOffset, InnerOffset);  // --+ (-X, -Y, +Z)
    CorePoints[5] = FVector(InnerOffset, -InnerOffset, InnerOffset);   // +-+ (+X, -Y, +Z)
    CorePoints[6] = FVector(-InnerOffset, InnerOffset, InnerOffset);   // -++ (-X, +Y, +Z)
    CorePoints[7] = FVector(InnerOffset, InnerOffset, InnerOffset);    // +++ (+X, +Y, +Z)


    // --- 1. 生成主面（6个）的顶点和三角形 ---
    // 这些是立方体的大平面表面。它们的顶点是倒角开始的地方。
    // 每个主面由4个顶点定义，这些顶点位于主平面和倒角半径的交点处。

    // +X 面 (法线: (1,0,0))
    // 这些顶点定义了 +X 面在倒角起点处的四个角点
    int32 VxPyPz = GetOrAddVertex(FVector(HalfSize, InnerOffset, InnerOffset), FVector(1, 0, 0), FVector2D(0, 1));
    int32 VxPyNz = GetOrAddVertex(FVector(HalfSize, InnerOffset, -InnerOffset), FVector(1, 0, 0), FVector2D(0, 0));
    int32 VxNyNz = GetOrAddVertex(FVector(HalfSize, -InnerOffset, -InnerOffset), FVector(1, 0, 0), FVector2D(1, 0));
    int32 VxNyPz = GetOrAddVertex(FVector(HalfSize, -InnerOffset, InnerOffset), FVector(1, 0, 0), FVector2D(1, 1));
    AddQuadInternal(Triangles, VxNyNz, VxNyPz, VxPyPz, VxPyNz);

    // -X 面 (法线: (-1,0,0))
    int32 NxPyPz = GetOrAddVertex(FVector(-HalfSize, InnerOffset, InnerOffset), FVector(-1, 0, 0), FVector2D(1, 1));
    int32 NxPyNz = GetOrAddVertex(FVector(-HalfSize, InnerOffset, -InnerOffset), FVector(-1, 0, 0), FVector2D(1, 0));
    int32 NxNyNz = GetOrAddVertex(FVector(-HalfSize, -InnerOffset, -InnerOffset), FVector(-1, 0, 0), FVector2D(0, 0));
    int32 NxNyPz = GetOrAddVertex(FVector(-HalfSize, -InnerOffset, InnerOffset), FVector(-1, 0, 0), FVector2D(0, 1));
    AddQuadInternal(Triangles, NxNyNz, NxPyNz, NxPyPz, NxNyPz);

    // +Y 面 (法线: (0,1,0))
    int32 PyPxPz = GetOrAddVertex(FVector(InnerOffset, HalfSize, InnerOffset), FVector(0, 1, 0), FVector2D(0, 1));
    int32 PyPxNz = GetOrAddVertex(FVector(InnerOffset, HalfSize, -InnerOffset), FVector(0, 1, 0), FVector2D(0, 0));
    int32 PyNxNz = GetOrAddVertex(FVector(-InnerOffset, HalfSize, -InnerOffset), FVector(0, 1, 0), FVector2D(1, 0));
    int32 PyNxPz = GetOrAddVertex(FVector(-InnerOffset, HalfSize, InnerOffset), FVector(0, 1, 0), FVector2D(1, 1));
    AddQuadInternal(Triangles, PyNxNz, PyPxNz, PyPxPz, PyNxPz);

    // -Y 面 (法线: (0,-1,0))
    int32 NyPxPz = GetOrAddVertex(FVector(InnerOffset, -HalfSize, InnerOffset), FVector(0, -1, 0), FVector2D(1, 1));
    int32 NyPxNz = GetOrAddVertex(FVector(InnerOffset, -HalfSize, -InnerOffset), FVector(0, -1, 0), FVector2D(1, 0));
    int32 NyNxNz = GetOrAddVertex(FVector(-InnerOffset, -HalfSize, -InnerOffset), FVector(0, -1, 0), FVector2D(0, 0));
    int32 NyNxPz = GetOrAddVertex(FVector(-InnerOffset, -HalfSize, InnerOffset), FVector(0, -1, 0), FVector2D(0, 1));
    AddQuadInternal(Triangles, NyNxNz, NyNxPz, NyPxPz, NyPxNz);

    // +Z 面 (法线: (0,0,1))
    int32 PzPxPy = GetOrAddVertex(FVector(InnerOffset, InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(1, 1));
    int32 PzNxPy = GetOrAddVertex(FVector(-InnerOffset, InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(0, 1));
    int32 PzNxNy = GetOrAddVertex(FVector(-InnerOffset, -InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(0, 0));
    int32 PzPxNy = GetOrAddVertex(FVector(InnerOffset, -InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(1, 0));
    AddQuadInternal(Triangles, PzNxNy, PzNxPy, PzPxPy, PzPxNy);

    // -Z 面 (法线: (0,0,-1))
    int32 NzPxPy = GetOrAddVertex(FVector(InnerOffset, InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(1, 0));
    int32 NzNxPy = GetOrAddVertex(FVector(-InnerOffset, InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(0, 0));
    int32 NzNxNy = GetOrAddVertex(FVector(-InnerOffset, -InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(0, 1));
    int32 NzPxNy = GetOrAddVertex(FVector(InnerOffset, -InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(1, 1));
    AddQuadInternal(Triangles, NzNxNy, NzPxNy, NzPxPy, NzNxPy);


    // --- 2. 生成边缘倒角（12个） ---
    // 这些是圆柱形分段，连接两个主面。
    // 每条边缘倒角都在两个 CorePoints 之间延伸，并连接主面上的相应顶点。

    // 定义边缘倒角的数据结构
    struct FEdgeChamferDef
    {
        int32 Core1Idx;         // 第一个 CorePoint 的索引
        int32 Core2Idx;         // 第二个 CorePoint 的索引
        FVector AxisDirection;  // 边缘的方向（例如，对于平行于X轴的边缘，方向是(1,0,0)）
        FVector Normal1;        // 第一个相邻面的法线（弧线的起始法线）
        FVector Normal2;        // 第二个相邻面的法线（弧线的结束法线）
    };

    TArray<FEdgeChamferDef> EdgeDefs; // 存储所有边缘倒角的定义

    // +X 方向的边缘 (平行于 X 轴的边缘)
    // Core1Idx, Core2Idx, 轴向, Normal1 (弧线起点法线), Normal2 (弧线终点法线)
    EdgeDefs.Add({ 0, 1, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1) }); // -Y/-Z 边缘 (从 (-X,-Y,-Z) 到 (+X,-Y,-Z))
    EdgeDefs.Add({ 2, 3, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0) }); // +Y/-Z 边缘 (从 (-X,+Y,-Z) 到 (+X,+Y,-Z)) - 法线顺序交换，以改变弧线方向
    EdgeDefs.Add({ 4, 5, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0) }); // -Y/+Z 边缘 (从 (-X,-Y,+Z) 到 (+X,-Y,+Z)) - 法线顺序交换
    EdgeDefs.Add({ 6, 7, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1) }); // +Y/+Z 边缘 (从 (-X,+Y,+Z) 到 (+X,+Y,+Z))

    // +Y 方向的边缘 (平行于 Y 轴的边缘)
    EdgeDefs.Add({ 0, 2, FVector(0,1,0), FVector(0,0,-1), FVector(-1,0,0) }); // -X/-Z 边缘 (从 (-X,-Y,-Z) 到 (-X,+Y,-Z)) - 法线顺序交换
    EdgeDefs.Add({ 1, 3, FVector(0,1,0), FVector(1,0,0), FVector(0,0,-1) }); // +X/-Z 边缘 (从 (+X,-Y,-Z) 到 (+X,+Y,-Z))
    EdgeDefs.Add({ 4, 6, FVector(0,1,0), FVector(-1,0,0), FVector(0,0,1) }); // -X/+Z 边缘 (从 (-X,-Y,+Z) 到 (-X,+Y,+Z))
    EdgeDefs.Add({ 5, 7, FVector(0,1,0), FVector(0,0,1), FVector(1,0,0) }); // +X/+Z 边缘 (从 (+X,-Y,+Z) 到 (+X,+Y,+Z)) - 法线顺序交换

    // +Z 方向的边缘 (平行于 Z 轴的边缘)
    EdgeDefs.Add({ 0, 4, FVector(0,0,1), FVector(-1,0,0), FVector(0,-1,0) }); // -X/-Y 边缘 (从 (-X,-Y,-Z) 到 (-X,-Y,+Z))
    EdgeDefs.Add({ 1, 5, FVector(0,0,1), FVector(0,-1,0), FVector(1,0,0) }); // +X/-Y 边缘 (从 (+X,-Y,-Z) 到 (+X,-Y,+Z)) - 法线顺序交换
    EdgeDefs.Add({ 2, 6, FVector(0,0,1), FVector(0,1,0), FVector(-1,0,0) }); // -X/+Y 边缘 (从 (-X,+Y,-Z) 到 (-X,+Y,+Z)) - 法线顺序交换
    EdgeDefs.Add({ 3, 7, FVector(0,0,1), FVector(1,0,0), FVector(0,1,0) }); // +X/+Y 边缘 (从 (+X,+Y,-Z) 到 (+X,+Y,+Z))


    for (const FEdgeChamferDef& EdgeDef : EdgeDefs)
    {
        TArray<int32> PrevStripStartIndices; // 存储上一段弧线“开始”端的顶点索引
        TArray<int32> PrevStripEndIndices;   // 存储上一段弧线“结束”端的顶点索引

        // 迭代分段，创建圆柱形（弧形）分段
        for (int32 s = 0; s <= Sections; ++s)
        {
            float Alpha = (float)s / Sections; // 插值因子，从0到1，用于控制沿弧线的进度

            // 关键计算：根据 Normal1 和 Normal2 插值得到当前分段的法线。
            // GetSafeNormal() 确保法线向量是单位向量。
            // 这决定了圆柱形弧线的弯曲方向和光照。
            FVector CurrentNormal = FMath::Lerp(EdgeDef.Normal1, EdgeDef.Normal2, Alpha).GetSafeNormal();

            // 计算当前分段边缘的起点和终点位置。
            // 这些点沿着 CurrentNormal 方向，从 CorePoints 偏移 ChamferSize。
            FVector PosStart = CorePoints[EdgeDef.Core1Idx] + CurrentNormal * ChamferSize;
            FVector PosEnd = CorePoints[EdgeDef.Core2Idx] + CurrentNormal * ChamferSize;

            // UV 映射：U 坐标沿着弧线变化 (Alpha)，V 坐标沿着边缘长度变化。
            FVector2D UV1 = FVector2D(Alpha, 0.0f); // 起点 UV
            FVector2D UV2 = FVector2D(Alpha, 1.0f); // 终点 UV

            int32 VtxStart = GetOrAddVertex(PosStart, CurrentNormal, UV1);
            int32 VtxEnd = GetOrAddVertex(PosEnd, CurrentNormal, UV2);

            if (s > 0) // 从第二个分段开始，可以形成四边形（两个三角形）
            {
                // 创建连接当前分段与上一个分段的四边形。
                // AddQuadInternal 的顶点顺序 (PrevStripStart, PrevStripEnd, CurrentStripEnd, CurrentStripStart)
                // 确保了标准的（外向的）逆时针缠绕顺序。
                //
                //   PrevStripStart  --  PrevStripEnd
                //          |                |
                //          |                |
                //   CurrentStripStart -- CurrentStripEnd
                AddQuadInternal(Triangles,
                    PrevStripStartIndices[0],   // V1
                    PrevStripEndIndices[0],     // V2
                    VtxEnd,                     // V3 (CurrentStripEnd)
                    VtxStart                    // V4 (CurrentStripStart)
                );
            }

            // 将当前分段的顶点作为下一次迭代的“上一个”顶点
            PrevStripStartIndices.Empty(); // 清空，然后添加
            PrevStripStartIndices.Add(VtxStart);
            PrevStripEndIndices.Empty(); // 清空，然后添加
            PrevStripEndIndices.Add(VtxEnd);
        }
    }


    // --- 3. 生成角落倒角（8个） ---
    // 这些是球形分段，连接三个主面和三个边缘倒角。
    // 每个角落都以一个 CorePoint 为中心。

    for (int32 i = 0; i < 8; ++i)
    {
        FVector CurrentCorePoint = CorePoints[i]; // 当前处理的 CorePoint

        // 此标志控制这四个特定角落的三角形缠绕顺序是否不同于其他角落。
        // 注意：顶点法线本身仍将指向外部。
        bool bSpecialCornerRenderingOrder = false;
        if (i == 4 || i == 7 || i == 2 || i == 1) // 对应的角落 CorePoints 索引: -x-y+z, +x+y+z, -x+y-z, +x-y-z
        {
            bSpecialCornerRenderingOrder = true;
        }

        // 定义四分之一球体的三个轴方向的法线。
        // 这些方向基于当前 CorePoint 的坐标符号。
        FVector AxisX = FVector(FMath::Sign(CurrentCorePoint.X), 0, 0); // X轴方向
        FVector AxisY = FVector(0, FMath::Sign(CurrentCorePoint.Y), 0); // Y轴方向
        FVector AxisZ = FVector(0, 0, FMath::Sign(CurrentCorePoint.Z)); // Z轴方向

        // 用于存储球形补丁顶点的网格。
        TArray<TArray<int32>> CornerVerticesGrid;
        CornerVerticesGrid.SetNum(Sections + 1);
        for (int32 Lat = 0; Lat <= Sections; ++Lat)
        {
            // Tapering grid for a sphere: 每一圈的经度分段数会逐渐减少，直到顶点。
            CornerVerticesGrid[Lat].SetNum(Sections + 1 - Lat);
        }

        // 生成四分之一球体的顶点
        for (int32 Lat = 0; Lat <= Sections; ++Lat) // 类似于纬度分段
        {
            float LatAlpha = (float)Lat / Sections; // 沿“纬度”的插值因子 (0到1)
            for (int32 Lon = 0; Lon <= Sections - Lat; ++Lon) // 类似于经度分段
            {
                float LonAlpha = (float)Lon / Sections; // 沿“经度”的插值因子 (0到1)

                // 关键计算：计算球体上当前点的法线向量。
                // 这是一个三线性插值，将三个轴方向的法线根据 LatAlpha 和 LonAlpha 加权平均。
                // 1.0f - LatAlpha - LonAlpha 确保当 LatAlpha 或 LonAlpha 增加时，AxisX 的权重减少。
                // 这使得法线从一个轴（例如，对于+++角从X轴）逐渐过渡到其他轴。
                FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) + AxisY * LatAlpha + AxisZ * LonAlpha);
                CurrentNormal.Normalize(); // 归一化，确保法线是单位向量

                // 顶点位置：从 CorePoint 沿着 CurrentNormal 方向偏移 ChamferSize。
                FVector CurrentPos = CurrentCorePoint + CurrentNormal * ChamferSize;

                // UV 坐标映射：直接使用经纬度插值因子作为 UV 坐标。
                FVector2D UV = FVector2D(LonAlpha, LatAlpha);

                // 如果需要，可以在这里根据 bSpecialCornerRenderingOrder 调整 UV 映射
                // 例如，如果视觉上“翻转”了角，可能需要翻转 UV 以保持纹理一致性。
                // if (bSpecialCornerRenderingOrder)
                // {
                //     UV = FVector2D(1.0f - LonAlpha, LatAlpha); // 示例：仅反转U轴，产生镜像纹理
                // }

                CornerVerticesGrid[Lat][Lon] = GetOrAddVertex(CurrentPos, CurrentNormal, UV);
            }
        }

        // 生成四分之一球体的三角形
        for (int32 Lat = 0; Lat < Sections; ++Lat)
        {
            for (int32 Lon = 0; Lon < Sections - Lat; ++Lon)
            {
                // 获取网格上当前四边形段的四个角点顶点索引
                int32 V00 = CornerVerticesGrid[Lat][Lon];
                int32 V10 = CornerVerticesGrid[Lat + 1][Lon];
                int32 V01 = CornerVerticesGrid[Lat][Lon + 1];

                // 关键算处：根据 bSpecialCornerRenderingOrder 有条件地调整三角形缠绕顺序。
                // 这会改变渲染引擎如何识别面的“正面”。
                if (bSpecialCornerRenderingOrder)
                {
                    // 对于这些特殊角落，使用不同的缠绕顺序。
                    // 标准（外向）缠绕： V00, V01, V10
                    // 此处反转为： V00, V10, V01。这将导致面被视为“背面”，如果材质是单面渲染，则在外部不可见。
                    Triangles.Add(V00); Triangles.Add(V01); Triangles.Add(V10);
                }
                else
                {
                    // 对于其他角落，使用标准的（外向的）缠绕顺序。
                    Triangles.Add(V00); Triangles.Add(V10); Triangles.Add(V01);
                }

                // 检查是否存在第二个三角形以形成完整的四边形（不是补丁的最边缘）
                if (Lon + 1 < CornerVerticesGrid[Lat + 1].Num())
                {
                    int32 V11 = CornerVerticesGrid[Lat + 1][Lon + 1];
                    if (bSpecialCornerRenderingOrder)
                    {
                        // 对于这些特殊角落，反转第二个三角形的缠绕顺序。
                        // 标准（外向）缠绕： V10, V11, V01
                        // 此处反转为： V10, V01, V11。
                        Triangles.Add(V10); Triangles.Add(V01); Triangles.Add(V11);
                    }
                    else
                    {
                        // 对于其他角落，使用标准的（外向的）缠绕顺序。
                        Triangles.Add(V10); Triangles.Add(V11); Triangles.Add(V01);
                    }
                }
            }
        }
    }

    // 最后，使用生成的数据创建网格节
    ProceduralMesh->CreateMeshSection_LinearColor(
        0,                   // 网格节索引（此处只有一个节）
        Vertices,            // 顶点位置
        Triangles,           // 三角形索引
        Normals,             // 顶点法线
        UV0,                 // UV 坐标 (TexCoord0)
        VertexColors,        // 顶点颜色
        Tangents,            // 顶点切线
        true                 // 为此节创建碰撞（设置为 true 可进行物理模拟和碰撞检测）
    );


	ProceduralMesh->SetMaterial(0, DynamicMaterial); // 设置第一个网格节的材质
    // 可选：为生成的网格设置材质
    // 尝试查找 StarterContent 中的 M_Basic_Wall 材质
    /*
    static ConstructorHelpers::FObjectFinder<UMaterialInstanceDynamic> MaterialFinder(TEXT("Material'/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall'"));
    if (MaterialFinder.Succeeded())
    {
        ProceduralMesh->SetMaterial(0, DynamicMaterial); // 设置第一个网格节的材质
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to find material. Make sure StarterContent is enabled or provide a valid path."));
    }
    */
}
