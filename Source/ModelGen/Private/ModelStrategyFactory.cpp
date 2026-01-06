// Copyright (c) 2024. All rights reserved.

#include "ModelStrategyFactory.h"
#include "BevelCube.h"
#include "Pyramid.h"
#include "Frustum.h"
#include "HollowPrism.h"
#include "PolygonTorus.h"
#include "Engine/World.h"


TMap<FString, TSubclassOf<AActor>> UCustomModelFactory::ModelTypeRegistry;
TMap<FString, UStaticMesh*> UCustomModelFactory::StaticMeshCache;
int32 UCustomModelFactory::CacheHitCount = 0;
int32 UCustomModelFactory::CacheMissCount = 0;
FCriticalSection UCustomModelFactory::CacheLock;
int32 UCustomModelFactory::CacheAccessCount = 0;
const int32 UCustomModelFactory::AUTO_CLEANUP_INTERVAL = 50; // 每 50 次访问自动清理一次

AActor* UCustomModelFactory::CreateModelActorWithParams(const FString& ModelTypeName, const TMap<FString, FString>& Parameters, UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    return CreateModelActorInternal(ModelTypeName, Parameters, World, Location, Rotation);
}

AActor* UCustomModelFactory::CreateModelActorInternal(const FString& ModelTypeName, const TMap<FString, FString>& Parameters, UWorld* World, const FVector& Location, const FRotator& Rotation)
{
    if (ModelTypeRegistry.Num() == 0)
    {
        InitializeDefaultModelTypes();
    }
    
    if (!World)
    {
        return nullptr;
    }
    
    if (TSubclassOf<AActor>* ModelClass = ModelTypeRegistry.Find(ModelTypeName))
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        
        AActor* NewActor = World->SpawnActor<AActor>(*ModelClass, Location, Rotation, SpawnParams);
        if (NewActor)
        {
            return NewActor;
        }
    }
    
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
}

void UCustomModelFactory::InitializeDefaultModelTypes()
{
    RegisterModelType(TEXT("BevelCube"), ABevelCube::StaticClass());
    RegisterModelType(TEXT("Pyramid"), APyramid::StaticClass());
    RegisterModelType(TEXT("Frustum"), AFrustum::StaticClass());
    RegisterModelType(TEXT("HollowPrism"), AHollowPrism::StaticClass());
    RegisterModelType(TEXT("PolygonTorus"), APolygonTorus::StaticClass());
}


FString UCustomModelFactory::GenerateCacheKey(const FString& ModelType, const TMap<FString, FString>& Parameters)
{
    FString Key = ModelType;
    
    TArray<FString> SortedKeys;
    Parameters.GetKeys(SortedKeys);
    SortedKeys.Sort();
    
    for (const FString& ParamKey : SortedKeys)
    {
        if (const FString* Value = Parameters.Find(ParamKey))
        {
            Key += TEXT("_") + ParamKey + TEXT("=") + *Value;
        }
    }
    
    return Key;
}

UStaticMesh* UCustomModelFactory::GetOrCreateStaticMesh(AProceduralMeshActor* ProceduralActor)
{
    if (!ProceduralActor)
    {
        return nullptr;
    }
    
    FString ModelType = ProceduralActor->GetClass()->GetName();
    TMap<FString, FString> Parameters;
    
    FString CacheKey = GenerateCacheKey(ModelType, Parameters);
    
    bool bShouldCleanup = false;
    {
        FScopeLock Lock(&CacheLock);
        CacheAccessCount++;
        if (CacheAccessCount >= AUTO_CLEANUP_INTERVAL)
        {
            CacheAccessCount = 0;
            bShouldCleanup = true;
        }
    }
    
    if (bShouldCleanup)
    {
        CleanupInvalidCache();
    }
    
    UStaticMesh* CachedMesh = nullptr;
    {
        FScopeLock Lock(&CacheLock);
        if (UStaticMesh** FoundMesh = StaticMeshCache.Find(CacheKey))
        {
            CachedMesh = *FoundMesh;
        }
    }
    
    if (CachedMesh != nullptr)
    {
        bool bIsValid = false;
        
        if (CachedMesh->IsValidLowLevel() && !CachedMesh->IsUnreachable())
        {
            if (IsValid(CachedMesh) && 
                !CachedMesh->IsPendingKill() &&
                !CachedMesh->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
            {
                if (CachedMesh->RenderData != nullptr && 
                    CachedMesh->RenderData->IsInitialized())
                {
                    bIsValid = true;
                }
            }
        }
        
        if (bIsValid)
        {
            FScopeLock Lock(&CacheLock);
            CacheHitCount++;
            return CachedMesh;
        }
        else
        {
            {
                FScopeLock Lock(&CacheLock);
                StaticMeshCache.Remove(CacheKey);
            }
        }
    }
    
    {
        FScopeLock Lock(&CacheLock);
        CacheMissCount++;
    }
    
    ProceduralActor->GenerateMesh();
    
    UStaticMesh* NewMesh = ProceduralActor->ConvertProceduralMeshToStaticMesh();
    if (NewMesh)
    {
        FScopeLock Lock(&CacheLock);
        StaticMeshCache.Add(CacheKey, NewMesh);
    }
    
    return NewMesh;
}

UStaticMesh* UCustomModelFactory::CreateModelStaticMesh(const FString& ModelTypeName, const TMap<FString, FString>& Parameters, UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }

    AProceduralMeshActor* TempActor = Cast<AProceduralMeshActor>(CreateModelActorInternal(ModelTypeName, Parameters, World, FVector::ZeroVector, FRotator::ZeroRotator));
    if (!TempActor)
    {
        return nullptr;
    }

    UStaticMesh* StaticMesh = GetOrCreateStaticMesh(TempActor);
    TempActor->Destroy();

    return StaticMesh;
}

UStaticMesh* UCustomModelFactory::CreateModelStaticMeshWithDefaults(const FString& ModelTypeName, UWorld* World)
{
    TMap<FString, FString> EmptyParameters;
    return CreateModelStaticMesh(ModelTypeName, EmptyParameters, World);
}

void UCustomModelFactory::ClearCache()
{
    FScopeLock Lock(&CacheLock);
    
    for (auto& Pair : StaticMeshCache)
    {
        if (IsValid(Pair.Value) && Pair.Value->IsRooted())
        {
            Pair.Value->RemoveFromRoot();
        }
    }
    
    StaticMeshCache.Empty();
    CacheHitCount = 0;
    CacheMissCount = 0;
}

void UCustomModelFactory::CleanupInvalidCache()
{
    FScopeLock Lock(&CacheLock);
    
    TArray<FString> KeysToRemove;
    
    for (auto It = StaticMeshCache.CreateIterator(); It; ++It)
    {
        UStaticMesh* Mesh = It.Value();
        
        bool bIsValid = false;
        bool bShouldRemove = false;
        
        if (Mesh == nullptr)
        {
            bShouldRemove = true;
        }
        else if (!Mesh->IsValidLowLevel())
        {
            bShouldRemove = true;
        }
        else if (Mesh->IsUnreachable())
        {
            bShouldRemove = true;
        }
        else if (!IsValid(Mesh))
        {
            bShouldRemove = true;
        }
        else
        {
            if (Mesh->RenderData == nullptr || !Mesh->RenderData->IsInitialized())
            {
                bShouldRemove = true;
            }
            else
            {
                
                if (Mesh->IsPendingKill() || Mesh->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
                {
                    bShouldRemove = true;
                }
                else
                {
                    bIsValid = true;
                }
            }
        }
        
        if (bShouldRemove)
        {
            if (Mesh != nullptr && Mesh->IsValidLowLevel() && Mesh->IsRooted())
            {
                Mesh->RemoveFromRoot();
            }
            KeysToRemove.Add(It.Key());
        }
    }
    
    for (const FString& Key : KeysToRemove)
    {
        StaticMeshCache.Remove(Key);
    }
    
    if (KeysToRemove.Num() > 0)
    {
    }
}

int32 UCustomModelFactory::GetCacheSize()
{
    FScopeLock Lock(&CacheLock);
    return StaticMeshCache.Num();
}

void UCustomModelFactory::LogCacheStats()
{
    FScopeLock Lock(&CacheLock);
    int32 TotalRequests = CacheHitCount + CacheMissCount;
    float HitRate = TotalRequests > 0 ? (float)CacheHitCount / TotalRequests * 100.0f : 0.0f;
    
}
