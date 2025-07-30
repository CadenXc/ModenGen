// ChamferCube.cpp

#include "ChamferCube.h"

// Sets default values
AChamferCube::AChamferCube()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;

    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ProceduralMesh->SetSimulatePhysics(false);

    GenerateChamferedCube(CubeSize, CubeChamferSize, ChamferSections);
}

void AChamferCube::BeginPlay()
{
    Super::BeginPlay();
}

void AChamferCube::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// Helper to add vertex with normal, UV and color, returning index
int32 AChamferCube::AddVertexInternal(TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents,
    const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    Vertices.Add(Pos);
    Normals.Add(Normal);
    UV0.Add(UV);
    VertexColors.Add(FLinearColor::White);

    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    TangentDirection.Normalize();
    Tangents.Add(FProcMeshTangent(TangentDirection, false));

    return Vertices.Num() - 1;
}

// Helper to add a quad (two triangles)
void AChamferCube::AddQuadInternal(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3, int32 V4)
{
    Triangles.Add(V1); Triangles.Add(V2); Triangles.Add(V3); // Triangle 1
    Triangles.Add(V1); Triangles.Add(V3); Triangles.Add(V4); // Triangle 2
    //Triangles.Add(V1); Triangles.Add(V3); Triangles.Add(V2); // Triangle 1: V1 -> V3 -> V2
    //Triangles.Add(V1); Triangles.Add(V4); Triangles.Add(V3); // Triangle 2: V1 -> V4 -> V3
}

// Dummy implementations for helper functions that are now integrated or changed
//void AChamferCube::GenerateMainFaces(float InnerSize, TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents, const FVector InnerCorner[8]) {}
//void AChamferCube::GenerateEdgeChamfers(float Size, float ChamferSize, int32 Sections, TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents, const FVector InnerCorner[8]) {}
//void AChamferCube::GenerateCornerChamfers(float ChamferSize, int32 Sections, TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV0, TArray<FLinearColor>& VertexColors, TArray<FProcMeshTangent>& Tangents, const FVector InnerCorner[8]) {}


// Main Generation Function
void AChamferCube::GenerateChamferedCube(float Size, float ChamferSize, int32 Sections)
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // Clear previous mesh data
    ProceduralMesh->ClearAllMeshSections();

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    // Clamp ChamferSize to prevent self-intersection or invalid geometry
    ChamferSize = FMath::Clamp(ChamferSize, 0.0f, (Size / 2.0f) - KINDA_SMALL_NUMBER);
    Sections = FMath::Max(1, Sections); // Ensure at least 1 section for proper geometry

    float HalfSize = Size / 2.0f;
    float InnerOffset = HalfSize - ChamferSize; // Distance from center to the inner plane of a face

    // Use a map to store unique vertices and their indices.
    // This is crucial for seamless connections between different parts of the mesh.
    TMap<FVector, int32> UniqueVerticesMap;

    auto GetOrAddVertex = [&](const FVector& Pos, const FVector& Normal, const FVector2D& UV) -> int32
        {
            // Simple hashing for FVector, might need custom FVector hasher for larger projects
            // For basic comparison, FVector::Equals with a tolerance is often used.
            // For TMap keys, you might need a custom hasher or use FVector as a struct if it supports hashing.
            // For now, let's use a linear search for simplicity to ensure uniqueness.
            // For production, a spatial hash or rounding positions to a grid might be better.

            int32* FoundIndex = UniqueVerticesMap.Find(Pos); // Using FVector as key for exact match
            if (FoundIndex)
            {
                return *FoundIndex;
            }

            // Add new vertex
            int32 NewIndex = AddVertexInternal(Vertices, Normals, UV0, VertexColors, Tangents, Pos, Normal, UV);
            UniqueVerticesMap.Add(Pos, NewIndex);
            return NewIndex;
        };


    // Define the 8 corner "core points" (centers of the spherical corners)
    FVector CorePoints[8];
    CorePoints[0] = FVector(-InnerOffset, -InnerOffset, -InnerOffset); // ---
    CorePoints[1] = FVector(InnerOffset, -InnerOffset, -InnerOffset); // +--
    CorePoints[2] = FVector(-InnerOffset, InnerOffset, -InnerOffset); // -+-
    CorePoints[3] = FVector(InnerOffset, InnerOffset, -InnerOffset); // ++-
    CorePoints[4] = FVector(-InnerOffset, -InnerOffset, InnerOffset); // --+
    CorePoints[5] = FVector(InnerOffset, -InnerOffset, InnerOffset); // +-+
    CorePoints[6] = FVector(-InnerOffset, InnerOffset, InnerOffset); // -++
    CorePoints[7] = FVector(InnerOffset, InnerOffset, InnerOffset); // +++


    // --- 1. Generate Vertices and Triangles for Main Faces (6 of them) ---
    // These are the large flat surfaces. Their vertices are where the chamfers begin.
    // Example: For +X face, X coord is HalfSize. Y and Z coords range from -InnerOffset to +InnerOffset.
    // Each main face is defined by 4 vertices at the intersection of the main plane and the chamfer radius.

    // +X Face
    int32 VxPyPz = GetOrAddVertex(FVector(HalfSize, InnerOffset, InnerOffset), FVector(1, 0, 0), FVector2D(0, 1));
    int32 VxPyNz = GetOrAddVertex(FVector(HalfSize, InnerOffset, -InnerOffset), FVector(1, 0, 0), FVector2D(0, 0));
    int32 VxNyNz = GetOrAddVertex(FVector(HalfSize, -InnerOffset, -InnerOffset), FVector(1, 0, 0), FVector2D(1, 0));
    int32 VxNyPz = GetOrAddVertex(FVector(HalfSize, -InnerOffset, InnerOffset), FVector(1, 0, 0), FVector2D(1, 1));
    AddQuadInternal(Triangles, VxNyNz, VxNyPz, VxPyPz, VxPyNz); // Winding for +X

    // -X Face
    int32 NxPyPz = GetOrAddVertex(FVector(-HalfSize, InnerOffset, InnerOffset), FVector(-1, 0, 0), FVector2D(1, 1));
    int32 NxPyNz = GetOrAddVertex(FVector(-HalfSize, InnerOffset, -InnerOffset), FVector(-1, 0, 0), FVector2D(1, 0));
    int32 NxNyNz = GetOrAddVertex(FVector(-HalfSize, -InnerOffset, -InnerOffset), FVector(-1, 0, 0), FVector2D(0, 0));
    int32 NxNyPz = GetOrAddVertex(FVector(-HalfSize, -InnerOffset, InnerOffset), FVector(-1, 0, 0), FVector2D(0, 1));
    AddQuadInternal(Triangles, NxNyNz, NxPyNz, NxPyPz, NxNyPz); // Winding for -X

    // +Y Face
    int32 PyPxPz = GetOrAddVertex(FVector(InnerOffset, HalfSize, InnerOffset), FVector(0, 1, 0), FVector2D(0, 1));
    int32 PyPxNz = GetOrAddVertex(FVector(InnerOffset, HalfSize, -InnerOffset), FVector(0, 1, 0), FVector2D(0, 0));
    int32 PyNxNz = GetOrAddVertex(FVector(-InnerOffset, HalfSize, -InnerOffset), FVector(0, 1, 0), FVector2D(1, 0)); // Note: typo corrected here if you copy-pasted (GetOrOrAddVertex -> GetOrAddVertex)
    int32 PyNxPz = GetOrAddVertex(FVector(-InnerOffset, HalfSize, InnerOffset), FVector(0, 1, 0), FVector2D(1, 1));
    AddQuadInternal(Triangles, PyNxNz, PyPxNz, PyPxPz, PyNxPz); // Winding for +Y

    // -Y Face
    int32 NyPxPz = GetOrAddVertex(FVector(InnerOffset, -HalfSize, InnerOffset), FVector(0, -1, 0), FVector2D(1, 1));
    int32 NyPxNz = GetOrAddVertex(FVector(InnerOffset, -HalfSize, -InnerOffset), FVector(0, -1, 0), FVector2D(1, 0));
    int32 NyNxNz = GetOrAddVertex(FVector(-InnerOffset, -HalfSize, -InnerOffset), FVector(0, -1, 0), FVector2D(0, 0));
    int32 NyNxPz = GetOrAddVertex(FVector(-InnerOffset, -HalfSize, InnerOffset), FVector(0, -1, 0), FVector2D(0, 1));
    AddQuadInternal(Triangles, NyNxNz, NyNxPz, NyPxPz, NyPxNz); // Winding for -Y

    // +Z Face
    int32 PzPxPy = GetOrAddVertex(FVector(InnerOffset, InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(1, 1));
    int32 PzNxPy = GetOrAddVertex(FVector(-InnerOffset, InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(0, 1));
    int32 PzNxNy = GetOrAddVertex(FVector(-InnerOffset, -InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(0, 0));
    int32 PzPxNy = GetOrAddVertex(FVector(InnerOffset, -InnerOffset, HalfSize), FVector(0, 0, 1), FVector2D(1, 0));
    AddQuadInternal(Triangles, PzNxNy, PzNxPy, PzPxPy, PzPxNy); // Winding for +Z

    // -Z Face
    int32 NzPxPy = GetOrAddVertex(FVector(InnerOffset, InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(1, 0));
    int32 NzNxPy = GetOrAddVertex(FVector(-InnerOffset, InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(0, 0));
    int32 NzNxNy = GetOrAddVertex(FVector(-InnerOffset, -InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(0, 1));
    int32 NzPxNy = GetOrAddVertex(FVector(InnerOffset, -InnerOffset, -HalfSize), FVector(0, 0, -1), FVector2D(1, 1));
    AddQuadInternal(Triangles, NzNxNy, NzPxNy, NzPxPy, NzNxPy); // Winding for -Z


    // --- 2. Generate Edge Chamfers (12 of them) ---
    // These are cylindrical segments that connect two main faces.
    // Each edge runs between two CorePoints and connects the respective vertices on the main faces.

    // Define edges by connecting CorePoints and their associated indices for main face vertices
    // {CorePoint1_Idx, CorePoint2_Idx, Axis1_Idx_On_CorePoint1, Axis2_Idx_On_CorePoint1}
    // Axis_Idx corresponds to {0=SphereCenter, 1=X-FaceVtx, 2=Y-FaceVtx, 3=Z-FaceVtx}
    // We need the vertices that are on the adjacent *main faces* for each edge.
    struct FEdgeChamferDef
    {
        int32 Core1Idx;
        int32 Core2Idx;
        FVector AxisDirection; // Direction from Core1 to Core2
        FVector Normal1;       // Normal of the first adjacent face
        FVector Normal2;       // Normal of the second adjacent face
        // We'll rely on the GetOrAddVertex to find the start/end points of the edge lines.
    };

    TArray<FEdgeChamferDef> EdgeDefs;

    // +X direction edges
    EdgeDefs.Add({ 0, 1, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1) }); // --X-Y-Z to +X-Y-Z
    //EdgeDefs.Add({ 2, 3, FVector(1,0,0), FVector(0,1,0), FVector(0,0,-1) }); // -X+Y-Z to +X+Y-Z
    EdgeDefs.Add({ 2, 3, FVector(1,0,0), FVector(0,0,-1), FVector(0,1,0) }); // --X-Y-Z to +X-Y-Z
    //EdgeDefs.Add({ 4, 5, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,1) }); // -X-Y+Z to +X-Y+Z
    EdgeDefs.Add({ 4, 5, FVector(1,0,0), FVector(0,0,1), FVector(0,-1,0) }); // -X-Y+Z to +X-Y+Z
    EdgeDefs.Add({ 6, 7, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1) }); // -X+Y+Z to +X+Y+Z

    // +Y direction edges
    //EdgeDefs.Add({ 0, 2, FVector(0,1,0), FVector(-1,0,0), FVector(0,0,-1) }); // -X-Y-Z to -X+Y-Z
    EdgeDefs.Add({ 0, 2, FVector(0,1,0), FVector(0,0,-1), FVector(-1,0,0) }); // -X-Y-Z to -X+Y-Z
    EdgeDefs.Add({ 1, 3, FVector(0,1,0), FVector(1,0,0), FVector(0,0,-1) }); // +X-Y-Z to +X+Y-Z
    EdgeDefs.Add({ 4, 6, FVector(0,1,0), FVector(-1,0,0), FVector(0,0,1) }); // -X-Y+Z to -X+Y+Z
    //EdgeDefs.Add({ 5, 7, FVector(0,1,0), FVector(1,0,0), FVector(0,0,1) }); // +X-Y+Z to +X+Y+Z
    EdgeDefs.Add({ 5, 7, FVector(0,1,0), FVector(0,0,1), FVector(1,0,0) }); // +X-Y+Z to +X+Y+Z

    // +Z direction edges
    EdgeDefs.Add({ 0, 4, FVector(0,0,1), FVector(-1,0,0), FVector(0,-1,0) }); // -X-Y-Z to -X-Y+Z
    //EdgeDefs.Add({ 1, 5, FVector(0,0,1), FVector(1,0,0), FVector(0,-1,0) }); // +X-Y-Z to +X-Y+Z
    EdgeDefs.Add({ 1, 5, FVector(0,0,1), FVector(0,-1,0), FVector(1,0,0) }); // +X-Y-Z to +X-Y+Z (Normals Swapped!)
    //EdgeDefs.Add({ 2, 6, FVector(0,0,1), FVector(-1,0,0), FVector(0,1,0) }); // -X+Y-Z to -X+Y+Z
    EdgeDefs.Add({ 2, 6, FVector(0,0,1), FVector(0,1,0), FVector(-1,0,0) }); // -X+Y-Z to -X+Y+Z
    EdgeDefs.Add({ 3, 7, FVector(0,0,1), FVector(1,0,0), FVector(0,1,0) }); // +X+Y-Z to +X+Y+Z


    for (const FEdgeChamferDef& EdgeDef : EdgeDefs)
    {
        // For each edge, we need to generate two "strips" of vertices that curve
        // from one main face to the other.
        TArray<int32> PrevStripStartIndices; // Stores indices of vertices at the "start" of the strip (e.g., near CorePoints[EdgeDef.Core1Idx])
        TArray<int32> CurrentStripStartIndices;

        TArray<int32> PrevStripEndIndices;   // Stores indices of vertices at the "end" of the strip (e.g., near CorePoints[EdgeDef.Core2Idx])
        TArray<int32> CurrentStripEndIndices;

        // Iterate through sections to create cylindrical segments
        for (int32 s = 0; s <= Sections; ++s)
        {
            float Alpha = (float)s / Sections; // Interpolation factor for the arc
            FVector CurrentNormal = FMath::Lerp(EdgeDef.Normal1, EdgeDef.Normal2, Alpha).GetSafeNormal();

            // Calculate start and end positions for this segment of the edge
            // These points are ChamferSize away from the respective CorePoints along the CurrentNormal
            FVector PosStart = CorePoints[EdgeDef.Core1Idx] + CurrentNormal * ChamferSize;
            FVector PosEnd = CorePoints[EdgeDef.Core2Idx] + CurrentNormal * ChamferSize;

            // UV mapping for cylindrical segment (U along arc, V along length)
            FVector2D UV1 = FVector2D(Alpha, 0.0f);
            FVector2D UV2 = FVector2D(Alpha, 1.0f);

            int32 VtxStart = GetOrAddVertex(PosStart, CurrentNormal, UV1);
            int32 VtxEnd = GetOrAddVertex(PosEnd, CurrentNormal, UV2);

            CurrentStripStartIndices.Add(VtxStart);
            CurrentStripEndIndices.Add(VtxEnd);

            if (s > 0)
            {
                // Create quads connecting the current segment to the previous one
                // Winding order is important.
                AddQuadInternal(Triangles,
                    PrevStripStartIndices[0], PrevStripEndIndices[0],
                    CurrentStripEndIndices[0], CurrentStripStartIndices[0]);
            }
            PrevStripStartIndices = CurrentStripStartIndices;
            PrevStripEndIndices = CurrentStripEndIndices;
            CurrentStripStartIndices.Empty(); // Clear for next iteration
            CurrentStripEndIndices.Empty();
        }
    }


    // --- 3. Generate Corner Chamfers (8 of them) ---
   // These are spherical sections, connecting three main faces and three edge chamfers.
   // Each corner is centered at a CorePoint.

    for (int32 i = 0; i < 8; ++i)
    {
        FVector CurrentCorePoint = CorePoints[i];

        // 判断当前是否是您要特殊处理（渲染顺序不同）的角落
        bool bSpecialCornerRenderingOrder = false; // 更名以明确其目的
        if (i == 4 || i == 7 || i == 2 || i == 1) // 对应 -x-y+z, +x+y+z, -x+y-z, +x-y-z
        {
            bSpecialCornerRenderingOrder = true;
        }

        // 定义三个轴方向的法线
        FVector AxisX = FVector(FMath::Sign(CurrentCorePoint.X), 0, 0);
        FVector AxisY = FVector(0, FMath::Sign(CurrentCorePoint.Y), 0);
        FVector AxisZ = FVector(0, 0, FMath::Sign(CurrentCorePoint.Z));

        TArray<TArray<int32>> CornerVerticesGrid;
        CornerVerticesGrid.SetNum(Sections + 1);
        for (int32 Lat = 0; Lat <= Sections; ++Lat)
        {
            CornerVerticesGrid[Lat].SetNum(Sections + 1 - Lat);
        }

        // 生成四分之一球体的顶点
        for (int32 Lat = 0; Lat <= Sections; ++Lat)
        {
            float LatAlpha = (float)Lat / Sections;
            for (int32 Lon = 0; Lon <= Sections - Lat; ++Lon)
            {
                float LonAlpha = (float)Lon / Sections;

                FVector CurrentNormal = (AxisX * (1.0f - LatAlpha - LonAlpha) + AxisY * LatAlpha + AxisZ * LonAlpha);
                CurrentNormal.Normalize();

                // *** 重要的是：这里不进行 CurrentNormal = -CurrentNormal; 的操作。***
                // 法线本身应保持指向外部，只有渲染顺序改变。

                FVector CurrentPos = CurrentCorePoint + CurrentNormal * ChamferSize;

                FVector2D UV = FVector2D(LonAlpha, LatAlpha);
                // 如果需要，这里可以根据 bSpecialCornerRenderingOrder 调整 UV 映射
                // 例如：
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
                int32 V00 = CornerVerticesGrid[Lat][Lon];
                int32 V10 = CornerVerticesGrid[Lat + 1][Lon];
                int32 V01 = CornerVerticesGrid[Lat][Lon + 1];

                // 根据是否是特殊角落，调整三角形缠绕顺序
                if (bSpecialCornerRenderingOrder)
                {
                    Triangles.Add(V00); Triangles.Add(V01); Triangles.Add(V10);
                }
                else
                {
                    Triangles.Add(V00); Triangles.Add(V10); Triangles.Add(V01); // 原始: V00, V01, V10 -> 交换 V01 和 V10
                }

                if (Lon + 1 < CornerVerticesGrid[Lat + 1].Num())
                {
                    int32 V11 = CornerVerticesGrid[Lat + 1][Lon + 1];
                    if (bSpecialCornerRenderingOrder)
                    {
                        Triangles.Add(V10); Triangles.Add(V11); Triangles.Add(V01);
                    }
                    else
                    {
                        Triangles.Add(V10); Triangles.Add(V01); Triangles.Add(V11); // 原始: V10, V11, V01 -> 交换 V11 和 V01
                    }
                }
            }
        }
    }
    // Finally, create the mesh section
    ProceduralMesh->CreateMeshSection_LinearColor(
        0,                 // Section Index
        Vertices,          // Vertex Positions
        Triangles,         // Triangle Indices
        Normals,           // Vertex Normals
        UV0,               // UV Coordinates (TexCoord0)
        VertexColors,      // Vertex Colors
        Tangents,          // Vertex Tangents
        true               // Create collision for this section
    );

    // Optional: Set a material
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