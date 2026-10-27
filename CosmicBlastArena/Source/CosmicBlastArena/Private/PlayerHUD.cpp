// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerHUD.h"
#include "CosmicBlastArenaCharacter.h"

void UPlayerHUD::NativeConstruct()
{
	Super::Construct();
	
	ACosmicBlastArenaCharacter* PlayerCharacter = Cast<ACosmicBlastArenaCharacter>(GetOwningPlayerPawn());
	
	if (!IsValid(PlayerCharacter)) return;
	
	PlayerCharacter->OnHealthChanged.AddDynamic(this, &UPlayerHUD::UpdateHealthUI);
	UpdateHealthUI(PlayerCharacter->CurrentHealth, PlayerCharacter->MaxHealth);
}

void UPlayerHUD::UpdateHealthUI(float CurrentHealth, float MaxHealth)
{
	if (!IsValid(HealthProgressBar) || MaxHealth <= 0.0f) return;
	
	const float HealthPercent = CurrentHealth / MaxHealth;
	HealthProgressBar->SetPercent(HealthPercent);
	HealthTextBlock->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), 
		FText::AsNumber(FMath::RoundToInt(CurrentHealth)), 
		FText::AsNumber(FMath::RoundToInt(MaxHealth))
	)
);
}
