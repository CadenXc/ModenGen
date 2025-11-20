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
            // 注意：AProceduralMeshActor 目前不支持参数设置
            // 如果需要设置参数，需要在子类中实现相应的接口
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
    // 注意：由于 AProceduralMeshActor 没有 GetParameters 方法，我们使用空参数
    // 如果需要支持参数缓存，需要在子类中实现相应的接口
    TMap<FString, FString> Parameters;
    
    FString CacheKey = GenerateCacheKey(ModelType, Parameters);
    
    // 定期自动清理无效缓存（每 N 次访问清理一次）
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
    
    // 在锁外执行清理（避免死锁）
    if (bShouldCleanup)
    {
        CleanupInvalidCache();
    }
    
    // 从缓存中安全获取 Mesh 指针（在锁保护下）
    UStaticMesh* CachedMesh = nullptr;
    {
        FScopeLock Lock(&CacheLock);
        if (UStaticMesh** FoundMesh = StaticMeshCache.Find(CacheKey))
        {
            CachedMesh = *FoundMesh;
        }
    }
    
    // 在锁外检查有效性（避免在锁内调用可能较慢的 IsValid，并减少锁持有时间）
    if (CachedMesh != nullptr)
    {
        // 使用更安全的检查方式：先检查对象是否还在 UObject 系统中
        bool bIsValid = false;
        
        if (CachedMesh->IsValidLowLevel() && !CachedMesh->IsUnreachable())
        {
            // 检查对象是否有效且未待销毁
            if (IsValid(CachedMesh) && 
                !CachedMesh->IsPendingKill() &&
                !CachedMesh->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
            {
                // 检查 RenderData 是否有效
                if (CachedMesh->RenderData != nullptr && 
                    CachedMesh->RenderData->IsInitialized())
                {
                    bIsValid = true;
                }
            }
        }
        
        if (bIsValid)
        {
            // 缓存命中，返回缓存的 Mesh
            FScopeLock Lock(&CacheLock);
            CacheHitCount++;
            return CachedMesh;
        }
        else
        {
            // 缓存项无效，移除它（在锁保护下）
            {
                FScopeLock Lock(&CacheLock);
                StaticMeshCache.Remove(CacheKey);
                UE_LOG(LogTemp, Verbose, TEXT("GetOrCreateStaticMesh: 移除无效的缓存项: %s"), *CacheKey);
            }
        }
    }
    
    // 缓存未命中或缓存项无效，创建新的 StaticMesh
    {
        FScopeLock Lock(&CacheLock);
        CacheMissCount++;
    }
    
    // 确保网格已生成
    ProceduralActor->GenerateMesh();
    
    UStaticMesh* NewMesh = ProceduralActor->ConvertProceduralMeshToStaticMesh();
    if (NewMesh)
    {
        FScopeLock Lock(&CacheLock);
        StaticMeshCache.Add(CacheKey, NewMesh);
        UE_LOG(LogTemp, Verbose, TEXT("GetOrCreateStaticMesh: 创建并缓存新的 StaticMesh: %s"), *CacheKey);
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
    
    // 从 Root 移除所有缓存的 StaticMesh
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
    
    // 收集需要删除的键（避免在迭代时直接删除）
    TArray<FString> KeysToRemove;
    
    for (auto It = StaticMeshCache.CreateIterator(); It; ++It)
    {
        UStaticMesh* Mesh = It.Value();
        
        // 检查 StaticMesh 是否有效
        bool bIsValid = false;
        bool bShouldRemove = false;
        
        if (Mesh == nullptr)
        {
            // 空指针，直接移除
            bShouldRemove = true;
        }
        else if (!Mesh->IsValidLowLevel())
        {
            // 对象已被销毁
            bShouldRemove = true;
        }
        else if (Mesh->IsUnreachable())
        {
            // 对象不可达（已被标记为待垃圾回收）
            bShouldRemove = true;
        }
        else if (!IsValid(Mesh))
        {
            // 对象无效
            bShouldRemove = true;
        }
        else
        {
            // 检查 RenderData 是否有效
            if (Mesh->RenderData == nullptr || !Mesh->RenderData->IsInitialized())
            {
                bShouldRemove = true;
            }
            else
            {
                // StaticMesh 有效，检查是否真的在被使用
                // 由于 TMap 中的指针是强引用，我们需要检查是否有其他引用
                // 如果 StaticMesh 的引用计数只有 TMap 的引用，说明没有被实际使用
                // 注意：这是一个启发式检查，不是完全准确的
                
                // 检查 StaticMesh 是否在待销毁状态
                if (Mesh->IsPendingKill() || Mesh->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
                {
                    bShouldRemove = true;
                }
                else
                {
                    // StaticMesh 看起来有效，保留在缓存中
                    bIsValid = true;
                }
            }
        }
        
        if (bShouldRemove)
        {
            // 从 Root 移除（如果之前被添加过）
            if (Mesh != nullptr && Mesh->IsValidLowLevel() && Mesh->IsRooted())
            {
                Mesh->RemoveFromRoot();
            }
            KeysToRemove.Add(It.Key());
        }
    }
    
    // 移除无效的缓存项
    for (const FString& Key : KeysToRemove)
    {
        StaticMeshCache.Remove(Key);
    }
    
    if (KeysToRemove.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("CleanupInvalidCache: 清理了 %d 个无效的缓存项"), KeysToRemove.Num());
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
    
    UE_LOG(LogTemp, Log, TEXT("缓存统计 - 大小: %d, 命中: %d, 未命中: %d, 命中率: %.1f%%"), 
           StaticMeshCache.Num(), CacheHitCount, CacheMissCount, HitRate);
}
