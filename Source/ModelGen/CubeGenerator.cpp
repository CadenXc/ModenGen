#include "CubeGenerator.h"

ACubeGenerator::ACubeGenerator()
{
    PrimaryActorTick.bCanEverTick = false;
    ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>("ProcMesh");
    RootComponent = ProcMesh;
}

void ACubeGenerator::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerateBeveledCube();
}

void ACubeGenerator::GenerateBeveledCube()
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
    TArray<FColor> Colors;

    const float HalfSize = Size * 0.5f;
    const float InnerSize = HalfSize - BevelRadius;

    // 8 corners of the cube
    FVector Corners[8] = {
        FVector(InnerSize, InnerSize, InnerSize),
        FVector(InnerSize, InnerSize, -InnerSize),
        FVector(InnerSize, -InnerSize, InnerSize),
        FVector(InnerSize, -InnerSize, -InnerSize),
        FVector(-InnerSize, InnerSize, InnerSize),
        FVector(-InnerSize, InnerSize, -InnerSize),
        FVector(-InnerSize, -InnerSize, InnerSize),
        FVector(-InnerSize, -InnerSize, -InnerSize)
    };

    // Add beveled corners
    AddCorner(Vertices, Triangles, Corners[0], FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1));
    AddCorner(Vertices, Triangles, Corners[1], FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, -1));
    AddCorner(Vertices, Triangles, Corners[2], FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1));
    AddCorner(Vertices, Triangles, Corners[3], FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 0, -1));
    AddCorner(Vertices, Triangles, Corners[4], FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1));
    AddCorner(Vertices, Triangles, Corners[5], FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, 0, -1));
    AddCorner(Vertices, Triangles, Corners[6], FVector(-1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1));
    AddCorner(Vertices, Triangles, Corners[7], FVector(-1, 0, 0), FVector(0, -1, 0), FVector(0, 0, -1));

    // Add flat faces
    AddQuad(Vertices, Triangles, Corners[2], Corners[0], Corners[4], Corners[6]); // Front
    AddQuad(Vertices, Triangles, Corners[1], Corners[3], Corners[7], Corners[5]); // Back
    AddQuad(Vertices, Triangles, Corners[1], Corners[0], Corners[2], Corners[3]); // Right
    AddQuad(Vertices, Triangles, Corners[4], Corners[5], Corners[7], Corners[6]); // Left
    AddQuad(Vertices, Triangles, Corners[4], Corners[0], Corners[1], Corners[5]); // Top
    AddQuad(Vertices, Triangles, Corners[7], Corners[3], Corners[2], Corners[6]); // Bottom

    // Generate normals and UVs
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        Normals.Add(Vertices[i].GetSafeNormal());
        UVs.Add(FVector2D(Vertices[i].X, Vertices[i].Y) / Size + 0.5f);
        Tangents.Add(FProcMeshTangent(1, 0, 0));
        Colors.Add(FColor::White);
    }

    ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
}

void ACubeGenerator::AddQuad(TArray<FVector>& Vertices, TArray<int32>& Triangles,
    const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{
    int32 Index = Vertices.Num();
    Vertices.Add(A); Vertices.Add(B); Vertices.Add(C); Vertices.Add(D);

    Triangles.Add(Index); Triangles.Add(Index + 1); Triangles.Add(Index + 2);
    Triangles.Add(Index); Triangles.Add(Index + 2); Triangles.Add(Index + 3);
}

void ACubeGenerator::AddCorner(TArray<FVector>& Vertices, TArray<int32>& Triangles,
    const FVector& Center, const FVector& X, const FVector& Y, const FVector& Z)
{
    int32 BaseIndex = Vertices.Num();

    // Generate vertices
    for (int32 i = 0; i <= BevelSegments; i++)
    {
        float Phi = PI * 0.5f * i / BevelSegments;
        for (int32 j = 0; j <= BevelSegments; j++)
        {
            float Theta = PI * 0.5f * j / BevelSegments;
            FVector Dir = (X * FMath::Sin(Phi) * FMath::Cos(Theta) +
                Y * FMath::Sin(Phi) * FMath::Sin(Theta) +
                Z * FMath::Cos(Phi)).GetSafeNormal();
            Vertices.Add(Center + Dir * BevelRadius);
        }
    }

    // Generate triangles
    for (int32 i = 0; i < BevelSegments; i++)
    {
        for (int32 j = 0; j < BevelSegments; j++)
        {
            int32 a = BaseIndex + i * (BevelSegments + 1) + j;
            int32 b = BaseIndex + (i + 1) * (BevelSegments + 1) + j;
            int32 c = BaseIndex + (i + 1) * (BevelSegments + 1) + j + 1;
            int32 d = BaseIndex + i * (BevelSegments + 1) + j + 1;

            Triangles.Add(a); Triangles.Add(b); Triangles.Add(c);
            Triangles.Add(a); Triangles.Add(c); Triangles.Add(d);
        }
    }
}
