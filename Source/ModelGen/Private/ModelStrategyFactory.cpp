// Copyright (c) 2024. All rights reserved.

#include "ModelStrategyFactory.h"
#include "BevelCube.h"
#include "Pyramid.h"
#include "Frustum.h"
#include "HollowPrism.h"
#include "PolygonTorus.h"
#include "Engine/World.h"

TMap<FString, TSubclassOf<AActor>> UCustomModelFactory::ModelTypeRegistry;

AActor* UCustomModelFactory::CreateModelActor(const FString& ModelTypeName, UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    // 确保注册表已初始化
    if (ModelTypeRegistry.Num() == 0)
    {
        InitializeDefaultModelTypes();
    }
    
    if (TSubclassOf<AActor>* ModelClass = ModelTypeRegistry.Find(ModelTypeName))
    {
        if (World)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            
            AActor* NewActor = World->SpawnActor<AActor>(*ModelClass, Location, Rotation, SpawnParams);
            if (NewActor)
            {
                UE_LOG(LogTemp, Log, TEXT("ModelFactory: 成功创建模型: %s"), *ModelTypeName);
                return NewActor;
            }
        }
    }
    
    UE_LOG(LogTemp, Error, TEXT("ModelFactory: 无法创建模型: %s"), *ModelTypeName);
    return nullptr;
}

TArray<FString> UCustomModelFactory::GetSupportedModelTypes()
{
    if (ModelTypeRegistry.Num() == 0)
    {
        InitializeDefaultModelTypes();
    }
    
    TArray<FString> ModelTypes;
    ModelTypeRegistry.GetKeys(ModelTypes);
    return ModelTypes;
}

void UCustomModelFactory::RegisterModelType(const FString& ModelTypeName, TSubclassOf<AActor> ModelClass)
{
    ModelTypeRegistry.Add(ModelTypeName, ModelClass);
    UE_LOG(LogTemp, Log, TEXT("ModelFactory: 注册模型类型: %s"), *ModelTypeName);
}

void UCustomModelFactory::InitializeDefaultModelTypes()
{
    // 注册默认的模型类型
    RegisterModelType(TEXT("BevelCube"), ABevelCube::StaticClass());
    RegisterModelType(TEXT("Pyramid"), APyramid::StaticClass());
    RegisterModelType(TEXT("Frustum"), AFrustum::StaticClass());
    RegisterModelType(TEXT("HollowPrism"), AHollowPrism::StaticClass());
    RegisterModelType(TEXT("PolygonTorus"), APolygonTorus::StaticClass());
}
