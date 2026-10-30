// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "CosmicBlastArenaCharacter.h"
#include "PlayerHUD.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class COSMICBLASTARENA_API UPlayerHUD : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	
	virtual void NativeConstruct() override;
	
public:
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> HealthTextBlock;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> AmmoTextBlock;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TimerTextBlock;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> ReloadingTextBlock;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCanvasPanel> GameplayHUD;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RoundIntroTextBlock;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> GrenadeTextBlock;
	
	UPROPERTY()
	TObjectPtr<ACosmicBlastArenaCharacter> PlayerCharacter;
	
	UFUNCTION()
	void UpdateHealthUI(float CurrentHealth, float MaxHealth);
	
	UFUNCTION()
	void UpdateFullAmmoUI(float CurrentAmmo, float ReserveAmmo);
	
	UFUNCTION()
	void UpdateCurrentAmmoUI(float CurrentAmmo, float ReserveAmmo);
	
	UFUNCTION()
	void UpdateGrenadesUI(int32 CurrentGrenade);
	
	UFUNCTION()
	void UpdateTimerUI();
	
	UFUNCTION()
	void ShowReloadingUI();
	
	UFUNCTION()
	void HideReloadingUI();
	
	UFUNCTION()
	void ShowGameplayHUD();
	
	UFUNCTION()
	void HideGameplayHUD();
	
	UFUNCTION()
	void ShowRoundAnnouncementUI();
	
	UFUNCTION()
	void HideRoundAnnouncementUI();
};
