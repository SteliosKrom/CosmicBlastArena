// Fill out your copyright notice in the Description page of Project Settings.

#include "AudioManager.h"
#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
UAudioManager::UAudioManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UAudioManager::BeginPlay()
{
	Super::BeginPlay();
	PlayBattleMusic();
}

// Called every frame
void UAudioManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAudioManager::PlayBattleMusic()
{
	if (!IsValid(BattleMusic)) return;
	UGameplayStatics::SpawnSound2D(this, BattleMusic);
}

void UAudioManager::PlaySFX(USoundBase* Sound)
{
	if (!IsValid(Sound)) return;
	UGameplayStatics::PlaySound2D(this, Sound);
}

