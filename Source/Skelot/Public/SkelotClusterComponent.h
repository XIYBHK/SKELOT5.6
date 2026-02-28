// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "SkelotWorldBase.h"

#include "SkelotClusterComponent.generated.h"


/*
used for rendering a batch of skeletal meshes. spawned and managed by ASkelotWorld.
*/
UCLASS(ClassGroup=Rendering, collapsecategories, hidecategories=(Object,Activation,"Components|Activation",Physics,Collision,Mesh,PhysicsVolume), meta=(DisplayName="Skelot集群组件"))
class SKELOT_API USKelotClusterComponent : public USkelotBaseComponent
{
	GENERATED_BODY()
public:
	FSetElementId DescId;
	FSetElementId ClusterId;
	
	USKelotClusterComponent();
	

	FSkelotInstanceRenderDesc& GetRenderDesc() const;
	FSkelotCluster& GetClusterStruct() const;
	
	ASkelotWorld* GetSkelotWorld() const;

	
	FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	FBoxSphereBounds ComputeBoundWS() const;

	FBox CalcApproximateLocalBound() const;
	float GetAverageSmallestBoundRadius() const;

	void SendRenderTransform_Concurrent() override;
	void SendRenderDynamicData_Concurrent() override;
	void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	bool IsPostLoadThreadSafe() const override { return false; }
	void PostLoad() override;
	void BeginDestroy() override;

	void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type Reason) override;



	int32 GetNumMaterials() const override;
	UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	int32 GetMaterialIndex(FName MaterialSlotName) const override;
	TArray<FName> GetMaterialSlotNames() const override;
	bool IsMaterialSlotNameValid(FName MaterialSlotName) const override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	bool ShouldCreateRenderState() const;


};

