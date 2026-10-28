// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerHUD.h"
#include "CosmicBlastArenaCharacter.h"

void UPlayerHUD::NativeConstruct()
{
	Super::Construct();
	
	ACosmicBlastArenaCharacter* PlayerCharacter = Cast<ACosmicBlastArenaCharacter>(GetOwningPlayerPawn());
	
	if (!IsValid(PlayerCharacter)) return;
	
	PlayerCharacter->OnHealthChanged.AddDynamic(this, &UPlayerHUD::UpdateHealthUI);
	PlayerCharacter->OnReloading.AddDynamic(this, &UPlayerHUD::UpdateFullAmmoUI);
	PlayerCharacter->OnShoot.AddDynamic(this, &UPlayerHUD::UpdateCurrentAmmoUI);
	
	UpdateHealthUI(PlayerCharacter->CurrentHealth, PlayerCharacter->MaxHealth);
	UpdateFullAmmoUI(PlayerCharacter->CurrentAmmo, PlayerCharacter->ReserveAmmo);
	UpdateCurrentAmmoUI(PlayerCharacter->CurrentAmmo, PlayerCharacter->ReserveAmmo);
}

void UPlayerHUD::UpdateHealthUI(float CurrentHealth, float MaxHealth) 
{
	if (!IsValid(HealthProgressBar) || MaxHealth <= 0.0f) return;
	
	const float HealthPercent = CurrentHealth / MaxHealth;
	HealthProgressBar->SetPercent(HealthPercent);
	HealthTextBlock->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), 
		FText::AsNumber(FMath::RoundToInt(CurrentHealth)), FText::AsNumber(FMath::RoundToInt(MaxHealth))));
}

void UPlayerHUD::UpdateFullAmmoUI(float CurrentAmmo, float ReserveAmmo)
{
	if (!IsValid(AmmoTextBlock)) return;
	
	AmmoTextBlock->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), 
		FText::AsNumber(FMath::RoundToInt(CurrentAmmo)), FText::AsNumber(FMath::RoundToInt(ReserveAmmo))));
}

void UPlayerHUD::UpdateCurrentAmmoUI(float CurrentAmmo, float ReserveAmmo)
{
	if (!IsValid(AmmoTextBlock)) return;
	
	AmmoTextBlock->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), 
		FText::AsNumber(FMath::RoundToInt(CurrentAmmo)), FText::AsNumber(ReserveAmmo)));
}
