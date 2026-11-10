// Fill out your copyright notice in the Description page of Project Settings.

#include "AmmoPickup.h"
#include "CosmicBlastArenaCharacter.h"

AAmmoPickup::AAmmoPickup()
{
	
}

void AAmmoPickup::ApplyPickupEffect(ACosmicBlastArenaCharacter* ArenaCharacter)
{
	if (IsValid(ArenaCharacter))
	{
		if (ArenaCharacter->ReserveAmmo >= ArenaCharacter->MaxReserveAmmo)
		{
			UE_LOG(LogTemp, Warning, TEXT("You have full ammo %.2d."), ArenaCharacter->ReserveAmmo);
			return;
		}
		
		ArenaCharacter->ReserveAmmo = FMath::Min(ArenaCharacter->ReserveAmmo + ArenaCharacter->AmmoBoost, ArenaCharacter->MaxReserveAmmo);
		ArenaCharacter->OnReserveAmmoChanged.Broadcast(ArenaCharacter->ReserveAmmo);
		Destroy();
	}
}