// Copyright Epic Games, Inc. All Rights Reserved.

#include "CosmicBlastArenaCharacter.h"
#include "CosmicBlastArenaGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "CosmicBlastArena.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUD.h"

ACosmicBlastArenaCharacter::ACosmicBlastArenaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ACosmicBlastArenaCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	APlayerController* PC = Cast<APlayerController>(NewController);
	
	if (!IsValid(PC)) return;
		
	PlayerHUD = CreateWidget<UUserWidget>(PC, PlayerHUDClass);
	HUD = Cast<UPlayerHUD>(PlayerHUD);
	
	if (!IsValid(PlayerHUD)) return;
	
	PlayerHUD->AddToPlayerScreen();
}

void ACosmicBlastArenaCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	GameMode = Cast<ACosmicBlastArenaGameMode>(GetWorld()->GetAuthGameMode());
	WeaponMesh = FindComponentByTag<USkeletalMeshComponent>(FName(TEXT("WeaponMesh")));

	if (!IsValid(WeaponMesh) || !WeaponMesh->DoesSocketExist(FName(TEXT("Muzzle"))))
	{
		UE_LOG(LogTemp, Error, TEXT("WeaponMesh or Muzzle not found!"));
		return;
	}
	
	if (IsValid(FollowCamera))
	{
		CurrentFOV = DefaultCameraFOV;
		FollowCamera->SetFieldOfView(CurrentFOV);
	}
}

void ACosmicBlastArenaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	Timer += DeltaTime;
	
	if (Timer >= 1.0f)
	{
		if (IsValid(HUD))
		{
			HUD->UpdateTimerUI();
		}
		Timer = 0.0f;
	}
	
	if (!IsValid(WeaponMesh)) return;
	if (!IsValid(FollowCamera)) return;
	
	const float TargetFOV = bIsAiming ? AimCameraFOV : DefaultCameraFOV;
	CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, FOVInterpSpeed);
	FollowCamera->SetFieldOfView(CurrentFOV);
}

void ACosmicBlastArenaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		//EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		//EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACosmicBlastArenaCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ACosmicBlastArenaCharacter::OnMoveCompleted);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ACosmicBlastArenaCharacter::OnMoveCompleted);
		
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACosmicBlastArenaCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACosmicBlastArenaCharacter::Look);
		
		// Shooting 
		EnhancedInputComponent->BindAction(ShootAction, ETriggerEvent::Started, this, &ACosmicBlastArenaCharacter::StartShooting);
		EnhancedInputComponent->BindAction(ShootAction, ETriggerEvent::Completed, this, &ACosmicBlastArenaCharacter::StopShooting);
		
		// Reloading
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ACosmicBlastArenaCharacter::Reload);
		
		// Aiming 
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ACosmicBlastArenaCharacter::OnAimStarted);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ACosmicBlastArenaCharacter::OnAimReleased);
		
		// Throwing 
		EnhancedInputComponent->BindAction(ThrowGrenadeAction, ETriggerEvent::Started, this, &ACosmicBlastArenaCharacter::OnThrowPrepare);
		EnhancedInputComponent->BindAction(ThrowGrenadeAction, ETriggerEvent::Completed, this, &ACosmicBlastArenaCharacter::OnThrowReleased);
	}
	else
	{
		UE_LOG(LogCosmicBlastArena, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ACosmicBlastArenaCharacter::ReceiveDamage_Implementation(float DamageAmount)
{
	if (bIsDead == true) return;
	
	UE_LOG(LogTemp, Warning, TEXT("Actor received damage!"));
	
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	
	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ACosmicBlastArenaCharacter::Die()
{
	if (!IsValid(DeathMontage))
		return;
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	
	if (!IsValid(AnimInstance))
		return;
	
	AnimInstance->Montage_Play(DeathMontage);
	Dying();
	UE_LOG(LogTemp, Warning, TEXT("%s actor is dead!"), *GetNameSafe(this));
	bIsDead = true;
	GetCharacterMovement()->DisableMovement();
}

bool ACosmicBlastArenaCharacter::IsAlive_Implementation()
{
	return !bIsDead;
}

void ACosmicBlastArenaCharacter::OnMoveCompleted(const FInputActionValue& Value)
{
	bIsMoving = false;
}

void ACosmicBlastArenaCharacter::Move(const FInputActionValue& Value)
{
	if (bIsThrowing == true) return;
	if (bIsDead == true) return;
	if (GameMode->bRoundStarted == false) return;
	
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();
	
	bIsMoving = !MovementVector.IsNearlyZero();
	
	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ACosmicBlastArenaCharacter::Look(const FInputActionValue& Value)
{
	if (bIsDead == true) return;
	if (GameMode->bRoundStarted == false) return;
	
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ACosmicBlastArenaCharacter::StartShooting()
{
	if (GetWorldTimerManager().IsTimerActive(ShootingTimerHandle)) return;
	
	Shoot();
	GetWorldTimerManager().SetTimer(ShootingTimerHandle, this, &ACosmicBlastArenaCharacter::Shoot, FireRate, true);
}

void ACosmicBlastArenaCharacter::StopShooting()
{
	GetWorldTimerManager().ClearTimer(ShootingTimerHandle);
}

void ACosmicBlastArenaCharacter::Shoot()
{
	if (CurrentAmmo <= 0) return;
	if (bIsReloading == true || bIsThrowing == true || bIsMoving == true) return;
	if (bIsDead == true) return;
	if (GameMode->bRoundStarted == false) return;
	
	UE_LOG(LogTemp, Display, TEXT("Shoot pressed!"));
	
	CurrentAmmo = FMath::Clamp(CurrentAmmo - 1, 0, 30);
	SpendAmmoCount++;
	
	OnShoot.Broadcast(CurrentAmmo, ReserveAmmo);
	
	UE_LOG(LogTemp, Display, TEXT("Current ammo: %d\n"), CurrentAmmo);
	UE_LOG(LogTemp, Display, TEXT("Spend ammo: %d\n"), SpendAmmoCount);
	
	const FVector Start = FollowCamera->GetComponentLocation();
	const FVector Direction = FollowCamera->GetForwardVector();
	const FVector End = Start + Direction * 5000.0f;
	
	FHitResult HitResult;
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);
	const FVector TraceEnd = bHit ? HitResult.ImpactPoint : End;
	
	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();
		
		if (IsValid(HitActor) && HitActor->Implements<UDamageable>())
		{
			if (!IDamageable::Execute_IsAlive(HitActor))
				return;
				
			if (HitResult.BoneName == "head")
			{
				UE_LOG(LogTemp, Warning, TEXT("That was a headshot!"));
				Damage = 50;
				UE_LOG(LogTemp, Display, TEXT("Head shot damage: %d"), Damage);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("That was a body shot!"));
				Damage = FMath::RandRange(7, 13);
				UE_LOG(LogTemp, Display, TEXT("Body shot damage: %d"), Damage);
			}
			
			IDamageable::Execute_ReceiveDamage(HitActor, Damage);
			UE_LOG(LogTemp, Warning, TEXT("Hit actor: %s"), *GetNameSafe(HitActor));
			DrawDebugPoint(GetWorld(), TraceEnd, 50.0f, FColor::Red, false, 2.0f);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No hit!"))
		}
	}
}

void ACosmicBlastArenaCharacter::FinishReload()
{
	CurrentAmmo = MagazineCapacity;
	ReserveAmmo = FMath::Clamp(ReserveAmmo -= SpendAmmoCount, 0, 120);
	SpendAmmoCount = 0;
	bIsReloading = false;
	OnReloading.Broadcast(CurrentAmmo, ReserveAmmo);
	HUD->HideReloadingUI();
	UE_LOG(LogTemp, Warning, TEXT("ReserveAmmo: %d\n"), ReserveAmmo);
}

void ACosmicBlastArenaCharacter::Reload(const FInputActionValue& Value)
{
	if (!IsValid(ReloadMontage)) return;
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	
	if (!IsValid(AnimInstance)) return;
	if (bIsReloading == true || bIsThrowing == true) return;
	if (bIsDead == true) return;
	if (GameMode->bRoundStarted == false) return;
	
	if (CurrentAmmo <= 0 && ReserveAmmo > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("Reload pressed!"));
		bIsReloading = true;
		AnimInstance->Montage_Play(ReloadMontage);
		HUD->ShowReloadingUI();
	}
}

void ACosmicBlastArenaCharacter::OnThrowPrepare(const FInputActionValue& Value)
{
	if (bIsReloading == true) return;
	if (bIsDead == true) return;
	if (GameMode->bRoundStarted == false) return;
	
	if (!IsValid(ThrowMontage)) return;
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	
	if (!IsValid(AnimInstance)) return;
	
	bIsGrenadeHolding = false;
	bThrowButtonHeld = true;
	bIsThrowing = true;
	
	AnimInstance->Montage_Play(ThrowMontage);
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Throw started!"));
	}
}

void ACosmicBlastArenaCharacter::OnThrowHold()
{
	if (!IsValid(ThrowMontage)) return;
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	
	if (!IsValid(AnimInstance)) return;
	
	bIsThrowing = true;
	bIsGrenadeHolding = true;
	AnimInstance->Montage_Pause(ThrowMontage);
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Throw is in hold!"));
	}
}

void ACosmicBlastArenaCharacter::OnThrowReleased(const FInputActionValue& Value)
{
	if (!IsValid(ThrowMontage)) return;
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	
	if (!IsValid(AnimInstance)) return;
	
	bThrowButtonHeld = false;
	
	if (bIsGrenadeHolding)
	{
		bIsGrenadeHolding = false;
		AnimInstance->Montage_Resume(ThrowMontage);
		bIsThrowing = false;
	}
	else
	{
		bIsThrowing = false;
		AnimInstance->Montage_Stop(0.2f, ThrowMontage);
	}
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Throw ended!"));
	}
}

void ACosmicBlastArenaCharacter::OnThrowFinished()
{
	bIsGrenadeHolding = false;
	bIsThrowing = false;
	bThrowButtonHeld = false;
}

void ACosmicBlastArenaCharacter::OnAimStarted(const FInputActionValue& Value)
{
	if (bIsDead == true) return;
	if (GameMode->bRoundStarted == false) return;
	
	bIsAiming = true;
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Aim started!"));
	}
}

void ACosmicBlastArenaCharacter::OnAimReleased(const FInputActionValue& Value)
{
	if (bIsDead == true) return;
	if (GameMode->bRoundStarted == false) return;
	
	bIsAiming = false;
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Aim ended!"));
	}
}

void ACosmicBlastArenaCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ACosmicBlastArenaCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ACosmicBlastArenaCharacter::DoJumpStart()
{
	if (bIsThrowing == true) return;
	if (GameMode->bRoundStarted == false) return;
	
	// signal the character to jump
	Jump();
}

void ACosmicBlastArenaCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
