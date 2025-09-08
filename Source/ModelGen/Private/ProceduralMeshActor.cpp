#include "ProceduralMeshActor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

AProceduralMeshActor::AProceduralMeshActor() {
  UE_LOG(LogTemp, Log, TEXT("=== ProceduralMeshActor 构造函数被调用 ==="));

  PrimaryActorTick.bCanEverTick = false;

  ProceduralMeshComponent =
      CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
  RootComponent = ProceduralMeshComponent;

  static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(
      TEXT("Material'/Engine/BasicShapes/"
           "BasicShapeMaterial.BasicShapeMaterial'"));
  if (DefaultMatFinder.Succeeded()) {
    ProceduralDefaultMaterial = DefaultMatFinder.Object;
    UE_LOG(LogTemp, Warning,
           TEXT("MW默认材质预加载失败，回退到引擎默认材质: %s"),
           *ProceduralDefaultMaterial->GetName());
  } else {
    UE_LOG(LogTemp, Error, TEXT("所有默认材质预加载失败"));
  }

  if (ProceduralMeshComponent) {
    UE_LOG(LogTemp, Log, TEXT("ProceduralMeshComponent 创建成功"));
    ProceduralMeshComponent->SetVisibility(bShowProceduralComponent);
    ProceduralMeshComponent->bUseAsyncCooking = bUseAsyncCooking;

    // 在构造时为PMC设置一个默认材质槽（后续生成段后会应用到各段）
    if (ProceduralDefaultMaterial) {
      ProceduralMeshComponent->SetMaterial(0, ProceduralDefaultMaterial);
    }
  } else {
    UE_LOG(LogTemp, Error, TEXT("ProceduralMeshComponent 创建失败"));
  }

  // 构造阶段已尝试预加载并设置默认材质
  UE_LOG(LogTemp, Log, TEXT("=== ProceduralMeshActor 构造函数完成 ==="));
}

void AProceduralMeshActor::OnConstruction(const FTransform& Transform) {
  Super::OnConstruction(Transform);  // 添加这行

  // 生成ProceduralMesh - 子类需要自己实现清理和生成逻辑
  if (ProceduralMeshComponent && IsValid()) {
    ProceduralMeshComponent->ClearAllMeshSections();
    GenerateMesh();
    ProceduralMeshComponent->SetVisibility(true);
    // 生成完段后再为每个Section设置默认材质
  }
}

void AProceduralMeshActor::SetPMCCollisionEnabled(bool bEnable) {
  bGenerateCollision = bEnable;
  if (ProceduralMeshComponent) {
    ProceduralMeshComponent->SetCollisionEnabled(
        bGenerateCollision ? ECollisionEnabled::QueryAndPhysics
                           : ECollisionEnabled::NoCollision);
  }
}

void AProceduralMeshActor::SetProceduralMeshVisibility(bool bVisible) {
  if (ProceduralMeshComponent) {
    ProceduralMeshComponent->SetVisibility(bVisible);
  }
}