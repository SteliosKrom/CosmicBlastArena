// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PlayerHUD.h"
#include "CosmicBlastArenaGameMode.generated.h"

class UMaterialInterface;
class UPlayerHUD;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class ACosmicBlastArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	ACosmicBlastArenaGameMode();
	
protected:
	
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;

public:
	
	UPROPERTY(VisibleAnywhere)
	bool bRoundStarted = false;
	
	FTimerHandle InitialDelayHandle;
	FTimerHandle RoundIntroHandle;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Timer")
	float RoundTimeRemaining;
	
	UPROPERTY()
	TObjectPtr<UPlayerHUD> PlayerHUD; 
	
	UPROPERTY()
	TObjectPtr<UPlayerHUD> PlayerHUDTwo;
	
	UPROPERTY(EditDefaultsOnly, Category="Player Materials")
	TObjectPtr<UMaterialInterface> PlayerOneMaterial;
	
	UPROPERTY(EditDefaultsOnly, Category="Player Materials")
	TObjectPtr<UMaterialInterface> PlayerTwoMaterial;
	
	UFUNCTION()
	void StartRound();
	
	UFUNCTION()
	void ShowRoundIntro();
};



