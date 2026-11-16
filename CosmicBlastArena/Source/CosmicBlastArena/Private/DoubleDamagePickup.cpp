// Fill out your copyright notice in the Description page of Project Settings.

#include "DoubleDamagePickup.h"
#include "CosmicBlastArenaGameMode.h"
#include "CosmicBlastArenaCharacter.h"

ADoubleDamagePickup::ADoubleDamagePickup()
{
	
}

void ADoubleDamagePickup::BeginPlay()
{
	Super::BeginPlay();
	GameMode = Cast<ACosmicBlastArenaGameMode>(GetWorld()->GetAuthGameMode());
	AudioManager = GameMode->FindComponentByClass<UAudioManager>();
}

void ADoubleDamagePickup::ApplyPickupEffect(ACosmicBlastArenaCharacter* ArenaCharacter)
{
	ArenaCharacter->ActivateDoubleDamage();
	AudioManager->PlaySFX(AudioManager->PickupSound);
	Destroy();
}
