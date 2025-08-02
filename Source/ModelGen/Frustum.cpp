// Frustum.cpp

#include "Frustum.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h" // For FMath::DegreesToRadians, etc.

// Sets default values
AFrustum::AFrustum()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProceduralMesh;

    ProceduralMesh->bUseAsyncCooking = true;
    ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ProceduralMesh->SetSimulatePhysics(false);

    GenerateFrustum(); // Generate on construction
}

void AFrustum::BeginPlay()
{
    Super::BeginPlay();
    GenerateFrustum(); // Generate on play (for runtime updates if parameters change)
}

void AFrustum::PostLoad()
{
    Super::PostLoad();
    GenerateFrustum(); // Generate after loading (for editor data)
}

#if WITH_EDITOR
void AFrustum::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property != nullptr)
    {
        FName PropertyName = PropertyChangedEvent.Property->GetFName();
        if (PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, TopRadius) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, BottomRadius) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, Height) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, TopSides) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, BottomSides) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, ChamferRadius) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, ChamferSections) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, ArcSegments) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, BendDegree) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, MinBendRadius) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FFrustumParameters, FrustumAngle))
        {
            GenerateFrustum();
        }
    }
}
#endif

void AFrustum::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

int32 AFrustum::AddVertexInternal(FMeshData& MeshData, const FVector& Pos, const FVector& Normal, const FVector2D& UV)
{
    MeshData.Vertices.Add(Pos);
    MeshData.Normals.Add(Normal);
    MeshData.UV0.Add(UV);
    MeshData.VertexColors.Add(FLinearColor::White);

    // Calculate tangent - simplified for now, often needs more sophisticated calculation
    FVector TangentDirection = FVector::CrossProduct(Normal, FVector::UpVector);
    if (TangentDirection.IsNearlyZero())
    {
        TangentDirection = FVector::CrossProduct(Normal, FVector::RightVector);
    }
    TangentDirection.Normalize();
    MeshData.Tangents.Add(FProcMeshTangent(TangentDirection, false));

    return MeshData.Vertices.Num() - 1;
}

void AFrustum::AddQuadInternal(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3, int32 V4)
{
    Triangles.Add(V1); Triangles.Add(V2); Triangles.Add(V3); // Triangle 1
    Triangles.Add(V1); Triangles.Add(V3); Triangles.Add(V4); // Triangle 2
}

void AFrustum::AddTriangleInternal(TArray<int32>& Triangles, int32 V1, int32 V2, int32 V3)
{
    Triangles.Add(V1); Triangles.Add(V2); Triangles.Add(V3);
}

void AFrustum::GenerateFrustum()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent is null!"));
        return;
    }

    ProceduralMesh->ClearAllMeshSections();

    FMeshData MeshData;

    // Clamp and validate parameters
    FrustumParameters.TopRadius = FMath::Max(0.01f, FrustumParameters.TopRadius);
    FrustumParameters.BottomRadius = FMath::Max(0.01f, FrustumParameters.BottomRadius);
    FrustumParameters.Height = FMath::Max(0.01f, FrustumParameters.Height);
    FrustumParameters.TopSides = FMath::Max(3, FrustumParameters.TopSides);
    FrustumParameters.BottomSides = FMath::Max(3, FrustumParameters.BottomSides);
    FrustumParameters.ChamferSections = FMath::Max(1, FrustumParameters.ChamferSections);
    FrustumParameters.ArcSegments = FMath::Max(1, FrustumParameters.ArcSegments);
    FrustumParameters.FrustumAngle = FMath::Clamp(FrustumParameters.FrustumAngle, 0.0f, 360.0f);
    FrustumParameters.MinBendRadius = FMath::Max(1.0f, FrustumParameters.MinBendRadius); // Ensure > 0

    // Chamfer radius must not be too large
    FrustumParameters.ChamferRadius = FMath::Clamp(FrustumParameters.ChamferRadius, 0.0f,
        FMath::Min(FMath::Min(FrustumParameters.TopRadius, FrustumParameters.BottomRadius), FrustumParameters.Height / 2.0f) - KINDA_SMALL_NUMBER);

    // Ensure TopSides <= BottomSides as per instruction
    FrustumParameters.TopSides = FMath::Min(FrustumParameters.TopSides, FrustumParameters.BottomSides);


    // Generate main geometry
    GenerateGeometry(MeshData);

    // Setup material
    SetupMaterial();

    // Create mesh section
    if (MeshData.Vertices.Num() > 0 && MeshData.Triangles.Num() > 0)
    {
        ProceduralMesh->CreateMeshSection_LinearColor(
            0,
            MeshData.Vertices,
            MeshData.Triangles,
            MeshData.Normals,
            MeshData.UV0,
            MeshData.VertexColors,
            MeshData.Tangents,
            true // Enable collision
        );
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Frustum mesh data is empty, cannot create mesh section."));
    }
}

void AFrustum::GenerateGeometry(FMeshData& MeshData)
{
    const float HalfHeight = FrustumParameters.Height / 2.0f;
    const float AngleStepBottom = FMath::DegreesToRadians(FrustumParameters.FrustumAngle) / FrustumParameters.BottomSides;
    const float AngleStepTop = FMath::DegreesToRadians(FrustumParameters.FrustumAngle) / FrustumParameters.TopSides;

    // Helper to calculate radial offset based on bend degree
    auto GetBentRadius = [&](float CurrentRadius, float AlphaZ) -> float
        {
            // AlphaZ from -1 (bottom) to 1 (top)
            float BendFactor = FMath::Sin(AlphaZ * PI * 0.5f); // Smooth bend from -1 to 1 based on height
            float MaxRadialOffset = CurrentRadius * FrustumParameters.BendDegree;
            float AdjustedRadius = CurrentRadius + MaxRadialOffset * BendFactor;

            // Ensure radius doesn't go below MinBendRadius
            return FMath::Max(AdjustedRadius, FrustumParameters.MinBendRadius);
        };

    TArray<TArray<int32>> SideVerticesGrid; // [ArcSegmentIndex][BottomSideSegmentIndex]
    SideVerticesGrid.SetNum(FrustumParameters.ArcSegments + 1);
    for (int32 i_arc = 0; i_arc <= FrustumParameters.ArcSegments; ++i_arc)
    {
        SideVerticesGrid[i_arc].SetNum(FrustumParameters.BottomSides);
    }

    // Generate rings of vertices for the side faces (including top and bottom)
    for (int32 i_arc = 0; i_arc <= FrustumParameters.ArcSegments; ++i_arc)
    {
        float AlphaHeight = (float)i_arc / FrustumParameters.ArcSegments; // 0.0 at bottom, 1.0 at top
        float Z = FMath::Lerp(-HalfHeight, HalfHeight, AlphaHeight);
        float RadiusLerp = FMath::Lerp(FrustumParameters.BottomRadius, FrustumParameters.TopRadius, AlphaHeight);

        float CurrentFrustumRadius = GetBentRadius(RadiusLerp, FMath::Lerp(-1.0f, 1.0f, AlphaHeight));

        for (int32 j_side = 0; j_side < FrustumParameters.BottomSides; ++j_side)
        {
            float Angle = j_side * AngleStepBottom;
            FVector Pos(CurrentFrustumRadius * FMath::Cos(Angle), CurrentFrustumRadius * FMath::Sin(Angle), Z);

            // Calculate normal for side faces (pointing outwards radially)
            FVector Normal = FVector(Pos.X, Pos.Y, 0).GetSafeNormal();
            if (FrustumParameters.BendDegree != 0.0f)
            {
                // For bending, the normal also needs a Z component
                // This is a simplified normal calculation; for true smoothness, it needs to consider the exact curve.
                float NormalZComponent = -FrustumParameters.BendDegree * FMath::Cos(AlphaHeight * PI);
                Normal += FVector(0, 0, NormalZComponent);
                Normal.Normalize();
            }

            // UV mapping for side faces
            FVector2D UV((float)j_side / FrustumParameters.BottomSides, AlphaHeight);

            SideVerticesGrid[i_arc][j_side] = AddVertexInternal(MeshData, Pos, Normal, UV);
        }
    }

    // Generate Side Faces
    for (int32 i_arc = 0; i_arc < FrustumParameters.ArcSegments; ++i_arc)
    {
        for (int32 j_side = 0; j_side < FrustumParameters.BottomSides; ++j_side)
        {
            int32 V00 = SideVerticesGrid[i_arc][j_side];
            int32 V10 = SideVerticesGrid[i_arc + 1][j_side];
            int32 V01 = SideVerticesGrid[i_arc][(j_side + 1) % FrustumParameters.BottomSides];
            int32 V11 = SideVerticesGrid[i_arc + 1][(j_side + 1) % FrustumParameters.BottomSides];

            AddQuadInternal(MeshData.Triangles, V00, V10, V11, V01); // Standard winding order
        }
    }

    // Generate Top and Bottom Faces
    GenerateTopAndBottomFaces(MeshData);

    // Generate Chamfers (edges between top/bottom and sides)
    GenerateChamfers(MeshData);

    // If FrustumAngle < 360, add the start and end cap faces
    if (FrustumParameters.FrustumAngle < 360.0f - KINDA_SMALL_NUMBER)
    {
        // Start Cap (at Angle = 0)
        FVector StartCapNormal = FVector(0, -1, 0); // Pointing 'left' from the slice.

        for (int32 i_cap_start = 0; i_cap_start < FrustumParameters.ArcSegments; ++i_cap_start)
        {
            int32 OuterVtx_Bottom_Start = SideVerticesGrid[i_cap_start][0];
            int32 OuterVtx_Top_Start = SideVerticesGrid[i_cap_start + 1][0];

            float Z_Bottom_Start = MeshData.Vertices[OuterVtx_Bottom_Start].Z;
            float Z_Top_Start = MeshData.Vertices[OuterVtx_Top_Start].Z;

            // These points form the "inner" edge along the Z-axis for the slice
            FVector InnerPointBottomPos_Start(0, 0, Z_Bottom_Start);
            FVector InnerPointTopPos_Start(0, 0, Z_Top_Start);

            int32 InnerVtx_Bottom_Start = AddVertexInternal(MeshData, InnerPointBottomPos_Start, StartCapNormal, FVector2D(0.0f, (float)i_cap_start / FrustumParameters.ArcSegments));
            int32 InnerVtx_Top_Start = AddVertexInternal(MeshData, InnerPointTopPos_Start, StartCapNormal, FVector2D(0.0f, (float)(i_cap_start + 1) / FrustumParameters.ArcSegments));

            AddQuadInternal(MeshData.Triangles, OuterVtx_Bottom_Start, OuterVtx_Top_Start, InnerVtx_Top_Start, InnerVtx_Bottom_Start); // Correct winding for outwards
        }

        // End Cap (at Angle = FrustumAngle)
        FVector EndCapNormal = FVector(-FMath::Sin(FMath::DegreesToRadians(FrustumParameters.FrustumAngle)), FMath::Cos(FMath::DegreesToRadians(FrustumParameters.FrustumAngle)), 0);

        for (int32 i_cap_end = 0; i_cap_end < FrustumParameters.ArcSegments; ++i_cap_end)
        {
            int32 OuterVtx_Bottom_End = SideVerticesGrid[i_cap_end][FrustumParameters.BottomSides - 1];
            int32 OuterVtx_Top_End = SideVerticesGrid[i_cap_end + 1][FrustumParameters.BottomSides - 1];

            float Z_Bottom_End = MeshData.Vertices[OuterVtx_Bottom_End].Z;
            float Z_Top_End = MeshData.Vertices[OuterVtx_Top_End].Z;

            // These points form the "inner" edge along the Z-axis for the slice
            FVector InnerPointBottomPos_End(0, 0, Z_Bottom_End);
            FVector InnerPointTopPos_End(0, 0, Z_Top_End);

            int32 InnerVtx_Bottom_End = AddVertexInternal(MeshData, InnerPointBottomPos_End, EndCapNormal, FVector2D(0.0f, (float)i_cap_end / FrustumParameters.ArcSegments));
            int32 InnerVtx_Top_End = AddVertexInternal(MeshData, InnerPointTopPos_End, EndCapNormal, FVector2D(0.0f, (float)(i_cap_end + 1) / FrustumParameters.ArcSegments));

            AddQuadInternal(MeshData.Triangles, InnerVtx_Bottom_End, InnerVtx_Top_End, OuterVtx_Top_End, OuterVtx_Bottom_End); // Reverse winding for outwards
        }
    }
}

void AFrustum::GenerateTopAndBottomFaces(FMeshData& MeshData)
{
    const float HalfHeight = FrustumParameters.Height / 2.0f;
    const float AngleStepBottom = FMath::DegreesToRadians(FrustumParameters.FrustumAngle) / FrustumParameters.BottomSides;
    const float AngleStepTop = FMath::DegreesToRadians(FrustumParameters.FrustumAngle) / FrustumParameters.TopSides;

    // Top Face
    int32 TopCenterVtx = AddVertexInternal(MeshData, FVector(0, 0, HalfHeight), FVector(0, 0, 1), FVector2D(0.5, 0.5));
    for (int32 i_top = 0; i_top < FrustumParameters.TopSides; ++i_top)
    {
        float Angle1 = i_top * AngleStepTop;
        float Angle2 = ((i_top + 1) % FrustumParameters.TopSides) * AngleStepTop;

        FVector P1(FrustumParameters.TopRadius * FMath::Cos(Angle1), FrustumParameters.TopRadius * FMath::Sin(Angle1), HalfHeight);
        FVector P2(FrustumParameters.TopRadius * FMath::Cos(Angle2), FrustumParameters.TopRadius * FMath::Sin(Angle2), HalfHeight);

        // Calculate UVs for circular top/bottom faces
        FVector2D UV1 = FVector2D(P1.X / (FrustumParameters.TopRadius * 2) + 0.5f, P1.Y / (FrustumParameters.TopRadius * 2) + 0.5f);
        FVector2D UV2 = FVector2D(P2.X / (FrustumParameters.TopRadius * 2) + 0.5f, P2.Y / (FrustumParameters.TopRadius * 2) + 0.5f);

        int32 Vtx1 = AddVertexInternal(MeshData, P1, FVector(0, 0, 1), UV1);
        int32 Vtx2 = AddVertexInternal(MeshData, P2, FVector(0, 0, 1), UV2);

        AddTriangleInternal(MeshData.Triangles, TopCenterVtx, Vtx2, Vtx1); // Clockwise for top face
    }

    // Bottom Face
    int32 BottomCenterVtx = AddVertexInternal(MeshData, FVector(0, 0, -HalfHeight), FVector(0, 0, -1), FVector2D(0.5, 0.5));
    for (int32 i_bottom = 0; i_bottom < FrustumParameters.BottomSides; ++i_bottom)
    {
        float Angle1 = i_bottom * AngleStepBottom;
        float Angle2 = ((i_bottom + 1) % FrustumParameters.BottomSides) * AngleStepBottom;

        FVector P1(FrustumParameters.BottomRadius * FMath::Cos(Angle1), FrustumParameters.BottomRadius * FMath::Sin(Angle1), -HalfHeight);
        FVector P2(FrustumParameters.BottomRadius * FMath::Cos(Angle2), FrustumParameters.BottomRadius * FMath::Sin(Angle2), -HalfHeight);

        // Calculate UVs for circular top/bottom faces
        FVector2D UV1 = FVector2D(P1.X / (FrustumParameters.BottomRadius * 2) + 0.5f, P1.Y / (FrustumParameters.BottomRadius * 2) + 0.5f);
        FVector2D UV2 = FVector2D(P2.X / (FrustumParameters.BottomRadius * 2) + 0.5f, P2.Y / (FrustumParameters.BottomRadius * 2) + 0.5f);

        int32 Vtx1 = AddVertexInternal(MeshData, P1, FVector(0, 0, -1), UV1);
        int32 Vtx2 = AddVertexInternal(MeshData, P2, FVector(0, 0, -1), UV2);

        AddTriangleInternal(MeshData.Triangles, BottomCenterVtx, Vtx1, Vtx2); // Counter-clockwise for bottom face
    }
}

void AFrustum::GenerateSideFaces(FMeshData& MeshData)
{
    // Side faces are generated directly within GenerateGeometry for easier bending and connection logic.
    // This function can be kept for future expansion if needed, but currently its logic is integrated.
}


void AFrustum::GenerateChamfers(FMeshData& MeshData)
{
    if (FrustumParameters.ChamferRadius < KINDA_SMALL_NUMBER) return;

    const float HalfHeight = FrustumParameters.Height / 2.0f;
    const float AngleStepBottom = FMath::DegreesToRadians(FrustumParameters.FrustumAngle) / FrustumParameters.BottomSides;
    const float AngleStepTop = FMath::DegreesToRadians(FrustumParameters.FrustumAngle) / FrustumParameters.TopSides;

    // Chamfers for the top edge
    for (int32 i_chamfer_top = 0; i_chamfer_top < FrustumParameters.TopSides; ++i_chamfer_top)
    {
        float AngleStart = i_chamfer_top * AngleStepTop;
        float AngleEnd = ((i_chamfer_top + 1) % FrustumParameters.TopSides) * AngleStepTop;

        TArray<int32> PrevChamferRingVtx; // Vertices from the previous chamfer section

        // Start from the main face edge and move towards the side face
        for (int32 s_chamfer_top = 0; s_chamfer_top <= FrustumParameters.ChamferSections; ++s_chamfer_top)
        {
            float AlphaChamfer = (float)s_chamfer_top / FrustumParameters.ChamferSections; // 0 at main face, 1 at side face
            float CurrentChamferRadius = FrustumParameters.ChamferRadius * AlphaChamfer;

            TArray<int32> CurrentChamferRingVtx;

            for (int32 j_chamfer_top_seg = 0; j_chamfer_top_seg < 2; ++j_chamfer_top_seg) // Two points per chamfer section segment
            {
                float CurrentAngle = (j_chamfer_top_seg == 0) ? AngleStart : AngleEnd;

                // Position on the top face edge (before chamfering)
                FVector BasePos = FVector(FrustumParameters.TopRadius * FMath::Cos(CurrentAngle), FrustumParameters.TopRadius * FMath::Sin(CurrentAngle), HalfHeight);

                // Normal interpolation for the chamfer surface
                FVector NormalTop = FVector(0, 0, 1); // Normal of the top face
                FVector NormalSide = FVector(FMath::Cos(CurrentAngle), FMath::Sin(CurrentAngle), 0).GetSafeNormal(); // Approximate normal of the side face at this angle

                FVector BlendedNormal = FMath::Lerp(NormalTop, NormalSide, AlphaChamfer).GetSafeNormal();

                // Position calculation for chamfer vertex
                FVector Pos = BasePos - NormalTop * CurrentChamferRadius + BlendedNormal * CurrentChamferRadius;

                // UV calculation for chamfer - simple linear blend for now
                FVector2D UV = FVector2D(FMath::Lerp(0.5f, (float)i_chamfer_top / FrustumParameters.TopSides, AlphaChamfer), FMath::Lerp(0.5f, 1.0f, AlphaChamfer));


                int32 Vtx = AddVertexInternal(MeshData, Pos, BlendedNormal, UV);
                CurrentChamferRingVtx.Add(Vtx);
            }

            if (s_chamfer_top > 0)
            {
                AddQuadInternal(MeshData.Triangles, PrevChamferRingVtx[0], CurrentChamferRingVtx[0], CurrentChamferRingVtx[1], PrevChamferRingVtx[1]);
            }
            PrevChamferRingVtx = CurrentChamferRingVtx;
        }
    }

    // Chamfers for the bottom edge (similar logic, adjust height and normal)
    for (int32 i_chamfer_bottom = 0; i_chamfer_bottom < FrustumParameters.BottomSides; ++i_chamfer_bottom)
    {
        float AngleStart = i_chamfer_bottom * AngleStepBottom;
        float AngleEnd = ((i_chamfer_bottom + 1) % FrustumParameters.BottomSides) * AngleStepBottom;

        TArray<int32> PrevChamferRingVtx;

        for (int32 s_chamfer_bottom = 0; s_chamfer_bottom <= FrustumParameters.ChamferSections; ++s_chamfer_bottom)
        {
            float AlphaChamfer = (float)s_chamfer_bottom / FrustumParameters.ChamferSections;
            float CurrentChamferRadius = FrustumParameters.ChamferRadius * AlphaChamfer;

            TArray<int32> CurrentChamferRingVtx;

            for (int32 j_chamfer_bottom_seg = 0; j_chamfer_bottom_seg < 2; ++j_chamfer_bottom_seg)
            {
                float CurrentAngle = (j_chamfer_bottom_seg == 0) ? AngleStart : AngleEnd;

                FVector BasePos = FVector(FrustumParameters.BottomRadius * FMath::Cos(CurrentAngle), FrustumParameters.BottomRadius * FMath::Sin(CurrentAngle), -HalfHeight);

                FVector NormalBottom = FVector(0, 0, -1);
                FVector NormalSide = FVector(FMath::Cos(CurrentAngle), FMath::Sin(CurrentAngle), 0).GetSafeNormal();

                FVector BlendedNormal = FMath::Lerp(NormalBottom, NormalSide, AlphaChamfer).GetSafeNormal();

                FVector Pos = BasePos - NormalBottom * CurrentChamferRadius + BlendedNormal * CurrentChamferRadius;

                FVector2D UV = FVector2D(FMath::Lerp(0.5f, (float)i_chamfer_bottom / FrustumParameters.BottomSides, AlphaChamfer), FMath::Lerp(0.5f, 0.0f, AlphaChamfer));

                int32 Vtx = AddVertexInternal(MeshData, Pos, BlendedNormal, UV);
                CurrentChamferRingVtx.Add(Vtx);
            }

            if (s_chamfer_bottom > 0)
            {
                AddQuadInternal(MeshData.Triangles, PrevChamferRingVtx[0], PrevChamferRingVtx[1], CurrentChamferRingVtx[1], CurrentChamferRingVtx[0]); // Reverse winding for bottom chamfer
            }
            PrevChamferRingVtx = CurrentChamferRingVtx;
        }
    }
}

void AFrustum::SetupMaterial()
{
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