// Fill out your copyright notice in the Description page of Project Settings.


#include "ArenaAnimInstance.h"

bool UArenaAnimInstance::GetUseFullBody()
{
	return bUseFullBody;
}

void UArenaAnimInstance::SetUsefulBody(bool bCond)
{
	bUseFullBody = bCond;
}
