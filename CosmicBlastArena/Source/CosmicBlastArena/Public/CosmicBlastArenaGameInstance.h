// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CosmicBlastArenaGameInstance.generated.h"

UCLASS()
class COSMICBLASTARENA_API UCosmicBlastArenaGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(BlueprintReadWrite, Category="Match")
	int32 PlayerOneScore = 0;
	
	UPROPERTY(BlueprintReadWrite, Category="Match")
	int32 PlayerTwoScore = 0;
	
	UPROPERTY(BlueprintReadWrite, Category="Match")
	int32 CurrentRound = 1;
};
