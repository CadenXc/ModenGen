// Copyright (c) 2024. All rights reserved.

#include "HollowPrismBuilder.h"
#include "ModelGenMeshData.h"
#include "Engine/Engine.h"

FHollowPrismBuilder::FHollowPrismBuilder(const FHollowPrismParameters& InParams)
    : Params(InParams)
{
}

bool FHollowPrismBuilder::Generate(FModelGenMeshData& OutMeshData)
{
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::Generate - Starting generation"));
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::Generate - Parameters: InnerRadius=%.2f, OuterRadius=%.2f, Height=%.2f, InnerSides=%d, OuterSides=%d"), 
           Params.InnerRadius, Params.OuterRadius, Params.Height, Params.InnerSides, Params.OuterSides);
    
    if (!ValidateParameters())
    {
        UE_LOG(LogTemp, Error, TEXT("FHollowPrismBuilder::Generate - Parameters validation failed"));
        return false;
    }

    // 清除现有数据
    Clear();

    // 预分配内存
    ReserveMemory();
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::Generate - Generating base geometry"));
    
    // 生成基础几何体
    GenerateBaseGeometry();
    
    // 输出详细信息
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::Generate - Generated %d vertices, %d triangles"), 
           MeshData.GetVertexCount(), MeshData.GetTriangleCount());
    
    // 验证生成的数据
    if (!ValidateGeneratedData())
    {
        UE_LOG(LogTemp, Error, TEXT("FHollowPrismBuilder::Generate - Generated data validation failed"));
        return false;
    }

    // 输出结果
    OutMeshData = MeshData;
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::Generate - Generation completed successfully"));
    return true;
}

bool FHollowPrismBuilder::ValidateParameters() const
{
    return Params.IsValid();
}

int32 FHollowPrismBuilder::CalculateVertexCountEstimate() const
{
    return Params.CalculateVertexCountEstimate();
}

int32 FHollowPrismBuilder::CalculateTriangleCountEstimate() const
{
    return Params.CalculateTriangleCountEstimate();
}

void FHollowPrismBuilder::GenerateBaseGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();
    
    GenerateSideWalls();
    GenerateTopCapWithTriangles();
    GenerateBottomCapWithTriangles();
    
    if (Params.BevelRadius > 0.0f)
    {
        GenerateTopBevelGeometry();
        GenerateBottomBevelGeometry();
    }
    
    if (!Params.IsFullCircle())
    {
        GenerateEndCaps();
    }
}

void FHollowPrismBuilder::GenerateSideWalls()
{
    GenerateInnerWalls();
    GenerateOuterWalls();
}

void FHollowPrismBuilder::GenerateInnerWalls()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Params.InnerSides;
    
    // 存储内墙顶点索引
    TArray<int32> InnerTopVertices, InnerBottomVertices;
    
    // 生成内环的顶点
    for (int32 i = 0; i <= Params.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 内环顶点
        const FVector InnerPos(Params.InnerRadius * FMath::Cos(Angle), 
                              Params.InnerRadius * FMath::Sin(Angle), 
                              HalfHeight - Params.BevelRadius);
        FVector InnerNormal = FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f);
        if (Params.bFlipNormals) InnerNormal = -InnerNormal;
        
        const int32 InnerTopVertex = GetOrAddVertex(InnerPos, InnerNormal, 
            FVector2D(static_cast<float>(i) / Params.InnerSides, 1.0f));
        InnerTopVertices.Add(InnerTopVertex);
        
        const FVector InnerBottomPos(Params.InnerRadius * FMath::Cos(Angle), 
                                   Params.InnerRadius * FMath::Sin(Angle), 
                                   -HalfHeight + Params.BevelRadius);
        const int32 InnerBottomVertex = GetOrAddVertex(InnerBottomPos, InnerNormal, 
            FVector2D(static_cast<float>(i) / Params.InnerSides, 0.0f));
        InnerBottomVertices.Add(InnerBottomVertex);
    }
    
    // 生成内墙四边形
    for (int32 i = 0; i < Params.InnerSides; ++i)
    {
        AddQuad(InnerTopVertices[i], InnerBottomVertices[i], 
               InnerBottomVertices[i + 1], InnerTopVertices[i + 1]);
    }
}

void FHollowPrismBuilder::GenerateOuterWalls()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Params.OuterSides;
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateOuterWalls - Generating %d outer walls"), Params.OuterSides);
    
    // 存储外墙顶点索引
    TArray<int32> OuterTopVertices, OuterBottomVertices;
    
    // 生成外环的顶点
    for (int32 i = 0; i <= Params.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 外环顶点
        const FVector OuterPos(Params.OuterRadius * FMath::Cos(Angle), 
                              Params.OuterRadius * FMath::Sin(Angle), 
                              HalfHeight - Params.BevelRadius);
        FVector OuterNormal = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        if (Params.bFlipNormals) OuterNormal = -OuterNormal;
        
        const int32 OuterTopVertex = GetOrAddVertex(OuterPos, OuterNormal, 
            FVector2D(static_cast<float>(i) / Params.OuterSides, 1.0f));
        OuterTopVertices.Add(OuterTopVertex);
        
        const FVector OuterBottomPos(Params.OuterRadius * FMath::Cos(Angle), 
                                   Params.OuterRadius * FMath::Sin(Angle), 
                                   -HalfHeight + Params.BevelRadius);
        const int32 OuterBottomVertex = GetOrAddVertex(OuterBottomPos, OuterNormal, 
            FVector2D(static_cast<float>(i) / Params.OuterSides, 0.0f));
        OuterBottomVertices.Add(OuterBottomVertex);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateOuterWalls - Generated %d outer vertices per ring"), OuterTopVertices.Num());
    
    // 生成外墙四边形
    for (int32 i = 0; i < Params.OuterSides; ++i)
    {
        AddQuad(OuterTopVertices[i], OuterTopVertices[i + 1], 
               OuterBottomVertices[i + 1], OuterBottomVertices[i]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateOuterWalls - Generated %d outer wall quads"), Params.OuterSides);
}

void FHollowPrismBuilder::GenerateTopCapWithTriangles()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    
    // 使用不同的角度步长来生成内外环
    const float InnerAngleStep = ArcAngleRadians / Params.InnerSides;
    const float OuterAngleStep = ArcAngleRadians / Params.OuterSides;
    
    // 顶面法线向上
    FVector Normal(0.0f, 0.0f, 1.0f);
    if (Params.bFlipNormals)
    {
        Normal = -Normal;
    }

    // 考虑倒角影响，调整半径
    const float InnerRadius = Params.InnerRadius + Params.BevelRadius;
    const float OuterRadius = Params.OuterRadius - Params.BevelRadius;
    
    // 存储顶点索引
    TArray<int32> InnerVertices, OuterVertices;
    
    // 生成内环顶点
    for (int32 i = 0; i <= Params.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * InnerAngleStep;
        
        // 内环顶点
        const FVector InnerPos(InnerRadius * FMath::Cos(Angle), InnerRadius * FMath::Sin(Angle), HalfHeight);
        const int32 InnerVertex = GetOrAddVertex(InnerPos, Normal, FVector2D(static_cast<float>(i) / Params.InnerSides, 0.5f));
        InnerVertices.Add(InnerVertex);
    }

    // 生成外环顶点
    for (int32 i = 0; i <= Params.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * OuterAngleStep;
        
        // 外环顶点
        const FVector OuterPos(OuterRadius * FMath::Cos(Angle), OuterRadius * FMath::Sin(Angle), HalfHeight);
        const int32 OuterVertex = GetOrAddVertex(OuterPos, Normal, FVector2D(static_cast<float>(i) / Params.OuterSides, 1.0f));
        OuterVertices.Add(OuterVertex);
    }
    
    // 生成顶面三角形 - 使用改进的三角化算法避免重叠和交叉
    // 计算内外环的公共边数，确保完全覆盖
    const int32 MaxSides = FMath::Max(Params.InnerSides, Params.OuterSides);
    
    for (int32 i = 0; i < MaxSides; ++i)
    {
        // 计算当前角度位置
        const float CurrentAngleRatio = static_cast<float>(i) / MaxSides;
        
        // 计算内环和外环对应的顶点索引
        const float InnerIndex = CurrentAngleRatio * Params.InnerSides;
        const float OuterIndex = CurrentAngleRatio * Params.OuterSides;
        
        // 计算下一个角度位置
        const float NextAngleRatio = static_cast<float>(i + 1) / MaxSides;
        const float NextInnerIndex = NextAngleRatio * Params.InnerSides;
        const float NextOuterIndex = NextAngleRatio * Params.OuterSides;
        
        // 获取当前和下一个内环顶点
        const int32 InnerIndexFloor = FMath::FloorToInt(InnerIndex);
        const int32 InnerIndexCeil = FMath::Min(InnerIndexFloor + 1, Params.InnerSides);
        const int32 NextInnerIndexFloor = FMath::FloorToInt(NextInnerIndex);
        const int32 NextInnerIndexCeil = FMath::Min(NextInnerIndexFloor + 1, Params.InnerSides);
        
        // 获取当前和下一个外环顶点
        const int32 OuterIndexFloor = FMath::FloorToInt(OuterIndex);
        const int32 OuterIndexCeil = FMath::Min(OuterIndexFloor + 1, Params.OuterSides);
        const int32 NextOuterIndexFloor = FMath::FloorToInt(NextOuterIndex);
        const int32 NextOuterIndexCeil = FMath::Min(NextOuterIndexFloor + 1, Params.OuterSides);
        
        // 根据权重选择最佳的内环顶点
        const float InnerWeight = InnerIndex - InnerIndexFloor;
        const int32 BestInnerVertex = (InnerWeight < 0.5f) ? InnerIndexFloor : InnerIndexCeil;
        
        const float NextInnerWeight = NextInnerIndex - NextInnerIndexFloor;
        const int32 BestNextInnerVertex = (NextInnerWeight < 0.5f) ? NextInnerIndexFloor : NextInnerIndexCeil;
        
        // 根据权重选择最佳的外环顶点
        const float OuterWeight = OuterIndex - OuterIndexFloor;
        const int32 BestOuterVertex = (OuterWeight < 0.5f) ? OuterIndexFloor : OuterIndexCeil;
        
        const float NextOuterWeight = NextOuterIndex - NextOuterIndexFloor;
        const int32 BestNextOuterVertex = (NextOuterWeight < 0.5f) ? NextOuterIndexFloor : NextOuterIndexCeil;
        
        // 生成两个三角形来覆盖当前扇形区域
        // 对于顶面，法线指向正Z方向，需要逆时针顶点顺序
        // 第一个三角形：当前内环顶点 -> 下一个外环顶点 -> 当前外环顶点
        AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestNextOuterVertex], OuterVertices[BestOuterVertex]);
        
        // 第二个三角形：当前内环顶点 -> 下一个内环顶点 -> 下一个外环顶点
        AddTriangle(InnerVertices[BestInnerVertex], InnerVertices[BestNextInnerVertex], OuterVertices[BestNextOuterVertex]);
    }
}

void FHollowPrismBuilder::GenerateBottomCapWithTriangles()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    
    // 使用不同的角度步长来生成内外环
    const float InnerAngleStep = ArcAngleRadians / Params.InnerSides;
    const float OuterAngleStep = ArcAngleRadians / Params.OuterSides;
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomCapWithTriangles - Generating bottom cap"));
    
    // 底面法线向下
    FVector Normal(0.0f, 0.0f, -1.0f);
    if (Params.bFlipNormals)
    {
        Normal = -Normal;
    }
    
    // 考虑倒角影响，调整半径
    const float InnerRadius = Params.InnerRadius + Params.BevelRadius;
    const float OuterRadius = Params.OuterRadius - Params.BevelRadius;
    
    // 存储顶点索引
    TArray<int32> InnerVertices, OuterVertices;
    
    // 生成内环顶点
    for (int32 i = 0; i <= Params.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * InnerAngleStep;
        
        // 内环顶点
        const FVector InnerPos(InnerRadius * FMath::Cos(Angle), 
                              InnerRadius * FMath::Sin(Angle), 
                              -HalfHeight);
        const int32 InnerVertex = GetOrAddVertex(InnerPos, Normal, 
            FVector2D(static_cast<float>(i) / Params.InnerSides, 0.5f));
        InnerVertices.Add(InnerVertex);
    }
    
    // 生成外环顶点
    for (int32 i = 0; i <= Params.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * OuterAngleStep;
        
        // 外环顶点
        const FVector OuterPos(OuterRadius * FMath::Cos(Angle), 
                              OuterRadius * FMath::Sin(Angle), 
                              -HalfHeight);
        const int32 OuterVertex = GetOrAddVertex(OuterPos, Normal, 
            FVector2D(static_cast<float>(i) / Params.OuterSides, 1.0f));
        OuterVertices.Add(OuterVertex);
    }
    
    // 生成底面三角形 - 使用改进的三角化算法避免重叠和交叉
    // 计算内外环的公共边数，确保完全覆盖
    const int32 MaxSides = FMath::Max(Params.InnerSides, Params.OuterSides);
    
    for (int32 i = 0; i < MaxSides; ++i)
    {
        // 计算当前角度位置
        const float CurrentAngleRatio = static_cast<float>(i) / MaxSides;
        
        // 计算内环和外环对应的顶点索引
        const float InnerIndex = CurrentAngleRatio * Params.InnerSides;
        const float OuterIndex = CurrentAngleRatio * Params.OuterSides;
        
        // 计算下一个角度位置
        const float NextAngleRatio = static_cast<float>(i + 1) / MaxSides;
        const float NextInnerIndex = NextAngleRatio * Params.InnerSides;
        const float NextOuterIndex = NextAngleRatio * Params.OuterSides;
        
        // 获取当前和下一个内环顶点
        const int32 InnerIndexFloor = FMath::FloorToInt(InnerIndex);
        const int32 InnerIndexCeil = FMath::Min(InnerIndexFloor + 1, Params.InnerSides);
        const int32 NextInnerIndexFloor = FMath::FloorToInt(NextInnerIndex);
        const int32 NextInnerIndexCeil = FMath::Min(NextInnerIndexFloor + 1, Params.InnerSides);
        
        // 获取当前和下一个外环顶点
        const int32 OuterIndexFloor = FMath::FloorToInt(OuterIndex);
        const int32 OuterIndexCeil = FMath::Min(OuterIndexFloor + 1, Params.OuterSides);
        const int32 NextOuterIndexFloor = FMath::FloorToInt(NextOuterIndex);
        const int32 NextOuterIndexCeil = FMath::Min(NextOuterIndexFloor + 1, Params.OuterSides);
        
        // 根据权重选择最佳的内环顶点
        const float InnerWeight = InnerIndex - InnerIndexFloor;
        const int32 BestInnerVertex = (InnerWeight < 0.5f) ? InnerIndexFloor : InnerIndexCeil;
        
        const float NextInnerWeight = NextInnerIndex - NextInnerIndexFloor;
        const int32 BestNextInnerVertex = (NextInnerWeight < 0.5f) ? NextInnerIndexFloor : NextInnerIndexCeil;
        
        // 根据权重选择最佳的外环顶点
        const float OuterWeight = OuterIndex - OuterIndexFloor;
        const int32 BestOuterVertex = (OuterWeight < 0.5f) ? OuterIndexFloor : OuterIndexCeil;
        
        const float NextOuterWeight = NextOuterIndex - NextOuterIndexFloor;
        const int32 BestNextOuterVertex = (NextOuterWeight < 0.5f) ? NextOuterIndexFloor : NextOuterIndexCeil;
        
        // 生成两个三角形来覆盖当前扇形区域
        // 对于底面，法线指向负Z方向，需要顺时针顶点顺序
        // 第一个三角形：当前内环顶点 -> 当前外环顶点 -> 下一个外环顶点
        AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestOuterVertex], OuterVertices[BestNextOuterVertex]);
        
        // 第二个三角形：当前内环顶点 -> 下一个外环顶点 -> 下一个内环顶点
        AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestNextOuterVertex], InnerVertices[BestNextInnerVertex]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomCapWithTriangles - Generated triangles for inner sides: %d, outer sides: %d"), 
           Params.InnerSides, Params.OuterSides);
}

void FHollowPrismBuilder::GenerateTopBevelGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopBevelGeometry - Generating top bevel"));
    
    // 生成顶部内环和外环倒角
    GenerateTopInnerBevel();
    GenerateTopOuterBevel();
}

void FHollowPrismBuilder::GenerateBottomBevelGeometry()
{
    const float HalfHeight = Params.GetHalfHeight();
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomBevelGeometry - Generating bottom bevel"));
    
    // 生成底部内环和外环倒角
    GenerateBottomInnerBevel();
    GenerateBottomOuterBevel();
}

void FHollowPrismBuilder::GenerateTopInnerBevel()
{
    const float BevelRadiusLocal = Params.BevelRadius;
    const int32 BevelSectionsLocal = Params.BevelSections;
    const float HalfHeight = Params.GetHalfHeight();

    if (BevelRadiusLocal <= 0.0f || BevelSectionsLocal <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopInnerBevel - Generating top inner bevel"));

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= BevelSectionsLocal; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSectionsLocal;

        // 内环倒角：从内半径开始，向外扩展
        const float StartRadius = Params.InnerRadius;
        const float EndRadius = Params.InnerRadius + BevelRadiusLocal;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(HalfHeight - BevelRadiusLocal, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.InnerSides + 1);

        for (int32 s = 0; s <= Params.InnerSides; ++s)
        {
            const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Params.InnerSides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 内环法线：指向内部（负径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector TopNormal = FVector(0, 0, 1.0f);
            FVector Normal = FMath::Lerp(-RadialDirection, TopNormal, alpha).GetSafeNormal();

            // 确保法线指向内部
            if (FVector::DotProduct(Normal, RadialDirection) > 0)
            {
                Normal = -Normal;
            }

            if (Params.bFlipNormals)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Params.InnerSides;
            const float V = (CurrentZ + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.InnerSides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(V00, V01, V11, V10);
            }
        }

        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::GenerateTopOuterBevel()
{
    const float BevelRadiusLocal = Params.BevelRadius;
    const int32 BevelSectionsLocal = Params.BevelSections;
    const float HalfHeight = Params.GetHalfHeight();

    if (BevelRadiusLocal <= 0.0f || BevelSectionsLocal <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopOuterBevel - Generating top outer bevel"));

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= BevelSectionsLocal; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSectionsLocal;

        // 外环倒角：从外半径开始，向内收缩
        const float StartRadius = Params.OuterRadius;
        const float EndRadius = Params.OuterRadius - BevelRadiusLocal;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(HalfHeight - BevelRadiusLocal, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.OuterSides + 1);

        for (int32 s = 0; s <= Params.OuterSides; ++s)
        {
            const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Params.OuterSides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 外环法线：指向外部（正径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector TopNormal = FVector(0, 0, 1.0f);
            FVector Normal = FMath::Lerp(RadialDirection, TopNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            if (FVector::DotProduct(Normal, RadialDirection) < 0)
            {
                Normal = -Normal;
            }

            if (Params.bFlipNormals)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Params.OuterSides;
            const float V = (CurrentZ + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.OuterSides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(V00, V10, V11, V01);
            }
        }

        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::GenerateBottomInnerBevel()
{
    const float BevelRadiusLocal = Params.BevelRadius;
    const int32 BevelSectionsLocal = Params.BevelSections;
    const float HalfHeight = Params.GetHalfHeight();

    if (BevelRadiusLocal <= 0.0f || BevelSectionsLocal <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomInnerBevel - Generating bottom inner bevel"));

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= BevelSectionsLocal; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSectionsLocal;

        // 内环倒角：从内半径开始，向外扩展
        const float StartRadius = Params.InnerRadius;
        const float EndRadius = Params.InnerRadius + BevelRadiusLocal;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(-HalfHeight + BevelRadiusLocal, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.InnerSides + 1);

        for (int32 s = 0; s <= Params.InnerSides; ++s)
        {
            const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Params.InnerSides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 内环法线：指向内部（负径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector BottomNormal = FVector(0, 0, -1.0f);
            FVector Normal = FMath::Lerp(-RadialDirection, BottomNormal, alpha).GetSafeNormal();

            // 确保法线指向内部
            if (FVector::DotProduct(Normal, RadialDirection) > 0)
            {
                Normal = -Normal;
            }

            if (Params.bFlipNormals)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Params.InnerSides;
            const float V = (CurrentZ + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.InnerSides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(V00, V10, V11, V01);
            }
        }

        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::GenerateBottomOuterBevel()
{
    const float BevelRadiusLocal = Params.BevelRadius;
    const int32 BevelSectionsLocal = Params.BevelSections;
    const float HalfHeight = Params.GetHalfHeight();

    if (BevelRadiusLocal <= 0.0f || BevelSectionsLocal <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomOuterBevel - Generating bottom outer bevel"));

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= BevelSectionsLocal; ++i)
    {
        const float alpha = static_cast<float>(i) / BevelSectionsLocal;

        // 外环倒角：从外半径开始，向内收缩
        const float StartRadius = Params.OuterRadius;
        const float EndRadius = Params.OuterRadius - BevelRadiusLocal;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(-HalfHeight + BevelRadiusLocal, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.OuterSides + 1);

        for (int32 s = 0; s <= Params.OuterSides; ++s)
        {
            const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Params.OuterSides;
            const float angle = StartAngle + s * LocalAngleStep;

            const FVector Position = FVector(
                CurrentRadius * FMath::Cos(angle),
                CurrentRadius * FMath::Sin(angle),
                CurrentZ
            );

            // 外环法线：指向外部（正径向方向）
            const FVector RadialDirection = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
            const FVector BottomNormal = FVector(0, 0, -1.0f);
            FVector Normal = FMath::Lerp(RadialDirection, BottomNormal, alpha).GetSafeNormal();

            // 确保法线指向外部
            if (FVector::DotProduct(Normal, RadialDirection) < 0)
            {
                Normal = -Normal;
            }

            if (Params.bFlipNormals)
            {
                Normal = -Normal;
            }

            const float U = static_cast<float>(s) / Params.OuterSides;
            const float V = (CurrentZ + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.OuterSides; ++s)
            {
                const int32 V00 = PrevRing[s];
                const int32 V10 = CurrentRing[s];
                const int32 V01 = PrevRing[s + 1];
                const int32 V11 = CurrentRing[s + 1];
                
                AddQuad(V00, V01, V11, V10);
            }
        }

        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::GenerateEndCaps()
{
    if (Params.ArcAngle >= 360.0f - KINDA_SMALL_NUMBER)
    {
        return; // 完整环形，不需要端盖
    }

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateEndCaps - Generating end caps"));

    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float EndAngle = ArcAngleRadians / 2.0f;

    // 生成起始端盖和结束端盖
    //GenerateEndCap(StartAngle, FVector(-1, 0, 0), true);   // 起始端盖
    //GenerateEndCap(EndAngle, FVector(1, 0, 0), false);      // 结束端盖
}

void FHollowPrismBuilder::GenerateEndCap(float Angle, const FVector& Normal, bool IsStart)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadiusLocal = Params.BevelRadius;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateEndCap - Generating end cap at angle %.2f"), Angle);

    // 计算倒角高度
    const float TopBevelHeight = FMath::Min(BevelRadiusLocal, Params.OuterRadius - Params.InnerRadius);
    const float BottomBevelHeight = FMath::Min(BevelRadiusLocal, Params.OuterRadius - Params.InnerRadius);

    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomBevelHeight;
    const float EndZ = HalfHeight - TopBevelHeight;

    // 按照指定顺序存储顶点：上底中心 -> 上倒角圆弧 -> 侧边 -> 下倒角圆弧 -> 下底中心
    TArray<int32> OrderedVertices;
    
    // 1. 上底中心顶点
    const int32 TopCenterVertex = GetOrAddVertex(
        FVector(0, 0, HalfHeight),
        Normal,
        FVector2D(0.5f, 1.0f)
    );
    OrderedVertices.Add(TopCenterVertex);

    // 2. 上倒角弧线顶点（从顶面到侧边）
    if (Params.BevelRadius > 0.0f)
    {
        const int32 TopBevelSections = Params.BevelSections;
        for (int32 i = 0; i < TopBevelSections; ++i) // 从顶面到侧边
        {
            const float Alpha = static_cast<float>(i) / TopBevelSections;
            
            // 从顶面开始（HalfHeight位置）到侧边（EndZ位置）
            const float CurrentZ = FMath::Lerp(HalfHeight, EndZ, Alpha);
            
            // 从顶面半径到侧边半径（减去倒角半径，与主体几何体保持一致）
            const float TopInnerRadius = Params.InnerRadius + Params.BevelRadius;
            const float TopOuterRadius = Params.OuterRadius - Params.BevelRadius;
            const float SideInnerRadius = Params.InnerRadius;
            const float SideOuterRadius = Params.OuterRadius;
            
            const float CurrentInnerRadius = FMath::Lerp(TopInnerRadius, SideInnerRadius, Alpha);
            const float CurrentOuterRadius = FMath::Lerp(TopOuterRadius, SideOuterRadius, Alpha);
            
            // 内环顶点
            const FVector InnerBevelPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
            const int32 InnerBevelVertex = GetOrAddVertex(InnerBevelPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(InnerBevelVertex);
            
            // 外环顶点
            const FVector OuterBevelPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
            const int32 OuterBevelVertex = GetOrAddVertex(OuterBevelPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(OuterBevelVertex);
        }
    }

    // 3. 侧边顶点（从上到下）
    for (int32 h = 0; h <= 1; ++h) // 使用4个分段
    {
        const float Z = FMath::Lerp(EndZ, StartZ, static_cast<float>(h) / 1.0f);

        // 内环顶点
        const FVector InnerEdgePos = FVector(Params.InnerRadius * FMath::Cos(Angle), Params.InnerRadius * FMath::Sin(Angle), Z);
        const int32 InnerEdgeVertex = GetOrAddVertex(InnerEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (Z + HalfHeight) / Params.Height));
        OrderedVertices.Add(InnerEdgeVertex);
        
        // 外环顶点
        const FVector OuterEdgePos = FVector(Params.OuterRadius * FMath::Cos(Angle), Params.OuterRadius * FMath::Sin(Angle), Z);
        const int32 OuterEdgeVertex = GetOrAddVertex(OuterEdgePos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (Z + HalfHeight) / Params.Height));
        OrderedVertices.Add(OuterEdgeVertex);
    }

    // 4. 下倒角弧线顶点（从侧边到底面）
    if (Params.BevelRadius > 0.0f)
    {
        const int32 BottomBevelSections = Params.BevelSections;
        for (int32 i = 1; i <= BottomBevelSections; ++i) // 从侧边到底面
        {
            const float Alpha = static_cast<float>(i) / BottomBevelSections;
            
            // 从侧边开始（StartZ位置）到底面（-HalfHeight位置）
            const float CurrentZ = FMath::Lerp(StartZ, -HalfHeight, Alpha);
            
            // 从侧边半径到底面半径（减去倒角半径，与主体几何体保持一致）
            const float SideInnerRadius = Params.InnerRadius;
            const float SideOuterRadius = Params.OuterRadius;
            const float BottomInnerRadius = Params.InnerRadius + Params.BevelRadius;
            const float BottomOuterRadius = Params.OuterRadius - Params.BevelRadius;
            
            const float CurrentInnerRadius = FMath::Lerp(SideInnerRadius, BottomInnerRadius, Alpha);
            const float CurrentOuterRadius = FMath::Lerp(SideOuterRadius, BottomOuterRadius, Alpha);
            
            // 内环顶点
            const FVector InnerBevelPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
            const int32 InnerBevelVertex = GetOrAddVertex(InnerBevelPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(InnerBevelVertex);
            
            // 外环顶点
            const FVector OuterBevelPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
            const int32 OuterBevelVertex = GetOrAddVertex(OuterBevelPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(OuterBevelVertex);
        }
    }

    // 5. 下底中心顶点
    const int32 BottomCenterVertex = GetOrAddVertex(
        FVector(0, 0, -HalfHeight),
        Normal,
        FVector2D(0.5f, 0.0f)
    );
    OrderedVertices.Add(BottomCenterVertex);

    // 生成端盖三角形
    for (int32 i = 0; i < OrderedVertices.Num() - 2; i += 2)
    {
        // 连接相邻的顶点对，形成三角形
        if (IsStart)
        {
            AddTriangle(OrderedVertices[i], OrderedVertices[i + 2], OrderedVertices[i + 1]);
            AddTriangle(OrderedVertices[i + 1], OrderedVertices[i + 2], OrderedVertices[i + 3]);
        }
        else
        {
            AddTriangle(OrderedVertices[i], OrderedVertices[i + 1], OrderedVertices[i + 2]);
            AddTriangle(OrderedVertices[i + 1], OrderedVertices[i + 3], OrderedVertices[i + 2]);
        }
    }
}

void FHollowPrismBuilder::CalculateRingVertices(float Radius, int32 Sides, float Z, float ArcAngle,
                                               TArray<FVector>& OutVertices, TArray<FVector2D>& OutUVs, float UVScale)
{
    OutVertices.Empty();
    OutUVs.Empty();

    const float ArcAngleRadians = FMath::DegreesToRadians(ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Sides;

    for (int32 i = 0; i <= Sides; i++)
    {
        const float angle = StartAngle + i * AngleStep;
        const FVector Vertex = FVector(
            Radius * FMath::Cos(angle),
            Radius * FMath::Sin(angle),
            Z
        );
        OutVertices.Add(Vertex);

        // UV映射
        const float U = (static_cast<float>(i) / Sides) * UVScale;
        const float V = (Z + Params.GetHalfHeight()) / Params.Height;
        OutUVs.Add(FVector2D(U, V));
    }
}
