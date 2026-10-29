// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Damageable.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Logging/LogMacros.h"
#include "CosmicBlastArenaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class USkeletalMeshComponent;
class UAnimMontage;
class UUserWidget;
class UPlayerHUD;

struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReloading, float, CurrentAmmo, float, ReserveAmmo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShoot, float, CurrentAmmo, float, ReserveAmmo);

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
	
	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaTime) override;
	
	virtual void PossessedBy(AController* NewController) override;
	
	void Move(const FInputActionValue& Value);
	
	void OnMoveCompleted(const FInputActionValue& Value);
	
	void Look(const FInputActionValue& Value);
	
	void Shoot();
	
	void Reload(const FInputActionValue& Value);
	
	void OnAimStarted(const FInputActionValue& Value);
	
	void OnAimReleased(const FInputActionValue& Value);
	
	void OnThrowPrepare(const FInputActionValue& Value);
	
	void OnThrowReleased(const FInputActionValue& Value);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
	TSubclassOf<UUserWidget> PlayerHUDClass;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> PlayerHUD;
	
	UPROPERTY()
	TObjectPtr<UPlayerHUD> HUD;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> ReloadMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> DeathMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> ThrowMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation")
	TObjectPtr<UAnimMontage> ShootMontage;
	
	UFUNCTION(BlueprintImplementableEvent, Category="Animation")
	void Dying();
	
	//UPROPERTY(EditAnywhere, Category="Input")
	//UInputAction* JumpAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ShootAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* AimAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ReloadAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ThrowGrenadeAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float DefaultCameraFOV = 90.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float AimCameraFOV = 60.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float FOVInterpSpeed = 20.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	bool bIsAiming = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	float FireRate = 0.12f;
	
	float CurrentFOV;
	
	float Timer = 0.0f;
	
	FTimerHandle ShootingTimerHandle;
	
public:

	/** CONSTRUCTOR */
	ACosmicBlastArenaCharacter();
	
	void StartShooting();
	
	void StopShooting();
	
	void FinishReload();
	
	void OnThrowHold();
	
	void OnThrowFinished();
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	virtual void ReceiveDamage_Implementation(float DamageAmount) override;
	
	virtual bool IsAlive_Implementation() override;
	
	UPROPERTY(BlueprintAssignable, Category="Reload")
	FOnReloading OnReloading;
	
	UPROPERTY(BlueprintAssignable, Category="Health")
	FOnHealthChanged OnHealthChanged;
	
	UPROPERTY(BlueprintAssignable, Category="Ammo")
	FOnShoot OnShoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	float MaxHealth = 200.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	float CurrentHealth = MaxHealth;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	float HealthBoost = 50.0f;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
	bool bIsDead = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	int Damage = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	int32 MagazineCapacity = 30;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	int32 CurrentAmmo = MagazineCapacity;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	int32 ReserveAmmo = 120;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	int32 SpendAmmoCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ammo")
	bool bIsReloading = false;
	
 	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grenade")
 	bool bIsThrowing = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grenade")
	bool bThrowButtonHeld = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grenade")
	bool bIsGrenadeHolding = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement")
	bool bIsMoving = false;
	
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