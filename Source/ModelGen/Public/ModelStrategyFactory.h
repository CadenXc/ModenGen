// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ModelStrategyFactory.generated.h"

// 前向声明
class AActor;
class UWorld;

UCLASS(BlueprintType)
class MODELGEN_API UCustomModelFactory : public UObject
{
    GENERATED_BODY()

    public:
    // 创建自定义模型Actor（带参数版本）
    UFUNCTION(BlueprintCallable, Category = "ModelFactory")
    static AActor* CreateModelActorWithParams(const FString& ModelTypeName, const TMap<FString, FString>& Parameters, UWorld* World, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator);
    
    // 获取所有支持的模型类型
    UFUNCTION(BlueprintCallable, Category = "ModelFactory")
    static TArray<FString> GetSupportedModelTypes();
    
    // 注册新的模型类型
    UFUNCTION(BlueprintCallable, Category = "ModelFactory")
    static void RegisterModelType(const FString& ModelTypeName, TSubclassOf<AActor> ModelClass);

    // 新增：使用ProceduralMeshActor获取StaticMesh的方法
    UFUNCTION(BlueprintCallable, Category = "ModelFactory|Cache")
    static UStaticMesh* GetOrCreateStaticMesh(AProceduralMeshActor* ProceduralActor);
    
    // 创建模型并返回StaticMesh（工厂管理临时Actor）
    UFUNCTION(BlueprintCallable, Category = "ModelFactory")
    static UStaticMesh* CreateModelStaticMesh(const FString& ModelTypeName, const TMap<FString, FString>& Parameters, UWorld* World);

    // 创建模型并返回StaticMesh（使用默认参数）
    UFUNCTION(BlueprintCallable, Category = "ModelFactory")
    static UStaticMesh* CreateModelStaticMeshWithDefaults(const FString& ModelTypeName, UWorld* World);
    
    // 新增：缓存管理方法
    UFUNCTION(BlueprintCallable, Category = "ModelFactory|Cache")
    static void ClearCache();
    
    UFUNCTION(BlueprintCallable, Category = "ModelFactory|Cache")
    static void CleanupInvalidCache();
    
    UFUNCTION(BlueprintCallable, Category = "ModelFactory|Cache")
    static int32 GetCacheSize();
    
    UFUNCTION(BlueprintCallable, Category = "ModelFactory|Cache")
    static void LogCacheStats();
    
    // 生成缓存键（用于比较模型是否相同）
    UFUNCTION(BlueprintCallable, Category = "ModelFactory|Cache")
    static FString GenerateCacheKey(const FString& ModelType, const TMap<FString, FString>& Parameters);

private:
    // 内部方法：统一的Actor创建逻辑
    static AActor* CreateModelActorInternal(const FString& ModelTypeName, const TMap<FString, FString>& Parameters, UWorld* World, const FVector& Location, const FRotator& Rotation);
    
private:
    // 模型类型注册表
    static TMap<FString, TSubclassOf<AActor>> ModelTypeRegistry;
    
    // 静态缓存：模型类型+参数哈希 -> StaticMesh
    static TMap<FString, UStaticMesh*> StaticMeshCache;
    
    // 缓存统计信息
    static int32 CacheHitCount;
    static int32 CacheMissCount;
    static int32 CacheAccessCount;
    
    // 自动清理间隔（每 N 次访问清理一次）
    static const int32 AUTO_CLEANUP_INTERVAL;
    
    // 线程安全锁：保护缓存访问
    static FCriticalSection CacheLock;
    
    // 初始化默认模型类型
    static void InitializeDefaultModelTypes();
};
