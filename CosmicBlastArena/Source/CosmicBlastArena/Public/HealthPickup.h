// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "HealthPickup.generated.h"

UCLASS()
class COSMICBLASTARENA_API AHealthPickup : public APickupBase
{
	GENERATED_BODY()
	
public:
	
	AHealthPickup();
	
	void ApplyPickupEffect(ACosmicBlastArenaCharacter* Character) override;
	
};
