// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AudioManager.h"
#include "PickupBase.h"
#include "DoubleDamagePickup.generated.h"

class UAudioManager;
class ACosmicBlastArenaGameMode;

UCLASS()
class COSMICBLASTARENA_API ADoubleDamagePickup : public APickupBase
{
	GENERATED_BODY()
	
public: 
	
	virtual void BeginPlay() override;
	
	ADoubleDamagePickup();
	
	void ApplyPickupEffect(ACosmicBlastArenaCharacter* Character) override;
	
	UPROPERTY()
	TObjectPtr<ACosmicBlastArenaGameMode> GameMode;
	
	UPROPERTY()
	TObjectPtr<UAudioManager> AudioManager;
};
