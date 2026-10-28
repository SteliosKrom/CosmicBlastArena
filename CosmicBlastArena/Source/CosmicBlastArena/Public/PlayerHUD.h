// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "PlayerHUD.generated.h"

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
	
	UFUNCTION()
	void UpdateHealthUI(float CurrentHealth, float MaxHealth);
	
	UFUNCTION()
	void UpdateFullAmmoUI(float CurrentAmmo, float ReserveAmmo);
	
	UFUNCTION()
	void UpdateCurrentAmmoUI(float CurrentAmmo, float ReserveAmmo);
};
