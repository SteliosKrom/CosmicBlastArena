// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PlayerHUD.h"
#include "CosmicBlastArenaGameMode.generated.h"

class UMaterialInterface;
class UPlayerHUD;
class APlayerStart;

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
	
	FTimerHandle InitialDelayHandle;
	FTimerHandle RoundIntroHandle;
	FTimerHandle EndRoundHandle;
	FTimerHandle RoundTransitionTimer;
	FTimerHandle BackToMenuHandle;
	
	int32 PlayerOneScore = 0;
	int32 PlayerTwoScore = 0;
	int32 CurrentRound = 1;
	
	UPROPERTY()
	TObjectPtr<UPlayerHUD> PlayerHUD; 
	
	UPROPERTY()
	TObjectPtr<UPlayerHUD> PlayerHUDTwo;
	
	UPROPERTY()
	TObjectPtr<ACosmicBlastArenaCharacter> CharacterOne;
	
	UPROPERTY()
	TObjectPtr<ACosmicBlastArenaCharacter> CharacterTwo;
	
	UPROPERTY()
	TObjectPtr<APlayerController> PlayerOneController;
	
	UPROPERTY()
	TObjectPtr<APlayerController> PlayerTwoController;
	
	UPROPERTY(EditInstanceOnly, Category="Respawn")
	TObjectPtr<APlayerStart> PlayerStartOne;

	UPROPERTY(EditInstanceOnly, Category="Respawn")
	TObjectPtr<APlayerStart> PlayerStartTwo;
	
	UPROPERTY(VisibleAnywhere)
	bool bRoundStarted = false;
	
	UPROPERTY(VisibleAnywhere)
	bool bRoundFinished = false;
	
	UPROPERTY(VisibleAnywhere)
	bool bGameEnded = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Timer")
	float RoundTimeRemaining;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Timer")
	float RoundDuration = 181.0f;
	
	UPROPERTY(EditDefaultsOnly, Category="Player Materials")
	TObjectPtr<UMaterialInterface> PlayerOneMaterial;
	
	UPROPERTY(EditDefaultsOnly, Category="Player Materials")
	TObjectPtr<UMaterialInterface> PlayerTwoMaterial;
	
	UFUNCTION()
	void StartRound();
	
	UFUNCTION()
	void StartNextRound();
	
	UFUNCTION()
	void EndRound();
	
	UFUNCTION()
	void EndGame();
	
	UFUNCTION()
	void RestartLevel();
	
	UFUNCTION()
	void HandlePlayerDeath();
	
	UFUNCTION()
	void ShowRoundIntro();
	
	UFUNCTION()
	void ResetScore();
	
	UFUNCTION()
	void BackToMenu();
};



