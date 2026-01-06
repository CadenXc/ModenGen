#include "ModelGenConvexDecomp.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Math/UnrealMathUtility.h"

bool FModelGenConvexDecomp::GenerateConvexHulls(
    UProceduralMeshComponent* ProceduralMeshComponent,
    UBodySetup* BodySetup,
    int32 HullCount,
    int32 MaxHullVerts,
    uint32 HullPrecision)
{
    if (!ProceduralMeshComponent || !BodySetup)
    {
        return false;
    }

    FMeshData MeshData;
    if (!ExtractMeshData(ProceduralMeshComponent, MeshData))
    {
        return false;
    }

    if (MeshData.Vertices.Num() < 4 || MeshData.Indices.Num() < 3)
    {
        return false;
    }


    FDecompParams Params;
    Params.TargetHullCount = FMath::Clamp(HullCount, 1, 64);
    Params.MaxHullVertices = FMath::Clamp(MaxHullVerts, 6, 32);
    Params.MaxDepth = FMath::Clamp(FMath::FloorToInt(FMath::Log2(static_cast<float>(HullPrecision) / 3000.0f)) + 5, 5, 15);
    Params.MinVolumeRatio = 0.001f;


    TArray<int32> AllTriangleIndices;
    const int32 NumTriangles = MeshData.Indices.Num() / 3;
    AllTriangleIndices.Reserve(NumTriangles);
    for (int32 i = 0; i < NumTriangles; ++i)
    {
        AllTriangleIndices.Add(i);
    }

    TArray<FKConvexElem> ConvexElems;
    RecursiveDecompose(MeshData, AllTriangleIndices, Params, 0, ConvexElems);

    if (ConvexElems.Num() < Params.TargetHullCount && ConvexElems.Num() > 0)
    {
        ConvexElems.Sort([](const FKConvexElem& A, const FKConvexElem& B) {
            return A.ElemBox.GetVolume() > B.ElemBox.GetVolume();
        });
        
        int32 NumToSplit = FMath::Min(ConvexElems.Num(), Params.TargetHullCount - ConvexElems.Num());
        TArray<FKConvexElem> NewElems;
        
        for (int32 i = 0; i < NumToSplit && ConvexElems.Num() + NewElems.Num() < Params.TargetHullCount; ++i)
        {
            const FKConvexElem& LargeElem = ConvexElems[i];
            if (LargeElem.VertexData.Num() > Params.MaxHullVertices)
            {
                FBox ElemBounds = LargeElem.ElemBox;
                if (ElemBounds.IsValid)
                {
                    FVector Center = ElemBounds.GetCenter();
                    FVector Extent = ElemBounds.GetExtent();
                    int32 LongestAxis = GetLongestAxis(ElemBounds);
                    
                    TArray<FVector> LeftPoints, RightPoints;
                    for (const FVector& Point : LargeElem.VertexData)
                    {
                        float Dist = 0.0f;
                        if (LongestAxis == 0) Dist = Point.X - Center.X;
                        else if (LongestAxis == 1) Dist = Point.Y - Center.Y;
                        else Dist = Point.Z - Center.Z;
                        
                        if (Dist < 0)
                        {
                            LeftPoints.Add(Point);
                        }
                        else
                        {
                            RightPoints.Add(Point);
                        }
                    }
                    
                    if (LeftPoints.Num() >= 4 && RightPoints.Num() >= 4)
                    {
                        FKConvexElem LeftElem, RightElem;
                        if (GenerateConvexHull(LeftPoints, LeftElem) && 
                            GenerateConvexHull(RightPoints, RightElem))
                        {
                            NewElems.Add(LeftElem);
                            NewElems.Add(RightElem);
                            ConvexElems.RemoveAt(i);
                            i--; 
                            NumToSplit--; 
                        }
                    }
                }
            }
        }
        
        ConvexElems.Append(NewElems);
    }
    
    if (ConvexElems.Num() > Params.TargetHullCount)
    {
        ConvexElems.Sort([](const FKConvexElem& A, const FKConvexElem& B) {
            return A.ElemBox.GetVolume() > B.ElemBox.GetVolume();
        });
        ConvexElems.SetNum(Params.TargetHullCount);
    }

    BodySetup->AggGeom.ConvexElems.Empty();
    for (FKConvexElem& Elem : ConvexElems)
    {
        if (Elem.VertexData.Num() >= 4)
        {
            BodySetup->AggGeom.ConvexElems.Add(Elem);
        }
    }

    const int32 FinalConvexCount = BodySetup->AggGeom.ConvexElems.Num();
    if (FinalConvexCount > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool FModelGenConvexDecomp::ExtractMeshData(UProceduralMeshComponent* ProceduralMeshComponent, FMeshData& OutMeshData)
{
    OutMeshData.Vertices.Empty();
    OutMeshData.Indices.Empty();

    for (int32 SectionIdx = 0; SectionIdx < ProceduralMeshComponent->GetNumSections(); ++SectionIdx)
    {
        FProcMeshSection* Section = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
        if (!Section) continue;

        const int32 FirstVertexIndex = OutMeshData.Vertices.Num();

        for (const FProcMeshVertex& ProcVertex : Section->ProcVertexBuffer)
        {
            OutMeshData.Vertices.Add(ProcVertex.Position);
        }

        for (uint32 Index : Section->ProcIndexBuffer)
        {
            OutMeshData.Indices.Add(FirstVertexIndex + Index);
        }
    }

    return OutMeshData.Vertices.Num() >= 4 && OutMeshData.Indices.Num() >= 3;
}

void FModelGenConvexDecomp::RecursiveDecompose(
    const FMeshData& MeshData,
    const TArray<int32>& TriangleIndices,
    const FDecompParams& Params,
    int32 CurrentDepth,
    TArray<FKConvexElem>& OutConvexElems)
{
    if (TriangleIndices.Num() == 0)
    {
        return;
    }

    TSet<int32> UniqueVertexIndices;
    for (int32 TriIdx : TriangleIndices)
    {
        const int32 BaseIdx = TriIdx * 3;
        if (BaseIdx + 2 < MeshData.Indices.Num())
        {
            UniqueVertexIndices.Add(MeshData.Indices[BaseIdx]);
            UniqueVertexIndices.Add(MeshData.Indices[BaseIdx + 1]);
            UniqueVertexIndices.Add(MeshData.Indices[BaseIdx + 2]);
        }
    }

    FBox CurrentBounds = CalculateTriangleBounds(MeshData, TriangleIndices);

    if (TriangleIndices.Num() <= 3)
    {
        TArray<FVector> Points;
        Points.Reserve(UniqueVertexIndices.Num());
        for (int32 VertIdx : UniqueVertexIndices)
        {
            if (VertIdx < MeshData.Vertices.Num())
            {
                FVector Vertex = MeshData.Vertices[VertIdx];
                if (CurrentBounds.IsValid && CurrentBounds.ExpandBy(1.0f).IsInside(Vertex))
                {
                    Points.Add(Vertex);
                }
            }
        }

        if (Points.Num() >= 4)
        {
            FKConvexElem ConvexElem;
            if (GenerateConvexHull(Points, ConvexElem))
            {
                OutConvexElems.Add(ConvexElem);
            }
        }
        return;
    }

    if (CurrentDepth >= Params.MaxDepth)
    {
        TArray<FVector> Points;
        Points.Reserve(UniqueVertexIndices.Num());
        for (int32 VertIdx : UniqueVertexIndices)
        {
            if (VertIdx < MeshData.Vertices.Num())
            {
                FVector Vertex = MeshData.Vertices[VertIdx];
                if (CurrentBounds.IsValid && CurrentBounds.ExpandBy(1.0f).IsInside(Vertex))
                {
                    Points.Add(Vertex);
                }
            }
        }

        if (Points.Num() >= 4)
        {
            FKConvexElem ConvexElem;
            if (GenerateConvexHull(Points, ConvexElem))
            {
                OutConvexElems.Add(ConvexElem);
            }
        }
        return;
    }

    if (UniqueVertexIndices.Num() <= Params.MaxHullVertices && 
        OutConvexElems.Num() >= Params.TargetHullCount)
    {
        TArray<FVector> Points;
        Points.Reserve(UniqueVertexIndices.Num());
        for (int32 VertIdx : UniqueVertexIndices)
        {
            if (VertIdx < MeshData.Vertices.Num())
            {
                FVector Vertex = MeshData.Vertices[VertIdx];
                if (CurrentBounds.IsValid && CurrentBounds.ExpandBy(1.0f).IsInside(Vertex))
                {
                    Points.Add(Vertex);
                }
            }
        }

        if (Points.Num() >= 4)
        {
            FKConvexElem ConvexElem;
            if (GenerateConvexHull(Points, ConvexElem))
            {
                OutConvexElems.Add(ConvexElem);
            }
        }
        return;
    }

    FBox Bounds = CalculateTriangleBounds(MeshData, TriangleIndices);
    if (!Bounds.IsValid)
    {
        return;
    }

    int32 LongestAxis = GetLongestAxis(Bounds);
    FVector Center = Bounds.GetCenter();
    FVector Extent = Bounds.GetExtent();

    FPlane SplitPlane;
    if (LongestAxis == 0) // X轴
    {
        SplitPlane = FPlane(Center, FVector(1, 0, 0));
    }
    else if (LongestAxis == 1) // Y轴
    {
        SplitPlane = FPlane(Center, FVector(0, 1, 0));
    }
    else // Z轴
    {
        SplitPlane = FPlane(Center, FVector(0, 0, 1));
    }

    TArray<int32> LeftTriangles, RightTriangles;
    SplitMeshByPlane(MeshData, TriangleIndices, SplitPlane, LeftTriangles, RightTriangles);

    const int32 TotalTriangles = TriangleIndices.Num();
    const float MinSplitRatio = 0.1f; 
    
    if (LeftTriangles.Num() < TotalTriangles * MinSplitRatio || 
        RightTriangles.Num() < TotalTriangles * MinSplitRatio)
    {
        FVector LeftCenter(0), RightCenter(0);
        int32 LeftCount = 0, RightCount = 0;
        
        for (int32 TriIdx : TriangleIndices)
        {
            const int32 BaseIdx = TriIdx * 3;
            if (BaseIdx + 2 >= MeshData.Indices.Num()) continue;
            
            FVector TriCenter(0);
            for (int32 i = 0; i < 3; ++i)
            {
                int32 VertIdx = MeshData.Indices[BaseIdx + i];
                if (VertIdx < MeshData.Vertices.Num())
                {
                    TriCenter += MeshData.Vertices[VertIdx];
                }
            }
            TriCenter /= 3.0f;
            
            float Dist = SplitPlane.PlaneDot(TriCenter);
            if (Dist < 0)
            {
                LeftCenter += TriCenter;
                LeftCount++;
            }
            else
            {
                RightCenter += TriCenter;
                RightCount++;
            }
        }
        
        if (LeftCount > 0 && RightCount > 0)
        {
            LeftCenter /= LeftCount;
            RightCenter /= RightCount;
            FVector NewCenter = (LeftCenter + RightCenter) * 0.5f;
            
            if (LongestAxis == 0)
            {
                SplitPlane = FPlane(NewCenter, FVector(1, 0, 0));
            }
            else if (LongestAxis == 1)
            {
                SplitPlane = FPlane(NewCenter, FVector(0, 1, 0));
            }
            else
            {
                SplitPlane = FPlane(NewCenter, FVector(0, 0, 1));
            }
            
            LeftTriangles.Empty();
            RightTriangles.Empty();
            SplitMeshByPlane(MeshData, TriangleIndices, SplitPlane, LeftTriangles, RightTriangles);
        }
    }

    const int32 RemainingHulls = Params.TargetHullCount - OutConvexElems.Num();
    const int32 DepthFactor = FMath::Max(1, Params.MaxDepth - CurrentDepth);
    const int32 MinTriangles = FMath::Max(4, TriangleIndices.Num() / FMath::Max(10, 20 - RemainingHulls * 2));

    if (LeftTriangles.Num() > 0)
    {
        FBox LeftBounds = CalculateTriangleBounds(MeshData, LeftTriangles);
        
        if (LeftTriangles.Num() <= MinTriangles)
        {
            TSet<int32> LeftVertices;
            for (int32 TriIdx : LeftTriangles)
            {
                const int32 BaseIdx = TriIdx * 3;
                if (BaseIdx + 2 < MeshData.Indices.Num())
                {
                    LeftVertices.Add(MeshData.Indices[BaseIdx]);
                    LeftVertices.Add(MeshData.Indices[BaseIdx + 1]);
                    LeftVertices.Add(MeshData.Indices[BaseIdx + 2]);
                }
            }
            TArray<FVector> LeftPoints;
            for (int32 VertIdx : LeftVertices)
            {
                if (VertIdx < MeshData.Vertices.Num())
                {
                    FVector Vertex = MeshData.Vertices[VertIdx];
                    if (LeftBounds.IsValid && LeftBounds.ExpandBy(1.0f).IsInside(Vertex))
                    {
                        LeftPoints.Add(Vertex);
                    }
                }
            }
            if (LeftPoints.Num() >= 4)
            {
                FKConvexElem ConvexElem;
                if (GenerateConvexHull(LeftPoints, ConvexElem))
                {
                    OutConvexElems.Add(ConvexElem);
                }
            }
        }
        else
        {
            if (LeftTriangles.Num() > MinTriangles)
            {
                RecursiveDecompose(MeshData, LeftTriangles, Params, CurrentDepth + 1, OutConvexElems);
            }
            else
            {
                TSet<int32> LeftVertices;
                for (int32 TriIdx : LeftTriangles)
                {
                    const int32 BaseIdx = TriIdx * 3;
                    if (BaseIdx + 2 < MeshData.Indices.Num())
                    {
                        LeftVertices.Add(MeshData.Indices[BaseIdx]);
                        LeftVertices.Add(MeshData.Indices[BaseIdx + 1]);
                        LeftVertices.Add(MeshData.Indices[BaseIdx + 2]);
                    }
                }
                TArray<FVector> LeftPoints;
                for (int32 VertIdx : LeftVertices)
                {
                    if (VertIdx < MeshData.Vertices.Num())
                    {
                        FVector Vertex = MeshData.Vertices[VertIdx];
                        if (LeftBounds.IsValid && LeftBounds.ExpandBy(1.0f).IsInside(Vertex))
                        {
                            LeftPoints.Add(Vertex);
                        }
                    }
                }
                if (LeftPoints.Num() >= 4)
                {
                    FKConvexElem ConvexElem;
                    if (GenerateConvexHull(LeftPoints, ConvexElem))
                    {
                        OutConvexElems.Add(ConvexElem);
                    }
                }
            }
        }
    }

    if (RightTriangles.Num() > 0)
    {
        FBox RightBounds = CalculateTriangleBounds(MeshData, RightTriangles);
        
        if (RightTriangles.Num() <= MinTriangles)
        {
            TSet<int32> RightVertices;
            for (int32 TriIdx : RightTriangles)
            {
                const int32 BaseIdx = TriIdx * 3;
                if (BaseIdx + 2 < MeshData.Indices.Num())
                {
                    RightVertices.Add(MeshData.Indices[BaseIdx]);
                    RightVertices.Add(MeshData.Indices[BaseIdx + 1]);
                    RightVertices.Add(MeshData.Indices[BaseIdx + 2]);
                }
            }
            TArray<FVector> RightPoints;
            for (int32 VertIdx : RightVertices)
            {
                if (VertIdx < MeshData.Vertices.Num())
                {
                    FVector Vertex = MeshData.Vertices[VertIdx];
                    if (RightBounds.IsValid && RightBounds.ExpandBy(1.0f).IsInside(Vertex))
                    {
                        RightPoints.Add(Vertex);
                    }
                }
            }
            if (RightPoints.Num() >= 4)
            {
                FKConvexElem ConvexElem;
                if (GenerateConvexHull(RightPoints, ConvexElem))
                {
                    OutConvexElems.Add(ConvexElem);
                }
            }
        }
        else
        {
            if (RightTriangles.Num() > MinTriangles)
            {
                RecursiveDecompose(MeshData, RightTriangles, Params, CurrentDepth + 1, OutConvexElems);
            }
            else
            {
                TSet<int32> RightVertices;
                for (int32 TriIdx : RightTriangles)
                {
                    const int32 BaseIdx = TriIdx * 3;
                    if (BaseIdx + 2 < MeshData.Indices.Num())
                    {
                        RightVertices.Add(MeshData.Indices[BaseIdx]);
                        RightVertices.Add(MeshData.Indices[BaseIdx + 1]);
                        RightVertices.Add(MeshData.Indices[BaseIdx + 2]);
                    }
                }
                TArray<FVector> RightPoints;
                for (int32 VertIdx : RightVertices)
                {
                    if (VertIdx < MeshData.Vertices.Num())
                    {
                        FVector Vertex = MeshData.Vertices[VertIdx];
                        if (RightBounds.IsValid && RightBounds.ExpandBy(1.0f).IsInside(Vertex))
                        {
                            RightPoints.Add(Vertex);
                        }
                    }
                }
                if (RightPoints.Num() >= 4)
                {
                    FKConvexElem ConvexElem;
                    if (GenerateConvexHull(RightPoints, ConvexElem))
                    {
                        OutConvexElems.Add(ConvexElem);
                    }
                }
            }
        }
    }
}

bool FModelGenConvexDecomp::GenerateConvexHull(const TArray<FVector>& Points, FKConvexElem& OutConvexElem)
{
    if (Points.Num() < 4)
    {
        return false;
    }

    TArray<FVector> UniquePoints;
    TMap<FIntVector, FVector> QuantizedMap;
    const float QuantizeScale = 100.0f;

    for (const FVector& Point : Points)
    {
        FIntVector Quantized(
            FMath::RoundToInt(Point.X * QuantizeScale),
            FMath::RoundToInt(Point.Y * QuantizeScale),
            FMath::RoundToInt(Point.Z * QuantizeScale)
        );

        if (!QuantizedMap.Contains(Quantized))
        {
            QuantizedMap.Add(Quantized, Point);
            UniquePoints.Add(Point);
        }
    }

    if (UniquePoints.Num() < 4)
    {
        UniquePoints = Points;
    }

    TArray<FFace*> AllFaces; 
    TArray<FFace*> PendingFaces;
    FFace* HeadFace = nullptr;
    TArray<int32> UnassignedPoints;
    
    for (int32 i = 0; i < UniquePoints.Num(); ++i)
    {
        UnassignedPoints.Add(i);
    }

    if (!BuildInitialTetrahedron(UniquePoints, AllFaces, UnassignedPoints))
    {
        OutConvexElem.VertexData = UniquePoints;
        OutConvexElem.UpdateElemBox();
        return UniquePoints.Num() >= 4;
    }

    PartitionOutsideSet(PendingFaces, AllFaces, UniquePoints, UnassignedPoints, HeadFace);

    const float Epsilon = 0.0001f;
    while (PendingFaces.Num() > 0)
    {
        FFace* CurrentFace = PendingFaces[0];
        PendingFaces.RemoveAt(0);

        int32 FurthestPointIdx = FindFurthestPoint(UniquePoints, CurrentFace);
        if (FurthestPointIdx == -1)
        {
            continue;
        }

        CurrentFace->OutsidePoints.Remove(FurthestPointIdx);

        TArray<FFace*> VisibleFaces;
        TMap<int32, FBoundaryEdge> BoundaryEdges;
        FindVisibleFaces(UniquePoints, FurthestPointIdx, CurrentFace, VisibleFaces, BoundaryEdges);

        if (VisibleFaces.Num() == 0 || BoundaryEdges.Num() < 3)
        {
            continue;
        }

        TArray<int32> UnassignedPointsForNewFaces;
        for (FFace* VisFace : VisibleFaces)
        {
            UnassignedPointsForNewFaces.Append(VisFace->OutsidePoints);
            VisFace->OutsidePoints.Empty();
            
            PendingFaces.Remove(VisFace);
        }

        TArray<FFace*> NewFaces;
        ConstructNewFaces(UniquePoints, FurthestPointIdx, NewFaces, BoundaryEdges, VisibleFaces);
        
        AllFaces.Append(NewFaces);

        for (FFace* VisFace : VisibleFaces)
        {
            AllFaces.Remove(VisFace);
            delete VisFace;
        }

        PartitionOutsideSet(PendingFaces, NewFaces, UniquePoints, UnassignedPointsForNewFaces, HeadFace);
    }

    TSet<int32> HullVertexIndices;
    TArray<FFace*> VisitedFaces;
    
    if (HeadFace)
    {
        TArray<FFace*> Stack;
        Stack.Add(HeadFace);
        HeadFace->bVisited = true;
        VisitedFaces.Add(HeadFace);

        while (Stack.Num() > 0)
        {
            FFace* Current = Stack.Pop();
            HullVertexIndices.Add(Current->V0);
            HullVertexIndices.Add(Current->V1);
            HullVertexIndices.Add(Current->V2);

            for (int32 i = 0; i < 3; ++i)
            {
                if (Current->Neighbor[i] && !Current->Neighbor[i]->bVisited)
                {
                    Current->Neighbor[i]->bVisited = true;
                    VisitedFaces.Add(Current->Neighbor[i]);
                    Stack.Add(Current->Neighbor[i]);
                }
            }
        }
    }

    for (FFace* Face : VisitedFaces)
    {
        Face->bVisited = false;
    }

    OutConvexElem.VertexData.Empty();
    OutConvexElem.VertexData.Reserve(HullVertexIndices.Num());
    for (int32 VertIdx : HullVertexIndices)
    {
        if (VertIdx < UniquePoints.Num())
        {
            OutConvexElem.VertexData.Add(UniquePoints[VertIdx]);
        }
    }

    for (FFace* Face : AllFaces)
    {
        delete Face;
    }

    if (OutConvexElem.VertexData.Num() > 32)
    {
        FVector Center(0);
        for (const FVector& P : OutConvexElem.VertexData)
        {
            Center += P;
        }
        Center /= OutConvexElem.VertexData.Num();

        OutConvexElem.VertexData.Sort([&Center](const FVector& A, const FVector& B) {
            return FVector::DistSquared(A, Center) > FVector::DistSquared(B, Center);
        });
        OutConvexElem.VertexData.SetNum(32);
    }

    OutConvexElem.UpdateElemBox();
    return OutConvexElem.VertexData.Num() >= 4;
}

bool FModelGenConvexDecomp::IsCollinear(const FVector& P0, const FVector& P1, const FVector& P2, float Epsilon)
{
    FVector Normal = FVector::CrossProduct(P1 - P0, P2 - P0);
    return Normal.SizeSquared() < Epsilon * Epsilon;
}

bool FModelGenConvexDecomp::IsCoplanar(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float Epsilon)
{
    FVector Normal = FVector::CrossProduct(P1 - P0, P2 - P0);
    float Dist = FMath::Abs(FVector::DotProduct(Normal, P3 - P0));
    return Dist < Epsilon;
}

bool FModelGenConvexDecomp::BuildInitialTetrahedron(
    const TArray<FVector>& Points,
    TArray<FFace*>& OutFaces,
    TArray<int32>& OutUnassignedPoints)
{
    if (Points.Num() < 4)
    {
        return false;
    }

    const float Epsilon = 0.0001f;

    int32 P0Idx = 0, P1Idx = 1, P2Idx = Points.Num() - 1;
    while (P1Idx < P2Idx && IsCollinear(Points[P0Idx], Points[P1Idx], Points[P2Idx], Epsilon))
    {
        P1Idx++;
    }
    if (P1Idx >= P2Idx)
    {
        return false;
    }

    int32 MinXIdx = 0, MaxXIdx = 0, MinYIdx = 0;
    for (int32 i = 1; i < Points.Num(); ++i)
    {
        if (Points[i].X < Points[MinXIdx].X) MinXIdx = i;
        else if (Points[i].X > Points[MaxXIdx].X) MaxXIdx = i;
        if (Points[i].Y < Points[MinYIdx].Y) MinYIdx = i;
    }

    if (!IsCollinear(Points[MinXIdx], Points[MaxXIdx], Points[MinYIdx], Epsilon))
    {
        P0Idx = MinXIdx;
        P1Idx = MaxXIdx;
        P2Idx = MinYIdx;
    }

    FVector Normal = FVector::CrossProduct(Points[P1Idx] - Points[P0Idx], Points[P2Idx] - Points[P0Idx]);
    float NormalLen = Normal.Size();
    if (NormalLen < Epsilon)
    {
        return false;
    }
    Normal /= NormalLen;
    float D = -FVector::DotProduct(Normal, Points[P0Idx]);

    float MinDist = 0.0f, MaxDist = 0.0f;
    int32 MinDistIdx = -1, MaxDistIdx = -1;
    for (int32 i = 0; i < Points.Num(); ++i)
    {
        if (i == P0Idx || i == P1Idx || i == P2Idx) continue;
        float Dist = FVector::DotProduct(Normal, Points[i]) + D;
        if (Dist < MinDist)
        {
            MinDist = Dist;
            MinDistIdx = i;
        }
        if (Dist > MaxDist)
        {
            MaxDist = Dist;
            MaxDistIdx = i;
        }
    }

    int32 P3Idx = MaxDistIdx;
    if (P3Idx == -1 || IsCoplanar(Points[P0Idx], Points[P1Idx], Points[P2Idx], Points[P3Idx], Epsilon))
    {
        if (MinDistIdx != -1)
        {
            int32 Temp = P0Idx;
            P0Idx = P2Idx;
            P2Idx = Temp;
            P3Idx = MinDistIdx;
            if (IsCoplanar(Points[P0Idx], Points[P1Idx], Points[P2Idx], Points[P3Idx], Epsilon))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    FFace* F0 = new FFace(P0Idx, P2Idx, P1Idx);
    FFace* F1 = new FFace(P0Idx, P1Idx, P3Idx);
    FFace* F2 = new FFace(P0Idx, P3Idx, P2Idx);
    FFace* F3 = new FFace(P1Idx, P2Idx, P3Idx);

    F0->Neighbor[0] = F2;
    F0->Neighbor[1] = F3;
    F0->Neighbor[2] = F1;

    F1->Neighbor[0] = F0;
    F1->Neighbor[1] = F3;
    F1->Neighbor[2] = F2;

    F2->Neighbor[0] = F1;
    F2->Neighbor[1] = F3;
    F2->Neighbor[2] = F0;

    F3->Neighbor[0] = F0;
    F3->Neighbor[1] = F2;
    F3->Neighbor[2] = F1;

    OutFaces.Add(F0);
    OutFaces.Add(F1);
    OutFaces.Add(F2);
    OutFaces.Add(F3);

    OutUnassignedPoints.Remove(P0Idx);
    OutUnassignedPoints.Remove(P1Idx);
    OutUnassignedPoints.Remove(P2Idx);
    OutUnassignedPoints.Remove(P3Idx);

    return true;
}

int32 FModelGenConvexDecomp::FindFurthestPoint(const TArray<FVector>& Points, FFace* Face)
{
    if (!Face || Face->OutsidePoints.Num() == 0)
    {
        return -1;
    }

    float MaxDist = -1.0f;
    int32 FurthestIdx = -1;

    for (int32 PointIdx : Face->OutsidePoints)
    {
        if (PointIdx >= Points.Num()) continue;
        float Dist = Face->GetDistance(Points, Points[PointIdx]);
        if (Dist > MaxDist)
        {
            MaxDist = Dist;
            FurthestIdx = PointIdx;
        }
    }

    return FurthestIdx;
}

void FModelGenConvexDecomp::FindVisibleFaces(
    const TArray<FVector>& Points,
    int32 PointIndex,
    FFace* StartFace,
    TArray<FFace*>& OutVisibleFaces,
    TMap<int32, FBoundaryEdge>& OutBoundaryEdges)
{
    OutVisibleFaces.Empty();
    OutBoundaryEdges.Empty();

    if (!StartFace) return;

    TSet<FFace*> BoundaryFaces;

    TArray<FFace*> Stack;
    Stack.Add(StartFace);
    StartFace->bVisited = true;
    OutVisibleFaces.Add(StartFace);

    while (Stack.Num() > 0)
    {
        FFace* Current = Stack.Pop();

        for (int32 i = 0; i < 3; ++i)
        {
            FFace* Neighbor = Current->Neighbor[i];
            if (!Neighbor) continue;
            
            if (Neighbor->bVisited) continue;

            float Dist = Neighbor->GetDistance(Points, Points[PointIndex]);
            if (Dist > 0.0001f)
            {
                Neighbor->bVisited = true;
                OutVisibleFaces.Add(Neighbor);
                Stack.Add(Neighbor);
            }
            else
            {
                int32 V0 = Current->V0;
                int32 V1 = Current->V1;
                int32 V2 = Current->V2;
                
                int32 EdgeV0, EdgeV1;
                if (i == 0)
                {
                    EdgeV0 = V0;
                    EdgeV1 = V1;
                }
                else if (i == 1)
                {
                    EdgeV0 = V1;
                    EdgeV1 = V2;
                }
                else
                {
                    EdgeV0 = V2;
                    EdgeV1 = V0;
                }
                
                int32 Key = FMath::Min(EdgeV0, EdgeV1) * 10000 + FMath::Max(EdgeV0, EdgeV1);
                if (!OutBoundaryEdges.Contains(Key))
                {
                    OutBoundaryEdges.Add(Key, FBoundaryEdge(EdgeV0, EdgeV1, Neighbor));
                    BoundaryFaces.Add(Neighbor);
                }
            }
        }
    }

    for (FFace* Face : OutVisibleFaces)
    {
        Face->bVisited = false;
    }
    for (FFace* Face : BoundaryFaces)
    {
        Face->bVisited = false;
    }
}

void FModelGenConvexDecomp::ConstructNewFaces(
    const TArray<FVector>& Points,
    int32 PointIndex,
    TArray<FFace*>& OutNewFaces,
    TMap<int32, FBoundaryEdge>& BoundaryEdges,
    const TArray<FFace*>& VisibleFaces)
{
    OutNewFaces.Empty();

    if (BoundaryEdges.Num() < 3) return;

    TArray<FBoundaryEdge> EdgeList;
    for (auto& Pair : BoundaryEdges)
    {
        EdgeList.Add(Pair.Value);
    }

    TArray<FBoundaryEdge> OrderedEdges;
    if (EdgeList.Num() > 0)
    {
        OrderedEdges.Add(EdgeList[0]);
        EdgeList.RemoveAt(0);

        while (EdgeList.Num() > 0)
        {
            int32 LastV1 = OrderedEdges.Last().V1;
            bool bFound = false;
            for (int32 i = 0; i < EdgeList.Num(); ++i)
            {
                if (EdgeList[i].V0 == LastV1)
                {
                    OrderedEdges.Add(EdgeList[i]);
                    EdgeList.RemoveAt(i);
                    bFound = true;
                    break;
                }
            }
            if (!bFound) break;
        }
    }

    for (const FBoundaryEdge& Edge : OrderedEdges)
    {
        FFace* NewFace = new FFace(PointIndex, Edge.V0, Edge.V1);
        
        NewFace->Neighbor[1] = Edge.NeighborFace;
        if (Edge.NeighborFace)
        {
            for (int32 i = 0; i < 3; ++i)
            {
                FFace* OldNeighbor = Edge.NeighborFace->Neighbor[i];
                if (!OldNeighbor) continue;
                
                bool bSharesEdge = false;
                if ((OldNeighbor->V0 == Edge.V0 && OldNeighbor->V1 == Edge.V1) ||
                    (OldNeighbor->V1 == Edge.V0 && OldNeighbor->V2 == Edge.V1) ||
                    (OldNeighbor->V2 == Edge.V0 && OldNeighbor->V0 == Edge.V1) ||
                    (OldNeighbor->V0 == Edge.V1 && OldNeighbor->V1 == Edge.V0) ||
                    (OldNeighbor->V1 == Edge.V1 && OldNeighbor->V2 == Edge.V0) ||
                    (OldNeighbor->V2 == Edge.V1 && OldNeighbor->V0 == Edge.V0))
                {
                    bSharesEdge = true;
                }
                
                if (bSharesEdge)
                {
                    bool bIsVisible = VisibleFaces.Contains(OldNeighbor);
                    
                    if (bIsVisible)
                    {
                        Edge.NeighborFace->Neighbor[i] = NewFace;
                        break;
                    }
                }
            }
        }

        OutNewFaces.Add(NewFace);
    }

    for (int32 i = 0; i < OutNewFaces.Num(); ++i)
    {
        int32 PrevIdx = (i - 1 + OutNewFaces.Num()) % OutNewFaces.Num();
        int32 NextIdx = (i + 1) % OutNewFaces.Num();
        OutNewFaces[i]->Neighbor[0] = OutNewFaces[PrevIdx];
        OutNewFaces[i]->Neighbor[2] = OutNewFaces[NextIdx];
    }
}

void FModelGenConvexDecomp::PartitionOutsideSet(
    TArray<FFace*>& PendingFaces,
    TArray<FFace*>& NewFaces,
    const TArray<FVector>& Points,
    TArray<int32>& UnassignedPoints,
    FFace*& HeadFace)
{
    for (FFace* Face : NewFaces)
    {
        Face->OutsidePoints.Empty();
        
        for (int32 i = UnassignedPoints.Num() - 1; i >= 0; --i)
        {
            int32 PointIdx = UnassignedPoints[i];
            if (PointIdx >= Points.Num()) continue;
            if (PointIdx == Face->V0 || PointIdx == Face->V1 || PointIdx == Face->V2) continue;

            float Dist = Face->GetDistance(Points, Points[PointIdx]);
            if (Dist > 0.0001f)
            {
                Face->OutsidePoints.Add(PointIdx);
                UnassignedPoints.RemoveAt(i);
            }
        }

        if (Face->OutsidePoints.Num() > 0)
        {
            PendingFaces.Add(Face);
        }
        else
        {
            HeadFace = Face;
        }
    }
}


FBox FModelGenConvexDecomp::CalculateTriangleBounds(const FMeshData& MeshData, const TArray<int32>& TriangleIndices)
{
    FBox Bounds(ForceInitToZero);
    bool bFirst = true;

    for (int32 TriIdx : TriangleIndices)
    {
        const int32 BaseIdx = TriIdx * 3;
        if (BaseIdx + 2 < MeshData.Indices.Num())
        {
            for (int32 i = 0; i < 3; ++i)
            {
                int32 VertIdx = MeshData.Indices[BaseIdx + i];
                if (VertIdx < MeshData.Vertices.Num())
                {
                    if (bFirst)
                    {
                        Bounds = FBox(MeshData.Vertices[VertIdx], MeshData.Vertices[VertIdx]);
                        bFirst = false;
                    }
                    else
                    {
                        Bounds += MeshData.Vertices[VertIdx];
                    }
                }
            }
        }
    }

    return Bounds;
}

void FModelGenConvexDecomp::SplitMeshByPlane(
    const FMeshData& MeshData,
    const TArray<int32>& TriangleIndices,
    const FPlane& SplitPlane,
    TArray<int32>& OutLeftTriangles,
    TArray<int32>& OutRightTriangles)
{
    OutLeftTriangles.Empty();
    OutRightTriangles.Empty();

    const float Epsilon = 0.0001f;

    for (int32 TriIdx : TriangleIndices)
    {
        const int32 BaseIdx = TriIdx * 3;
        if (BaseIdx + 2 >= MeshData.Indices.Num())
        {
            continue;
        }

        int32 V0Idx = MeshData.Indices[BaseIdx];
        int32 V1Idx = MeshData.Indices[BaseIdx + 1];
        int32 V2Idx = MeshData.Indices[BaseIdx + 2];

        if (V0Idx >= MeshData.Vertices.Num() || 
            V1Idx >= MeshData.Vertices.Num() || 
            V2Idx >= MeshData.Vertices.Num())
        {
            continue;
        }

        FVector V0 = MeshData.Vertices[V0Idx];
        FVector V1 = MeshData.Vertices[V1Idx];
        FVector V2 = MeshData.Vertices[V2Idx];

        FVector TriCenter = (V0 + V1 + V2) / 3.0f;
        float CenterDist = SplitPlane.PlaneDot(TriCenter);

        if (CenterDist < -Epsilon)
        {
            OutLeftTriangles.Add(TriIdx);
        }
        else if (CenterDist > Epsilon)
        {
            OutRightTriangles.Add(TriIdx);
        }
        else
        {
            float D0 = SplitPlane.PlaneDot(V0);
            float D1 = SplitPlane.PlaneDot(V1);
            float D2 = SplitPlane.PlaneDot(V2);
            
            int32 LeftCount = 0;
            if (D0 < -Epsilon) LeftCount++;
            if (D1 < -Epsilon) LeftCount++;
            if (D2 < -Epsilon) LeftCount++;
            
            if (LeftCount >= 2)
            {
                OutLeftTriangles.Add(TriIdx);
            }
            else
            {
                OutRightTriangles.Add(TriIdx);
            }
        }
    }
}

int32 FModelGenConvexDecomp::GetLongestAxis(const FBox& Bounds)
{
    FVector Extent = Bounds.GetExtent();
    if (Extent.X >= Extent.Y && Extent.X >= Extent.Z)
    {
        return 0;
    }
    else if (Extent.Y >= Extent.Z)
    {
        return 1;
    }
    else
    {
        return 2;
    }
}

