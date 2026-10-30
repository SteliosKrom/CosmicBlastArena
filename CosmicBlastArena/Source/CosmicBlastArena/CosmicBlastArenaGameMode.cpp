// Copyright Epic Games, Inc. All Rights Reserved.

#include "CosmicBlastArenaGameMode.h"
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
	
	APlayerController* PlayerOneController = UGameplayStatics::GetPlayerController(this, 0);
	APlayerController* PlayerTwoController = UGameplayStatics::GetPlayerController(this, 1);
	
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
	
	RoundTimeRemaining = 181.0f;
	
	if (!IsValid(PlayerOneCharacter) || !IsValid(PlayerTwoCharacter)) return;
	
	ACosmicBlastArenaCharacter* CharacterOne = Cast<ACosmicBlastArenaCharacter>(PlayerOneController->GetPawn());
	ACosmicBlastArenaCharacter* CharacterTwo = Cast<ACosmicBlastArenaCharacter>(PlayerTwoController->GetPawn());
	
	if (!IsValid(CharacterOne) || !IsValid(CharacterTwo)) return;
	
	PlayerHUD = CharacterOne-> GetPlayerHUD();
	PlayerHUDTwo = CharacterTwo->GetPlayerHUD();
	
	if (!IsValid(PlayerHUD) || !IsValid(PlayerHUDTwo)) return;
	
	GetWorldTimerManager().SetTimer(InitialDelayHandle, this, &ACosmicBlastArenaGameMode::ShowRoundIntro, 3.0f, false);
}

void ACosmicBlastArenaGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bRoundStarted == true && RoundTimeRemaining >= 0.0f)
		RoundTimeRemaining -=  DeltaTime;
}

void ACosmicBlastArenaGameMode::StartRound()
{
	PlayerHUD->HideRoundAnnouncementUI();
	PlayerHUDTwo->HideRoundAnnouncementUI();
	
	PlayerHUD->ShowGameplayHUD();
	PlayerHUDTwo->ShowGameplayHUD();
	
	bRoundStarted = true;
}

void ACosmicBlastArenaGameMode::ShowRoundIntro()
{
	PlayerHUD->ShowRoundAnnouncementUI();
	PlayerHUDTwo->ShowRoundAnnouncementUI();

	GetWorldTimerManager().SetTimer(RoundIntroHandle, this, &ACosmicBlastArenaGameMode::StartRound, 3.0f, false);
}


