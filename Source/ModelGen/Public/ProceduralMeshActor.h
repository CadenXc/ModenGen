// Copyright (c) 2024. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

#include "ProceduralMeshActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS(BlueprintType, meta=(DisplayName = "Procedural Mesh Actor"))
class MODELGEN_API AProceduralMeshActor : public AActor
{
    GENERATED_BODY()

public:
    AProceduralMeshActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Component")
    UProceduralMeshComponent* ProceduralMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    UStaticMeshComponent* StaticMeshComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bShowProceduralComponent = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bUseAsyncCooking = true;

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Component")
    void SetProceduralMeshVisibility(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    void SetPMCCollisionEnabled(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Collision")
    void GeneratePMCollisionData();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Collision")
    bool bGenerateCollision = true;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|StaticMesh")
    bool bShowStaticMeshComponent = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProceduralMesh|Materials")
    UMaterialInterface* StaticMeshMaterial = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "ProceduralMesh|Materials")
    UMaterialInterface* ProceduralDefaultMaterial = nullptr;

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    UStaticMesh* ConvertProceduralMeshToStaticMesh();

    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|StaticMesh",
        meta = (CallInEditor = "true", DisplayName = "转换到 StaticMesh"))
    void UpdateStaticMeshComponent();

   protected:

    virtual bool IsValid() const {return true;}

    UProceduralMeshComponent* GetProceduralMesh() const { return ProceduralMeshComponent; }

public:
    UFUNCTION(BlueprintCallable, Category = "ProceduralMesh|Operations")
    virtual void GenerateMesh() { }

private:
    UStaticMesh* CreateStaticMeshObject() const;
    bool BuildMeshDescriptionFromPMC(FMeshDescription& OutMeshDescription, UStaticMesh* StaticMesh) const;
    bool BuildStaticMeshGeometryFromProceduralMesh(UStaticMesh* StaticMesh) const;
    bool InitializeStaticMeshRenderData(UStaticMesh* StaticMesh) const;
    void SetupBodySetupProperties(UBodySetup* BodySetup) const;
    bool GenerateSimpleCollision(UBodySetup* BodySetup, UStaticMesh* StaticMesh) const;
    int32 CreateConvexMeshesManually(UBodySetup* BodySetup, IPhysXCookingModule* PhysXCookingModule) const;
    bool ExtractTriMeshDataFromPMC(TArray<FVector>& OutVertices, TArray<FTriIndices>& OutIndices) const;
    bool ExtractTriMeshDataFromRenderData(UStaticMesh* StaticMesh, TArray<FVector>& OutVertices, TArray<FTriIndices>& OutIndices) const;
    bool GenerateComplexCollision(UBodySetup* BodySetup, IPhysXCookingModule* PhysXCookingModule) const;
    void LogCollisionStatistics(UBodySetup* BodySetup, UStaticMesh* StaticMesh) const;
    void SetupBodySetupAndCollision(UStaticMesh* StaticMesh) const;
    void GenerateTangentsManually(FMeshDescription& MeshDescription) const;
};
