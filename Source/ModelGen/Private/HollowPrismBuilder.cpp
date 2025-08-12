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
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Params.InnerSides);
    
    // 存储内墙顶点索引
    TArray<int32> InnerTopVertices, InnerBottomVertices;
    
    // 生成内环的顶点
    for (int32 i = 0; i <= Params.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 内环顶点
        const FVector InnerPos = CalculateVertexPosition(Params.InnerRadius, Angle, HalfHeight - Params.BevelRadius);
        FVector InnerNormal = FVector(-FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f);
        if (Params.bFlipNormals) InnerNormal = -InnerNormal;
        
        const int32 InnerTopVertex = GetOrAddVertex(InnerPos, InnerNormal, 
            FVector2D(static_cast<float>(i) / Params.InnerSides, 1.0f));
        InnerTopVertices.Add(InnerTopVertex);
        
        const FVector InnerBottomPos = CalculateVertexPosition(Params.InnerRadius, Angle, -HalfHeight + Params.BevelRadius);
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
    const float StartAngle = CalculateStartAngle();
    const float AngleStep = CalculateAngleStep(Params.OuterSides);
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateOuterWalls - Generating %d outer walls"), Params.OuterSides);
    
    // 存储外墙顶点索引
    TArray<int32> OuterTopVertices, OuterBottomVertices;
    
    // 生成外环的顶点
    for (int32 i = 0; i <= Params.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * AngleStep;
        
        // 外环顶点
        const FVector OuterPos = CalculateVertexPosition(Params.OuterRadius, Angle, HalfHeight - Params.BevelRadius);
        FVector OuterNormal = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        if (Params.bFlipNormals) OuterNormal = -OuterNormal;
        
        const int32 OuterTopVertex = GetOrAddVertex(OuterPos, OuterNormal, 
            FVector2D(static_cast<float>(i) / Params.OuterSides, 1.0f));
        OuterTopVertices.Add(OuterTopVertex);
        
        const FVector OuterBottomPos = CalculateVertexPosition(Params.OuterRadius, Angle, -HalfHeight + Params.BevelRadius);
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
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopCapWithTriangles - Generating top cap"));
    
    // 存储顶点索引
    TArray<int32> InnerVertices, OuterVertices;
    
    // 使用公共方法生成顶点
    GenerateCapVertices(InnerVertices, OuterVertices, true);
    
    // 使用公共方法生成三角形
    GenerateCapTriangles(InnerVertices, OuterVertices, true);
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopCapWithTriangles - Generated %d inner vertices, %d outer vertices"), 
           InnerVertices.Num(), OuterVertices.Num());
}

void FHollowPrismBuilder::GenerateBottomCapWithTriangles()
{
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomCapWithTriangles - Generating bottom cap"));
    
    // 存储顶点索引
    TArray<int32> InnerVertices, OuterVertices;
    
    // 使用公共方法生成顶点
    GenerateCapVertices(InnerVertices, OuterVertices, false);
    
    // 使用公共方法生成三角形
    GenerateCapTriangles(InnerVertices, OuterVertices, false);
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomCapWithTriangles - Generated %d inner vertices, %d outer vertices"), 
           InnerVertices.Num(), OuterVertices.Num());
}

void FHollowPrismBuilder::GenerateTopBevelGeometry()
{
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateTopBevelGeometry - Generating top bevel"));
    
    // 使用统一的倒角生成方法
    GenerateBevelGeometry(true, true);   // 顶部内环倒角
    GenerateBevelGeometry(true, false);  // 顶部外环倒角
}

void FHollowPrismBuilder::GenerateBottomBevelGeometry()
{
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBottomBevelGeometry - Generating bottom bevel"));
    
    // 使用统一的倒角生成方法
    GenerateBevelGeometry(false, true);   // 底部内环倒角
    GenerateBevelGeometry(false, false);  // 底部外环倒角
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
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateEndCap - Generating end cap at angle %.2f"), Angle);

    // 使用重构后的方法生成端盖
    TArray<int32> OrderedVertices;
    GenerateEndCapVertices(Angle, Normal, IsStart, OrderedVertices);
    
    // 生成端盖三角形
    GenerateEndCapTriangles(OrderedVertices, IsStart);
}

// 通用计算辅助方法实现
float FHollowPrismBuilder::CalculateStartAngle() const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    return -ArcAngleRadians / 2.0f;
}

float FHollowPrismBuilder::CalculateAngleStep(int32 Sides) const
{
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    return ArcAngleRadians / Sides;
}

float FHollowPrismBuilder::CalculateInnerRadius(bool bIncludeBevel) const
{
    if (bIncludeBevel)
    {
        return Params.InnerRadius + Params.BevelRadius;
    }
    return Params.InnerRadius;
}

float FHollowPrismBuilder::CalculateOuterRadius(bool bIncludeBevel) const
{
    if (bIncludeBevel)
    {
        return Params.OuterRadius - Params.BevelRadius;
    }
    return Params.OuterRadius;
}

FVector FHollowPrismBuilder::CalculateVertexPosition(float Radius, float Angle, float Z) const
{
    return FVector(
        Radius * FMath::Cos(Angle),
        Radius * FMath::Sin(Angle),
        Z
    );
}

// 新增：公共三角化方法实现
void FHollowPrismBuilder::GenerateCapTriangles(const TArray<int32>& InnerVertices, 
                                              const TArray<int32>& OuterVertices, 
                                              bool bIsTopCap)
{
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
        
        // 根据是否为顶面选择顶点顺序
        if (bIsTopCap)
        {
            // 对于顶面，法线指向正Z方向，需要逆时针顶点顺序
            // 第一个三角形：当前内环顶点 -> 下一个外环顶点 -> 当前外环顶点
            AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestNextOuterVertex], OuterVertices[BestOuterVertex]);
            
            // 第二个三角形：当前内环顶点 -> 下一个内环顶点 -> 下一个外环顶点
            AddTriangle(InnerVertices[BestInnerVertex], InnerVertices[BestNextInnerVertex], OuterVertices[BestNextOuterVertex]);
        }
        else
        {
            // 对于底面，法线指向负Z方向，需要顺时针顶点顺序
            // 第一个三角形：当前内环顶点 -> 当前外环顶点 -> 下一个外环顶点
            AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestOuterVertex], OuterVertices[BestNextOuterVertex]);
            
            // 第二个三角形：当前内环顶点 -> 下一个外环顶点 -> 下一个内环顶点
            AddTriangle(InnerVertices[BestInnerVertex], OuterVertices[BestNextOuterVertex], InnerVertices[BestNextInnerVertex]);
        }
    }
}

// 新增：生成单个端盖的顶点
void FHollowPrismBuilder::GenerateCapVertices(TArray<int32>& OutInnerVertices, 
                                             TArray<int32>& OutOuterVertices, 
                                             bool bIsTopCap)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float StartAngle = CalculateStartAngle();
    
    // 使用不同的角度步长来生成内外环
    const float InnerAngleStep = CalculateAngleStep(Params.InnerSides);
    const float OuterAngleStep = CalculateAngleStep(Params.OuterSides);
    
    // 根据是否为顶面设置法线
    FVector Normal(0.0f, 0.0f, bIsTopCap ? 1.0f : -1.0f);
    if (Params.bFlipNormals)
    {
        Normal = -Normal;
    }
    
    // 考虑倒角影响，调整半径
    const float InnerRadius = CalculateInnerRadius(true);
    const float OuterRadius = CalculateOuterRadius(true);
    
    // 清空输出数组
    OutInnerVertices.Empty();
    OutOuterVertices.Empty();
    
    // 预分配内存
    OutInnerVertices.Reserve(Params.InnerSides + 1);
    OutOuterVertices.Reserve(Params.OuterSides + 1);
    
    // 生成内环顶点
    for (int32 i = 0; i <= Params.InnerSides; ++i)
    {
        const float Angle = StartAngle + i * InnerAngleStep;
        
        // 内环顶点
        const FVector InnerPos = CalculateVertexPosition(InnerRadius, Angle, bIsTopCap ? HalfHeight : -HalfHeight);
        const int32 InnerVertex = GetOrAddVertex(InnerPos, Normal, 
            FVector2D(static_cast<float>(i) / Params.InnerSides, 0.5f));
        OutInnerVertices.Add(InnerVertex);
    }

    // 生成外环顶点
    for (int32 i = 0; i <= Params.OuterSides; ++i)
    {
        const float Angle = StartAngle + i * OuterAngleStep;
        
        // 外环顶点
        const FVector OuterPos = CalculateVertexPosition(OuterRadius, Angle, bIsTopCap ? HalfHeight : -HalfHeight);
        const int32 OuterVertex = GetOrAddVertex(OuterPos, Normal, 
            FVector2D(static_cast<float>(i) / Params.OuterSides, 1.0f));
        OutOuterVertices.Add(OuterVertex);
    }
}

// 新增：统一的倒角生成方法实现
void FHollowPrismBuilder::GenerateBevelGeometry(bool bIsTop, bool bIsInner)
{
    const float BevelRadiusLocal = Params.BevelRadius;
    const int32 BevelSectionsLocal = Params.BevelSegments;
    
    if (BevelRadiusLocal <= 0.0f || BevelSectionsLocal <= 0) return;
    
    UE_LOG(LogTemp, Log, TEXT("FHollowPrismBuilder::GenerateBevelGeometry - Generating %s %s bevel"), 
           bIsTop ? TEXT("top") : TEXT("bottom"), 
           bIsInner ? TEXT("inner") : TEXT("outer"));
    
    TArray<int32> PrevRing;
    
    for (int32 i = 0; i <= BevelSectionsLocal; ++i)
    {
        TArray<int32> CurrentRing;
        GenerateBevelRing(PrevRing, CurrentRing, bIsTop, bIsInner, i, BevelSectionsLocal);
        
        // 连接相邻环
        if (i > 0 && PrevRing.Num() > 0)
        {
            ConnectBevelRings(PrevRing, CurrentRing, bIsInner, bIsTop);
        }
        
        PrevRing = CurrentRing;
    }
}

void FHollowPrismBuilder::GenerateBevelRing(const TArray<int32>& PrevRing, 
                                           TArray<int32>& OutCurrentRing,
                                           bool bIsTop, bool bIsInner, 
                                           int32 RingIndex, int32 TotalRings)
{
    const float HalfHeight = Params.GetHalfHeight();
    const float BevelRadiusLocal = Params.BevelRadius;
    
    const float alpha = static_cast<float>(RingIndex) / TotalRings;
    
    // 根据位置和类型计算参数
    const float ZOffset = bIsTop ? HalfHeight : -HalfHeight;
    const float ZDirection = bIsTop ? 1.0f : -1.0f;
    const float RadiusDirection = bIsInner ? 1.0f : -1.0f;
    
    // 计算当前半径和Z坐标
    const float StartRadius = bIsInner ? Params.InnerRadius : Params.OuterRadius;
    const float EndRadius = StartRadius + (RadiusDirection * BevelRadiusLocal);
    const float CurrentRadius = FMath::Lerp(StartRadius, EndRadius, alpha);
    const float CurrentZ = FMath::Lerp(ZOffset - (ZDirection * BevelRadiusLocal), ZOffset, alpha);
    
    // 确定边数和角度步长
    const int32 Sides = bIsInner ? Params.InnerSides : Params.OuterSides;
    const float ArcAngleRadians = FMath::DegreesToRadians(Params.ArcAngle);
    const float StartAngle = -ArcAngleRadians / 2.0f;
    const float LocalAngleStep = ArcAngleRadians / Sides;
    
    // 预分配内存
    OutCurrentRing.Empty();
    OutCurrentRing.Reserve(Sides + 1);
    
    // 生成当前环的顶点
    for (int32 s = 0; s <= Sides; ++s)
    {
        const float angle = StartAngle + s * LocalAngleStep;
        
        const FVector Position = FVector(
            CurrentRadius * FMath::Cos(angle),
            CurrentRadius * FMath::Sin(angle),
            CurrentZ
        );
        
        // 计算法线
        const FVector Normal = CalculateBevelNormal(angle, alpha, bIsInner, bIsTop);
        
        // 计算UV坐标
        const float U = static_cast<float>(s) / Sides;
        const float V = (CurrentZ + HalfHeight) / Params.Height;
        
        OutCurrentRing.Add(GetOrAddVertex(Position, Normal, FVector2D(U, V)));
    }
}

FVector FHollowPrismBuilder::CalculateBevelNormal(float Angle, float Alpha, bool bIsInner, bool bIsTop) const
{
    const FVector RadialDirection = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
    const FVector FaceNormal = FVector(0, 0, bIsTop ? 1.0f : -1.0f);
    
    // 根据是否为内环选择径向方向
    const FVector RadialComponent = bIsInner ? -RadialDirection : RadialDirection;
    
    // 在径向方向和面法线之间插值
    FVector Normal = FMath::Lerp(RadialComponent, FaceNormal, Alpha).GetSafeNormal();
    
    // 确保法线方向正确
    const float DotProduct = FVector::DotProduct(Normal, RadialDirection);
    if ((bIsInner && DotProduct > 0) || (!bIsInner && DotProduct < 0))
    {
        Normal = -Normal;
    }
    
    return Params.bFlipNormals ? -Normal : Normal;
}

void FHollowPrismBuilder::ConnectBevelRings(const TArray<int32>& PrevRing, 
                                           const TArray<int32>& CurrentRing, 
                                           bool bIsInner, bool bIsTop)
{
    const int32 Sides = bIsInner ? Params.InnerSides : Params.OuterSides;
    
    for (int32 s = 0; s < Sides; ++s)
    {
        const int32 V00 = PrevRing[s];
        const int32 V10 = CurrentRing[s];
        const int32 V01 = PrevRing[s + 1];
        const int32 V11 = CurrentRing[s + 1];
        
        // 根据是否为内环和是否为顶面选择四边形顶点顺序
        if (bIsInner)
        {
            // 内环：根据顶面/底面选择顶点顺序
            if (bIsTop)
            {
                AddQuad(V00, V01, V11, V10);  // 顶面内环：逆时针
            }
            else
            {
                AddQuad(V00, V10, V11, V01);  // 底面内环：顺时针
            }
        }
        else
        {
            // 外环：根据顶面/底面选择顶点顺序
            if (bIsTop)
            {
                AddQuad(V00, V10, V11, V01);  // 顶面外环：逆时针
            }
            else
            {
                AddQuad(V00, V01, V11, V10);  // 底面外环：顺时针
            }
        }
    }
}

// 新增：端盖重构相关方法实现
void FHollowPrismBuilder::GenerateEndCapVertices(float Angle, const FVector& Normal, bool IsStart,
                                                TArray<int32>& OutOrderedVertices)
{
    const float HalfHeight = Params.GetHalfHeight();
    
    // 清空输出数组
    OutOrderedVertices.Empty();
    
    // 1. 上底中心顶点
    const int32 TopCenterVertex = GetOrAddVertex(
        FVector(0, 0, HalfHeight),
        Normal,
        FVector2D(0.5f, 1.0f)
    );
    OutOrderedVertices.Add(TopCenterVertex);

    // 2. 上倒角弧线顶点（从顶面到侧边）
    if (Params.BevelRadius > 0.0f)
    {
        GenerateEndCapBevelVertices(Angle, Normal, IsStart, true, OutOrderedVertices);
    }

    // 3. 侧边顶点（从上到下）
    GenerateEndCapSideVertices(Angle, Normal, IsStart, OutOrderedVertices);

    // 4. 下倒角弧线顶点（从侧边到底面）
    if (Params.BevelRadius > 0.0f)
    {
        GenerateEndCapBevelVertices(Angle, Normal, IsStart, false, OutOrderedVertices);
    }

    // 5. 下底中心顶点
    const int32 BottomCenterVertex = GetOrAddVertex(
        FVector(0, 0, -HalfHeight),
        Normal,
        FVector2D(0.5f, 0.0f)
    );
    OutOrderedVertices.Add(BottomCenterVertex);
}

void FHollowPrismBuilder::GenerateEndCapBevelVertices(float Angle, const FVector& Normal, bool IsStart,
                                                     bool bIsTopBevel, TArray<int32>& OutVertices)
{
    const float HalfHeight = Params.GetHalfHeight();
    float TopBevelHeight, BottomBevelHeight;
    CalculateEndCapBevelHeights(TopBevelHeight, BottomBevelHeight);
    
    float StartZ, EndZ;
    CalculateEndCapZRange(TopBevelHeight, BottomBevelHeight, StartZ, EndZ);
    
    const int32 BevelSections = Params.BevelSegments;
    const float StartZPos = bIsTopBevel ? HalfHeight : StartZ;
    const float EndZPos = bIsTopBevel ? EndZ : -HalfHeight;
    const int32 StartIndex = bIsTopBevel ? 0 : 1;
    const int32 EndIndex = bIsTopBevel ? BevelSections : BevelSections + 1;
    
    for (int32 i = StartIndex; i < EndIndex; ++i)
    {
        const float Alpha = static_cast<float>(i) / BevelSections;
        const float CurrentZ = FMath::Lerp(StartZPos, EndZPos, Alpha);
        
        // 计算当前半径
        float CurrentInnerRadius, CurrentOuterRadius;
        if (bIsTopBevel)
        {
            // 从顶面半径到侧边半径
            const float TopInnerRadius = Params.InnerRadius + Params.BevelRadius;
            const float TopOuterRadius = Params.OuterRadius - Params.BevelRadius;
            CurrentInnerRadius = FMath::Lerp(TopInnerRadius, Params.InnerRadius, Alpha);
            CurrentOuterRadius = FMath::Lerp(TopOuterRadius, Params.OuterRadius, Alpha);
        }
        else
        {
            // 从侧边半径到底面半径
            CurrentInnerRadius = FMath::Lerp(Params.InnerRadius, Params.InnerRadius + Params.BevelRadius, Alpha);
            CurrentOuterRadius = FMath::Lerp(Params.OuterRadius, Params.OuterRadius - Params.BevelRadius, Alpha);
        }
        
        // 内环顶点
        const FVector InnerBevelPos = FVector(CurrentInnerRadius * FMath::Cos(Angle), 
                                            CurrentInnerRadius * FMath::Sin(Angle), CurrentZ);
        const int32 InnerBevelVertex = GetOrAddVertex(InnerBevelPos, Normal, 
            FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
        OutVertices.Add(InnerBevelVertex);
        
        // 外环顶点
        const FVector OuterBevelPos = FVector(CurrentOuterRadius * FMath::Cos(Angle), 
                                            CurrentOuterRadius * FMath::Sin(Angle), CurrentZ);
        const int32 OuterBevelVertex = GetOrAddVertex(OuterBevelPos, Normal, 
            FVector2D(IsStart ? 0.0f : 1.0f, (CurrentZ + HalfHeight) / Params.Height));
        OutVertices.Add(OuterBevelVertex);
    }
}

void FHollowPrismBuilder::GenerateEndCapSideVertices(float Angle, const FVector& Normal, bool IsStart,
                                                    TArray<int32>& OutVertices)
{
    const float HalfHeight = Params.GetHalfHeight();
    float TopBevelHeight, BottomBevelHeight;
    CalculateEndCapBevelHeights(TopBevelHeight, BottomBevelHeight);
    
    float StartZ, EndZ;
    CalculateEndCapZRange(TopBevelHeight, BottomBevelHeight, StartZ, EndZ);
    
    // 侧边顶点（从上到下）
    for (int32 h = 0; h <= 1; ++h)
    {
        const float Z = FMath::Lerp(EndZ, StartZ, static_cast<float>(h) / 1.0f);

        // 内环顶点
        const FVector InnerEdgePos = FVector(Params.InnerRadius * FMath::Cos(Angle), 
                                           Params.InnerRadius * FMath::Sin(Angle), Z);
        const int32 InnerEdgeVertex = GetOrAddVertex(InnerEdgePos, Normal, 
            FVector2D(IsStart ? 0.0f : 1.0f, (Z + HalfHeight) / Params.Height));
        OutVertices.Add(InnerEdgeVertex);
        
        // 外环顶点
        const FVector OuterEdgePos = FVector(Params.OuterRadius * FMath::Cos(Angle), 
                                           Params.OuterRadius * FMath::Sin(Angle), Z);
        const int32 OuterEdgeVertex = GetOrAddVertex(OuterEdgePos, Normal, 
            FVector2D(IsStart ? 0.0f : 1.0f, (Z + HalfHeight) / Params.Height));
        OutVertices.Add(OuterEdgeVertex);
    }
}

void FHollowPrismBuilder::GenerateEndCapTriangles(const TArray<int32>& OrderedVertices, bool IsStart)
{
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

void FHollowPrismBuilder::CalculateEndCapBevelHeights(float& OutTopBevelHeight, float& OutBottomBevelHeight) const
{
    const float BevelRadiusLocal = Params.BevelRadius;
    const float MaxBevelHeight = Params.OuterRadius - Params.InnerRadius;
    
    OutTopBevelHeight = FMath::Min(BevelRadiusLocal, MaxBevelHeight);
    OutBottomBevelHeight = FMath::Min(BevelRadiusLocal, MaxBevelHeight);
}

void FHollowPrismBuilder::CalculateEndCapZRange(float TopBevelHeight, float BottomBevelHeight, 
                                               float& OutStartZ, float& OutEndZ) const
{
    const float HalfHeight = Params.GetHalfHeight();
    
    // 调整主体几何体的范围，避免与倒角重叠
    OutStartZ = -HalfHeight + BottomBevelHeight;
    OutEndZ = HalfHeight - TopBevelHeight;
}

