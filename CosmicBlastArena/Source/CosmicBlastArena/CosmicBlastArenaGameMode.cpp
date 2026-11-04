// Copyright Epic Games, Inc. All Rights Reserved.

#include "CosmicBlastArenaGameMode.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "CosmicBlastArenaCharacter.h"
#include "GameFramework/Character.h"

ACosmicBlastArenaGameMode::ACosmicBlastArenaGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACosmicBlastArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	bRoundStarted = false;
	
	UGameplayStatics::CreatePlayer(this, 1, true);
	
	PlayerOneController = UGameplayStatics::GetPlayerController(this, 0);
	PlayerTwoController = UGameplayStatics::GetPlayerController(this, 1);
	
	ACharacter* PlayerOneCharacter = Cast<ACharacter>(PlayerOneController->GetPawn());
	ACharacter* PlayerTwoCharacter = Cast<ACharacter>(PlayerTwoController->GetPawn());
	
	if (PlayerOneCharacter)
	{
		PlayerOneCharacter->GetMesh()->SetMaterial(0, PlayerOneMaterial);
	}
	
	if (PlayerTwoCharacter)
	{
		PlayerTwoCharacter->GetMesh()->SetMaterial(1, PlayerTwoMaterial);
	}
	
	RoundTimeRemaining = RoundDuration;
	
	if (!IsValid(PlayerOneCharacter) || !IsValid(PlayerTwoCharacter)) return;
	
	CharacterOne = Cast<ACosmicBlastArenaCharacter>(PlayerOneController->GetPawn());
	CharacterTwo = Cast<ACosmicBlastArenaCharacter>(PlayerTwoController->GetPawn());
	
	if (!IsValid(CharacterOne) || !IsValid(CharacterTwo)) return;
	
	PlayerHUD = CharacterOne-> GetPlayerHUD();
	PlayerHUDTwo = CharacterTwo->GetPlayerHUD();
	
	if (!IsValid(PlayerHUD) || !IsValid(PlayerHUDTwo)) return;
	
	GetWorldTimerManager().SetTimer(InitialDelayHandle, this, &ACosmicBlastArenaGameMode::ShowRoundIntro, 3.0f, false);
}

void ACosmicBlastArenaGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bRoundStarted == true && RoundTimeRemaining > 0.0f)
	{
		RoundTimeRemaining -=  DeltaTime;
		
		if (RoundTimeRemaining <= 0.0f)
		{
			EndRound();
		}
	}
}

void ACosmicBlastArenaGameMode::StartRound()
{
	bRoundStarted = true;
	bRoundFinished = false;
	
	RoundTimeRemaining = RoundDuration;
	
	PlayerHUD->HideRoundAnnouncementUI();
	PlayerHUDTwo->HideRoundAnnouncementUI();
	
	PlayerHUD->ShowGameplayHUD();
	PlayerHUDTwo->ShowGameplayHUD();
	
	bRoundStarted = true;
}

void ACosmicBlastArenaGameMode::EndRound()
{
	bRoundStarted = false;
	bRoundFinished = true;
	
	RoundTimeRemaining = RoundDuration;
	
	if (CharacterOne->CurrentHealth < CharacterTwo->CurrentHealth)
	{
		PlayerTwoScore++;
	}
	else if (CharacterOne->CurrentHealth > CharacterTwo->CurrentHealth)
	{
		PlayerOneScore++;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Draw!"));
	}
	
	PlayerHUD->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
	PlayerHUDTwo->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
	GetWorldTimerManager().SetTimer(RoundTransitionTimer, this, &ACosmicBlastArenaGameMode::StartNextRound, 5.0f, false);
}

void ACosmicBlastArenaGameMode::HandlePlayerDeath()
{
	if (bRoundStarted == true && RoundTimeRemaining > 0.0f)
	{
		if (CharacterOne->CurrentHealth <= 0.0f)
		{
			PlayerOneScore++;
			PlayerHUD->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
			PlayerHUDTwo->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
			bRoundFinished = true;
		}
		else if (CharacterTwo->CurrentHealth <= 0.0f)
		{
			PlayerTwoScore++;
			PlayerHUD->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
			PlayerHUDTwo->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
			bRoundFinished = true;
		}
		
		if (bRoundFinished == true)
		{
			GetWorldTimerManager().SetTimer(RoundTransitionTimer, this, &ACosmicBlastArenaGameMode::StartNextRound, 5.0f, false);
		}
	}
}

void ACosmicBlastArenaGameMode::StartNextRound()
{
	RespawnPlayers();
	CurrentRound++;
	
	PlayerHUD->UpdateNextRoundUI(CurrentRound);
	PlayerHUDTwo->UpdateNextRoundUI(CurrentRound);
	
	PlayerHUD->HideGameplayHUD();
	PlayerHUDTwo->HideGameplayHUD();
	
	ShowRoundIntro();
}

void ACosmicBlastArenaGameMode::RespawnPlayers()
{
	RestartPlayerAtPlayerStart(PlayerOneController, PlayerStartOne);
	RestartPlayerAtPlayerStart(PlayerTwoController, PlayerStartTwo);
}

void ACosmicBlastArenaGameMode::ShowRoundIntro()
{
	PlayerHUD->ShowRoundAnnouncementUI();
	PlayerHUDTwo->ShowRoundAnnouncementUI();
	GetWorldTimerManager().SetTimer(RoundIntroHandle, this, &ACosmicBlastArenaGameMode::StartRound, 3.0f, false);
}


