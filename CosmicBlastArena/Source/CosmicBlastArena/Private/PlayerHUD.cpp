// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerHUD.h"
#include "CosmicBlastArenaCharacter.h"
#include "CosmicBlastArenaGameMode.h"

void UPlayerHUD::NativeConstruct()
{
	Super::NativeConstruct();
	
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

void UPlayerHUD::UpdateTimerUI()
{
	if (!IsValid(TimerTextBlock)) return;
	
	ACosmicBlastArenaGameMode* GameMode = Cast<ACosmicBlastArenaGameMode>(GetWorld()->GetAuthGameMode());
	
	if (!IsValid(GameMode)) return;
	
	int32 Minutes = FMath::FloorToInt(GameMode->RoundTimeRemaining / 60.0f);
	int32 Seconds = FMath::FloorToInt(GameMode->RoundTimeRemaining) % 60;
	
	FString TimerString = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
	TimerTextBlock->SetText(FText::FromString(TimerString));
	
	UE_LOG(LogTemp, Warning, TEXT("RoundTimeRemaining: %02d:%02d"), Minutes, Seconds);
}

void UPlayerHUD::ShowReloadingUI() 
{
	if (!IsValid(ReloadingTextBlock)) return;
	ReloadingTextBlock->SetVisibility(ESlateVisibility::Visible);
}

void UPlayerHUD::HideReloadingUI()
{
	if (!IsValid(ReloadingTextBlock)) return;
	ReloadingTextBlock->SetVisibility(ESlateVisibility::Hidden);
}

void UPlayerHUD::ShowGameplayHUD()
{
	if (!IsValid(GameplayHUD)) return;
	GameplayHUD->SetVisibility(ESlateVisibility::Visible);
}

void UPlayerHUD::HideGameplayHUD()
{
	if (!IsValid(GameplayHUD)) return;
	GameplayHUD->SetVisibility(ESlateVisibility::Hidden);
}

void UPlayerHUD::ShowRoundAnnouncementUI()
{
	if (!IsValid(RoundIntroTextBlock)) return;
	RoundIntroTextBlock->SetVisibility(ESlateVisibility::Visible);
}

void UPlayerHUD::HideRoundAnnouncementUI()
{
	if (!IsValid(RoundIntroTextBlock)) return;
	RoundIntroTextBlock->SetVisibility(ESlateVisibility::Hidden);
}
