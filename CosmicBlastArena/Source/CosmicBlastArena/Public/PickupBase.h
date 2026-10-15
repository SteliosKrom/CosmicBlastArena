// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupBase.generated.h"

class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class COSMICBLASTARENA_API APickupBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APickupBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* PickupMesh;
	
	UPROPERTY(VisibleAnywhere, Category="Components")
	USphereComponent* PickupSphere;
	
	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* overlappedComponent, 
		AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
		bool bFromSweep, const FHitResult& SweepResult); 

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
