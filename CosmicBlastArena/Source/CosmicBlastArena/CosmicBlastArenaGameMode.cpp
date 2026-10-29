// Copyright Epic Games, Inc. All Rights Reserved.

#include "CosmicBlastArenaGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

ACosmicBlastArenaGameMode::ACosmicBlastArenaGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACosmicBlastArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	
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
	
	RoundTimeRemaining = 180.0f;
}

void ACosmicBlastArenaGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (RoundTimeRemaining >= 0.0f)
		RoundTimeRemaining -=  DeltaTime;
}


