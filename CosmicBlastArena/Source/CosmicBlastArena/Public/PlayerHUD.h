// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
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
	
	UFUNCTION()
	void UpdateHealthUI(float CurrentHealth, float MaxHealth);
	
	UFUNCTION()
	void UpdateFullAmmoUI(float CurrentAmmo, float ReserveAmmo);
	
	UFUNCTION()
	void UpdateCurrentAmmoUI(float CurrentAmmo, float ReserveAmmo);
	
	UFUNCTION()
	void UpdateTimerUI();
	
	UFUNCTION()
	void ShowReloadingUI();
	
	UFUNCTION()
	void HideReloadingUI();
};
