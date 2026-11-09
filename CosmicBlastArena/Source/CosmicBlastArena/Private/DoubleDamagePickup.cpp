// Fill out your copyright notice in the Description page of Project Settings.

#include "DoubleDamagePickup.h"
#include "CosmicBlastArenaCharacter.h"

ADoubleDamagePickup::ADoubleDamagePickup()
{
	
}

void ADoubleDamagePickup::ApplyPickupEffect(ACosmicBlastArenaCharacter* ArenaCharacter)
{
	ArenaCharacter->ActivateDoubleDamage();
	Destroy();
}
