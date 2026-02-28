// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotInstanceComponent.h"
#include "SkelotAnimCollection.h"
#include "SkelotWorld.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"



USkelotInstanceComponent::USkelotInstanceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void USkelotInstanceComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport /*= ETeleportType::None*/)
{
	ASkelotWorld* SKWorld = ASkelotWorld::Get(GetWorld(), false);
	if (SKWorld && SKWorld->IsHandleValid(Handle))
	{
		SKWorld->SetInstanceTransform(Handle.InstanceIndex, this->GetComponentTransform());
	}
}

void USkelotInstanceComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	DestroySkelotInstance();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void USkelotInstanceComponent::OnRegister()
{
	Super::OnRegister();
	SetRenderParams(RenderParams);
}

void USkelotInstanceComponent::OnUnregister()
{
	DestroySkelotInstance();
	Super::OnUnregister();
}

void USkelotInstanceComponent::EndPlay(EEndPlayReason::Type Reason)
{
	DestroySkelotInstance();
	Super::EndPlay(Reason);
	
}

FTransform USkelotInstanceComponent::GetSocketTransform(FName SocketName, ERelativeTransformSpace TransformSpace) const
{
	ASkelotWorld* SKWorld = ASkelotWorld::Get(GetWorld(), false);
	if(SKWorld && SKWorld->IsHandleValid(Handle))
	{
		FTransform SocketT = SKWorld->GetInstanceSocketTransform(Handle.InstanceIndex, SocketName, nullptr, false);
		return SocketT * Super::GetSocketTransform(SocketName, TransformSpace);
	}

	return FTransform::Identity;
}

bool USkelotInstanceComponent::DoesSocketExist(FName InSocketName) const
{
	if (RenderParams)
	{
		if(RenderParams->Data.AnimCollection->BonesToCache.Contains(InSocketName))
			return true;

		const USkeletalMeshSocket* SKMeshSocket = ASkelotWorld::FindSkeletalSocket(RenderParams->Data, InSocketName, nullptr);
		if(SKMeshSocket)
			return RenderParams->Data.AnimCollection->BonesToCache.Contains(SKMeshSocket->BoneName);
	}

	return false;
}

bool USkelotInstanceComponent::HasAnySockets() const
{
	USkelotAnimCollection* AC = RenderParams ? RenderParams->Data.AnimCollection : nullptr;
	if (AC)
	{
		return AC->BonesToCache.Num() > 0;
	}

	return false;
}

void USkelotInstanceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateChildTransforms(EUpdateTransformFlags::OnlyUpdateIfUsingSocket);
}


void USkelotInstanceComponent::SetRenderParams(USkelotRenderParams* Params)
{
	DestroySkelotInstance();
	if (Params)
	{
		if(GetWorld() && GetWorld()->IsGameWorld())
		{
			if (ASkelotWorld* SKWorld = ASkelotWorld::Get(GetWorld()))
			{
				Handle = SKWorld->CreateInstance(GetComponentTransform(), RenderParams);
				bWantsOnUpdateTransform = true;
			}
		}
	}
}

void USkelotInstanceComponent::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	if (RenderParams)
	{
		for (const FSkelotMeshRenderDesc& MeshDesc : RenderParams->Data.Meshes)
		{
			if (MeshDesc.Mesh)
			{
				const TArray<USkeletalMeshSocket*> SKList = MeshDesc.Mesh->GetActiveSocketList();
				for (USkeletalMeshSocket* SocketPtr : SKList)
				{
					if(SocketPtr)
						OutSockets.Emplace(SocketPtr->SocketName, EComponentSocketType::Socket);
				}
			}
		}

		for(FName BoneName : RenderParams->Data.AnimCollection->BonesToCache)
			OutSockets.Emplace(BoneName, EComponentSocketType::Bone);
	}
}

void USkelotInstanceComponent::DestroySkelotInstance()
{
	if(GetWorld())
	{
		ASkelotWorld* SKWorld = ASkelotWorld::Get(GetWorld(), false);
		if (SKWorld)
		{
			SKWorld->DestroyInstance(Handle);
			Handle = FSkelotInstanceHandle();
			bWantsOnUpdateTransform = false;
		}
	}
}


#if WITH_EDITOR
void USkelotInstanceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USkelotInstanceComponent, RenderParams))
	{
		if(GetWorld() && GetWorld()->IsGameWorld())
			SetRenderParams(RenderParams);
	}
}

void USkelotInstanceComponent::PreEditChange(FProperty* PropertyThatWillChange)
{
	if (PropertyThatWillChange)
	{
		if (PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(USkelotInstanceComponent, RenderParams))
		{
			DestroySkelotInstance();
		}
	}

	Super::PreEditChange(PropertyThatWillChange);
}

#endif


