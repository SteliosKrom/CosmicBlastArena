// Copyright Epic Games, Inc. All Rights Reserved.

#include "CosmicBlastArenaGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "CosmicBlastArenaCharacter.h"
#include "GameFramework/Character.h"
#include "CosmicBlastArenaGameInstance.h"

ACosmicBlastArenaGameMode::ACosmicBlastArenaGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACosmicBlastArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	UCosmicBlastArenaGameInstance* MyGameInstance = Cast<UCosmicBlastArenaGameInstance>(GetGameInstance());
	
	if (!IsValid(MyGameInstance)) return;
	
	PlayerOneScore = MyGameInstance->PlayerOneScore;
	PlayerTwoScore = MyGameInstance->PlayerTwoScore;
	CurrentRound = MyGameInstance->CurrentRound;
	
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
	
	PlayerHUD->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
	PlayerHUDTwo->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);

	PlayerHUD->UpdateNextRoundUI(CurrentRound);
	PlayerHUDTwo->UpdateNextRoundUI(CurrentRound);
	
	ShowRoundIntro();
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
	
	if (PlayerOneScore >= 2 || PlayerTwoScore >= 2)
	{
		EndGame();
		return;
	}
	
	PlayerHUD->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
	PlayerHUDTwo->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
	
	GetWorldTimerManager().SetTimer(RoundTransitionTimer, this, 
		&ACosmicBlastArenaGameMode::StartNextRound, 5.0f, false);
}

void ACosmicBlastArenaGameMode::RestartLevel()
{
	UCosmicBlastArenaGameInstance* MyGameInstance = Cast<UCosmicBlastArenaGameInstance>(GetGameInstance());
	
	if (!IsValid(MyGameInstance)) return;
	
	MyGameInstance->PlayerOneScore = PlayerOneScore;
	MyGameInstance->PlayerTwoScore = PlayerTwoScore;
	MyGameInstance->CurrentRound = CurrentRound;
	
	UGameplayStatics::OpenLevel(this, 
		*UGameplayStatics::GetCurrentLevelName(this, true));
}

void ACosmicBlastArenaGameMode::HandlePlayerDeath()
{
	if (bRoundStarted == true && RoundTimeRemaining > 0.0f)
	{
		if (PlayerOneScore >= 2 || PlayerTwoScore >= 2)
		{
			EndGame();
			return;
		}
		
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
			GetWorldTimerManager().SetTimer(RoundTransitionTimer, this, 
				&ACosmicBlastArenaGameMode::StartNextRound, 5.0f, false);
		}
	}
}

void ACosmicBlastArenaGameMode::EndGame()
{
	bGameEnded = true;
	
	PlayerHUD->HideGameplayHUD();
	PlayerHUDTwo->HideGameplayHUD();
	
	if (PlayerOneScore == 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player 1 wins!"));
		PlayerHUD->ShowPlayerOneWinsUI();
		PlayerHUDTwo->ShowPlayerOneWinsUI();
	}
	
	if (PlayerTwoScore == 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player 2 wins!"));
		PlayerHUD->ShowPlayerTwoWinsUI();
		PlayerHUDTwo->ShowPlayerTwoWinsUI();
	}
	
	GetWorldTimerManager().SetTimer(BackToMenuHandle, this, 
		&ACosmicBlastArenaGameMode::BackToMenu, 5.0f, false);
}

void ACosmicBlastArenaGameMode::BackToMenu()
{
	UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

void ACosmicBlastArenaGameMode::ResetScore()
{
	PlayerOneScore = 0;
	PlayerTwoScore = 0;
	
	PlayerHUD->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
	PlayerHUDTwo->UpdateScoreUI(PlayerOneScore, PlayerTwoScore);
}

void ACosmicBlastArenaGameMode::StartNextRound()
{
	UE_LOG(LogTemp, Warning, TEXT("StartNextRound called!"));
	CurrentRound++;
	RestartLevel();
}

void ACosmicBlastArenaGameMode::ShowRoundIntro()
{
	PlayerHUD->ShowRoundAnnouncementUI();
	PlayerHUDTwo->ShowRoundAnnouncementUI();
	GetWorldTimerManager().SetTimer(RoundIntroHandle, this, 
		&ACosmicBlastArenaGameMode::StartRound, 3.0f, false);
}


