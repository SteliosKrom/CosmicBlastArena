// Copyright Epic Games, Inc. All Rights Reserved.

#include "CosmicBlastArenaCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "CosmicBlastArena.h"
#include "DrawDebugHelpers.h"

ACosmicBlastArenaCharacter::ACosmicBlastArenaCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
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

void ACosmicBlastArenaCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	WeaponMesh = FindComponentByTag<USkeletalMeshComponent>(FName(TEXT("WeaponMesh")));

	if (!IsValid(WeaponMesh) || !WeaponMesh->DoesSocketExist(FName(TEXT("Muzzle"))))
	{
		UE_LOG(LogTemp, Error, TEXT("WeaponMesh or Muzzle not found!"));
		return;
	}
}

void ACosmicBlastArenaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!IsValid(WeaponMesh)) 
		return;
	
	const FName MuzzleSocket = TEXT("Muzzle");
	const FVector Start = WeaponMesh->GetSocketLocation(MuzzleSocket);
	const FVector Direction = WeaponMesh->GetSocketRotation(MuzzleSocket).Vector();
	const FVector End = Start + Direction * 5000.0f;
	
	FHitResult HitResult;
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);
	const FVector TraceEnd = bHit ? HitResult.ImpactPoint : End;
	
	DrawDebugLine(GetWorld(), Start, TraceEnd, bHit ? FColor::Green : FColor::Red, false, 0.0f, 0, 1.0f);
}

void ACosmicBlastArenaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACosmicBlastArenaCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACosmicBlastArenaCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACosmicBlastArenaCharacter::Look);
		
		// Shooting 
		EnhancedInputComponent->BindAction(ShootAction, ETriggerEvent::Started, this, &ACosmicBlastArenaCharacter::Shoot);
		
		// Reloading
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ACosmicBlastArenaCharacter::Reload);
	}
	else
	{
		UE_LOG(LogCosmicBlastArena, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ACosmicBlastArenaCharacter::ReceiveDamage_Implementation(float DamageAmount)
{
	if (bIsDead || DamageAmount <= 0.0f) return;
	
	UE_LOG(LogTemp, Warning, TEXT("Actor received damage!"));
	
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	
	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ACosmicBlastArenaCharacter::Die()
{
	if (bIsDead) return;
	
	bIsDead = true;
	GetCharacterMovement()->DisableMovement();
	UE_LOG(LogTemp, Warning, TEXT("%s actor is dead!"), *GetNameSafe(this));
}

void ACosmicBlastArenaCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ACosmicBlastArenaCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ACosmicBlastArenaCharacter::Shoot(const FInputActionValue& Value)
{
	if (CurrentAmmo <= 0)
		return;
	
	UE_LOG(LogTemp, Display, TEXT("Shoot pressed!"));
	
	CurrentAmmo = FMath::Clamp(CurrentAmmo - 1, 0, 10);
	SpendAmmoCount++;
	
	UE_LOG(LogTemp, Display, TEXT("Current ammo: %d\n"), CurrentAmmo);
	UE_LOG(LogTemp, Display, TEXT("Spend ammo: %d\n"), SpendAmmoCount);
	
	const FName MuzzleSocket = TEXT("Muzzle");
	const FVector Start = WeaponMesh->GetSocketLocation(MuzzleSocket);
	const FVector Direction = WeaponMesh->GetSocketRotation(MuzzleSocket).Vector();
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
			if (bIsDead == true)
				return;
			
			if (HitResult.BoneName == "head")
			{
				UE_LOG(LogTemp, Warning, TEXT("That was a headshot!"));
				Damage = 50;
				UE_LOG(LogTemp, Display, TEXT("Head shot damage: %d"), Damage);
			}
			else
			{
				Damage = FMath::RandRange(7, 13);
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

void ACosmicBlastArenaCharacter::Reload(const FInputActionValue& Value)
{
	if (CurrentAmmo <= 0 && ReserveAmmo > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("Reload pressed!"));
		
		CurrentAmmo = MagazineCapacity;
		bIsReloading = true;
		ReserveAmmo = FMath::Clamp(ReserveAmmo -= SpendAmmoCount, 0, 30);
		SpendAmmoCount = 0;
		
		UE_LOG(LogTemp, Warning, TEXT("ReserveAmmo: %d\n"), ReserveAmmo);
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
	// signal the character to jump
	Jump();
}

void ACosmicBlastArenaCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
