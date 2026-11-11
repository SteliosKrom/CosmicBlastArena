// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerHUD.h"
#include "CosmicBlastArenaCharacter.h"
#include "CosmicBlastArenaGameMode.h"
#include "Animation/WidgetAnimation.h"

void UPlayerHUD::NativeConstruct()
{
	Super::NativeConstruct();
	
	PlayerCharacter = Cast<ACosmicBlastArenaCharacter>(GetOwningPlayerPawn());
	
	if (!IsValid(PlayerCharacter)) return;
	
	PlayerCharacter->OnHealthChanged.AddDynamic(this, &UPlayerHUD::UpdateHealthUI);
	PlayerCharacter->OnReloading.AddDynamic(this, &UPlayerHUD::UpdateFullAmmoUI);
	PlayerCharacter->OnShoot.AddDynamic(this, &UPlayerHUD::UpdateCurrentAmmoUI);
	PlayerCharacter->OnDoubleDamageChanged.AddDynamic(this, &UPlayerHUD::UpdateDoubleDamageUI);
	PlayerCharacter->OnReserveAmmoChanged.AddDynamic(this, &UPlayerHUD::UpdateReserveAmmoUI);
	PlayerCharacter->OnDamageTaken.AddDynamic(this, &UPlayerHUD::ShowDamageOverlay);
	
	UpdateHealthUI(PlayerCharacter->CurrentHealth, PlayerCharacter->MaxHealth);
	UpdateFullAmmoUI(PlayerCharacter->CurrentAmmo, PlayerCharacter->ReserveAmmo);
	UpdateCurrentAmmoUI(PlayerCharacter->CurrentAmmo, PlayerCharacter->ReserveAmmo);
}

void UPlayerHUD::UpdateHealthUI(float CurrentHealth, float MaxHealth)
{
	if (!IsValid(HealthProgressBar) || !IsValid(HealthTextBlock) || MaxHealth <= 0.0f) return;
	
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

void UPlayerHUD::UpdateGrenadesUI(int32 CurrentGrenade)
{
	if (!IsValid(GrenadeTextBlock)) return;
	GrenadeTextBlock->SetText(FText::AsNumber(PlayerCharacter->CurrentGrenade));
}

void UPlayerHUD::UpdateScoreUI(int32 PlayerOneScore, int32 PlayerTwoScore)
{
	if (!IsValid(PlayerOneScoreTextBlock) || !IsValid(PlayerTwoScoreTextBlock)) return;
	PlayerOneScoreTextBlock->SetText(FText::AsNumber(PlayerOneScore));
	PlayerTwoScoreTextBlock->SetText(FText::AsNumber(PlayerTwoScore));
}

void UPlayerHUD::UpdateNextRoundUI(int32 CurrentRound)
{
	if (!IsValid(RoundIntroTextBlock)) return;
	RoundIntroTextBlock->SetText(FText::FromString(FString::Printf(TEXT("ROUND %d"), CurrentRound)));
}

void UPlayerHUD::UpdateDoubleDamageUI(int32 SecondsRemaining, bool IsActive)
{
	if (!IsValid(DoubleDamageTextBlock)) return;
	
	if (IsActive)
	{
		DoubleDamageTextBlock->SetVisibility(ESlateVisibility::Visible);
		DoubleDamageTextBlock->SetText(FText::Format(FText::FromString(TEXT("DOUBLE DAMAGE: {0}s")), FText::AsNumber(SecondsRemaining)));
	}
	else
	{
		DoubleDamageTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPlayerHUD::UpdateReserveAmmoUI(int32 NewReserveAmmo)
{
	if (!IsValid(AmmoTextBlock)) return;
	
	AmmoTextBlock->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), FText::AsNumber(PlayerCharacter->CurrentAmmo), FText::AsNumber(NewReserveAmmo)));
}

void UPlayerHUD::ShowDamageOverlay()
{
	if (!IsValid(DamageEffect)) return;
	if (!IsValid(DamageAnimation)) return;
	
	DamageEffect->SetVisibility(ESlateVisibility::Visible);
	PlayAnimation(DamageAnimation);
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

void UPlayerHUD::ShowPlayerOneWinsUI()
{
	if (!IsValid(PlayerOneWinsTextBlock)) return;
	PlayerOneWinsTextBlock->SetVisibility(ESlateVisibility::Visible);
}

void UPlayerHUD::HidePlayerOneWinsUI()
{
	if (!IsValid(PlayerOneWinsTextBlock)) return;
	PlayerOneWinsTextBlock->SetVisibility(ESlateVisibility::Hidden);
}

void UPlayerHUD::ShowPlayerTwoWinsUI()
{
	if (!IsValid(PlayerTwoWinsTextBlock)) return;
	PlayerTwoWinsTextBlock->SetVisibility(ESlateVisibility::Visible);
}

void UPlayerHUD::HidePlayerTwoWinsUI()
{
	if (!IsValid(PlayerTwoWinsTextBlock)) return;
	PlayerTwoScoreTextBlock->SetVisibility(ESlateVisibility::Hidden);
}
