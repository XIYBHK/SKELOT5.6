// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotCharacter.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"


ACharacter_Skelot::ACharacter_Skelot()
{
}

void ACharacter_Skelot::SetRenderParams(USkelotRenderParams* Params)
{
	DestroySkelotInstance();
	if (Params)
	{
		if (GetWorld() && GetWorld()->IsGameWorld())
		{
			if (ASkelotWorld* SKWorld = ASkelotWorld::Get(GetWorld()))
			{
				Handle = SKWorld->CreateInstance(GetMesh()->GetComponentTransform(), RenderParams);
			}
		}
	}
}



void ACharacter_Skelot::DestroySkelotInstance()
{
	ASkelotWorld* SKWorld = ASkelotWorld::Get(GetWorld(), false);
	if (SKWorld)
	{
		SKWorld->DestroyInstance(Handle);
		Handle = FSkelotInstanceHandle();
	}
}

void ACharacter_Skelot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ASkelotWorld* SKWorld = ASkelotWorld::Get(GetWorld(), false);
	if (SKWorld && SKWorld->IsHandleValid(Handle))
	{
		SKWorld->SetInstanceTransform(Handle.InstanceIndex, GetMesh()->GetComponentTransform());
	}
}

void ACharacter_Skelot::BeginPlay()
{
	SetRenderParams(RenderParams);

	Super::BeginPlay();
}