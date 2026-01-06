#include "ProceduralMeshActor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "NavCollision.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"
#include "ModelGenConvexDecomp.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "HAL/PlatformProperties.h"
#include "Interface_CollisionDataProviderCore.h"
#include "IPhysXCookingModule.h"
#include "IPhysXCooking.h"
#include "PhysicsPublicCore.h"
#include "Modules/ModuleManager.h"

AProceduralMeshActor::AProceduralMeshActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProceduralMeshComponent =
        CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    RootComponent = ProceduralMeshComponent;

    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
    if (StaticMeshComponent)
    {
        StaticMeshComponent->SetupAttachment(ProceduralMeshComponent);
        StaticMeshComponent->SetVisibility(bShowStaticMeshComponent);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(
        TEXT("Material'/Engine/BasicShapes/"
            "BasicShapeMaterial.BasicShapeMaterial'"));
    if (DefaultMatFinder.Succeeded())
    {
        ProceduralDefaultMaterial = DefaultMatFinder.Object;
    }

    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetVisibility(bShowProceduralComponent);
        ProceduralMeshComponent->bUseAsyncCooking = bUseAsyncCooking;
        ProceduralMeshComponent->SetCollisionEnabled(
            bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        if (ProceduralDefaultMaterial)
        {
            ProceduralMeshComponent->SetMaterial(0, ProceduralDefaultMaterial);
        }
    }
}

void AProceduralMeshActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (ProceduralMeshComponent && IsValid())
    {
        ProceduralMeshComponent->ClearAllMeshSections();
        ProceduralMeshComponent->SetCollisionEnabled(
            bGenerateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ProceduralMeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
        GenerateMesh();
        ProceduralMeshComponent->SetVisibility(true);
    }
}

void AProceduralMeshActor::SetPMCCollisionEnabled(bool bEnable)
{
    bGenerateCollision = bEnable;
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetCollisionEnabled(
            bGenerateCollision ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision);
    }
}

void AProceduralMeshActor::GeneratePMCollisionData()
{
    if (!ProceduralMeshComponent || ProceduralMeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        return;
    }

    if (!ProceduralMeshComponent->ProcMeshBodySetup)
    {
        ProceduralMeshComponent->ProcMeshBodySetup = NewObject<UBodySetup>(
            ProceduralMeshComponent, 
            UBodySetup::StaticClass(), 
            NAME_None, 
            RF_Transactional);
        
        if (!ProceduralMeshComponent->ProcMeshBodySetup)
        {
            return;
        }
    }
    
    if (!ProceduralMeshComponent->ProcMeshBodySetup)
    {
        return;
    }

    UBodySetup* PMCBodySetup = ProceduralMeshComponent->ProcMeshBodySetup;
    int32 CurrentElementCount = PMCBodySetup->AggGeom.GetElementCount();
    
    if (CurrentElementCount == 0)
    {
        const int32 NumSections = ProceduralMeshComponent->GetNumSections();
        if (NumSections == 0)
        {
            return;
        }
        FBox BoundingBox(ForceInitToZero);
        bool bHasVertices = false;
        
        for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
        {
            FProcMeshSection* SectionData = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
            if (SectionData && SectionData->ProcVertexBuffer.Num() > 0)
            {
                for (const FProcMeshVertex& Vertex : SectionData->ProcVertexBuffer)
                {
                    BoundingBox += Vertex.Position;
                    bHasVertices = true;
                }
            }
        }
        
        if (bHasVertices && BoundingBox.IsValid)
        {
            FVector Center = BoundingBox.GetCenter();
            FVector Extent = BoundingBox.GetExtent();
            
            if (Extent.X > 0.0f && Extent.Y > 0.0f && Extent.Z > 0.0f)
            {
                FKBoxElem BoxElem;
                BoxElem.Center = Center;
                BoxElem.X = Extent.X * 2.0f;
                BoxElem.Y = Extent.Y * 2.0f;
                BoxElem.Z = Extent.Z * 2.0f;
                
                PMCBodySetup->AggGeom.BoxElems.Add(BoxElem);
                PMCBodySetup->CreatePhysicsMeshes();
            }
        }
    }
}

void AProceduralMeshActor::SetProceduralMeshVisibility(bool bVisible)
{
    if (ProceduralMeshComponent)
    {
        ProceduralMeshComponent->SetVisibility(bVisible);
    }
}

void AProceduralMeshActor::UpdateStaticMeshComponent()
{
    if (!StaticMeshComponent)
    {
        return;
    }

    if (!ProceduralMeshComponent)
    {
        return;
    }

    const int32 NumSections = ProceduralMeshComponent->GetNumSections();
    if (NumSections == 0)
    {
        return;
    }
    UStaticMesh* ConvertedMesh = ConvertProceduralMeshToStaticMesh();
    if (ConvertedMesh)
    {
        StaticMeshComponent->SetStaticMesh(ConvertedMesh);
        StaticMeshComponent->StreamingDistanceMultiplier = 10.0f;

        if (ConvertedMesh->BodySetup)
        {
            FName ProfileName = ConvertedMesh->BodySetup->DefaultInstance.GetCollisionProfileName();
            if (!ProfileName.IsNone())
            {
                StaticMeshComponent->SetCollisionProfileName(ProfileName);
            }
            else
            {
                StaticMeshComponent->SetCollisionEnabled(ConvertedMesh->BodySetup->DefaultInstance.GetCollisionEnabled());
                StaticMeshComponent->SetCollisionObjectType(ConvertedMesh->BodySetup->DefaultInstance.GetObjectType());
            }
        }

        if (StaticMeshMaterial)
        {
            StaticMeshComponent->SetMaterial(0, StaticMeshMaterial);
        }
        else if (ProceduralDefaultMaterial)
        {
            StaticMeshComponent->SetMaterial(0, ProceduralDefaultMaterial);
        }
        else
        {
            for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
            {
                UMaterialInterface* SectionMaterial = ProceduralMeshComponent->GetMaterial(SectionIdx);
                if (SectionMaterial)
                {
                    StaticMeshComponent->SetMaterial(SectionIdx, SectionMaterial);
                }
            }
        }

        StaticMeshComponent->SetVisibility(bShowStaticMeshComponent, true);

        if (!StaticMeshComponent->GetAttachParent())
        {
            StaticMeshComponent->AttachToComponent(ProceduralMeshComponent, FAttachmentTransformRules::KeepWorldTransform);
        }

        StaticMeshComponent->MarkRenderStateDirty();
        StaticMeshComponent->RecreatePhysicsState();
    }
}

UStaticMesh* AProceduralMeshActor::ConvertProceduralMeshToStaticMesh() 
{
  if (!ProceduralMeshComponent) {
    return nullptr;
  }

  const int32 NumSections = ProceduralMeshComponent->GetNumSections();
  if (NumSections == 0) {
    return nullptr;
  }

  UStaticMesh* StaticMesh = CreateStaticMeshObject();
  if (!StaticMesh) {
    return nullptr;
  }

  if (!BuildStaticMeshGeometryFromProceduralMesh(StaticMesh)) {
    return nullptr;
  }

  InitializeStaticMeshRenderData(StaticMesh);
  SetupBodySetupAndCollision(StaticMesh);
  StaticMesh->CreateNavCollision(true);

  return StaticMesh;
}
UStaticMesh* AProceduralMeshActor::CreateStaticMeshObject() const
{
  UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
      GetTransientPackage(), NAME_None, RF_Public | RF_Transient);

  if (!StaticMesh) {
    return nullptr;
  }

  StaticMesh->SetFlags(RF_Public | RF_Transient);
  StaticMesh->NeverStream = true;
  StaticMesh->LightMapResolution = 64;
  StaticMesh->LightMapCoordinateIndex = 0;
  StaticMesh->LightmapUVDensity = 512.0f;
  StaticMesh->LODForCollision = 0;
  StaticMesh->bAllowCPUAccess = true;
  StaticMesh->bIsBuiltAtRuntime = true;
  StaticMesh->bGenerateMeshDistanceField = false;
  StaticMesh->bHasNavigationData = false;
  StaticMesh->bSupportPhysicalMaterialMasks = false;
  StaticMesh->bSupportUniformlyDistributedSampling = false;
  StaticMesh->LpvBiasMultiplier = 1.0f;

  const int32 NumSections = ProceduralMeshComponent ? ProceduralMeshComponent->GetNumSections() : 0;
  for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
  {
    UMaterialInterface* SectionMaterial = ProceduralMeshComponent->GetMaterial(SectionIdx);
    FName MaterialSlotName = FName(*FString::Printf(TEXT("MaterialSlot_%d"), SectionIdx));
    FStaticMaterial NewStaticMaterial(SectionMaterial, MaterialSlotName);
    NewStaticMaterial.UVChannelData = FMeshUVChannelInfo(1024.f);
    StaticMesh->StaticMaterials.Add(NewStaticMaterial);
  }

  return StaticMesh;
}
bool AProceduralMeshActor::BuildMeshDescriptionFromPMC(FMeshDescription& OutMeshDescription, UStaticMesh* StaticMesh) const
{
  if (!ProceduralMeshComponent) {
    return false;
  }

  const int32 NumSections = ProceduralMeshComponent->GetNumSections();
  if (NumSections == 0) {
    return false;
  }

  FStaticMeshAttributes Attributes(OutMeshDescription);
  Attributes.Register();
  
  TPolygonGroupAttributesRef<FName> PolygonGroupMaterialSlotNames =
      Attributes.GetPolygonGroupMaterialSlotNames();

  FMeshDescriptionBuilder MeshDescBuilder;
  MeshDescBuilder.SetMeshDescription(&OutMeshDescription);
  MeshDescBuilder.EnablePolyGroups();
  MeshDescBuilder.SetNumUVLayers(2);

  TMap<FVector, FVertexID> VertexMap;

  for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx) {
    FProcMeshSection* SectionData =
        ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
    if (!SectionData || SectionData->ProcVertexBuffer.Num() < 3 ||
        SectionData->ProcIndexBuffer.Num() < 3) {
      continue;
    }

    FName MaterialSlotName = StaticMesh->StaticMaterials[SectionIdx].MaterialSlotName;
    const FPolygonGroupID PolygonGroup = MeshDescBuilder.AppendPolygonGroup();
    PolygonGroupMaterialSlotNames.Set(PolygonGroup, MaterialSlotName);

    TArray<FVertexInstanceID> VertexInstanceIDs;

    for (const FProcMeshVertex& ProcVertex : SectionData->ProcVertexBuffer) {
      FVertexID VertexID;

      if (FVertexID* FoundID = VertexMap.Find(ProcVertex.Position)) {
        VertexID = *FoundID;
      } else {
        VertexID = MeshDescBuilder.AppendVertex(ProcVertex.Position);
        VertexMap.Add(ProcVertex.Position, VertexID);
      }
      
      FVertexInstanceID InstanceID = MeshDescBuilder.AppendInstance(VertexID);
      MeshDescBuilder.SetInstanceNormal(InstanceID, ProcVertex.Normal);
      MeshDescBuilder.SetInstanceUV(InstanceID, ProcVertex.UV0, 0);
      MeshDescBuilder.SetInstanceUV(InstanceID, ProcVertex.UV0, 1);
      MeshDescBuilder.SetInstanceColor(InstanceID, FVector4(ProcVertex.Color));
      VertexInstanceIDs.Add(InstanceID);
    }

    for (int32 i = 0; i < SectionData->ProcIndexBuffer.Num(); i += 3) {
      if (i + 2 >= SectionData->ProcIndexBuffer.Num()) {
        break;
      }

      int32 Index1 = SectionData->ProcIndexBuffer[i + 0];
      int32 Index2 = SectionData->ProcIndexBuffer[i + 1];
      int32 Index3 = SectionData->ProcIndexBuffer[i + 2];
      
      if (Index1 >= VertexInstanceIDs.Num() ||
          Index2 >= VertexInstanceIDs.Num() ||
          Index3 >= VertexInstanceIDs.Num()) {
        continue;
      }
      
      FVertexInstanceID V1 = VertexInstanceIDs[Index1];
      FVertexInstanceID V2 = VertexInstanceIDs[Index2];
      FVertexInstanceID V3 = VertexInstanceIDs[Index3];

      MeshDescBuilder.AppendTriangle(V1, V2, V3, PolygonGroup);
    }
  }

  return OutMeshDescription.Vertices().Num() > 0;
}

void AProceduralMeshActor::GenerateTangentsManually(FMeshDescription& MeshDescription) const
{
    FStaticMeshAttributes Attributes(MeshDescription);
    
    TVertexAttributesRef<FVector> VertexPositions = Attributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
    TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
    
    for (const FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs())
    {
        const TArray<FTriangleID>& TriangleIDs = MeshDescription.GetPolygonTriangleIDs(PolygonID);
        
        for (const FTriangleID TriangleID : TriangleIDs)
        {
            TArrayView<const FVertexInstanceID> VertexInstances = MeshDescription.GetTriangleVertexInstances(TriangleID);
            
            if (VertexInstances.Num() != 3)
            {
                continue;
            }

            const FVertexInstanceID Instance0 = VertexInstances[0];
            const FVertexInstanceID Instance1 = VertexInstances[1];
            const FVertexInstanceID Instance2 = VertexInstances[2];
            
            const FVector P0 = VertexPositions[MeshDescription.GetVertexInstanceVertex(Instance0)];
            const FVector P1 = VertexPositions[MeshDescription.GetVertexInstanceVertex(Instance1)];
            const FVector P2 = VertexPositions[MeshDescription.GetVertexInstanceVertex(Instance2)];
            
            const FVector2D UV0 = VertexInstanceUVs.Get(Instance0, 0);
            const FVector2D UV1 = VertexInstanceUVs.Get(Instance1, 0);
            const FVector2D UV2 = VertexInstanceUVs.Get(Instance2, 0);
            
            const FVector Edge1 = P1 - P0;
            const FVector Edge2 = P2 - P0;
            
            const FVector2D DeltaUV1 = UV1 - UV0;
            const FVector2D DeltaUV2 = UV2 - UV0;
            
            float Det = (DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X);
            
            if (FMath::IsNearlyZero(Det))
            {
                Det = 1.0f; 
            }
            
            const float InvDet = 1.0f / Det;
            
            FVector FaceTangent;
            FaceTangent.X = InvDet * (DeltaUV2.Y * Edge1.X - DeltaUV1.Y * Edge2.X);
            FaceTangent.Y = InvDet * (DeltaUV2.Y * Edge1.Y - DeltaUV1.Y * Edge2.Y);
            FaceTangent.Z = InvDet * (DeltaUV2.Y * Edge1.Z - DeltaUV1.Y * Edge2.Z);
            FaceTangent.Normalize();
            
            const FVertexInstanceID CurrentInstances[3] = { Instance0, Instance1, Instance2 };
            
            for (int i = 0; i < 3; i++)
            {
                FVertexInstanceID CurrentID = CurrentInstances[i];
                const FVector Normal = VertexInstanceNormals[CurrentID];
                
                FVector OrthoTangent = (FaceTangent - Normal * FVector::DotProduct(Normal, FaceTangent));
                OrthoTangent.Normalize();
                
                if (OrthoTangent.IsZero())
                {
                    FVector TangentX = FVector::CrossProduct(Normal, FVector::UpVector);
                    if (TangentX.IsZero()) TangentX = FVector::CrossProduct(Normal, FVector::RightVector);
                    OrthoTangent = TangentX.GetSafeNormal();
                }
                
                VertexInstanceTangents[CurrentID] = OrthoTangent;
                
                FVector Bitangent = FVector::CrossProduct(Normal, OrthoTangent);
                float Sign = (FVector::DotProduct(FVector::CrossProduct(Normal, FaceTangent), Bitangent) < 0.0f) ? -1.0f : 1.0f;
                VertexInstanceBinormalSigns[CurrentID] = Sign;
            }
        }
    }
}

bool AProceduralMeshActor::BuildStaticMeshGeometryFromProceduralMesh(UStaticMesh* StaticMesh) const
{
  if (!StaticMesh) {
    return false;
  }

  FMeshDescription MeshDescription;
  
  FStaticMeshAttributes Attributes(MeshDescription);
  Attributes.Register();

  if (!BuildMeshDescriptionFromPMC(MeshDescription, StaticMesh)) {
    return false;
  }

  GenerateTangentsManually(MeshDescription);
  TArray<const FMeshDescription*> MeshDescPtrs;
  MeshDescPtrs.Emplace(&MeshDescription);
  
  UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
  BuildParams.bUseHashAsGuid = true;
  BuildParams.bMarkPackageDirty = true;
  BuildParams.bBuildSimpleCollision = false;
  BuildParams.bCommitMeshDescription = true;
  
  StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);

  return true;
}
bool AProceduralMeshActor::InitializeStaticMeshRenderData(UStaticMesh* StaticMesh) const
{
  if (!StaticMesh || !StaticMesh->RenderData || StaticMesh->RenderData->LODResources.Num() < 1)
  {
    return false;
  }

  StaticMesh->NeverStream = true; 
  StaticMesh->bIgnoreStreamingMipBias = true;
  StaticMesh->LightMapCoordinateIndex = 0;

  StaticMesh->RenderData->ScreenSize[0].Default = 0.0f;
  StaticMesh->RenderData->ScreenSize[1].Default = 0.0f;

  StaticMesh->CalculateExtendedBounds();
  if (StaticMesh->ExtendedBounds.SphereRadius < 10.0f)
  {
    StaticMesh->ExtendedBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(500.0f), 1000.0f);
    if (StaticMesh->RenderData) StaticMesh->RenderData->Bounds = StaticMesh->ExtendedBounds;
  }

  const float ForcedUVDensity = 1024.0f;
  if (StaticMesh->StaticMaterials.Num() > 0)
  {
    for (FStaticMaterial& Mat : StaticMesh->StaticMaterials)
    {
      Mat.UVChannelData.LocalUVDensities[0] = ForcedUVDensity;
      Mat.UVChannelData.LocalUVDensities[1] = ForcedUVDensity;
      Mat.UVChannelData.LocalUVDensities[2] = ForcedUVDensity;
      Mat.UVChannelData.LocalUVDensities[3] = ForcedUVDensity;
    }
  }

  FStaticMeshLODResources& LODResources = StaticMesh->RenderData->LODResources[0];
  LODResources.bHasColorVertexData = true;

  StaticMesh->InitResources();

  StaticMesh->bForceMiplevelsToBeResident = true;
  StaticMesh->SetForceMipLevelsToBeResident(30.0f, 0);

  return true;
}

void AProceduralMeshActor::SetupBodySetupProperties(UBodySetup* BodySetup) const
{
  if (!BodySetup) {
    return;
  }
  
  BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
  BodySetup->bDoubleSidedGeometry = false;
  BodySetup->bMeshCollideAll = true;
  
  if (ProceduralMeshComponent && ProceduralMeshComponent->ProcMeshBodySetup && ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial)
  {
    BodySetup->PhysMaterial = ProceduralMeshComponent->ProcMeshBodySetup->PhysMaterial;
  }
  
  BodySetup->DefaultInstance.SetCollisionProfileName(TEXT("BlockAll"));
  BodySetup->DefaultInstance.SetEnableGravity(false);
  BodySetup->DefaultInstance.bUseCCD = false;
}
bool AProceduralMeshActor::GenerateSimpleCollision(UBodySetup* BodySetup, UStaticMesh* StaticMesh) const
{
  if (!BodySetup || !ProceduralMeshComponent) {
    return false;
  }

  int32 HullCount = 8;
  int32 MaxHullVerts = 16;
  uint32 HullPrecision = 100000;
  
  bool bSuccess = FModelGenConvexDecomp::GenerateConvexHulls(
    ProceduralMeshComponent,
    BodySetup,
    HullCount,
    MaxHullVerts,
    HullPrecision);
  
  if (bSuccess && BodySetup->AggGeom.ConvexElems.Num() > 0)
  {
    return true;
  }
  else
  {
    if (StaticMesh && StaticMesh->RenderData.IsValid())
    {
      const FBoxSphereBounds& Bounds = StaticMesh->RenderData->Bounds;
      if (Bounds.BoxExtent.X > 0.0f && Bounds.BoxExtent.Y > 0.0f && Bounds.BoxExtent.Z > 0.0f)
      {
        FKBoxElem BoxElem;
        BoxElem.Center = Bounds.Origin;
        BoxElem.X = Bounds.BoxExtent.X * 2.0f;
        BoxElem.Y = Bounds.BoxExtent.Y * 2.0f;
        BoxElem.Z = Bounds.BoxExtent.Z * 2.0f;
        BodySetup->AggGeom.BoxElems.Add(BoxElem);
        return true;
      }
    }
  }
  
  return false;
}

// 辅助函数：手动创建所有 ConvexMesh
int32 AProceduralMeshActor::CreateConvexMeshesManually(UBodySetup* BodySetup, IPhysXCookingModule* PhysXCookingModule) const
{
  if (!BodySetup || !PhysXCookingModule || !PhysXCookingModule->GetPhysXCooking()) {
    return 0;
  }

  if (BodySetup->AggGeom.GetElementCount() == 0) {
    return 0;
  }

  // 确保所有 ConvexElem 都调用了 UpdateElemBox() 并设置了有效的 Transform
  int32 ValidConvexElemCount = 0;
  for (FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
  {
    if (ConvexElem.VertexData.Num() < 4)
    {
      continue;
    }
    
    bool bHasValidVertices = true;
    for (const FVector& Vert : ConvexElem.VertexData)
    {
      if (Vert.ContainsNaN() || 
          !FMath::IsFinite(Vert.X) || 
          !FMath::IsFinite(Vert.Y) || 
          !FMath::IsFinite(Vert.Z))
      {
        bHasValidVertices = false;
        break;
      }
    }
    
    if (!bHasValidVertices)
    {
      continue;
    }
    
    ConvexElem.UpdateElemBox();
    if (!ConvexElem.GetTransform().IsValid())
    {
      ConvexElem.SetTransform(FTransform::Identity);
    }
    ConvexElem.BakeTransformToVerts();
    ValidConvexElemCount++;
  }
  
  BodySetup->bNeverNeedsCookedCollisionData = false;
  BodySetup->InvalidatePhysicsData();
  
  int32 ValidConvexMeshCount = 0;
  for (int32 ElemIdx = 0; ElemIdx < BodySetup->AggGeom.ConvexElems.Num(); ++ElemIdx)
  {
    FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems[ElemIdx];
    
    if (ConvexElem.VertexData.Num() < 4)
    {
      continue;
    }
    
    bool bHasValidVertices = true;
    for (const FVector& Vert : ConvexElem.VertexData)
    {
      if (Vert.ContainsNaN() || 
          !FMath::IsFinite(Vert.X) || 
          !FMath::IsFinite(Vert.Y) || 
          !FMath::IsFinite(Vert.Z))
      {
        bHasValidVertices = false;
        break;
      }
    }
    
    if (!bHasValidVertices)
    {
      continue;
    }
    
    ConvexElem.UpdateElemBox();
    if (!ConvexElem.GetTransform().IsValid())
    {
      ConvexElem.SetTransform(FTransform::Identity);
    }
    
    if (!ConvexElem.GetTransform().Equals(FTransform::Identity))
    {
      ConvexElem.BakeTransformToVerts();
    }
    physx::PxConvexMesh* NewConvexMesh = nullptr;
    EPhysXMeshCookFlags CookFlags = EPhysXMeshCookFlags::Default;
    
    EPhysXCookingResult Result = PhysXCookingModule->GetPhysXCooking()->CreateConvex(
      FName(FPlatformProperties::GetPhysicsFormat()),
      CookFlags,
      ConvexElem.VertexData,
      NewConvexMesh
    );
    
    switch (Result)
    {
    case EPhysXCookingResult::Succeeded:
      ConvexElem.SetConvexMesh(NewConvexMesh);
      ValidConvexMeshCount++;
      break;
    case EPhysXCookingResult::SucceededWithInflation:
      ConvexElem.SetConvexMesh(NewConvexMesh);
      ValidConvexMeshCount++;
      break;
    case EPhysXCookingResult::Failed:
      break;
    default:
      break;
    }
  }
  
  BodySetup->bCreatedPhysicsMeshes = true;
  
  return ValidConvexMeshCount;
}
bool AProceduralMeshActor::ExtractTriMeshDataFromPMC(TArray<FVector>& OutVertices, TArray<FTriIndices>& OutIndices) const
{
  if (!ProceduralMeshComponent) {
    return false;
  }

  OutVertices.Empty();
  OutIndices.Empty();
  
  int32 TotalVertices = 0;
  int32 TotalSkippedTriangles = 0;
  
  for (int32 SectionIdx = 0; SectionIdx < ProceduralMeshComponent->GetNumSections(); ++SectionIdx)
  {
    FProcMeshSection* SectionData = ProceduralMeshComponent->GetProcMeshSection(SectionIdx);
    if (!SectionData) continue;
    
    const int32 SectionVertexOffset = TotalVertices;
    const int32 SectionVertexCount = SectionData->ProcVertexBuffer.Num();
    
    for (const FProcMeshVertex& ProcVertex : SectionData->ProcVertexBuffer)
    {
      OutVertices.Add(ProcVertex.Position);
    }
    const int32 NumSectionIndices = SectionData->ProcIndexBuffer.Num();
    if (NumSectionIndices % 3 == 0 && SectionVertexCount > 0)
    {
      for (int32 Idx = 0; Idx < NumSectionIndices - 2; Idx += 3)
      {
        if (Idx < 0 || Idx + 2 >= NumSectionIndices)
        {
          break;
        }
        
        uint32 LocalV0 = SectionData->ProcIndexBuffer[Idx];
        uint32 LocalV1 = SectionData->ProcIndexBuffer[Idx + 1];
        uint32 LocalV2 = SectionData->ProcIndexBuffer[Idx + 2];
        
        if (LocalV0 >= static_cast<uint32>(SectionVertexCount) || 
            LocalV1 >= static_cast<uint32>(SectionVertexCount) || 
            LocalV2 >= static_cast<uint32>(SectionVertexCount))
        {
          TotalSkippedTriangles++;
          continue;
        }
        
        const uint32 V0 = SectionVertexOffset + LocalV0;
        const uint32 V1 = SectionVertexOffset + LocalV1;
        const uint32 V2 = SectionVertexOffset + LocalV2;
        
        if (V0 < static_cast<uint32>(OutVertices.Num()) && 
            V1 < static_cast<uint32>(OutVertices.Num()) && 
            V2 < static_cast<uint32>(OutVertices.Num()))
        {
          FTriIndices Tri;
          Tri.v0 = V0;
          Tri.v1 = V1;
          Tri.v2 = V2;
          OutIndices.Add(Tri);
        }
        else
        {
          TotalSkippedTriangles++;
        }
      }
    }
    
    TotalVertices = OutVertices.Num();
  }
  
  if (OutVertices.Num() > 0 && OutIndices.Num() > 0)
  {
    return true;
  }
  
  return false;
}

// 辅助函数：从 RenderData 提取 TriMesh 数据
bool AProceduralMeshActor::ExtractTriMeshDataFromRenderData(UStaticMesh* StaticMesh, TArray<FVector>& OutVertices, TArray<FTriIndices>& OutIndices) const
{
  if (!StaticMesh || !StaticMesh->RenderData || StaticMesh->RenderData->LODResources.Num() == 0) {
    return false;
  }

  OutVertices.Empty();
  OutIndices.Empty();
  
  const FStaticMeshLODResources& LODResource = StaticMesh->RenderData->LODResources[0];
  const FPositionVertexBuffer& PositionVertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
  const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;

  // 提取顶点
  const int32 NumVertices = PositionVertexBuffer.GetNumVertices();
  if (NumVertices > 0)
  {
    OutVertices.Reserve(NumVertices);
    for (int32 VertIdx = 0; VertIdx < NumVertices; ++VertIdx)
    {
      if (VertIdx >= 0 && VertIdx < NumVertices)
      {
        OutVertices.Add(PositionVertexBuffer.VertexPosition(VertIdx));
      }
      else
      {
        break;
      }
    }

    // 提取索引并转换为 FTriIndices 格式
    const int32 NumIndices = IndexBuffer.GetNumIndices();
    
    if (NumIndices > 0 && NumIndices % 3 == 0)
    {
      OutIndices.Reserve(NumIndices / 3);
      
      int32 SkippedTriangles = 0;
      for (int32 Idx = 0; Idx < NumIndices - 2; Idx += 3)
      {
        if (Idx < 0 || Idx + 2 >= NumIndices)
        {
          break;
        }
        
        uint32 V0 = IndexBuffer.GetIndex(Idx);
        uint32 V1 = IndexBuffer.GetIndex(Idx + 1);
        uint32 V2 = IndexBuffer.GetIndex(Idx + 2);
        
        if (V0 >= static_cast<uint32>(NumVertices) || 
            V1 >= static_cast<uint32>(NumVertices) || 
            V2 >= static_cast<uint32>(NumVertices))
        {
          SkippedTriangles++;
          continue;
        }
        
        FTriIndices Tri;
        Tri.v0 = V0;
        Tri.v1 = V1;
        Tri.v2 = V2;
        OutIndices.Add(Tri);
      }
      
      if (OutIndices.Num() > 0)
      {
        return true;
      }
    }
  }
  
  return false;
}

bool AProceduralMeshActor::GenerateComplexCollision(UBodySetup* BodySetup, IPhysXCookingModule* PhysXCookingModule) const
{
  if (!BodySetup) {
    return false;
  }

  if (BodySetup->TriMeshes.Num() > 0)
  {
    BodySetup->TriMeshes.Empty();
  }
  
  TArray<FVector> NewVertices;
  TArray<FTriIndices> NewIndices;
  
  if (ExtractTriMeshDataFromPMC(NewVertices, NewIndices))
  {
    if (NewVertices.Num() > 0 && NewIndices.Num() > 0)
    {
      if (!PhysXCookingModule)
      {
        PhysXCookingModule = GetPhysXCookingModule();
      }
      
      if (PhysXCookingModule && PhysXCookingModule->GetPhysXCooking())
      {
        BodySetup->TriMeshes.AddZeroed();

        EPhysXMeshCookFlags RuntimeCookFlags = EPhysXMeshCookFlags::Default;
        if (UPhysicsSettings::Get()->bSuppressFaceRemapTable)
        {
          RuntimeCookFlags |= EPhysXMeshCookFlags::SuppressFaceRemapTable;
        }

        TArray<uint16> MaterialIndices;
        MaterialIndices.AddZeroed(NewIndices.Num());

        const bool bError = !PhysXCookingModule->GetPhysXCooking()->CreateTriMesh(
          FName(FPlatformProperties::GetPhysicsFormat()),
          RuntimeCookFlags,
          NewVertices,
          NewIndices,
          MaterialIndices,
          true,
          BodySetup->TriMeshes[0]);

        if (!bError)
        {
          BodySetup->bCreatedPhysicsMeshes = true;
          return true;
        }
        else
        {
          BodySetup->TriMeshes.Empty();
        }
      }
    }
  }
  
  return false;
}

void AProceduralMeshActor::LogCollisionStatistics(UBodySetup* BodySetup, UStaticMesh* StaticMesh) const
{
}
void AProceduralMeshActor::SetupBodySetupAndCollision(UStaticMesh* StaticMesh) const
{
  if (!StaticMesh) {
    return;
  }

  StaticMesh->CreateBodySetup();
  UBodySetup* NewBodySetup = StaticMesh->BodySetup;
  if (!NewBodySetup)
  {
    return;
  }

  GenerateSimpleCollision(NewBodySetup, StaticMesh);
  SetupBodySetupProperties(NewBodySetup);

  IPhysXCookingModule* PhysXCookingModule = GetPhysXCookingModule();
  if (NewBodySetup->AggGeom.GetElementCount() > 0)
  {
    CreateConvexMeshesManually(NewBodySetup, PhysXCookingModule);
  }
  
  TArray<FVector> NewVertices;
  TArray<FTriIndices> NewIndices;
  bool bDataFromPMC = ExtractTriMeshDataFromPMC(NewVertices, NewIndices);
  if (!bDataFromPMC)
  {
    ExtractTriMeshDataFromRenderData(StaticMesh, NewVertices, NewIndices);
  }
  
  if (NewVertices.Num() > 0 && NewIndices.Num() > 0)
  {
    if (!PhysXCookingModule)
    {
      PhysXCookingModule = GetPhysXCookingModule();
    }
    
    if (PhysXCookingModule && PhysXCookingModule->GetPhysXCooking())
    {
      NewBodySetup->TriMeshes.AddZeroed();

      EPhysXMeshCookFlags RuntimeCookFlags = EPhysXMeshCookFlags::Default;
      if (UPhysicsSettings::Get()->bSuppressFaceRemapTable)
      {
        RuntimeCookFlags |= EPhysXMeshCookFlags::SuppressFaceRemapTable;
      }

      TArray<uint16> MaterialIndices;
      MaterialIndices.AddZeroed(NewIndices.Num());

      const bool bError = !PhysXCookingModule->GetPhysXCooking()->CreateTriMesh(
        FName(FPlatformProperties::GetPhysicsFormat()),
        RuntimeCookFlags,
        NewVertices,
        NewIndices,
        MaterialIndices,
        true,
        NewBodySetup->TriMeshes[0]);

      if (!bError)
      {
        NewBodySetup->bCreatedPhysicsMeshes = true;
      }
      else
      {
        NewBodySetup->TriMeshes.Empty();
      }
    }
  }
  
  LogCollisionStatistics(NewBodySetup, StaticMesh);
}
