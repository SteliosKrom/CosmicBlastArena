// Fill out your copyright notice in the Description page of Project Settings.

#include "PickupBase.h"

#include "CosmicBlastArenaCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

// Sets default values
APickupBase::APickupBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	RootComponent = PickupMesh;
	
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(RootComponent);
	
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &APickupBase::OnPickupOverlap);
	PickupSphere->SetGenerateOverlapEvents(true);
}

// Called when the game starts or when spawned
void APickupBase::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void APickupBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APickupBase::OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		ACosmicBlastArenaCharacter* ArenaCharacter = Cast<ACosmicBlastArenaCharacter>(OtherActor);
		
		if (!IsValid(ArenaCharacter)) return;
		
		if (ArenaCharacter->CurrentHealth >= ArenaCharacter->MaxHealth)
		{
			UE_LOG(LogTemp, Warning, TEXT("You have full health %.2f. You cannot heal!"), ArenaCharacter->CurrentHealth);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Pickup overlapped by: %s"), *OtherActor->GetName());
			ArenaCharacter->CurrentHealth += ArenaCharacter->HealthBoost;
		}
	}
}
