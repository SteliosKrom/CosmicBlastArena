// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Damageable.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "CosmicBlastArenaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class USkeletalMeshComponent;

struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ACosmicBlastArenaCharacter : public ACharacter, public IDamageable
{
	GENERATED_BODY()
	
protected:
	
	// FUNCTIONS
	void Move(const FInputActionValue& Value);
	
	void Look(const FInputActionValue& Value);
	
	void Shoot(const FInputActionValue& Value);
	
	void Reload(const FInputActionValue& Value);
	
	// COMPONENTS
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh = nullptr;
	
	// PROPERTIES
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ShootAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ReloadAction;

public:

	/** CONSTRUCTOR */
	ACosmicBlastArenaCharacter();

	// VIRTUAL FUNCTIONS
	
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// PROPERTIES
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 200.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = MaxHealth;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	bool bIsDead = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	int Damage = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	int32 MagazineCapacity = 10;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	int32 CurrentAmmo = MagazineCapacity;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	int32 ReserveAmmo = 30;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	int32 SpendAmmoCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	bool bIsReloading = false;
	
	// FUNCTIONS
	
	virtual void ReceiveDamage_Implementation(float DamageAmount) override;
	
	virtual bool IsAlive_Implementation() override;
	
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);
	
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);
	
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();
	
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();
	
	UFUNCTION(BlueprintCallable, Category="Health")
	void Die();
	
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};