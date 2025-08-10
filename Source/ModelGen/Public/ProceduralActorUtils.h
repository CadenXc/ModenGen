// Copyright (c) 2024. All rights reserved.

/**
 * @file ProceduralActorUtils.h
 * @brief 通用的 Actor 侧工具：组件初始化、材质应用、碰撞设置（纯头文件）
 */

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "ProceduralMeshComponent.h"

namespace ModelGen
{
namespace ActorUtils
{

// 安全创建并配置 ProceduralMesh 作为 RootComponent（若传入已有组件则仅配置）
inline UProceduralMeshComponent* EnsureProceduralMesh(AActor* Owner,
                                                      UProceduralMeshComponent*& InOutProcMesh,
                                                      const TCHAR* Name,
                                                      bool bUseAsyncCooking,
                                                      bool bGenerateCollision)
{
    if (!Owner)
    {
        return nullptr;
    }

    if (!InOutProcMesh)
    {
        InOutProcMesh = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
        if (InOutProcMesh)
        {
            Owner->SetRootComponent(InOutProcMesh);
        }
    }

    if (InOutProcMesh)
    {
        InOutProcMesh->bUseAsyncCooking = bUseAsyncCooking;
        InOutProcMesh->SetCollisionEnabled(bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        InOutProcMesh->SetSimulatePhysics(false);
    }

    return InOutProcMesh;
}

// 应用材质：优先使用传入材质，否则回退到 Engine BasicShapeMaterial
inline void ApplyMaterialOrDefault(UProceduralMeshComponent* ProcMesh, UMaterialInterface* Material)
{
    if (!ProcMesh)
    {
        return;
    }

    if (Material)
    {
        ProcMesh->SetMaterial(0, Material);
        return;
    }

    static UMaterial* DefaultMaterial = nullptr;
    if (!DefaultMaterial)
    {
        DefaultMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    }

    if (DefaultMaterial)
    {
        ProcMesh->SetMaterial(0, DefaultMaterial);
    }
}

// 统一设置碰撞
inline void ConfigureCollision(UProceduralMeshComponent* ProcMesh, bool bGenerateCollision)
{
    if (!ProcMesh)
    {
        return;
    }

    if (bGenerateCollision)
    {
        ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProcMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
    }
    else
    {
        ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

} // namespace ActorUtils
} // namespace ModelGen

