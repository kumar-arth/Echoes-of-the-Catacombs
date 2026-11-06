// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WallDartTrap.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;
class ADartProjectile;

UENUM(BlueprintType)
enum class EDartTrapState : uint8
{
	Armed,
	Triggered,
	Reloading
};

UCLASS()
class ECHOESOFTHECATACOMB_API AWallDartTrap : public AActor
{
	GENERATED_BODY()

public:
	AWallDartTrap();

protected:
	virtual void BeginPlay() override;

public:
	// -------------------------------------------------------------
	// Components
	// -------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	USceneComponent* SceneRoot;

	/** Wall dispenser head mounted on the shooter wall */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	UStaticMeshComponent* DispenserMesh;

	/** Receiver plate mounted on the opposite wall */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	UStaticMeshComponent* ReceiverMesh;

	/** Visual tripwire stretched across the corridor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	UStaticMeshComponent* TripwireMesh;

	/** Tripwire trigger collision box at ankle/knee height */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	UBoxComponent* TripwireBox;

	// -------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	TSubclassOf<ADartProjectile> DartProjectileClass;

	/** Number of darts fired in a volley */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	int32 VolleyCount = 4;

	/** Time delay before reloading the tripwire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float ReloadTime = 3.5f;

	/** Delay between tripping the wire and firing (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float FireDelay = 0.02f;

	/** Height of the tripwire above the floor (allows player to jump or crouch!) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float TripwireHeight = 40.0f;

	/** Corridor width span between dispenser and receiver */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float CorridorSpan = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Audio")
	USoundBase* TriggerSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Audio")
	USoundBase* FireSound;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap|State")
	EDartTrapState TrapState = EDartTrapState::Armed;

	// -------------------------------------------------------------
	// Actions
	// -------------------------------------------------------------

	UFUNCTION()
	void OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                           bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintCallable, Category = "Trap|Action")
	void TriggerTrap();

	UFUNCTION(BlueprintCallable, Category = "Trap|Action")
	void FireDartVolley();

	UFUNCTION(BlueprintCallable, Category = "Trap|Action")
	void FireFollowupVolley();

	UFUNCTION(BlueprintCallable, Category = "Trap|Action")
	void ResetTrap();

	UFUNCTION(BlueprintCallable, Category = "Trap|Action")
	void HighlightTrap(float Duration = 3.0f);

protected:
	FTimerHandle FireTimerHandle;
	FTimerHandle FollowupTimerHandle;
	FTimerHandle ReloadTimerHandle;
	FTimerHandle HighlightTimerHandle;

	UFUNCTION()
	void OnHighlightExpired();
};
