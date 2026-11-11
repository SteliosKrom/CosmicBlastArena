// Fill out your copyright notice in the Description page of Project Settings.


#include "MyActor.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AMyActor::AMyActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMyActor::BeginPlay()
{
	Super::BeginPlay();
	
	PlayerTwoController = UGameplayStatics::GetPlayerController(this, 1);
	
	if (IsValid(PlayerTwoController))
	{
		UGameplayStatics::RemovePlayer(PlayerTwoController, true);
	}
}

// Called every frame
void AMyActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

