// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ArenaAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class COSMICBLASTARENA_API UArenaAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
protected:
	
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Animation|Montage")
	bool bUseFullBody = false;
	
public:
	
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	bool GetUseFullBody();
	
	void SetUsefulBody(bool bCond);
};
