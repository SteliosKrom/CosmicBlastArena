// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AudioManager.h"
#include "PickupBase.h"
#include "HealthPickup.generated.h"

class UAudioManager;
class ACosmicBlastArenaGameMode;

UCLASS()
class COSMICBLASTARENA_API AHealthPickup : public APickupBase
{
	GENERATED_BODY()
	
public:
	
	AHealthPickup();
	
	virtual void BeginPlay() override;
	
	void ApplyPickupEffect(ACosmicBlastArenaCharacter* Character) override;
	
	UPROPERTY()
	TObjectPtr<ACosmicBlastArenaGameMode> GameMode;
	
	UPROPERTY()
	TObjectPtr<UAudioManager> AudioManager;
};
