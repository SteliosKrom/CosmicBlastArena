// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CosmicBlastArenaGameMode.generated.h"

class UMaterialInterface;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class ACosmicBlastArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	ACosmicBlastArenaGameMode();
	
protected:
	
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;
	
public:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Timer")
	float RoundTimeRemaining = 180.0f;
	
	UPROPERTY(EditDefaultsOnly, Category="Player Materials")
	TObjectPtr<UMaterialInterface> PlayerOneMaterial;
	
	UPROPERTY(EditDefaultsOnly, Category="Player Materials")
	TObjectPtr<UMaterialInterface> PlayerTwoMaterial;
};



