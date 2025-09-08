// Copyright (c) 2024. All rights reserved.

#include "BevelCube.h"
#include "ModelGenMeshData.h"
#include "BevelCubeBuilder.h"


ABevelCube::ABevelCube()
{
    UE_LOG(LogTemp, Log, TEXT("=== BevelCube 构造函数被调用 ==="));
    
    PrimaryActorTick.bCanEverTick = false;
    
    UE_LOG(LogTemp, Log, TEXT("BevelCube 构造函数完成"));
}

void ABevelCube::GenerateMesh()
{
    UE_LOG(LogTemp, Log, TEXT("=== BevelCube::GenerateMesh 开始 ==="));
    UE_LOG(LogTemp, Log, TEXT("Actor名称: %s"), *GetName());
    UE_LOG(LogTemp, Log, TEXT("CubeSize: %f"), CubeSize);
    UE_LOG(LogTemp, Log, TEXT("BevelRadius: %f"), BevelRadius);
    UE_LOG(LogTemp, Log, TEXT("BevelSegments: %d"), BevelSegments);
    
    if (!IsValid()) 
    {
        UE_LOG(LogTemp, Error, TEXT("BevelCube::GenerateMesh - 参数验证失败"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("参数验证通过，开始生成网格"));
    
    FBevelCubeBuilder Builder(*this);
    UE_LOG(LogTemp, Log, TEXT("BevelCubeBuilder 创建完成"));
    
    FModelGenMeshData MeshData;
    UE_LOG(LogTemp, Log, TEXT("ModelGenMeshData 创建完成"));
    
    UE_LOG(LogTemp, Log, TEXT("开始调用 Builder.Generate"));
    if (Builder.Generate(MeshData))
    {
        UE_LOG(LogTemp, Log, TEXT("Builder.Generate 成功，开始转换到ProceduralMesh"));
        MeshData.ToProceduralMesh(GetProceduralMesh(), 0);
        UE_LOG(LogTemp, Log, TEXT("网格转换完成"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Builder.Generate 失败"));
    }
    
    UE_LOG(LogTemp, Log, TEXT("=== BevelCube::GenerateMesh 完成 ==="));
}

bool ABevelCube::IsValid() const
{
    bool bValid = CubeSize > 0.0f && 
                  BevelRadius >= 0.0f && BevelRadius < GetHalfSize() && 
                  BevelSegments >= 1 && BevelSegments <= 10;
    
    if (!bValid)
    {
        UE_LOG(LogTemp, Warning, TEXT("BevelCube::IsValid - 参数验证失败:"));
        UE_LOG(LogTemp, Warning, TEXT("  CubeSize > 0.0f: %s (值: %f)"), CubeSize > 0.0f ? TEXT("true") : TEXT("false"), CubeSize);
        UE_LOG(LogTemp, Warning, TEXT("  BevelRadius >= 0.0f: %s (值: %f)"), BevelRadius >= 0.0f ? TEXT("true") : TEXT("false"), BevelRadius);
        UE_LOG(LogTemp, Warning, TEXT("  BevelRadius < GetHalfSize(): %s (值: %f < %f)"), 
               BevelRadius < GetHalfSize() ? TEXT("true") : TEXT("false"), BevelRadius, GetHalfSize());
        UE_LOG(LogTemp, Warning, TEXT("  BevelSegments >= 1: %s (值: %d)"), BevelSegments >= 1 ? TEXT("true") : TEXT("false"), BevelSegments);
        UE_LOG(LogTemp, Warning, TEXT("  BevelSegments <= 10: %s (值: %d)"), BevelSegments <= 10 ? TEXT("true") : TEXT("false"), BevelSegments);
    }
    
    return bValid;
}

int32 ABevelCube::GetVertexCount() const
{
    // 简化的顶点数量预估：基础立方体 + 边缘倒角 + 角落倒角
    return 24 + 24 * (BevelSegments + 1) + 4 * (BevelSegments + 1) * (BevelSegments + 1);
}

int32 ABevelCube::GetTriangleCount() const
{
    // 简化的三角形数量预估：基础立方体 + 边缘倒角 + 角落倒角
    return 12 + 24 * BevelSegments + 8 * BevelSegments * BevelSegments;
}

// 设置参数函数实现
void ABevelCube::SetCubeSize(float NewCubeSize)
{
    UE_LOG(LogTemp, Log, TEXT("BevelCube::SetCubeSize - 旧值: %f, 新值: %f"), CubeSize, NewCubeSize);
    
    if (NewCubeSize > 0.0f && NewCubeSize != CubeSize)
    {
        CubeSize = NewCubeSize;
        UE_LOG(LogTemp, Log, TEXT("CubeSize 已更新，开始重新生成网格"));
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("CubeSize 无需更新或值无效"));
    }
}

void ABevelCube::SetBevelRadius(float NewBevelRadius)
{
    UE_LOG(LogTemp, Log, TEXT("BevelCube::SetBevelRadius - 旧值: %f, 新值: %f"), BevelRadius, NewBevelRadius);
    
    if (NewBevelRadius >= 0.0f && NewBevelRadius < GetHalfSize() && NewBevelRadius != BevelRadius)
    {
        BevelRadius = NewBevelRadius;
        UE_LOG(LogTemp, Log, TEXT("BevelRadius 已更新，开始重新生成网格"));
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("BevelRadius 无需更新或值无效"));
    }
}

void ABevelCube::SetBevelSegments(int32 NewBevelSegments)
{
    UE_LOG(LogTemp, Log, TEXT("BevelCube::SetBevelSegments - 旧值: %d, 新值: %d"), BevelSegments, NewBevelSegments);
    
    if (NewBevelSegments >= 1 && NewBevelSegments <= 10 && NewBevelSegments != BevelSegments)
    {
        BevelSegments = NewBevelSegments;
        UE_LOG(LogTemp, Log, TEXT("BevelSegments 已更新，开始重新生成网格"));
        if (ProceduralMeshComponent)
        {
            ProceduralMeshComponent->ClearAllMeshSections();
            GenerateMesh();
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("BevelSegments 无需更新或值无效"));
    }
}
