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
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::Generate - Parameters: InnerRadius=%.2f, OuterRadius=%.2f, Height=%.2f, Sides=%d"), 
           Params.InnerRadius, Params.OuterRadius, Params.Height, Params.Sides);
    
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
    
    // 生成侧面墙壁
    GenerateSideWalls();
    
    // 生成顶底面
    GenerateTopCapWithQuads();
    GenerateBottomCapWithQuads();
    
    // 生成倒角几何体
    if (Params.BevelRadius > 0.0f)
    {
        GenerateTopBevelGeometry();
        GenerateBottomBevelGeometry();
    }
    
    // 生成端面（如果不是完整的360度）
    if (!Params.IsFullCircle())
    {
        GenerateEndCaps();
    }
}

void FHollowPrismBuilder::GenerateSideWalls()
{
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateSideWalls - Starting side wall generation"));
    
    // 生成内墙和外墙
    GenerateInnerWalls();
    GenerateOuterWalls();
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateSideWalls - Completed side wall generation"));
}

void FHollowPrismBuilder::GenerateInnerWalls()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Params.Sides;
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateInnerWalls - Generating %d inner walls"), Params.Sides);
    
    // 存储内墙顶点索引
    TArray<int32> InnerTopVertices, InnerBottomVertices;
    
    // 生成内环的顶点
    for (int32 i = 0; i <= Params.Sides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 内环顶点/
        const FVector InnerPos(Params.InnerRadius * FMath::Cos(Angle), 
                              Params.InnerRadius * FMath::Sin(Angle), 
                              HalfHeight - Params.BevelRadius);
        FVector InnerNormal = FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f);
        if (Params.bFlipNormals) InnerNormal = -InnerNormal;
        
        const int32 InnerTopVertex = GetOrAddVertex(InnerPos, InnerNormal, 
            FVector2D(static_cast<float>(i) / Params.Sides, 1.0f));
        InnerTopVertices.Add(InnerTopVertex);
        
        const FVector InnerBottomPos(Params.InnerRadius * FMath::Cos(Angle), 
                                   Params.InnerRadius * FMath::Sin(Angle), 
                                   -HalfHeight + Params.BevelRadius);
        const int32 InnerBottomVertex = GetOrAddVertex(InnerBottomPos, InnerNormal, 
            FVector2D(static_cast<float>(i) / Params.Sides, 0.0f));
        InnerBottomVertices.Add(InnerBottomVertex);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateInnerWalls - Generated %d inner vertices per ring"), InnerTopVertices.Num());
    
    // 生成内墙四边形
    for (int32 i = 0; i < Params.Sides; ++i)
    {
        AddQuad(InnerTopVertices[i], InnerBottomVertices[i], 
               InnerBottomVertices[i + 1], InnerTopVertices[i + 1]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateInnerWalls - Generated %d inner wall quads"), Params.Sides);
}

void FHollowPrismBuilder::GenerateOuterWalls()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Params.Sides;
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateOuterWalls - Generating %d outer walls"), Params.Sides);
    
    // 存储外墙顶点索引
    TArray<int32> OuterTopVertices, OuterBottomVertices;
    
    // 生成外环的顶点
    for (int32 i = 0; i <= Params.Sides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 外环顶点
        const FVector OuterPos(Params.OuterRadius * FMath::Cos(Angle), 
                              Params.OuterRadius * FMath::Sin(Angle), 
                              HalfHeight - Params.BevelRadius);
        FVector OuterNormal = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        if (Params.bFlipNormals) OuterNormal = -OuterNormal;
        
        const int32 OuterTopVertex = GetOrAddVertex(OuterPos, OuterNormal, 
            FVector2D(static_cast<float>(i) / Params.Sides, 1.0f));
        OuterTopVertices.Add(OuterTopVertex);
        
        const FVector OuterBottomPos(Params.OuterRadius * FMath::Cos(Angle), 
                                   Params.OuterRadius * FMath::Sin(Angle), 
                                   -HalfHeight + Params.BevelRadius);
        const int32 OuterBottomVertex = GetOrAddVertex(OuterBottomPos, OuterNormal, 
            FVector2D(static_cast<float>(i) / Params.Sides, 0.0f));
        OuterBottomVertices.Add(OuterBottomVertex);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateOuterWalls - Generated %d outer vertices per ring"), OuterTopVertices.Num());
    
    // 生成外墙四边形
    for (int32 i = 0; i < Params.Sides; ++i)
    {
        AddQuad(OuterTopVertices[i], OuterTopVertices[i + 1], 
               OuterBottomVertices[i + 1], OuterBottomVertices[i]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateOuterWalls - Generated %d outer wall quads"), Params.Sides);
}

void FHollowPrismBuilder::GenerateTopCapWithQuads()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Params.Sides;
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopCapWithQuads - Generating top cap"));
    
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
    
    // 生成内外环顶点
    for (int32 i = 0; i <= Params.Sides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 内环顶点
        const FVector InnerPos(InnerRadius * FMath::Cos(Angle), 
                              InnerRadius * FMath::Sin(Angle), 
                              HalfHeight);
        const int32 InnerVertex = GetOrAddVertex(InnerPos, Normal, 
            FVector2D(static_cast<float>(i) / Params.Sides, 0.5f));
        InnerVertices.Add(InnerVertex);
        
        // 外环顶点
        const FVector OuterPos(OuterRadius * FMath::Cos(Angle), 
                              OuterRadius * FMath::Sin(Angle), 
                              HalfHeight);
        const int32 OuterVertex = GetOrAddVertex(OuterPos, Normal, 
            FVector2D(static_cast<float>(i) / Params.Sides, 1.0f));
        OuterVertices.Add(OuterVertex);
    }
    
    // 生成顶面四边形
    for (int32 i = 0; i < Params.Sides; ++i)
    {
        AddQuad(InnerVertices[i], InnerVertices[i + 1], 
               OuterVertices[i + 1], OuterVertices[i]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopCapWithQuads - Generated %d quads"), Params.Sides);
}

void FHollowPrismBuilder::GenerateBottomCapWithQuads()
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float AngleStep = ArcAngleRadians / Params.Sides;
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomCapWithQuads - Generating bottom cap"));
    
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
    
    // 生成内外环顶点
    for (int32 i = 0; i <= Params.Sides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 内环顶点
        const FVector InnerPos(InnerRadius * FMath::Cos(Angle), 
                              InnerRadius * FMath::Sin(Angle), 
                              -HalfHeight);
        const int32 InnerVertex = GetOrAddVertex(InnerPos, Normal, 
            FVector2D(static_cast<float>(i) / Params.Sides, 0.5f));
        InnerVertices.Add(InnerVertex);
        
        // 外环顶点
        const FVector OuterPos(OuterRadius * FMath::Cos(Angle), 
                              OuterRadius * FMath::Sin(Angle), 
                              -HalfHeight);
        const int32 OuterVertex = GetOrAddVertex(OuterPos, Normal, 
            FVector2D(static_cast<float>(i) / Params.Sides, 1.0f));
        OuterVertices.Add(OuterVertex);
    }
    
    // 生成底面四边形
    for (int32 i = 0; i < Params.Sides; ++i)
    {
        AddQuad(OuterVertices[i], OuterVertices[i + 1], 
               InnerVertices[i + 1], InnerVertices[i]);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomCapWithQuads - Generated %d quads"), Params.Sides);
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
    const float ChamferRadius = Params.BevelRadius;
    const int32 ChamferSections = Params.BevelSections;
    const float HalfHeight = Params.GetHalfHeight();

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopInnerBevel - Generating top inner bevel"));

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 内环倒角：从内半径开始，向外扩展
        const float StartRadius = Params.InnerRadius;
        const float EndRadius = Params.InnerRadius + ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(HalfHeight - ChamferRadius, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.Sides + 1);

        for (int32 s = 0; s <= Params.Sides; ++s)
        {
            const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Params.Sides;
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

            const float U = static_cast<float>(s) / Params.Sides;
            const float V = (CurrentZ + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.Sides; ++s)
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
    const float ChamferRadius = Params.BevelRadius;
    const int32 ChamferSections = Params.BevelSections;
    const float HalfHeight = Params.GetHalfHeight();

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopOuterBevel - Generating top outer bevel"));

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 外环倒角：从外半径开始，向内收缩
        const float StartRadius = Params.OuterRadius;
        const float EndRadius = Params.OuterRadius - ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(HalfHeight - ChamferRadius, HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.Sides + 1);

        for (int32 s = 0; s <= Params.Sides; ++s)
        {
            const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Params.Sides;
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

            const float U = static_cast<float>(s) / Params.Sides;
            const float V = (CurrentZ + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.Sides; ++s)
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
    const float ChamferRadius = Params.BevelRadius;
    const int32 ChamferSections = Params.BevelSections;
    const float HalfHeight = Params.GetHalfHeight();

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomInnerBevel - Generating bottom inner bevel"));

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 内环倒角：从内半径开始，向外扩展
        const float StartRadius = Params.InnerRadius;
        const float EndRadius = Params.InnerRadius + ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(-HalfHeight + ChamferRadius, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.Sides + 1);

        for (int32 s = 0; s <= Params.Sides; ++s)
        {
            const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Params.Sides;
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

            const float U = static_cast<float>(s) / Params.Sides;
            const float V = (CurrentZ + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.Sides; ++s)
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
    const float ChamferRadius = Params.BevelRadius;
    const int32 ChamferSections = Params.BevelSections;
    const float HalfHeight = Params.GetHalfHeight();

    if (ChamferRadius <= 0.0f || ChamferSections <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomOuterBevel - Generating bottom outer bevel"));

    TArray<int32> PrevRing;

    for (int32 i = 0; i <= ChamferSections; ++i)
    {
        const float alpha = static_cast<float>(i) / ChamferSections;

        // 外环倒角：从外半径开始，向内收缩
        const float StartRadius = Params.OuterRadius;
        const float EndRadius = Params.OuterRadius - ChamferRadius;
        const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
        const float CurrentZ = FMath::Lerp(-HalfHeight + ChamferRadius, -HalfHeight, alpha);

        TArray<int32> CurrentRing;
        CurrentRing.Reserve(Params.Sides + 1);

        for (int32 s = 0; s <= Params.Sides; ++s)
        {
            const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
            const float StartAngle = -ArcAngleRadians / 2.0f;
            const float LocalAngleStep = ArcAngleRadians / Params.Sides;
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

            const float U = static_cast<float>(s) / Params.Sides;
            const float V = (CurrentZ + HalfHeight) / Params.Height;

            CurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
        }

        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            for (int32 s = 0; s < Params.Sides; ++s)
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
    GenerateEndCap(StartAngle, FVector(-1, 0, 0), true);   // 起始端盖
    GenerateEndCap(EndAngle, FVector(1, 0, 0), false);      // 结束端盖
}

void FHollowPrismBuilder::GenerateEndCap(float Angle, const FVector& Normal, bool IsStart)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float ChamferRadius = Params.BevelRadius;

    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateEndCap - Generating end cap at angle %.2f"), Angle);

    // 计算倒角高度
    const float TopChamferHeight = FMath::Min(ChamferRadius, Params.OuterRadius - Params.InnerRadius);
    const float BottomChamferHeight = FMath::Min(ChamferRadius, Params.OuterRadius - Params.InnerRadius);

    // 调整主体几何体的范围，避免与倒角重叠
    const float StartZ = -HalfHeight + BottomChamferHeight;
    const float EndZ = HalfHeight - TopChamferHeight;

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
        const int32 TopChamferSections = Params.BevelSections;
        for (int32 i = 0; i < TopChamferSections; ++i) // 从顶面到侧边
        {
            const float Alpha = static_cast<float>(i) / TopChamferSections;
            
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
            const FVector InnerChamferPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
            const int32 InnerChamferVertex = GetOrAddVertex(InnerChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(InnerChamferVertex);
            
            // 外环顶点
            const FVector OuterChamferPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
            const int32 OuterChamferVertex = GetOrAddVertex(OuterChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(OuterChamferVertex);
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
        const int32 BottomChamferSections = Params.BevelSections;
        for (int32 i = 1; i <= BottomChamferSections; ++i) // 从侧边到底面
        {
            const float Alpha = static_cast<float>(i) / BottomChamferSections;
            
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
            const FVector InnerChamferPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
            const int32 InnerChamferVertex = GetOrAddVertex(InnerChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(InnerChamferVertex);
            
            // 外环顶点
            const FVector OuterChamferPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
            const int32 OuterChamferVertex = GetOrAddVertex(OuterChamferPos, Normal, FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
            OrderedVertices.Add(OuterChamferVertex);
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
