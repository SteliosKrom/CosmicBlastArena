// Fill out your copyright notice in the Description page of Project Settings.

#include "GrenadeHoldNotify.h"
#include "CosmicBlastArenaCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UGrenadeHoldNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if (!IsValid(MeshComp)) return;
	
	ACosmicBlastArenaCharacter* Character = Cast<ACosmicBlastArenaCharacter>(MeshComp->GetOwner());
	
	if (!IsValid(Character)) return;
	
	Character->OnThrowHold();
}

