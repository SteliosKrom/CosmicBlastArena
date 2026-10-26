// Fill out your copyright notice in the Description page of Project Settings.

#include "ThrowGrenadeFinishedNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "CosmicBlastArenaCharacter.h"

void UThrowGrenadeFinishedNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if (!IsValid(MeshComp)) return;
	
	ACosmicBlastArenaCharacter* Character = Cast<ACosmicBlastArenaCharacter>(MeshComp->GetOwner());
	
	if (!IsValid(Character)) return;
	
	Character->OnThrowFinished();
}

