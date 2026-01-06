#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "ModelStrategyFactory.h"
#include "CustomModelActor.generated.h"

UCLASS(BlueprintType, meta=(DisplayName = "Custom Model Actor"))
class MODELGEN_API ACustomModelActor : public AActor
{
    GENERATED_BODY()
    
public:    
    ACustomModelActor();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

public:    
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CustomModel|Component")
    UStaticMeshComponent* StaticMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomModel|Materials")
    UMaterialInterface* StaticMeshMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomModel|Display")
    bool bShowStaticMesh = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomModel|Model", 
        meta = (DisplayName = "模型类型"))
    FString ModelTypeName = TEXT("BevelCube");

    UFUNCTION(BlueprintCallable, Category = "CustomModel|Model", 
        meta = (DisplayName = "Generate Mesh", CallInEditor = "true"))
    void GenerateMesh();

    UFUNCTION(BlueprintCallable, Category = "CustomModel|Model", 
        meta = (DisplayName = "Set Model Type"))
    void SetModelType(const FString& NewModelTypeName);

protected:
    void UpdateStaticMesh();
};
