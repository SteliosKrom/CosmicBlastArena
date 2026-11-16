// Fill out your copyright notice in the Description page of Project Settings.

#include "HealthPickup.h"
#include "CosmicBlastArenaCharacter.h"
#include "CosmicBlastArenaGameMode.h"

AHealthPickup::AHealthPickup()
{
	
}

void AHealthPickup::BeginPlay()
{
	Super::BeginPlay();
	GameMode = Cast<ACosmicBlastArenaGameMode>(GetWorld()->GetAuthGameMode());
	AudioManager = GameMode->FindComponentByClass<UAudioManager>();
}

void AHealthPickup::ApplyPickupEffect(ACosmicBlastArenaCharacter* ArenaCharacter)
{
	if (IsValid(ArenaCharacter))
	{
		if (ArenaCharacter->CurrentHealth >= ArenaCharacter->MaxHealth)
		{
			UE_LOG(LogTemp, Warning, TEXT("You have full health %.2d."), ArenaCharacter->CurrentHealth);
			return;
		}
		ArenaCharacter->CurrentHealth = FMath::Min(ArenaCharacter->CurrentHealth + ArenaCharacter->HealthBoost, ArenaCharacter->MaxHealth);
		ArenaCharacter->OnHealthChanged.Broadcast(ArenaCharacter->CurrentHealth, ArenaCharacter->MaxHealth);
		AudioManager->PlaySFX(AudioManager->PickupSound);
		Destroy();
	}
}