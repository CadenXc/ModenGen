// Copyright (c) 2024. All rights reserved.

#include "ModelGenVHACD.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "VHACD.h"

bool FModelGenVHACD::GenerateConvexHulls(
    UProceduralMeshComponent* ProceduralMeshComponent,
    UBodySetup* BodySetup,
    int32 HullCount,
    int32 MaxHullVerts,
    uint32 HullPrecision)
{
    if (!ProceduralMeshComponent || !BodySetup)
    {
        UE_LOG(LogTemp, Warning, TEXT("VHACD: 输入参数无效"));
        return false;
    }

    // 1. 从ProceduralMeshComponent提取顶点和索引
    TArray<FVector3f> Vertices;
    TArray<uint32> Indices;
    
    for (int32 SectionIdx = 0; SectionIdx < ProceduralMeshComponent->GetNumSections(); ++SectionIdx)
    {
        FProcMeshSection* Section = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
        if (!Section) continue;
        
        const int32 FirstVertexIndex = Vertices.Num();
        
        // 添加顶点
        for (const FProcMeshVertex& ProcVertex : Section->ProcVertexBuffer)
        {
            Vertices.Add(FVector3f(ProcVertex.Position));
        }
        
        // 添加索引（需要调整偏移）
        for (uint32 Index : Section->ProcIndexBuffer)
        {
            Indices.Add(FirstVertexIndex + Index);
        }
    }
    
    if (Vertices.Num() < 3 || Indices.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("VHACD: 顶点或索引数量不足（顶点=%d, 索引=%d）"), Vertices.Num(), Indices.Num());
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("VHACD: 开始计算，顶点数=%d, 三角形数=%d"), Vertices.Num(), Indices.Num() / 3);
    
    // 2. 创建VHACD实例
    VHACD::IVHACD* VHACDInterface = VHACD::CreateVHACD();
    if (!VHACDInterface)
    {
        UE_LOG(LogTemp, Error, TEXT("VHACD: 无法创建VHACD实例"));
        return false;
    }
    
    // 3. 配置VHACD参数
    VHACD::IVHACD::Parameters VHACDParams;
    VHACDParams.m_resolution = HullPrecision;
    VHACDParams.m_maxConvexHulls = FMath::Clamp(HullCount, 1, 64);
    VHACDParams.m_maxNumVerticesPerCH = FMath::Clamp(MaxHullVerts, 6, 32);
    VHACDParams.m_concavity = 0.001;
    VHACDParams.m_planeDownsampling = 4;
    VHACDParams.m_convexhullDownsampling = 4;
    VHACDParams.m_alpha = 0.05;
    VHACDParams.m_beta = 0.05;
    VHACDParams.m_pca = 0;
    VHACDParams.m_mode = 0; // 0: voxel-based (recommended)
    VHACDParams.m_minVolumePerCH = 0.0001;
    VHACDParams.m_convexhullApproximation = true;
    VHACDParams.m_projectHullVertices = true;
    
    // 4. 调用VHACD计算（使用float版本）
    const float* Points = reinterpret_cast<const float*>(Vertices.GetData());
    const uint32_t NumPoints = Vertices.Num();
    const uint32_t* Triangles = Indices.GetData();
    const uint32_t NumTriangles = Indices.Num() / 3;
    
    UE_LOG(LogTemp, Log, TEXT("VHACD: 参数 - HullCount=%d, MaxHullVerts=%d, Precision=%d"), 
        VHACDParams.m_maxConvexHulls, VHACDParams.m_maxNumVerticesPerCH, VHACDParams.m_resolution);
    
    bool bSuccess = VHACDInterface->Compute(Points, NumPoints, Triangles, NumTriangles, VHACDParams);
    
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("VHACD: 计算失败"));
        VHACDInterface->Release();
        return false;
    }
    
    // 5. 获取结果并转换为FKConvexElem
    const uint32_t NumConvexHulls = VHACDInterface->GetNConvexHulls();
    UE_LOG(LogTemp, Log, TEXT("VHACD: 成功生成 %d 个凸包"), NumConvexHulls);
    
    BodySetup->AggGeom.ConvexElems.Empty();
    
    for (uint32_t HullIdx = 0; HullIdx < NumConvexHulls; ++HullIdx)
    {
        VHACD::IVHACD::ConvexHull VHACHull;
        VHACDInterface->GetConvexHull(HullIdx, VHACHull);
        
        if (VHACHull.m_nPoints >= 4)
        {
            FKConvexElem ConvexElem;
            ConvexElem.VertexData.Reserve(VHACHull.m_nPoints);
            
            // 转换顶点（从double*转换为FVector）
            for (uint32_t PointIdx = 0; PointIdx < VHACHull.m_nPoints; ++PointIdx)
            {
                const double* Point = &VHACHull.m_points[PointIdx * 3];
                ConvexElem.VertexData.Add(FVector(Point[0], Point[1], Point[2]));
            }
            
            ConvexElem.UpdateElemBox();
            BodySetup->AggGeom.ConvexElems.Add(ConvexElem);
            
            UE_LOG(LogTemp, Log, TEXT("VHACD: 凸包[%d] - 顶点数=%d, 中心=(%.2f, %.2f, %.2f), 大小=(%.2f, %.2f, %.2f)"),
                HullIdx, VHACHull.m_nPoints,
                ConvexElem.ElemBox.GetCenter().X,
                ConvexElem.ElemBox.GetCenter().Y,
                ConvexElem.ElemBox.GetCenter().Z,
                ConvexElem.ElemBox.GetSize().X,
                ConvexElem.ElemBox.GetSize().Y,
                ConvexElem.ElemBox.GetSize().Z);
        }
    }
    
    VHACDInterface->Release();
    
    const int32 FinalConvexCount = BodySetup->AggGeom.ConvexElems.Num();
    if (FinalConvexCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("VHACD: 最终生成 %d 个有效凸包元素"), FinalConvexCount);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("VHACD: 未生成有效的凸包元素"));
        return false;
    }
}

