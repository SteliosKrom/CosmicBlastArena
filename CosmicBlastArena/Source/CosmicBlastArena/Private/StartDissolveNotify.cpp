// Fill out your copyright notice in the Description page of Project Settings.

#include "StartDissolveNotify.h"
#include "CosmicBlastArenaCharacter.h"

void UStartDissolveNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, 
	const FAnimNotifyEventReference& EventReference)
{
	ACosmicBlastArenaCharacter* ArenaCharacter = Cast<ACosmicBlastArenaCharacter>(MeshComp->GetOwner());\
	if (!IsValid(ArenaCharacter)) return;
	
	ArenaCharacter->InitializeDissolveMaterials();
	ArenaCharacter->bIsDissolving = true;
}
