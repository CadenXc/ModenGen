// Copyright (c) 2024. All rights reserved.

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
        UE_LOG(LogTemp, Warning, TEXT("ConvexDecomp: 输入参数无效"));
        return false;
    }

    // 1. 提取网格数据
    FMeshData MeshData;
    if (!ExtractMeshData(ProceduralMeshComponent, MeshData))
    {
        UE_LOG(LogTemp, Warning, TEXT("ConvexDecomp: 无法提取网格数据"));
        return false;
    }

    if (MeshData.Vertices.Num() < 4 || MeshData.Indices.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("ConvexDecomp: 顶点或索引数量不足（顶点=%d, 索引=%d）"), 
            MeshData.Vertices.Num(), MeshData.Indices.Num());
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("ConvexDecomp: 开始计算，顶点数=%d, 三角形数=%d"), 
        MeshData.Vertices.Num(), MeshData.Indices.Num() / 3);

    // 2. 配置参数
    FDecompParams Params;
    Params.TargetHullCount = FMath::Clamp(HullCount, 1, 64);
    Params.MaxHullVertices = FMath::Clamp(MaxHullVerts, 6, 32);
    // 将HullPrecision映射到最大递归深度（经验值：100000 -> 深度10）
    // 增加深度以支持更复杂的分割
    Params.MaxDepth = FMath::Clamp(FMath::FloorToInt(FMath::Log2(static_cast<float>(HullPrecision) / 3000.0f)) + 5, 5, 15);
    Params.MinVolumeRatio = 0.001f; // 最小体积比例为0.1%（进一步降低阈值）

    UE_LOG(LogTemp, Log, TEXT("ConvexDecomp: 参数 - HullCount=%d, MaxHullVerts=%d, MaxDepth=%d"), 
        Params.TargetHullCount, Params.MaxHullVertices, Params.MaxDepth);

    // 3. 初始化所有三角形的索引
    TArray<int32> AllTriangleIndices;
    const int32 NumTriangles = MeshData.Indices.Num() / 3;
    AllTriangleIndices.Reserve(NumTriangles);
    for (int32 i = 0; i < NumTriangles; ++i)
    {
        AllTriangleIndices.Add(i);
    }

    // 4. 递归分解
    TArray<FKConvexElem> ConvexElems;
    RecursiveDecompose(MeshData, AllTriangleIndices, Params, 0, ConvexElems);

    // 5. 如果凸包数量不足，尝试进一步分割较大的凸包
    if (ConvexElems.Num() < Params.TargetHullCount && ConvexElems.Num() > 0)
    {
        // 按体积排序，优先分割最大的
        ConvexElems.Sort([](const FKConvexElem& A, const FKConvexElem& B) {
            return A.ElemBox.GetVolume() > B.ElemBox.GetVolume();
        });
        
        // 尝试分割最大的几个凸包
        int32 NumToSplit = FMath::Min(ConvexElems.Num(), Params.TargetHullCount - ConvexElems.Num());
        TArray<FKConvexElem> NewElems;
        
        for (int32 i = 0; i < NumToSplit && ConvexElems.Num() + NewElems.Num() < Params.TargetHullCount; ++i)
        {
            const FKConvexElem& LargeElem = ConvexElems[i];
            // 如果顶点数足够多，尝试分割
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
                    
                    // 如果分割后两边都有足够的点，创建两个新凸包
                    if (LeftPoints.Num() >= 4 && RightPoints.Num() >= 4)
                    {
                        FKConvexElem LeftElem, RightElem;
                        if (GenerateConvexHull(LeftPoints, LeftElem) && 
                            GenerateConvexHull(RightPoints, RightElem))
                        {
                            NewElems.Add(LeftElem);
                            NewElems.Add(RightElem);
                            // 移除原来的大凸包
                            ConvexElems.RemoveAt(i);
                            i--; // 调整索引
                            NumToSplit--; // 减少需要分割的数量
                        }
                    }
                }
            }
        }
        
        ConvexElems.Append(NewElems);
    }
    
    // 6. 限制凸包数量（如果超过目标数量，保留体积较大的）
    if (ConvexElems.Num() > Params.TargetHullCount)
    {
        // 按体积排序，保留较大的
        ConvexElems.Sort([](const FKConvexElem& A, const FKConvexElem& B) {
            return A.ElemBox.GetVolume() > B.ElemBox.GetVolume();
        });
        ConvexElems.SetNum(Params.TargetHullCount);
    }

    // 7. 添加到BodySetup
    BodySetup->AggGeom.ConvexElems.Empty();
    for (FKConvexElem& Elem : ConvexElems)
    {
        if (Elem.VertexData.Num() >= 4)
        {
            BodySetup->AggGeom.ConvexElems.Add(Elem);
            UE_LOG(LogTemp, Log, TEXT("ConvexDecomp: 凸包 - 顶点数=%d, 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
                Elem.VertexData.Num(),
                Elem.ElemBox.GetCenter().X, Elem.ElemBox.GetCenter().Y, Elem.ElemBox.GetCenter().Z,
                Elem.ElemBox.GetSize().X, Elem.ElemBox.GetSize().Y, Elem.ElemBox.GetSize().Z);
        }
    }

    const int32 FinalConvexCount = BodySetup->AggGeom.ConvexElems.Num();
    if (FinalConvexCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("ConvexDecomp: 最终生成 %d 个有效凸包元素"), FinalConvexCount);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ConvexDecomp: 未生成有效的凸包元素"));
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

        // 添加顶点
        for (const FProcMeshVertex& ProcVertex : Section->ProcVertexBuffer)
        {
            OutMeshData.Vertices.Add(ProcVertex.Position);
        }

        // 添加索引（需要调整偏移）
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

    // 收集所有相关顶点（只使用这些三角形中的顶点，确保不包含其他区域的顶点）
    TSet<int32> UniqueVertexIndices;
    for (int32 TriIdx : TriangleIndices)
    {
        const int32 BaseIdx = TriIdx * 3;
        if (BaseIdx + 2 < MeshData.Indices.Num())
        {
            // 只添加属于这些三角形的顶点
            UniqueVertexIndices.Add(MeshData.Indices[BaseIdx]);
            UniqueVertexIndices.Add(MeshData.Indices[BaseIdx + 1]);
            UniqueVertexIndices.Add(MeshData.Indices[BaseIdx + 2]);
        }
    }

    // 计算当前区域的包围盒，用于后续过滤顶点
    FBox CurrentBounds = CalculateTriangleBounds(MeshData, TriangleIndices);

    // 如果三角形数量太少，直接生成凸包
    if (TriangleIndices.Num() <= 3)
    {
        TArray<FVector> Points;
        Points.Reserve(UniqueVertexIndices.Num());
        // 只使用在当前包围盒内的顶点，避免包含其他区域的顶点
        for (int32 VertIdx : UniqueVertexIndices)
        {
            if (VertIdx < MeshData.Vertices.Num())
            {
                FVector Vertex = MeshData.Vertices[VertIdx];
                // 检查顶点是否在当前区域内（使用扩展的包围盒，允许小的容差）
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

    // 如果达到最大深度，直接生成凸包
    if (CurrentDepth >= Params.MaxDepth)
    {
        TArray<FVector> Points;
        Points.Reserve(UniqueVertexIndices.Num());
        // 只使用在当前包围盒内的顶点
        for (int32 VertIdx : UniqueVertexIndices)
        {
            if (VertIdx < MeshData.Vertices.Num())
            {
                FVector Vertex = MeshData.Vertices[VertIdx];
                // 检查顶点是否在当前区域内
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

    // 如果已经达到目标数量，但还可以继续分割以获得更好的效果
    // 只有当顶点数很少时才停止
    if (UniqueVertexIndices.Num() <= Params.MaxHullVertices && 
        OutConvexElems.Num() >= Params.TargetHullCount)
    {
        TArray<FVector> Points;
        Points.Reserve(UniqueVertexIndices.Num());
        // 只使用在当前包围盒内的顶点
        for (int32 VertIdx : UniqueVertexIndices)
        {
            if (VertIdx < MeshData.Vertices.Num())
            {
                FVector Vertex = MeshData.Vertices[VertIdx];
                // 检查顶点是否在当前区域内
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

    // 计算包围盒
    FBox Bounds = CalculateTriangleBounds(MeshData, TriangleIndices);
    if (!Bounds.IsValid)
    {
        return;
    }

    // 选择最长轴进行分割
    int32 LongestAxis = GetLongestAxis(Bounds);
    FVector Center = Bounds.GetCenter();
    FVector Extent = Bounds.GetExtent();

    // 创建分割平面
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

    // 分割网格
    TArray<int32> LeftTriangles, RightTriangles;
    SplitMeshByPlane(MeshData, TriangleIndices, SplitPlane, LeftTriangles, RightTriangles);

    // 如果分割后一边的三角形数量太少（少于10%），说明分割不够均匀，尝试调整分割平面
    const int32 TotalTriangles = TriangleIndices.Num();
    const float MinSplitRatio = 0.1f; // 至少保留10%的三角形
    
    if (LeftTriangles.Num() < TotalTriangles * MinSplitRatio || 
        RightTriangles.Num() < TotalTriangles * MinSplitRatio)
    {
        // 分割不够均匀，尝试使用更智能的分割策略
        // 基于三角形中心点的分布来调整分割平面
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
        
        // 如果分割不均匀，使用新的中心点重新分割
        if (LeftCount > 0 && RightCount > 0)
        {
            LeftCenter /= LeftCount;
            RightCenter /= RightCount;
            FVector NewCenter = (LeftCenter + RightCenter) * 0.5f;
            
            // 使用新的中心点创建分割平面
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
            
            // 重新分割
            LeftTriangles.Empty();
            RightTriangles.Empty();
            SplitMeshByPlane(MeshData, TriangleIndices, SplitPlane, LeftTriangles, RightTriangles);
        }
    }

    // 检查分割是否有效
    // 使用三角形数量作为主要判断条件，而不是体积（体积可能不准确）
    // 根据目标凸包数量和当前深度动态调整最小三角形数量
    // 如果还需要更多凸包，允许更细的分割
    const int32 RemainingHulls = Params.TargetHullCount - OutConvexElems.Num();
    const int32 DepthFactor = FMath::Max(1, Params.MaxDepth - CurrentDepth);
    const int32 MinTriangles = FMath::Max(4, TriangleIndices.Num() / FMath::Max(10, 20 - RemainingHulls * 2));

    // 递归处理左右两部分
    if (LeftTriangles.Num() > 0)
    {
        // 计算左侧区域的包围盒
        FBox LeftBounds = CalculateTriangleBounds(MeshData, LeftTriangles);
        
        // 如果三角形数量太少，直接生成凸包
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
            // 只使用在左侧包围盒内的顶点
            for (int32 VertIdx : LeftVertices)
            {
                if (VertIdx < MeshData.Vertices.Num())
                {
                    FVector Vertex = MeshData.Vertices[VertIdx];
                    // 检查顶点是否在左侧区域内
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
            // 如果三角形数量足够，继续递归分割
            if (LeftTriangles.Num() > MinTriangles)
            {
                RecursiveDecompose(MeshData, LeftTriangles, Params, CurrentDepth + 1, OutConvexElems);
            }
            else
            {
                // 体积太小，直接生成凸包
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
                // 只使用在左侧包围盒内的顶点
                for (int32 VertIdx : LeftVertices)
                {
                    if (VertIdx < MeshData.Vertices.Num())
                    {
                        FVector Vertex = MeshData.Vertices[VertIdx];
                        // 检查顶点是否在左侧区域内
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
        // 计算右侧区域的包围盒
        FBox RightBounds = CalculateTriangleBounds(MeshData, RightTriangles);
        
        // 如果三角形数量太少，直接生成凸包
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
            // 只使用在右侧包围盒内的顶点
            for (int32 VertIdx : RightVertices)
            {
                if (VertIdx < MeshData.Vertices.Num())
                {
                    FVector Vertex = MeshData.Vertices[VertIdx];
                    // 检查顶点是否在右侧区域内
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
            // 如果三角形数量足够，继续递归分割
            if (RightTriangles.Num() > MinTriangles)
            {
                RecursiveDecompose(MeshData, RightTriangles, Params, CurrentDepth + 1, OutConvexElems);
            }
            else
            {
                // 体积太小，直接生成凸包
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
                // 只使用在右侧包围盒内的顶点
                for (int32 VertIdx : RightVertices)
                {
                    if (VertIdx < MeshData.Vertices.Num())
                    {
                        FVector Vertex = MeshData.Vertices[VertIdx];
                        // 检查顶点是否在右侧区域内
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

    // 去重：移除重复的点
    TArray<FVector> UniquePoints;
    TMap<FIntVector, FVector> QuantizedMap;
    const float QuantizeScale = 100.0f; // 1cm精度

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

    // 使用QuickHull算法（基于参考实现）
    TArray<FFace*> AllFaces; // 用于内存管理
    TArray<FFace*> PendingFaces;
    FFace* HeadFace = nullptr;
    TArray<int32> UnassignedPoints;
    
    // 初始化未分配点列表
    for (int32 i = 0; i < UniquePoints.Num(); ++i)
    {
        UnassignedPoints.Add(i);
    }

    // 构建初始四面体
    if (!BuildInitialTetrahedron(UniquePoints, AllFaces, UnassignedPoints))
    {
        // 如果无法构建四面体，使用简化的凸包
        OutConvexElem.VertexData = UniquePoints;
        OutConvexElem.UpdateElemBox();
        return UniquePoints.Num() >= 4;
    }

    // 分配外部点集
    PartitionOutsideSet(PendingFaces, AllFaces, UniquePoints, UnassignedPoints, HeadFace);

    // 如果存在待处理面，进行QuickHull扫描
    const float Epsilon = 0.0001f;
    while (PendingFaces.Num() > 0)
    {
        // 从待处理面中选择一个
        FFace* CurrentFace = PendingFaces[0];
        PendingFaces.RemoveAt(0);

        // 找到最远的点
        int32 FurthestPointIdx = FindFurthestPoint(UniquePoints, CurrentFace);
        if (FurthestPointIdx == -1)
        {
            continue;
        }

        // 从外部点集中移除这个点
        CurrentFace->OutsidePoints.Remove(FurthestPointIdx);

        // 找到所有可见面
        TArray<FFace*> VisibleFaces;
        TMap<int32, FBoundaryEdge> BoundaryEdges;
        FindVisibleFaces(UniquePoints, FurthestPointIdx, CurrentFace, VisibleFaces, BoundaryEdges);

        if (VisibleFaces.Num() == 0 || BoundaryEdges.Num() < 3)
        {
            continue;
        }

        // 收集所有可见面的外部点
        TArray<int32> UnassignedPointsForNewFaces;
        for (FFace* VisFace : VisibleFaces)
        {
            UnassignedPointsForNewFaces.Append(VisFace->OutsidePoints);
            VisFace->OutsidePoints.Empty();
            
            // 从待处理列表中移除可见面
            PendingFaces.Remove(VisFace);
        }

        // 构建新面
        TArray<FFace*> NewFaces;
        ConstructNewFaces(UniquePoints, FurthestPointIdx, NewFaces, BoundaryEdges, VisibleFaces);
        
        // 将新面添加到AllFaces
        AllFaces.Append(NewFaces);

        // 删除可见面（从AllFaces中移除并释放内存）
        for (FFace* VisFace : VisibleFaces)
        {
            AllFaces.Remove(VisFace);
            delete VisFace;
        }

        // 分配外部点集
        PartitionOutsideSet(PendingFaces, NewFaces, UniquePoints, UnassignedPointsForNewFaces, HeadFace);
    }

    // 从面中提取所有唯一的顶点
    TSet<int32> HullVertexIndices;
    TArray<FFace*> VisitedFaces;
    
    // 递归遍历所有面（从HeadFace开始）
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

    // 重置访问标志
    for (FFace* Face : VisitedFaces)
    {
        Face->bVisited = false;
    }

    // 构建输出
    OutConvexElem.VertexData.Empty();
    OutConvexElem.VertexData.Reserve(HullVertexIndices.Num());
    for (int32 VertIdx : HullVertexIndices)
    {
        if (VertIdx < UniquePoints.Num())
        {
            OutConvexElem.VertexData.Add(UniquePoints[VertIdx]);
        }
    }

    // 清理内存
    for (FFace* Face : AllFaces)
    {
        delete Face;
    }

    // 如果顶点数太多，进行简化
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

    // 检查是否所有点共线
    int32 P0Idx = 0, P1Idx = 1, P2Idx = Points.Num() - 1;
    while (P1Idx < P2Idx && IsCollinear(Points[P0Idx], Points[P1Idx], Points[P2Idx], Epsilon))
    {
        P1Idx++;
    }
    if (P1Idx >= P2Idx)
    {
        return false; // 所有点共线
    }

    // 找到X轴最小、最大和Y轴最小的点
    int32 MinXIdx = 0, MaxXIdx = 0, MinYIdx = 0;
    for (int32 i = 1; i < Points.Num(); ++i)
    {
        if (Points[i].X < Points[MinXIdx].X) MinXIdx = i;
        else if (Points[i].X > Points[MaxXIdx].X) MaxXIdx = i;
        if (Points[i].Y < Points[MinYIdx].Y) MinYIdx = i;
    }

    // 如果这三个点不共线，使用它们
    if (!IsCollinear(Points[MinXIdx], Points[MaxXIdx], Points[MinYIdx], Epsilon))
    {
        P0Idx = MinXIdx;
        P1Idx = MaxXIdx;
        P2Idx = MinYIdx;
    }

    // 计算平面
    FVector Normal = FVector::CrossProduct(Points[P1Idx] - Points[P0Idx], Points[P2Idx] - Points[P0Idx]);
    float NormalLen = Normal.Size();
    if (NormalLen < Epsilon)
    {
        return false;
    }
    Normal /= NormalLen;
    float D = -FVector::DotProduct(Normal, Points[P0Idx]);

    // 找到距离平面最远的点
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
        // 尝试使用最小距离的点
        if (MinDistIdx != -1)
        {
            // 交换P0和P2
            int32 Temp = P0Idx;
            P0Idx = P2Idx;
            P2Idx = Temp;
            P3Idx = MinDistIdx;
            if (IsCoplanar(Points[P0Idx], Points[P1Idx], Points[P2Idx], Points[P3Idx], Epsilon))
            {
                return false; // 所有点共面
            }
        }
        else
        {
            return false;
        }
    }

    // 创建四个面
    FFace* F0 = new FFace(P0Idx, P2Idx, P1Idx);
    FFace* F1 = new FFace(P0Idx, P1Idx, P3Idx);
    FFace* F2 = new FFace(P0Idx, P3Idx, P2Idx);
    FFace* F3 = new FFace(P1Idx, P2Idx, P3Idx);

    // 设置邻居关系
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

    // 从未分配点列表中移除已使用的点
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

    TSet<FFace*> BoundaryFaces; // 记录所有边界面的集合

    // 使用BFS找到所有可见面
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
            
            // 如果邻居已经被访问过，跳过
            if (Neighbor->bVisited) continue;

            float Dist = Neighbor->GetDistance(Points, Points[PointIndex]);
            if (Dist > 0.0001f) // 点在面的外部，面可见
            {
                Neighbor->bVisited = true;
                OutVisibleFaces.Add(Neighbor);
                Stack.Add(Neighbor);
            }
            else // 面不可见，边是边界
            {
                // 记录边界边
                int32 V0 = Current->V0;
                int32 V1 = Current->V1;
                int32 V2 = Current->V2;
                
                // 根据邻居索引确定边的顶点
                // Neighbor[0] 对应边 V0-V1
                // Neighbor[1] 对应边 V1-V2
                // Neighbor[2] 对应边 V2-V0
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
                
                // 使用较小的索引作为key（确保边的方向一致）
                int32 Key = FMath::Min(EdgeV0, EdgeV1) * 10000 + FMath::Max(EdgeV0, EdgeV1);
                if (!OutBoundaryEdges.Contains(Key))
                {
                    OutBoundaryEdges.Add(Key, FBoundaryEdge(EdgeV0, EdgeV1, Neighbor));
                    BoundaryFaces.Add(Neighbor);
                }
            }
        }
    }

    // 重置访问标志
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

    // 构建边界边的链
    TArray<FBoundaryEdge> EdgeList;
    for (auto& Pair : BoundaryEdges)
    {
        EdgeList.Add(Pair.Value);
    }

    // 按顺序连接边界边
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

    // 为每条边界边创建新面
    for (const FBoundaryEdge& Edge : OrderedEdges)
    {
        FFace* NewFace = new FFace(PointIndex, Edge.V0, Edge.V1);
        
        // 设置邻居关系
        NewFace->Neighbor[1] = Edge.NeighborFace;
        if (Edge.NeighborFace)
        {
            // 更新邻居面的邻居关系（找到指向已删除面的邻居）
            // 需要找到Edge.NeighborFace中哪条边对应这条边界边
            for (int32 i = 0; i < 3; ++i)
            {
                FFace* OldNeighbor = Edge.NeighborFace->Neighbor[i];
                if (!OldNeighbor) continue;
                
                // 检查这个邻居是否包含Edge的两个顶点（说明是共享这条边的面）
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
                
                // 如果这个邻居共享边界边，并且它在可见面列表中（将被删除），则更新邻居关系
                if (bSharesEdge)
                {
                    // 检查OldNeighbor是否在可见面列表中
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

    // 设置新面之间的邻居关系
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
    // 为每个新面分配外部点
    for (FFace* Face : NewFaces)
    {
        Face->OutsidePoints.Empty();
        
        for (int32 i = UnassignedPoints.Num() - 1; i >= 0; --i)
        {
            int32 PointIdx = UnassignedPoints[i];
            if (PointIdx >= Points.Num()) continue;
            if (PointIdx == Face->V0 || PointIdx == Face->V1 || PointIdx == Face->V2) continue;

            float Dist = Face->GetDistance(Points, Points[PointIdx]);
            if (Dist > 0.0001f) // 点在外部
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
            HeadFace = Face; // 没有外部点的面作为头面
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

    const float Epsilon = 0.0001f; // 容差值，避免边界问题

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

        // 使用三角形中心点来判断，这样更准确，避免跨越平面的三角形被错误分类
        FVector TriCenter = (V0 + V1 + V2) / 3.0f;
        float CenterDist = SplitPlane.PlaneDot(TriCenter);

        // 如果中心点在平面左侧（负值），分配给左侧；否则分配给右侧
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
            // 如果中心点正好在平面上（容差范围内），根据大多数顶点的位置决定
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
        return 0; // X轴
    }
    else if (Extent.Y >= Extent.Z)
    {
        return 1; // Y轴
    }
    else
    {
        return 2; // Z轴
    }
}

