// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "DoubleDamagePickup.generated.h"

UCLASS()
class COSMICBLASTARENA_API ADoubleDamagePickup : public APickupBase
{
	GENERATED_BODY()
	
public: 
	
	ADoubleDamagePickup();
	
	void ApplyPickupEffect(ACosmicBlastArenaCharacter* Character) override;
};
