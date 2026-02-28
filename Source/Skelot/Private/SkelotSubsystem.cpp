// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotSubsystem.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "Components/SceneComponent.h"
#include "UObject/Package.h"
#include "Kismet/GameplayStatics.h"
#include "KismetTraceUtils.h"
#include "SkelotAnimCollection.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkelotWorld.h"
#include "Animation/AnimSequenceBase.h"
#include "SkelotSettings.h"


ASkelotWorld* USkelotWorldSubsystem::Internal_GetSingleton(const UObject* WorldContextObject, bool bCreateIfNotFound)
{
	return WorldContextObject ? Internal_GetSingleton(WorldContextObject->GetWorld(), bCreateIfNotFound) : nullptr;
}

ASkelotWorld* USkelotWorldSubsystem::Internal_GetSingleton(const UWorld* World, bool bCreateIfNotFound)
{
	USkelotWorldSubsystem* SkelotWSS = World ? World->GetSubsystem<USkelotWorldSubsystem>() : nullptr;
	if (!SkelotWSS)
		return nullptr;

	if (!SkelotWSS->PrimaryInstance)
	{
		//take the one already in map if any
		SkelotWSS->PrimaryInstance = (ASkelotWorld*)UGameplayStatics::GetActorOfClass(World, ASkelotWorld::StaticClass());

		if (!SkelotWSS->PrimaryInstance && bCreateIfNotFound)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;

			UClass* ClassPtr = GetDefault<USkelotDeveloperSettings>()->SkelotWorldClass.TryLoadClass<ASkelotWorld>();
			ClassPtr = ClassPtr ? ClassPtr : ASkelotWorld::StaticClass();

			SkelotWSS->PrimaryInstance = const_cast<UWorld*>(World)->SpawnActor<ASkelotWorld>(ClassPtr, SpawnParams);
		}
	}

	if (SkelotWSS->PrimaryInstance)
	{
		check(!SkelotWSS->PrimaryInstance->IsTemplate());
		check(SkelotWSS->PrimaryInstance->SOA.Slots.Num() > 0);
	}

	return SkelotWSS->PrimaryInstance;
}

ASkelotWorld* USkelotWorldSubsystem::GetSingleton(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	ASkelotWorld* SK = GetSingleton(WorldContextObject);
	return SK && SK->IsHandleValid(Handle) ? SK : nullptr;
}

ASkelotWorld* USkelotWorldSubsystem::GetSingleton(const UObject* WorldContextObject, FSkelotInstanceHandle Handle0, FSkelotInstanceHandle Handle1)
{
	ASkelotWorld* SK = GetSingleton(WorldContextObject);
	return SK && SK->IsHandleValid(Handle0) && SK->IsHandleValid(Handle1) ? SK : nullptr;
}

ASkelotWorld* USkelotWorldSubsystem::GetSingleton(const UObject* WorldContextObject)
{
	return Internal_GetSingleton(WorldContextObject, true);
}


bool USkelotWorldSubsystem::Skelot_AttachMesh(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const USkeletalMesh* Mesh, FName OrName, int32 OrIndex /*= -1*/, bool bAttach /*= true*/)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		if (Mesh)
			return Singleton->InstanceAttachMesh_ByAsset(Handle.InstanceIndex, Mesh, bAttach);
		else if (!OrName.IsNone())
			return Singleton->InstanceAttachMesh_ByName(Handle.InstanceIndex, OrName, bAttach);
		else if (OrIndex != -1)
			return Singleton->InstanceAttachMesh_ByIndex(Handle.InstanceIndex, OrIndex, bAttach);
	}
	return false;
}

void USkelotWorldSubsystem::Skelot_DetachMeshes(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		Singleton->InstanceDetachMeshes(Handle.InstanceIndex);
		
}

void USkelotWorldSubsystem::Skelot_AttachMeshes(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const TArray<USkeletalMesh*>& Meshes, bool bAttach /*= true*/)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		for(USkeletalMesh* SKMesh : Meshes)
			Singleton->InstanceAttachMesh_ByAsset(Handle.InstanceIndex, SKMesh, bAttach);
}

void USkelotWorldSubsystem::Skelot_AttachRandomhMeshByGroup(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, FName GroupName)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		Singleton->AttachRandomhMeshByGroup(Handle.InstanceIndex, GroupName);
}

void USkelotWorldSubsystem::Skelot_AttachAllMeshGroups(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		Singleton->AttachAllMeshGroups(Handle.InstanceIndex);
}

float USkelotWorldSubsystem::Skelot_PlayAnimation(const UObject* WorldContextObject, FSkelotInstanceHandle H, const FSkelotAnimPlayParams& Params)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		return Singleton->InstancePlayAnimation(H.InstanceIndex, Params);

	return -1;
}

USkelotAnimCollection* USkelotWorldSubsystem::Skelot_GetAnimCollection(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		return Singleton->GetInstanceAnimCollection(Handle);

	return nullptr;
}

void USkelotWorldSubsystem::Skelot_SetRenderParam(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, USkelotRenderParams* RenderParams)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		Singleton->SetInstanceRenderParams(Handle.InstanceIndex, RenderParams);
}

void USkelotWorldSubsystem::Skelot_SetRenderDesc(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const FSkelotInstanceRenderDesc& RenderParams)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		Singleton->SetInstanceRenderParams(Handle.InstanceIndex, RenderParams);
}

void USkelotWorldSubsystem::Skelot_SetCustomDataFloat4(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, int32 Offset, const FVector4f& Value)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		Singleton->SetInstanceCustomDataFloat(Handle.InstanceIndex, &Value.X, Offset, 4);
}

float USkelotWorldSubsystem::Skelot_GetCustomDataFloat(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, int32 FloatIndex)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		const float* Floats = Singleton->GetInstanceCustomDataFloats(Handle.InstanceIndex);
		if (Floats)
		{
			if(FloatIndex < Singleton->SOA.MaxNumCustomDataFloat)
				return Floats[FloatIndex];
		}
	}

	return 0;
}


void USkelotWorldSubsystem::SkelotQueryLocationOverlappingSphere(const UObject* WorldContextObject, const FVector& Center, float Radius, TArray<FSkelotInstanceHandle>& Instances)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		Singleton->QueryLocationOverlappingSphere(Center, Radius, Instances);
}

void USkelotWorldSubsystem::Skelot_RemoveInvalidHandles(const UObject* WorldContextObject, bool bMaintainOrder, TArray<FSkelotInstanceHandle>& Handles)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		Singleton->RemoveInvalidHandles(bMaintainOrder, Handles);
}

void USkelotWorldSubsystem::Skelot_GetAllHandles(const UObject* WorldContextObject, TArray<FSkelotInstanceHandle>& Handles)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		Singleton->GetAllHandles(Handles);
}

void USkelotWorldSubsystem::SkelotAttachChild(const UObject* WorldContextObject, FSkelotInstanceHandle Parent, FSkelotInstanceHandle Child, FName SocketOrBoneName, const FTransform& ReletiveTransform)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
		Singleton->InstanceAttachChild(Parent, Child, SocketOrBoneName, ReletiveTransform);
}
void USkelotWorldSubsystem::SkelotDetachFromParent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		Singleton->DetachInstanceFromParent(Handle.InstanceIndex);
}



FTransform USkelotWorldSubsystem::SkelotGetTransform(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		return Singleton->GetInstanceTransform(Handle.InstanceIndex);

	return FTransform::Identity;
}

void USkelotWorldSubsystem::SkelotSetTransform(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, const FTransform& Transform, bool bRelative)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		if(!bRelative)
		{
			Singleton->SetInstanceTransform(Handle.InstanceIndex, Transform);
		}
		else
		{
			if (FSkelotAttachParentData* AttachFrag = Singleton->GetInstanceAttachParentData(Handle.InstanceIndex))
			{
				AttachFrag->RelativeTransform = FTransform3f(Transform);
			}
		}
	}
}


FTransform USkelotWorldSubsystem::SkelotGetSocketTransform(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, FName SocketOrBoneName, USkeletalMesh* InMesh /*= nullptr*/, bool bWorldSpace /*= true*/)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		return Singleton->GetInstanceSocketTransform(Handle.InstanceIndex, SocketOrBoneName, InMesh, bWorldSpace);

	return FTransform::Identity;
}

UObject* USkelotWorldSubsystem::SkelotGetUserObject(const UObject* WorldContextObject, ESkelotValidity& ExecResult, FSkelotInstanceHandle Handle, TSubclassOf<UObject> Class /*= TSubclassOf<UObject>()*/)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		UObject* Obj = Singleton->GetInstanceUserObject(Handle.InstanceIndex);
		if (Obj && (!Class || Obj->IsA(Class)))
		{
			ExecResult = ESkelotValidity::Valid;
			return Obj;
		}
	}

	ExecResult = ESkelotValidity::NotValid;
	return nullptr;
}

void USkelotWorldSubsystem::SkelotSetUserObject(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, UObject* Object)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		return Singleton->SetInstanceUserObject(Handle.InstanceIndex, Object);

}

void USkelotWorldSubsystem::SkelotEnableDynamicPose(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, bool bEnable)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		Singleton->EnableInstanceDynamicPose(Handle.InstanceIndex, bEnable);
}

void USkelotWorldSubsystem::SkelotBindToComponent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, USkeletalMeshComponent* Component, int32 UserData, bool bCopyTransform)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		if(Singleton->EnableInstanceDynamicPose(Handle.InstanceIndex, true))
		{
			Singleton->TieDynamicPoseToComponent(Handle.InstanceIndex, Component, UserData, bCopyTransform);
		}
	}
}

void USkelotWorldSubsystem::SkelotUnbindFromComponent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, bool bKeepDynamicPoseEnabled, USkeletalMeshComponent*& OutComponent, int32& OutUserData)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		OutUserData = -1;
		OutComponent = Singleton->UntieDynamicPoseFromComponent(Handle.InstanceIndex, &OutUserData);
		Singleton->EnableInstanceDynamicPose(Handle.InstanceIndex, bKeepDynamicPoseEnabled);
	}
}

USkeletalMeshComponent* USkelotWorldSubsystem::SkelotGetBoundComponent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		FSkelotFrag_DynPoseTie* Data = Singleton->GetDynamicPoseTieData(Handle.InstanceIndex);
		return Data ? Data->SKMeshComponent : nullptr;
	}
	return nullptr;
}

void USkelotWorldSubsystem::Skelot_SetLifespan(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, float Lifespan)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		Singleton->SetInstancelifespan(Handle, Lifespan);
	}
}

void USkelotWorldSubsystem::Skelot_ClearTimer(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		Singleton->ClearInstanceTimer(Handle);
}



void USkelotWorldSubsystem::SetRootMotionParams(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, bool bExtractRootMotion, bool bApplyRootMotion)
{
	if (ASkelotWorld* SKWorld = GetSingleton(WorldContextObject, Handle))
	{
		SKWorld->SOA.Slots[Handle.InstanceIndex].bExtractRootMotion = bExtractRootMotion;
		SKWorld->SOA.Slots[Handle.InstanceIndex].bApplyRootMotion = bApplyRootMotion;
	}
}

void USkelotWorldSubsystem::Skelot_SetTimer(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, float Interval, bool bLoop, bool bGameTime, FSkelotGeneralDynamicDelegate Delegate, FName PayloadTag /*= FName()*/, UObject* PayloadObject /*= nullptr*/)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		FSkelotInstanceTimerData* TD = Singleton->Internal_SetInstanceTimer(Handle, Interval, bLoop, bGameTime);
		if (TD)
		{
			TD->Data.Delegate.Set<FSkelotGeneralDynamicDelegate>(Delegate);
			TD->Data.PayloadTag = PayloadTag;
			TD->Data.PayloadObject = PayloadObject;
		}
	}
}

FSkelotInstanceHandle USkelotWorldSubsystem::Skelot_CreateInstanceAttached(const UObject* WorldContextObject, USkelotRenderParams* RenderParams, FSkelotInstanceHandle Parent, FName SocketOrBoneName, const FTransform& ReletiveTransform)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		FSkelotInstanceHandle H = Singleton->CreateInstance(FTransform::Identity, RenderParams);
		Singleton->InstanceAttachChild(Parent, H, SocketOrBoneName, ReletiveTransform);
		return H;
	}

	return FSkelotInstanceHandle();
}

void USkelotWorldSubsystem::SkelotGetChildren(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, TArray<FSkelotInstanceHandle>& Children)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		Singleton->GetInstanceChildren<true>(Handle.InstanceIndex, Children);
	}
}

FSkelotInstanceHandle USkelotWorldSubsystem::SkelotGetParent(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
		return Singleton->IndexToHandle(Singleton->GetInstanceParentIndex(Handle.InstanceIndex));

	return FSkelotInstanceHandle();
}

bool USkelotWorldSubsystem::SkelotIsChildOf(const UObject* WorldContextObject, FSkelotInstanceHandle Child, FSkelotInstanceHandle Parent)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Child, Parent))
		return Singleton->IsInstanceChildOf(Child.InstanceIndex, Parent.InstanceIndex);

	return false;
}

void USkelotWorldSubsystem::Skelot_DestroyInstance(const UObject* WorldContextObject, FSkelotInstanceHandle H)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		Singleton->DestroyInstance(H);
	}
}

void USkelotWorldSubsystem::Skelot_DestroyInstances(const UObject* WorldContextObject, const TArray<FSkelotInstanceHandle>& Handles)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		for (FSkelotInstanceHandle H : Handles)
			Singleton->DestroyInstance(H);
	}
}

FSkelotInstanceHandle USkelotWorldSubsystem::Skelot_CreateInstance(const UObject* WorldContextObject, const FTransform& Transform, USkelotRenderParams* Params, UObject* UserObject)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		FSkelotInstanceHandle H = Singleton->CreateInstance(Transform, Params);
		if(UserObject)
			Singleton->SetInstanceUserObject(H.InstanceIndex, UserObject);

		return H;
	}

	return FSkelotInstanceHandle{};
}

void USkelotWorldSubsystem::Skelot_CreateInstances(const UObject* WorldContextObject, const TArray<FTransform>& Transforms, USkelotRenderParams* RenderParams, TArray<FSkelotInstanceHandle>& OutHandles)
{
	OutHandles.Reset();
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		OutHandles.Reserve(Transforms.Num());
		for (const FTransform& Transform : Transforms)
		{
			FSkelotInstanceHandle H = Singleton->CreateInstance(Transform, RenderParams);
			OutHandles.Add(H);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Collision Channel API Implementation

void USkelotWorldSubsystem::Skelot_SetInstanceCollisionChannel(const UObject* WorldContextObject, int32 InstanceIndex, ESkelotCollisionChannel Channel)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		Singleton->SetInstanceCollisionChannel(InstanceIndex, static_cast<uint8>(Channel));
	}
}

ESkelotCollisionChannel USkelotWorldSubsystem::Skelot_GetInstanceCollisionChannel(const UObject* WorldContextObject, int32 InstanceIndex)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		return static_cast<ESkelotCollisionChannel>(Singleton->GetInstanceCollisionChannel(InstanceIndex));
	}
	return ESkelotCollisionChannel::Channel0;
}

void USkelotWorldSubsystem::Skelot_SetInstanceCollisionChannelByHandle(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, ESkelotCollisionChannel Channel)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		Singleton->SetInstanceCollisionChannel(Handle, static_cast<uint8>(Channel));
	}
}

ESkelotCollisionChannel USkelotWorldSubsystem::Skelot_GetInstanceCollisionChannelByHandle(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		return static_cast<ESkelotCollisionChannel>(Singleton->GetInstanceCollisionChannel(Handle));
	}
	return ESkelotCollisionChannel::Channel0;
}

void USkelotWorldSubsystem::Skelot_SetInstanceCollisionMask(const UObject* WorldContextObject, int32 InstanceIndex, uint8 Mask)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		Singleton->SetInstanceCollisionMask(InstanceIndex, Mask);
	}
}

uint8 USkelotWorldSubsystem::Skelot_GetInstanceCollisionMask(const UObject* WorldContextObject, int32 InstanceIndex)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject))
	{
		return Singleton->GetInstanceCollisionMask(InstanceIndex);
	}
	return SkelotCollision::DefaultCollisionMask;
}

void USkelotWorldSubsystem::Skelot_SetInstanceCollisionMaskByHandle(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, uint8 Mask)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		Singleton->SetInstanceCollisionMask(Handle, Mask);
	}
}

uint8 USkelotWorldSubsystem::Skelot_GetInstanceCollisionMaskByHandle(const UObject* WorldContextObject, FSkelotInstanceHandle Handle)
{
	if (ASkelotWorld* Singleton = GetSingleton(WorldContextObject, Handle))
	{
		return Singleton->GetInstanceCollisionMask(Handle);
	}
	return SkelotCollision::DefaultCollisionMask;
}

UActorComponent* USkelotFunctionLib::SpawnComponent(const UObject* WorldContextObject, TSubclassOf<UActorComponent> Class, const FTransform& Transform)
{
	UActorComponent* Comp = NewObject<UActorComponent>(GetTransientPackage(), Class);
	if (Comp)
	{
		if (USceneComponent* SC = Cast<USceneComponent>(Comp))
		{
			SC->SetAbsolute(true, true, true);
			SC->SetRelativeTransform(Transform);
			SC->SetComponentToWorld(Transform);
		}
		Comp->RegisterComponentWithWorld(WorldContextObject->GetWorld());
	}
	return Comp;
}

USkeletalMeshComponent* USkelotFunctionLib::ConstructSkeletalMeshComponents(const UObject* WorldContextObject, FSkelotInstanceHandle Handle, UObject* Outer, bool bSetCustomPrimitiveDataFloat)
{
	ASkelotWorld* SKWorld = ASkelotWorld::Get(WorldContextObject);
	if (!IsValid(SKWorld) || !SKWorld->IsHandleValid(Handle))
		return nullptr;

	const FSkelotInstanceRenderDesc& RenderDesc = SKWorld->GetInstanceDesc(Handle.InstanceIndex);
	if (!RenderDesc.AnimCollection)
		return nullptr;

	USkeletalMeshComponent* RootComp = nullptr;
	Outer = Outer ? Outer : SKWorld;

	for (const uint8* SMIIter = SKWorld->GetInstanceSubmeshIndices(Handle.InstanceIndex); *SMIIter != 0xFF; SMIIter++)
	{
		const FSkelotMeshRenderDesc& MRD = RenderDesc.Meshes[*SMIIter];
		USkeletalMeshComponent* MeshComp = NewObject<USkeletalMeshComponent>(Outer);
		MeshComp->SetSkeletalMesh(MRD.Mesh);
		MeshComp->OverrideMaterials = MRD.OverrideMaterials;
		
		if (!RootComp)
		{
			RootComp = MeshComp;
			MeshComp->SetWorldTransform(SKWorld->GetInstanceTransform(Handle.InstanceIndex));
		}
		else
		{
			MeshComp->SetupAttachment(RootComp);
			MeshComp->SetLeaderPoseComponent(RootComp);
		}

		if (bSetCustomPrimitiveDataFloat)
		{
			for (int32 FloatIndex = 0; FloatIndex < RenderDesc.NumCustomDataFloat; FloatIndex++)
				MeshComp->SetCustomPrimitiveDataFloat(FloatIndex, SKWorld->GetInstanceCustomDataFloats(Handle.InstanceIndex)[FloatIndex]);
		}

		MeshComp->RegisterComponent();


	}

	const FSkelotInstancesSOA::FAnimData& AD = SKWorld->SOA.AnimDatas[Handle.InstanceIndex];
	if (RootComp && AD.IsSequenceValid())
	{
		RootComp->PlayAnimation(AD.CurrentAsset, SKWorld->SOA.Slots[Handle.InstanceIndex].bAnimationLooped);
		RootComp->SetPosition(AD.AnimationTime, false);
		RootComp->SetPlayRate(AD.AnimationPlayRate);
	}

	return RootComp;
}
