// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AudioManager.h"
#include "CosmicBlastArenaGameMode.h"
#include "PickupBase.h"
#include "AmmoPickup.generated.h"

class UAudioManager;
class ACosmicBlastArenaGameMode;

UCLASS()
class COSMICBLASTARENA_API AAmmoPickup : public APickupBase
{
	GENERATED_BODY()
	
public:
	
	virtual void BeginPlay() override;
	
	AAmmoPickup();
	
	void ApplyPickupEffect(ACosmicBlastArenaCharacter* Character) override;
	
	UPROPERTY()
	TObjectPtr<ACosmicBlastArenaGameMode> GameMode;
	
	UPROPERTY()
	TObjectPtr<UAudioManager> AudioManager;
};
