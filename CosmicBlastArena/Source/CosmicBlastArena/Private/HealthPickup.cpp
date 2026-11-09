// Fill out your copyright notice in the Description page of Project Settings.

#include "HealthPickup.h"
#include "CosmicBlastArenaCharacter.h"

AHealthPickup::AHealthPickup()
{
	
}

void AHealthPickup::ApplyPickupEffect(ACosmicBlastArenaCharacter* ArenaCharacter)
{
	if (IsValid(ArenaCharacter))
	{
		if (ArenaCharacter->CurrentHealth >= ArenaCharacter->MaxHealth)
		{
			UE_LOG(LogTemp, Warning, TEXT("You have full health %.2f. You cannot heal!"), ArenaCharacter->CurrentHealth);
			return;
		}
		ArenaCharacter->CurrentHealth = FMath::Min(ArenaCharacter->CurrentHealth + ArenaCharacter->HealthBoost, ArenaCharacter->MaxHealth);
		ArenaCharacter->OnHealthChanged.Broadcast(ArenaCharacter->CurrentHealth, ArenaCharacter->MaxHealth);
		Destroy();
	}
}