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
    // 创建自定义模型Actor
    UFUNCTION(BlueprintCallable, Category = "ModelFactory")
    static AActor* CreateModelActor(const FString& ModelTypeName, UWorld* World, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator);
    
    // 获取所有支持的模型类型
    UFUNCTION(BlueprintCallable, Category = "ModelFactory")
    static TArray<FString> GetSupportedModelTypes();
    
    // 注册新的模型类型
    UFUNCTION(BlueprintCallable, Category = "ModelFactory")
    static void RegisterModelType(const FString& ModelTypeName, TSubclassOf<AActor> ModelClass);
    
private:
    // 模型类型注册表
    static TMap<FString, TSubclassOf<AActor>> ModelTypeRegistry;
    
    // 初始化默认模型类型
    static void InitializeDefaultModelTypes();
};
