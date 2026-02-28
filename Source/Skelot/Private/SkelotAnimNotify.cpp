// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotAnimNotify.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Animation/AnimSequenceBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "SkelotWorld.h"

void UAnimNotify_SkelotPlaySound::OnNotify(ASkelotWorld* SKWorld, FSkelotInstanceHandle Handle, UAnimSequenceBase* Animation) const
{
	if (Sound)
	{
		if (!Sound->IsOneShot())
			return;

// 		if (bFollow)
// 		{
// 			//#TODO
// 			//UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(Sound, Component, AttachName, FVector(ForceInit), EAttachLocation::SnapToTarget, false, VolumeMultiplier, PitchMultiplier);
// 			//Component->InstanceAttachChildComponent(InstanceIndex, AC, FTransform3f::Identity, AttachName, true);
// 		}
// 		else
		{
			UGameplayStatics::PlaySoundAtLocation(SKWorld->GetWorld(), Sound, SKWorld->GetInstanceLocation(Handle.InstanceIndex), VolumeMultiplier, PitchMultiplier);
		}
	}
}


void UAnimNotify_SkelotPlayNiagaraEffect::OnNotify(ASkelotWorld* SKWorld, FSkelotInstanceHandle Handle, UAnimSequenceBase* Animation) const
{
	if (!Template || Template->IsLooping())
		return;

	FTransform LocalTransform(RotationOffsetQuat.GetNormalized(), LocationOffset, Scale);
	const FTransform MeshTransform = LocalTransform * SKWorld->GetInstanceSocketTransform(Handle.InstanceIndex, SocketName);
	UNiagaraComponent* NComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(SKWorld->GetWorld(), Template, MeshTransform.GetLocation(), MeshTransform.GetRotation().Rotator(), MeshTransform.GetScale3D(), true);
	if (NComp)
	{
		if (Attached)
		{
			//#TODO
			//Component->InstanceAttachChildComponent(InstanceIndex, NComp, (FTransform3f)LocalTransform, SocketName, true);
		}

		if(LifeSpan > 0)
		{
			FTimerHandle TH;
			SKWorld->GetWorld()->GetTimerManager().SetTimer(TH, FTimerDelegate::CreateUObject(NComp, &UAudioComponent::DestroyComponent, true), LifeSpan, false);
		}
	}
}


