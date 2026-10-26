// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GrenadeHoldNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

UCLASS()
class COSMICBLASTARENA_API UGrenadeHoldNotify : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
