// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioManager.generated.h"

class USoundBase;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class COSMICBLASTARENA_API UAudioManager : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAudioManager();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio")
	TObjectPtr<USoundBase> BattleMusic;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SFX")
	TObjectPtr<USoundBase> ShootingSound;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SFX")
	TObjectPtr<USoundBase> DeathSound;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SFX")
	TObjectPtr<USoundBase> PickupSound;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SFX")
	TObjectPtr<USoundBase> GrenadeSound;
	
	UFUNCTION(BlueprintCallable, Category="Audio")
	void PlayBattleMusic();
	
	UFUNCTION(BlueprintCallable, Category="Audio")
	void PlaySFX(USoundBase* sound);
};
