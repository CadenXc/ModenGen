void AChamferCube::GenerateChamferedCube(float Size, float ChamferSize, int32 Sections)
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    // Validate ChamferSize
    if (ChamferSize < 0.0f || ChamferSize >= Size / 2.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("ChamferSize out of valid range. Clamping."));
        ChamferSize = FMath::Clamp(ChamferSize, 0.0f, Size / 2.0f - KINDA_SMALL_NUMBER);
    }

    // Data arrays for ProceduralMeshComponent
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    // Half size of the cube (excluding chamfer)
    float HalfSize = Size / 2.0f;
    float InnerSize = HalfSize - ChamferSize; // Size of the un-chamfered part
    float OuterSize = HalfSize; // Overall half size including chamfer

    // Helper function to add a quad (two triangles)
    auto AddQuad = [&](int32 V1, int32 V2, int32 V3, int32 V4)
    {
        Triangles.Add(V1); Triangles.Add(V2); Triangles.Add(V3);
        Triangles.Add(V1); Triangles.Add(V3); Triangles.Add(V4);
    };

    // Helper function to add vertex with normal, UV and color
    auto AddVertex = [&](const FVector& Pos, const FVector& Normal, const FVector2D& UV) -> int32
    {
        Vertices.Add(Pos);
        Normals.Add(Normal);
        UV0.Add(UV);
        VertexColors.Add(FLinearColor::White); // Default white color
        Tangents.Add(FProcMeshTangent(Normal.GetSafeNormal2D().GetRotated(-90).GetSafeNormal(), false)); // Basic tangent
        return Vertices.Num() - 1;
    };


    // --- Core Vertices for the Un-chamfered Cube ---
    // These are the 8 corners if it were a regular cube, before chamfering
    // We will generate the chamfered parts by blending from these.
    FVector P[8];
    P[0] = FVector(-InnerSize, -InnerSize, -InnerSize); // FBL - Front-Bottom-Left
    P[1] = FVector( InnerSize, -InnerSize, -InnerSize); // FBR - Front-Bottom-Right
    P[2] = FVector(-InnerSize,  InnerSize, -InnerSize); // BBL - Back-Bottom-Left
    P[3] = FVector( InnerSize,  InnerSize, -InnerSize); // BBR - Back-Bottom-Right
    P[4] = FVector(-InnerSize, -InnerSize,  InnerSize); // FTL - Front-Top-Left
    P[5] = FVector( InnerSize, -InnerSize,  InnerSize); // FTR - Front-Top-Right
    P[6] = FVector(-InnerSize,  InnerSize,  InnerSize); // BTL - Back-Top-Left
    P[7] = FVector( InnerSize,  InnerSize,  InnerSize); // BTR - Back-Top-Right


    // --- Faces Generation ---
    // We'll generate 6 faces first, then the 12 edges, and finally the 8 corners.
    // Each "face" of the chamfered cube will be a central flat part.
    // A chamfered cube has 6 faces (original), 12 edge chamfers, and 8 corner chamfers.

    // Define the 6 central flat faces
    // Each face is defined by 4 vertices at InnerSize
    struct FFaceInfo
    {
        FVector Normal;
        FVector CornerA, CornerB, CornerC, CornerD; // Vertices of the flat face
        FVector2D UVOffset; // For basic UVs
    };

    TArray<FFaceInfo> Faces;
    // Front face (+X)
    Faces.Add({FVector(1,0,0), P[1], P[5], P[7], P[3], FVector2D(0,0)});
    // Back face (-X)
    Faces.Add({FVector(-1,0,0), P[2], P[6], P[4], P[0], FVector2D(1,0)});
    // Right face (+Y)
    Faces.Add({FVector(0,1,0), P[3], P[7], P[6], P[2], FVector2D(0,1)});
    // Left face (-Y)
    Faces.Add({FVector(0,-1,0), P[0], P[4], P[5], P[1], FVector2D(1,1)});
    // Top face (+Z)
    Faces.Add({FVector(0,0,1), P[4], P[6], P[7], P[5], FVector2D(0,0)});
    // Bottom face (-Z)
    Faces.Add({FVector(0,0,-1), P[0], P[1], P[3], P[2], FVector2D(0,1)});

    for (const FFaceInfo& Face : Faces)
    {
        int32 Index0 = AddVertex(Face.CornerA, Face.Normal, FVector2D(0,0) + Face.UVOffset);
        int32 Index1 = AddVertex(Face.CornerB, Face.Normal, FVector2D(1,0) + Face.UVOffset);
        int32 Index2 = AddVertex(Face.CornerC, Face.Normal, FVector2D(1,1) + Face.UVOffset);
        int32 Index3 = AddVertex(Face.CornerD, Face.Normal, FVector2D(0,1) + Face.UVOffset);
        AddQuad(Index0, Index1, Index2, Index3);
    }

    // --- Edge Chamfers ---
    // There are 12 edges. Each edge chamfer is a rounded rectangular strip.
    // We'll iterate through each axis for edges.

    // Helper function for chamfered edge vertices
    auto GetChamferedEdgeVertex = [&](const FVector& DirectionA, const FVector& DirectionB, const FVector& EdgeStart) -> FVector
    {
        // Point on the sphere with radius ChamferSize
        FVector NormalizedDirection = (DirectionA + DirectionB).GetSafeNormal();
        return EdgeStart + NormalizedDirection * ChamferSize;
    };

    // Helper function to rotate a point around an axis
    auto RotatePointAroundAxis = [&](const FVector& Point, const FVector& Axis, float AngleRad) -> FVector
    {
        FQuat Rotation = FQuat(Axis, AngleRad);
        return Rotation.RotateVector(Point);
    };

    // Edge normal generation (averaged from adjacent face normals)
    auto GetEdgeNormal = [&](const FVector& VtxPos, const FVector& EdgeDirection, const FVector& PlaneNormal1, const FVector& PlaneNormal2) -> FVector
    {
         // For chamfered edges, the normal should point outwards from the center of the chamfer
        FVector CenterChamfer = VtxPos - (PlaneNormal1 + PlaneNormal2).GetSafeNormal() * ChamferSize; // Approximate center of the arc
        return (VtxPos - CenterChamfer).GetSafeNormal();
    };

    // Define edge groups for easier iteration
    struct FEdgeInfo
    {
        int32 P_Index1, P_Index2; // Indices into the P array (InnerSize points)
        FVector Axis; // Axis along which the edge extends
        FVector Plane1Normal, Plane2Normal; // Normals of the two faces forming this edge
        FVector Plane3Normal, Plane4Normal; // Normals of the two *other* faces that meet at the corners of this edge
    };

    // Pre-calculated edge definitions (connecting P indices, axis, and adjacent face normals)
    TArray<FEdgeInfo> Edges;
    // X-axis edges
    Edges.Add({0,1, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1)}); // P0-P1 (-Y, -Z)
    Edges.Add({2,3, FVector(1,0,0), FVector(0,1,0), FVector(0,0,-1)}); // P2-P3 (+Y, -Z)
    Edges.Add({4,5, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,1)}); // P4-P5 (-Y, +Z)
    Edges.Add({6,7, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1)}); // P6-P7 (+Y, +Z)
    // Y-axis edges
    Edges.Add({0,2, FVector(0,1,0), FVector(-1,0,0), FVector(0,0,-1)}); // P0-P2 (-X, -Z)
    Edges.Add({1,3, FVector(0,1,0), FVector(1,0,0), FVector(0,0,-1)}); // P1-P3 (+X, -Z)
    Edges.Add({4,6, FVector(0,1,0), FVector(-1,0,0), FVector(0,0,1)}); // P4-P6 (-X, +Z)
    Edges.Add({5,7, FVector(0,1,0), FVector(1,0,0), FVector(0,0,1)}); // P5-P7 (+X, +Z)
    // Z-axis edges
    Edges.Add({0,4, FVector(0,0,1), FVector(-1,0,0), FVector(0,-1,0)}); // P0-P4 (-X, -Y)
    Edges.Add({1,5, FVector(0,0,1), FVector(1,0,0), FVector(0,-1,0)}); // P1-P5 (+X, -Y)
    Edges.Add({2,6, FVector(0,0,1), FVector(-1,0,0), FVector(0,1,0)}); // P2-P6 (-X, +Y)
    Edges.Add({3,7, FVector(0,0,1), FVector(1,0,0), FVector(0,1,0)}); // P3-P7 (+X, +Y)

    // Generating edge sections
    for (const FEdgeInfo& Edge : Edges)
    {
        FVector Corner1 = P[Edge.P_Index1]; // Start point of the edge (inner cube)
        FVector Corner2 = P[Edge.P_Index2]; // End point of the edge (inner cube)

        // Define the two "inner" points of the edge chamfer relative to Corner1 and Corner2
        // These points are where the chamfer starts from the flat faces
        FVector EdgeDir = (Corner2 - Corner1).GetSafeNormal();

        // Calculate perpendicular directions for the chamfer curve
        FVector Perpendicular1 = FVector::CrossProduct(EdgeDir, Edge.Plane1Normal).GetSafeNormal();
        FVector Perpendicular2 = FVector::CrossProduct(EdgeDir, Edge.Plane2Normal).GetSafeNormal();

        // Ensure Perpendicular1 and Perpendicular2 are correctly oriented for the curve
        // This is crucial to ensure the arc sweeps correctly.
        // A more robust way: use the actual corner point and the adjacent face normals to get start/end angles

        // Let's determine the true start/end vectors for the arc based on the normals
        FVector ArcStartVec = Edge.Plane1Normal;
        FVector ArcEndVec = Edge.Plane2Normal;

        // Adjust start and end vectors to be relative to the corner, for use with spherical coordinates
        FVector NormalizedCorner = Corner1.GetSafeNormal();
        FVector ChamferOrigin = NormalizedCorner * InnerSize; // Origin for the spherical corner part

        // Now, refine ArcStartVec and ArcEndVec to point FROM the InnerSize point outwards
        ArcStartVec = (Corner1 - (Corner1 - Edge.Plane1Normal * ChamferSize)).GetSafeNormal();
        ArcEndVec = (Corner1 - (Corner1 - Edge.Plane2Normal * ChamferSize)).GetSafeNormal();


        TArray<int32> EdgeVtxIndicesA; // Vertices at Corner1 side of the edge
        TArray<int32> EdgeVtxIndicesB; // Vertices at Corner2 side of the edge

        // Generate vertices along the chamfer arc
        for (int32 i = 0; i <= Sections; ++i)
        {
            float Alpha = (float)i / Sections;
            // Slerp between the two normal directions for the vertex position on the arc
            FVector CurrentNormal = FMath::Lerp(Edge.Plane1Normal, Edge.Plane2Normal, Alpha).GetSafeNormal();
            FVector CurrentPosA = Corner1 + CurrentNormal * ChamferSize;
            FVector CurrentPosB = Corner2 + CurrentNormal * ChamferSize;

            // For a chamfered edge, the normal should point along the direction from the inner corner to the current point
            FVector NormalA = (CurrentPosA - Corner1).GetSafeNormal();
            FVector NormalB = (CurrentPosB - Corner2).GetSafeNormal();

            // Simple UV mapping for the edge
            FVector2D UVA = FVector2D(Alpha, 0.0f);
            FVector2D UVB = FVector2D(Alpha, 1.0f);

            EdgeVtxIndicesA.Add(AddVertex(CurrentPosA, NormalA, UVA));
            EdgeVtxIndicesB.Add(AddVertex(CurrentPosB, NormalB, UVB));
        }

        // Create quads for the edge chamfer strip
        for (int32 i = 0; i < Sections; ++i)
        {
            int32 V0 = EdgeVtxIndicesA[i];
            int32 V1 = EdgeVtxIndicesB[i];
            int32 V2 = EdgeVtxIndicesB[i+1];
            int32 V3 = EdgeVtxIndicesA[i+1];
            AddQuad(V0, V1, V2, V3);
        }
    }

    // --- Corner Chamfers ---
    // There are 8 corners. Each corner chamfer is a rounded triangular patch.
    // This is the most complex part. A chamfered corner is a spherical patch.

    // Helper to get the normal for a corner chamfer point (radiating from the corner)
    auto GetCornerNormal = [&](const FVector& VtxPos, const FVector& CornerCenter) -> FVector
    {
        return (VtxPos - CornerCenter).GetSafeNormal();
    };

    // Define corner groups
    struct FCornerInfo
    {
        int32 P_Index; // Index into P array for the inner corner point
        FVector DirX, DirY, DirZ; // Directions to form the positive axes for this corner
    };

    TArray<FCornerInfo> Corners;
    Corners.Add({0, FVector(-1,0,0), FVector(0,-1,0), FVector(0,0,-1)}); // P0: -X,-Y,-Z
    Corners.Add({1, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1)}); // P1: +X,-Y,-Z
    Corners.Add({2, FVector(-1,0,0), FVector(0,1,0), FVector(0,0,-1)}); // P2: -X,+Y,-Z
    Corners.Add({3, FVector(1,0,0), FVector(0,1,0), FVector(0,0,-1)}); // P3: +X,+Y,-Z
    Corners.Add({4, FVector(-1,0,0), FVector(0,-1,0), FVector(0,0,1)}); // P4: -X,-Y,+Z
    Corners.Add({5, FVector(1,0,0), FVector(0,-1,0), FVector(0,0,1)}); // P5: +X,-Y,+Z
    Corners.Add({6, FVector(-1,0,0), FVector(0,1,0), FVector(0,0,1)}); // P6: -X,+Y,+Z
    Corners.Add({7, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1)}); // P7: +X,+Y,+Z

    for (const FCornerInfo& Corner : Corners)
    {
        FVector CornerCenter = P[Corner.P_Index];

        // Define the three primary normal vectors that define this corner's chamfer region
        FVector N1 = Corner.DirX; // e.g., FVector(-1,0,0) for P0
        FVector N2 = Corner.DirY; // e.g., FVector(0,-1,0) for P0
        FVector N3 = Corner.DirZ; // e.g., FVector(0,0,-1) for P0

        // Vertices forming the spherical triangle for this corner
        TArray<TArray<int32>> CornerGridIndices;
        CornerGridIndices.SetNum(Sections + 1);
        for (int32 i = 0; i <= Sections; ++i)
        {
            CornerGridIndices[i].SetNum(Sections + 1);
        }

        for (int32 i = 0; i <= Sections; ++i)
        {
            float AlphaI = (float)i / Sections;
            // Interpolate along the first two normals
            FVector BaseDir1 = FMath::Lerp(N1, N2, AlphaI).GetSafeNormal();

            for (int32 j = 0; j <= Sections; ++j)
            {
                float AlphaJ = (float)j / Sections;

                // Interpolate from the BaseDir1 towards N3
                FVector CurrentDir = FMath::Lerp(BaseDir1, N3, AlphaJ).GetSafeNormal();

                // The vertex position is offset from the corner center by ChamferSize in CurrentDir
                FVector Pos = CornerCenter + CurrentDir * ChamferSize;
                FVector Normal = GetCornerNormal(Pos, CornerCenter); // Normal points from center to surface
                FVector2D UV = FVector2D(AlphaI, AlphaJ); // Simple UV mapping for the corner

                CornerGridIndices[i][j] = AddVertex(Pos, Normal, UV);
            }
        }

        // Now, generate triangles from the grid. This creates a patch that might be more than needed for a single corner.
        // A more robust spherical corner would map a triangular region of a sphere.
        // For a "correct" chamfered corner (a quarter-sphere for 3 planes):
        // We need to generate vertices on a spherical triangle defined by the three planes.
        // Simplified approach for now: generate a patch and refine if needed.
        // A common way is to make an Octahedron sphere and cull parts.

        // Let's retry the corner generation with a clearer spherical patch approach.
        // The idea is to generate points on a sphere from the corner center,
        // bounded by the planes of the faces.

        // This is a simplified spherical corner generation.
        // For a perfectly smooth quarter-sphere corner, you'd calculate points using spherical coordinates
        // based on the three directions (e.g., -X, -Y, -Z for P0).

        // Let's assume P0 (-X,-Y,-Z) for example.
        // The three main directions are (-1,0,0), (0,-1,0), (0,0,-1)
        // The spherical patch is defined by these three vectors.

        TArray<FVector> CornerVertices;
        TArray<int32> CornerTriangles;
        TArray<FVector> CornerNormals;
        TArray<FVector2D> CornerUVs;

        // Generate a small sphere patch at the corner
        // We'll use a simplified triangulation for now.
        // You can use a more advanced spherical triangulation algorithm for perfect spheres.

        // Center vertex for the corner (if it were a pyramid, not a chamfer)
        // For a chamfered corner, the origin of the spherical part is the inner corner P[P_Index]

        // The three "poles" of the spherical corner are where the edges meet
        FVector Pole1 = CornerCenter + ChamferSize * Corner.DirX;
        FVector Pole2 = CornerCenter + ChamferSize * Corner.DirY;
        FVector Pole3 = CornerCenter + ChamferSize * Corner.DirZ;

        // Add the three pole vertices to the main list
        int32 PoleIdx1 = AddVertex(Pole1, (Pole1 - CornerCenter).GetSafeNormal(), FVector2D(0,0));
        int32 PoleIdx2 = AddVertex(Pole2, (Pole2 - CornerCenter).GetSafeNormal(), FVector2D(0.5,0));
        int32 PoleIdx3 = AddVertex(Pole3, (Pole3 - CornerCenter).GetSafeNormal(), FVector2D(1,0));

        // Connect these three poles to form the initial "triangle" of the corner
        Triangles.Add(PoleIdx1); Triangles.Add(PoleIdx2); Triangles.Add(PoleIdx3);

        // A more accurate chamfer corner would fill the space between the edge curves.
        // This requires iterating along the edge curves and connecting them to the center point,
        // or creating a spherical mesh segment.

        // Given the complexity of perfect corner chamfers using procedural mesh directly,
        // let's simplify for now and ensure the core cube and edges work.
        // A robust chamfered corner requires generating a triangular patch of a sphere.
        // For now, we'll try to connect the existing edge vertices to approximate it.

        // This part is conceptually the most difficult for a completely procedural mesh from scratch.
        // A common simplification for a "rounded" box is to blend normals,
        // or to use a library that handles boolean operations or constructive solid geometry.

        // Let's refine the corner logic by connecting to the *already generated* edge chamfers.
        // This implies we need to be careful with vertex re-use and indexing.
        // This is advanced, so let's try a simpler approach for the corner for now:
        // Just connecting the three points (from the flat faces) that meet at this corner.
        // This will make it a flat triangular cut, not a rounded corner.
        // For a true chamfered corner (sphere segment), the math is significantly more complex
        // involving spherical coordinates or more advanced surface generation.

        // Let's re-think the edge and corner strategy for simplicity and correctness.

        // --- Simplified Approach for Chamfering (Common in Procedural Geometry) ---
        // Instead of separate Face/Edge/Corner generation, it's often easier to define
        // a single set of 'control points' and then 'round' them.
        // Or, for chamfering:
        // 1. Generate the initial cube's 8 inner corners (P[0]...P[7]).
        // 2. For each corner, project points outwards along its 3 axes by ChamferSize.
        //    These are the vertices of the *new* outer boundary.
        // 3. Connect these new vertices to form faces, edges, and rounded corners.

        // This is getting very complex for a step-by-step. Let's start with a simpler form
        // of chamfering: flat cuts at edges and corners first, then introduce curvature.

        // Let's revert the corner and edge chamfering to something simpler,
        // and build up. The face generation is fine.

        // --- Simplified Chamfer (Flat Cut) ---
        // A chamfered cube essentially means:
        // - The 6 original faces become smaller, inner faces.
        // - The 12 original edges become new rectangular faces.
        // - The 8 original corners become new triangular faces.

        // This is the easiest to implement first.
        // The "rounded" part comes from a sphere/cylinder blend, which is harder.

        // For now, let's create a *flat-chamfered* cube.

        // Re-initialize for simpler flat chamfer
        Vertices.Empty();
        Triangles.Empty();
        Normals.Empty();
        UV0.Empty();
        VertexColors.Empty();
        Tangents.Empty();

        // Core points for the inner "cuboid" and outer "cuboid"
        // Inner points (where flat faces end)
        FVector InnerPts[8];
        InnerPts[0] = FVector(-InnerSize, -InnerSize, -InnerSize);
        InnerPts[1] = FVector( InnerSize, -InnerSize, -InnerSize);
        InnerPts[2] = FVector(-InnerSize,  InnerSize, -InnerSize);
        InnerPts[3] = FVector( InnerSize,  InnerSize, -InnerSize);
        InnerPts[4] = FVector(-InnerSize, -InnerSize,  InnerSize);
        InnerPts[5] = FVector( InnerSize, -InnerSize,  InnerSize);
        InnerPts[6] = FVector(-InnerSize,  InnerSize,  InnerSize);
        InnerPts[7] = FVector( InnerSize,  InnerSize,  InnerSize);

        // Outer points (where chamfers start, but still axis-aligned)
        // These are basically the original cube corners, but now define the extent of the chamfer
        FVector OuterPts[8];
        OuterPts[0] = FVector(-OuterSize, -OuterSize, -OuterSize);
        OuterPts[1] = FVector( OuterSize, -OuterSize, -OuterSize);
        OuterPts[2] = FVector(-OuterSize,  OuterSize, -OuterSize);
        OuterPts[3] = FVector( OuterSize,  OuterSize, -OuterSize);
        OuterPts[4] = FVector(-OuterSize, -OuterSize,  OuterSize);
        OuterPts[5] = FVector( OuterSize, -OuterSize,  OuterSize);
        OuterPts[6] = FVector(-OuterSize,  InnerSize,  OuterSize); // Typo corrected from InnerSize
        OuterPts[7] = FVector( OuterSize,  OuterSize,  OuterSize);


        // Vertices are generated for each face independently to ensure correct normals
        // A more advanced approach would use shared vertices and calculate smooth normals.
        // For a hard-edged flat chamfer, independent vertices per face is fine.

        int32 StartIndex = 0;

        // --- Create 6 Main Faces (Inner Squares) ---
        // Front (+X)
        StartIndex = Vertices.Num();
        AddVertex(InnerPts[1], FVector(1,0,0), FVector2D(0,0)); // FBR
        AddVertex(InnerPts[5], FVector(1,0,0), FVector2D(1,0)); // FTR
        AddVertex(InnerPts[7], FVector(1,0,0), FVector2D(1,1)); // BTR
        AddVertex(InnerPts[3], FVector(1,0,0), FVector2D(0,1)); // BBR
        AddQuad(StartIndex+0, StartIndex+1, StartIndex+2, StartIndex+3); // P1, P5, P7, P3

        // Back (-X)
        StartIndex = Vertices.Num();
        AddVertex(InnerPts[0], FVector(-1,0,0), FVector2D(0,0)); // FBL
        AddVertex(InnerPts[4], FVector(-1,0,0), FVector2D(1,0)); // FTL
        AddVertex(InnerPts[6], FVector(-1,0,0), FVector2D(1,1)); // BTL
        AddVertex(InnerPts[2], FVector(-1,0,0), FVector2D(0,1)); // BBL
        AddQuad(StartIndex+0, StartIndex+1, StartIndex+2, StartIndex+3); // P0, P4, P6, P2

        // Right (+Y)
        StartIndex = Vertices.Num();
        AddVertex(InnerPts[3], FVector(0,1,0), FVector2D(0,0)); // BBR
        AddVertex(InnerPts[7], FVector(0,1,0), FVector2D(1,0)); // BTR
        AddVertex(InnerPts[6], FVector(0,1,0), FVector2D(1,1)); // BTL
        AddVertex(InnerPts[2], FVector(0,1,0), FVector2D(0,1)); // BBL
        AddQuad(StartIndex+0, StartIndex+1, StartIndex+2, StartIndex+3); // P3, P7, P6, P2

        // Left (-Y)
        StartIndex = Vertices.Num();
        AddVertex(InnerPts[0], FVector(0,-1,0), FVector2D(0,0)); // FBL
        AddVertex(InnerPts[4], FVector(0,-1,0), FVector2D(1,0)); // FTL
        AddVertex(InnerPts[5], FVector(0,-1,0), FVector2D(1,1)); // FTR
        AddVertex(InnerPts[1], FVector(0,-1,0), FVector2D(0,1)); // FBR
        AddQuad(StartIndex+0, StartIndex+1, StartIndex+2, StartIndex+3); // P0, P4, P5, P1

        // Top (+Z)
        StartIndex = Vertices.Num();
        AddVertex(InnerPts[4], FVector(0,0,1), FVector2D(0,0)); // FTL
        AddVertex(InnerPts[6], FVector(0,0,1), FVector2D(1,0)); // BTL
        AddVertex(InnerPts[7], FVector(0,0,1), FVector2D(1,1)); // BTR
        AddVertex(InnerPts[5], FVector(0,0,1), FVector2D(0,1)); // FTR
        AddQuad(StartIndex+0, StartIndex+1, StartIndex+2, StartIndex+3); // P4, P6, P7, P5

        // Bottom (-Z)
        StartIndex = Vertices.Num();
        AddVertex(InnerPts[0], FVector(0,0,-1), FVector2D(0,0)); // FBL
        AddVertex(InnerPts[1], FVector(0,0,-1), FVector2D(1,0)); // FBR
        AddVertex(InnerPts[3], FVector(0,0,-1), FVector2D(1,1)); // BBR
        AddVertex(InnerPts[2], FVector(0,0,-1), FVector2D(0,1)); // BBL
        AddQuad(StartIndex+0, StartIndex+1, StartIndex+2, StartIndex+3); // P0, P1, P3, P2


        // --- Create 12 Edge Chamfer Faces (Rectangular Strips) ---
        // Each edge connects two flat faces. Normal is perpendicular to the edge and outward.

        // Edges along X-axis
        // P0-P1 (-Y,-Z to -Y,-Z)
        StartIndex = Vertices.Num();
        AddVertex(InnerPts[0] + FVector(0, -ChamferSize, 0), FVector(0,-1,0), FVector2D(0,0)); // from -Y face
        AddVertex(InnerPts[1] + FVector(0, -ChamferSize, 0), FVector(0,-1,0), FVector2D(1,0)); // from -Y face
        AddVertex(InnerPts[1] + FVector(0, 0, -ChamferSize), FVector(0,0,-1), FVector2D(1,1)); // from -Z face
        AddVertex(InnerPts[0] + FVector(0, 0, -ChamferSize), FVector(0,0,-1), FVector2D(0,1)); // from -Z face
        AddQuad(StartIndex+0, StartIndex+1, StartIndex+2, StartIndex+3); // This is not a proper quad for chamfered edges
        // We need to define the 4 vertices of the chamfer face:
        // Two from one inner face, two from the adjacent inner face

        // Re-think edge chamfers for flat cuts:
        // Each edge chamfer connects 4 points:
        // (InnerSize, Y, Z_Inner) , (InnerSize, Y, Z_Outer)
        // (InnerSize, Y_Inner, Z) , (InnerSize, Y_Outer, Z)
        // Example: Edge (1,5) is +X, -Y side.
        // Points are (InnerSize, -InnerSize, Z) and (InnerSize, -OuterSize, Z)

        // Let's explicitly define edge points for clarity for flat chamfers
        // Edges connect two "face-cut" points. E.g., for X axis edge at (+Y,+Z)
        // Start points: (InnerSize, OuterSize, OuterSize) & (OuterSize, InnerSize, OuterSize)
        // End points: (InnerSize, OuterSize, InnerSize) & (OuterSize, OuterSize, InnerSize)

        // This is actually better done by iterating through edges and creating a flat rectangular strip.
        // Example: Edge between faces +X and +Y (i.e., at X=InnerSize, Y=InnerSize, Z between -OuterSize and OuterSize)
        // Vertices would be:
        // (InnerSize, InnerSize, -OuterSize), (OuterSize, InnerSize, -OuterSize), (OuterSize, InnerSize, OuterSize), (InnerSize, InnerSize, OuterSize)
        // This is still describing a flat face *aligned* with the axis, not a chamfer that cuts at 45 degrees.

        // For a **flat chamfer**, the definition is simpler:
        // Vertices are at +/-InnerSize or +/-OuterSize.
        // Faces are at +/-InnerSize.
        // Edge chamfers connect a point from an InnerFace to a point from an adjacent InnerFace,
        // through the OuterSize extent.

        // Let's define the 24 "face" points for a flat chamfered cube
        // Each original corner becomes 3 new vertices,
        // each original edge becomes 4 new vertices (2 from each face)
        // each original face becomes 4 new vertices.

        // Vertices for the 8 corners of the *overall* cube, but with chamfer applied.
        // These points will be the `OuterSize` extent.
        FVector V[24]; // To store the vertices of the chamfered cube explicitly.
        int32 vIdx = 0;

        // Define points at various offsets for clarity:
        // X, Y, Z signs define octant
        // C = ChamferSize, S = Size/2 (OuterSize), I = Size/2 - ChamferSize (InnerSize)

        // Corners for the -X,-Y,-Z octant (P[0] equivalent)
        // Points on the -X face (at X=-InnerSize)
        V[vIdx++] = FVector(-InnerSize, -InnerSize, -OuterSize); // Bottom-Left-Front of -X face
        V[vIdx++] = FVector(-InnerSize, -OuterSize, -InnerSize); // Bottom-Front-Left of -X face
        V[vIdx++] = FVector(-InnerSize, -OuterSize, OuterSize);  // Top-Front-Left of -X face (error, should be -InnerSize, +OuterSize, +InnerSize for BTL)
        V[vIdx++] = FVector(-InnerSize, InnerSize, OuterSize);   // Corrected P6 relative to -X face
        // This explicit vertex definition is error prone. Let's use a more systematic approach.

        // --- Common Chamfered Cube Construction (Flat) ---
        // 1. Define 24 vertices. For each face, there are 4 vertices.
        // Example: +X face has 4 vertices:
        // (OuterSize, -InnerSize, -InnerSize)
        // (OuterSize,  InnerSize, -InnerSize)
        // (OuterSize,  InnerSize,  InnerSize)
        // (OuterSize, -InnerSize,  InnerSize)
        // These are the corners of the inner square face.

        // 2. Define the edge chamfer vertices. For each original edge, there are 4 vertices.
        // Example: Edge between +X face and +Y face (X=OuterSize, Y=OuterSize).
        // This edge segment runs from Z=-InnerSize to Z=InnerSize.
        // (OuterSize, InnerSize, -InnerSize) - point on +X face edge
        // (InnerSize, OuterSize, -InnerSize) - point on +Y face edge
        // (OuterSize, InnerSize,  InnerSize) - point on +X face edge
        // (InnerSize, OuterSize,  InnerSize) - point on +Y face edge
        // These form a quad. Its normal is (1,1,0).GetSafeNormal().

        // 3. Define the corner chamfer vertices. For each original corner, there are 3 vertices.
        // Example: Corner (+X,+Y,+Z)
        // (OuterSize, InnerSize, InnerSize) - on +X face
        // (InnerSize, OuterSize, InnerSize) - on +Y face
        // (InnerSize, InnerSize, OuterSize) - on +Z face
        // These three points form a triangle. Its normal is (1,1,1).GetSafeNormal().

        // Let's implement this flat chamfered cube.

        // Clear previous data
        Vertices.Empty(); Triangles.Empty(); Normals.Empty(); UV0.Empty(); VertexColors.Empty(); Tangents.Empty();

        // Store indices for re-use:
        // We need 8 "core" indices, each referring to 3 points on the 3 adjacent faces.
        // And 12 "edge" indices, each referring to 2 points from the two adjacent faces.
        // And 6 "face" indices, each referring to 4 points.

        // This is getting really hairy with manual indexing.
        // Let's use a standard cube generation algorithm and then transform vertices
        // to achieve the chamfer, then re-calculate normals.

        // The simplest approach for a *rounded* chamfer is often to
        // build an ordinary cube, then "shave" its corners/edges
        // or to build a sphere and "flatten" its poles.
        // For a true chamfer (flat or rounded), we need careful control over vertices.

        // Let's define all the unique vertices first.
        // There are 8 "corner" vertices, 12 "edge" vertices, 6 "face" vertices.
        // Each face has 4 inner vertices. (6 * 4 = 24 vertices)
        // Each edge chamfer has 4 vertices. (12 * 4 = 48 vertices)
        // Each corner chamfer has 3 vertices. (8 * 3 = 24 vertices)
        // Total = 24 + 48 + 24 = 96 vertices if all are unique for hard edges.
        // Total = 8 (corners) + 12*2 (edges) + 6*4 (faces) if shared.
        // This is why a library for CSG (Constructive Solid Geometry) is often preferred.

        // Let's try to generate the flat faces, then the flat edge chamfers, then flat corner chamfers.

        // Vertices for the flat internal faces: 6 faces * 4 vertices each = 24 unique vertices
        // These are `InnerSize` from the center on two axes, `OuterSize` from the center on one axis.
        // No, these are `InnerSize` on all 3 axes, and their normals define the face direction.
        // This was `InnerPts` defined earlier.

        // Let's use a struct to hold the 8 key points and their corresponding normals for the flat faces.
        // This will make defining vertices for the connecting chamfers easier.

        // Coordinates for the 8 inner corners (P[0]...P[7] as defined before)
        // P_xyz = FVector(X_sign * InnerSize, Y_sign * InnerSize, Z_sign * InnerSize)

        // Vertices for the 6 faces (each uses 4 of these inner corners)
        // Faces are already defined using `InnerPts` in the previous iteration.
        // We need to store their indices for later use in edges/corners.
        // The previous face generation block is correct for the central faces.
        // Let's keep `Vertices`, `Triangles`, etc. arrays and rebuild.

        // --- Re-doing for Flat Chamfer ---
        // Much simpler structure:
        // 8 corner groups (each has 3 points for the corner chamfer, and points for adjacent edges/faces)
        // 12 edge groups (each has 2 points for edge chamfer)
        // 6 face groups (each has 4 points for the face)

        // Let's start with a simplified point generation strategy based on the "rounded" cube concept,
        // but making it flat.

        // Consider a point (x,y,z) on the cube surface.
        // If |x| > InnerSize, it's on a chamfer.
        // We need to define the 24 unique vertices for a flat chamfered cube.
        // These are the vertices where 3 faces meet, or 2 faces meet.

        // The most robust way for *flat* chamfers:
        // Generate 8 corner points, 12 edge points, and 6 face points.
        // Then connect them. This often means 24 vertices (3 * 8 for corners) + 24 (2 * 12 for edges).
        // This creates a mesh with 24 faces: 6 squares, 12 rectangles, 8 triangles.

        Vertices.Empty(); Triangles.Empty(); Normals.Empty(); UV0.Empty(); VertexColors.Empty(); Tangents.Empty();

        // Function to get a chamfered vertex position and normal
        auto GetChamferedVertexInfo = [&](float InX, float InY, float InZ, FVector& OutPos, FVector& OutNormal)
        {
            OutPos = FVector(InX, InY, InZ);

            // Determine normal based on which planes the point is "on" or "closest to"
            // For a flat chamfer, points on the same flat face have the same normal.
            // Points on edge chamfer faces have averaged normals of two adjacent faces.
            // Points on corner chamfer faces have averaged normals of three adjacent faces.

            // This is getting too complex for manual calculation of normals for each of 24 vertices.
            // Let's simplify by using the *actual vertex positions* and inferring normals.
            // Or, generate a standard cube, then scale / push vertices.

        };

        // Let's use a simpler approach that uses the `Size` and `ChamferSize`
        // to directly compute the vertices of the 24 faces.
        // This will generate 24 * 4 vertices for quads, or 24 * 3 for triangles if not using quads.
        // (6 main faces * 4 vertices) + (12 edge chamfers * 4 vertices) + (8 corner chamfers * 3 vertices)
        // This means a lot of duplicate vertices if we don't manage them.

        // A very robust way for this kind of geometry is to use a 3D grid and marching cubes,
        // or to build from 8 base octants.

        // --- Let's try the more common "rounded cube" approach (easier to implement a true chamfer) ---
        // Instead of calculating all 24 faces with their specific vertices,
        // we calculate a smaller set of key points and then extrude/connect them.

        // Vertices are generated for each face section
        // The cube is made of 8 corners, 12 edges, 6 faces.
        // A chamfered cube essentially rounds these features.

        // Let's try generating a "rounded" cube directly by defining its "skeleton".
        // 8 points at (±(HalfSize-ChamferSize), ±(HalfSize-ChamferSize), ±(HalfSize-ChamferSize))
        // These are the inner cube's corners (InnerPts).

        // Then, for each face, create 4 vertices at (+-InnerSize, +-InnerSize, +/-HalfSize) etc.
        // These would be the vertices of the "face" part of the chamfered cube.

        // For edges, create cylindrical segments.
        // For corners, create spherical segments.

        // This is the correct, but more involved, method for a *rounded* chamfer.

        // --- Re-implementing for Rounded Chamfer (Arc-based) ---
        // Vertices
        // The 8 central inner points of the cube (P[0] to P[7] as defined initially)
        // These points serve as the *origins* for the chamfer arcs.

        // We need to iterate over faces, then edges, then corners.
        // Each element will contribute its own set of vertices and triangles.

        // Let's use a simpler loop for the vertices generation.
        // We have 8 unique "corner groups" on the cube.
        // Each corner is defined by 3 axes. Example: (+X,+Y,+Z) corner.
        // We need to define points along the arcs between these axes.

        // Helper for rounded points (simplified)
        auto CreateRoundedQuadFace = [&](const FVector& BaseCenter, const FVector& NormalX, const FVector& NormalY, const FVector& FaceNormal)
        {
            // This is for the flat parts of the faces.
            // We assume FaceNormal is already normalized.
            // BaseCenter is the center of the face.

            FVector Point1 = BaseCenter - NormalX * InnerSize - NormalY * InnerSize;
            FVector Point2 = BaseCenter + NormalX * InnerSize - NormalY * InnerSize;
            FVector Point3 = BaseCenter + NormalX * InnerSize + NormalY * InnerSize;
            FVector Point4 = BaseCenter - NormalX * InnerSize + NormalY * InnerSize;

            int32 V0 = AddVertex(Point1, FaceNormal, FVector2D(0,0));
            int32 V1 = AddVertex(Point2, FaceNormal, FVector2D(1,0));
            int32 V2 = AddVertex(Point3, FaceNormal, FVector2D(1,1));
            int32 V3 = AddVertex(Point4, FaceNormal, FVector2D(0,1));
            AddQuad(V0, V1, V2, V3);
        };

        // Front Face (+X)
        CreateRoundedQuadFace(FVector(OuterSize, 0, 0), FVector(0,1,0), FVector(0,0,1), FVector(1,0,0));
        // Back Face (-X)
        CreateRoundedQuadFace(FVector(-OuterSize, 0, 0), FVector(0,-1,0), FVector(0,0,1), FVector(-1,0,0));
        // Right Face (+Y)
        CreateRoundedQuadFace(FVector(0, OuterSize, 0), FVector(1,0,0), FVector(0,0,1), FVector(0,1,0));
        // Left Face (-Y)
        CreateRoundedQuadFace(FVector(0, -OuterSize, 0), FVector(-1,0,0), FVector(0,0,1), FVector(0,-1,0));
        // Top Face (+Z)
        CreateRoundedQuadFace(FVector(0, 0, OuterSize), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1));
        // Bottom Face (-Z)
        CreateRoundedQuadFace(FVector(0, 0, -OuterSize), FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1));

        // Edge Chamfers (Cylindrical Segments)
        // 12 edges, each needs 2 sets of Sections+1 vertices.
        // For each edge, we define start and end points in the chamfer plane.
        // Example: Edge along X axis, at (+Y,+Z) corner of inner box.
        // This edge connects points from (+Y) face and (+Z) face.
        // Start points: (InnerSize, OuterSize, InnerSize) and (InnerSize, InnerSize, OuterSize)
        // End points: (InnerSize, OuterSize, -InnerSize) and (InnerSize, InnerSize, -OuterSize)

        // Define edges using their two adjacent normals
        struct FRolledEdgeInfo
        {
            FVector InnerPointStart; // Point on the inner cube
            FVector Axis;            // Direction of the edge
            FVector Normal1, Normal2; // Normals of the two faces that meet at this edge
        };

        TArray<FRolledEdgeInfo> RolledEdges;
        // X-axis edges
        RolledEdges.Add({FVector(-InnerSize, -InnerSize, -InnerSize), FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1)}); // P0-P1 dir
        RolledEdges.Add({FVector(-InnerSize,  InnerSize, -InnerSize), FVector(1,0,0), FVector(0,1,0), FVector(0,0,-1)}); // P2-P3 dir
        RolledEdges.Add({FVector(-InnerSize, -InnerSize,  InnerSize), FVector(1,0,0), FVector(0,-1,0), FVector(0,0,1)}); // P4-P5 dir
        RolledEdges.Add({FVector(-InnerSize,  InnerSize,  InnerSize), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1)}); // P6-P7 dir
        // Y-axis edges
        RolledEdges.Add({FVector(-InnerSize, -InnerSize, -InnerSize), FVector(0,1,0), FVector(-1,0,0), FVector(0,0,-1)}); // P0-P2 dir
        RolledEdges.Add({FVector( InnerSize, -InnerSize, -InnerSize), FVector(0,1,0), FVector(1,0,0), FVector(0,0,-1)}); // P1-P3 dir
        RolledEdges.Add({FVector(-InnerSize, -InnerSize,  InnerSize), FVector(0,1,0), FVector(-1,0,0), FVector(0,0,1)}); // P4-P6 dir
        RolledEdges.Add({FVector( InnerSize, -InnerSize,  InnerSize), FVector(0,1,0), FVector(1,0,0), FVector(0,0,1)}); // P5-P7 dir
        // Z-axis edges
        RolledEdges.Add({FVector(-InnerSize, -InnerSize, -InnerSize), FVector(0,0,1), FVector(-1,0,0), FVector(0,-1,0)}); // P0-P4 dir
        RolledEdges.Add({FVector( InnerSize, -InnerSize, -InnerSize), FVector(0,0,1), FVector(1,0,0), FVector(0,-1,0)}); // P1-P5 dir
        RolledEdges.Add({FVector(-InnerSize,  InnerSize, -InnerSize), FVector(0,0,1), FVector(-1,0,0), FVector(0,1,0)}); // P2-P6 dir
        RolledEdges.Add({FVector( InnerSize,  InnerSize, -InnerSize), FVector(0,0,1), FVector(1,0,0), FVector(0,1,0)}); // P3-P7 dir


        for (const FRolledEdgeInfo& Edge : RolledEdges)
        {
            FVector CornerOrigin = Edge.InnerPointStart;
            FVector EdgeDirection = Edge.Axis;
            FVector NormalA = Edge.Normal1;
            FVector NormalB = Edge.Normal2;

            TArray<int32> IndicesA; // Vertices at one end of the edge strip
            TArray<int32> IndicesB; // Vertices at the other end of the edge strip

            for (int32 i = 0; i <= Sections; ++i)
            {
                float Alpha = (float)i / Sections;
                // Interpolate normal vector for cylindrical surface
                FVector CurrentNormal = FMath::Lerp(NormalA, NormalB, Alpha).GetSafeNormal();
                FVector CurrentPosA = CornerOrigin + CurrentNormal * ChamferSize;
                FVector CurrentPosB = CurrentPosA + EdgeDirection * (Size - ChamferSize * 2); // Extend along the edge axis

                // Adjust UVs for texture mapping along the edge
                FVector2D UVA = FVector2D(Alpha, 0.0f);
                FVector2D UVB = FVector2D(Alpha, 1.0f);

                IndicesA.Add(AddVertex(CurrentPosA, CurrentNormal, UVA));
                IndicesB.Add(AddVertex(CurrentPosB, CurrentNormal, UVB));
            }

            // Create quads for the cylindrical strip
            for (int32 i = 0; i < Sections; ++i)
            {
                int32 V0 = IndicesA[i];
                int32 V1 = IndicesB[i];
                int32 V2 = IndicesB[i+1];
                int32 V3 = IndicesA[i+1];
                AddQuad(V0, V1, V2, V3);
            }
        }


        // Corner Chamfers (Spherical Segments)
        // 8 corners, each needs a spherical patch.
        // This is the trickiest part, as it's a triangle on a sphere.
        // We can approximate by generating a grid over the spherical triangle.

        // Define corners by their 3 adjacent normals
        struct FRolledCornerInfo
        {
            FVector InnerPoint; // The "original" corner point of the inner cube
            FVector Normal1, Normal2, Normal3; // Normals of the 3 faces meeting at this corner
        };

        TArray<FRolledCornerInfo> RolledCorners;
        RolledCorners.Add({FVector(-InnerSize, -InnerSize, -InnerSize), FVector(-1,0,0), FVector(0,-1,0), FVector(0,0,-1)});
        RolledCorners.Add({FVector( InnerSize, -InnerSize, -InnerSize), FVector(1,0,0), FVector(0,-1,0), FVector(0,0,-1)});
        RolledCorners.Add({FVector(-InnerSize,  InnerSize, -InnerSize), FVector(-1,0,0), FVector(0,1,0), FVector(0,0,-1)});
        RolledCorners.Add({FVector( InnerSize,  InnerSize, -InnerSize), FVector(1,0,0), FVector(0,1,0), FVector(0,0,-1)});
        RolledCorners.Add({FVector(-InnerSize, -InnerSize,  InnerSize), FVector(-1,0,0), FVector(0,-1,0), FVector(0,0,1)});
        RolledCorners.Add({FVector( InnerSize, -InnerSize,  InnerSize), FVector(1,0,0), FVector(0,-1,0), FVector(0,0,1)});
        RolledCorners.Add({FVector(-InnerSize,  InnerSize,  InnerSize), FVector(-1,0,0), FVector(0,1,0), FVector(0,0,1)});
        RolledCorners.Add({FVector( InnerSize,  InnerSize,  InnerSize), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1)});


        for (const FRolledCornerInfo& Corner : RolledCorners)
        {
            FVector CornerOrigin = Corner.InnerPoint;
            FVector N1 = Corner.Normal1;
            FVector N2 = Corner.Normal2;
            FVector N3 = Corner.Normal3;

            TArray<TArray<int32>> CornerGrid; // [u][v] indices
            CornerGrid.SetNum(Sections + 1);
            for (int32 i = 0; i <= Sections; ++i)
            {
                CornerGrid[i].SetNum(Sections + 1);
            }

            for (int32 i = 0; i <= Sections; ++i)
            {
                for (int32 j = 0; j <= Sections - i; ++j) // Create a triangular patch
                {
                    float U = (float)i / Sections;
                    float V = (float)j / Sections;
                    float W = 1.0f - U - V; // Barycentric coordinates sum to 1

                    if (W < 0) W = 0; // Clamp to avoid issues

                    // Interpolate normals to get point on spherical patch
                    FVector CurrentNormal = (N1 * U + N2 * V + N3 * W).GetSafeNormal();
                    FVector CurrentPos = CornerOrigin + CurrentNormal * ChamferSize;

                    FVector2D UV = FVector2D(U, V); // Simple UV mapping for the patch

                    CornerGrid[i][j] = AddVertex(CurrentPos, CurrentNormal, UV);
                }
            }

            // Generate triangles for the spherical patch
            for (int32 i = 0; i < Sections; ++i)
            {
                for (int32 j = 0; j < Sections - i; ++j)
                {
                    // Quad from (i,j) (i+1,j) (i+1,j+1) (i,j+1)
                    // but it's a triangular grid
                    int32 V0 = CornerGrid[i][j];
                    int32 V1 = CornerGrid[i+1][j];
                    int32 V2 = CornerGrid[i][j+1];

                    Triangles.Add(V0); Triangles.Add(V1); Triangles.Add(V2);

                    // Second triangle if it forms a quad piece (i+1,j+1)
                    if (j < Sections - i - 1) // Ensure we don't go out of bounds for the diagonal
                    {
                        int32 V3 = CornerGrid[i+1][j];
                        int32 V4 = CornerGrid[i][j+1];
                        int32 V5 = CornerGrid[i+1][j+1];
                        Triangles.Add(V3); Triangles.Add(V5); Triangles.Add(V4);
                    }
                }
            }
        }


        // Finally, create the mesh section
        ProceduralMesh->CreateMeshSection_LinearColor(
            0,                  // Section Index
            Vertices,           // Vertex Positions
            Triangles,          // Triangle Indices
            Normals,            // Vertex Normals
            UV0,                // UV Coordinates (TexCoord0)
            VertexColors,       // Vertex Colors
            Tangents,           // Vertex Tangents
            false               // Create collision (we set this to true in constructor, but can override)
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