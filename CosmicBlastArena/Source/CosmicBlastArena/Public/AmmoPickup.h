// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "AmmoPickup.generated.h"

UCLASS()
class COSMICBLASTARENA_API AAmmoPickup : public APickupBase
{
	GENERATED_BODY()
	
public:
	
	AAmmoPickup();
	
	void ApplyPickupEffect(ACosmicBlastArenaCharacter* Character) override;
};
