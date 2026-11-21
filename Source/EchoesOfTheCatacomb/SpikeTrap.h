// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpikeTrap.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

UENUM(BlueprintType)
enum class ESpikeTrapState : uint8
{
	Dormant,
	Warning,
	Springing,
	Sprung,
	Retracting
};

UCLASS()
class ECHOESOFTHECATACOMB_API ASpikeTrap : public AActor
{
	GENERATED_BODY()

public:
	ASpikeTrap();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// Components
	// -------------------------------------------------------------

	/** Root scene component centered at the grid cell */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	USceneComponent* SceneRoot;

	/** Small stone pressure plate embedded flush in the floor (100x100 cm) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	UStaticMeshComponent* BasePlateMesh;

	/** Movable container that translates up/down with the spikes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	USceneComponent* SpikesContainer;

	/** Compact instanced mesh for the sharp metal spikes (3x3 grid) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	UInstancedStaticMeshComponent* SpikesISM;

	/** Overlap trigger volume that detects pawns stepping onto the pressure plate */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	UBoxComponent* TriggerBox;

	/** Damage collision box active while spikes are extended */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap|Components")
	UBoxComponent* DamageBox;

	// -------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------

	/** If true, spikes cycle up/down rhythmically. Default false = Pure Pressure Plate! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	bool bIsPeriodic = false;

	/** Warning delay between stepping on the plate and spikes impaling (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float WarningDelay = 0.25f;

	/** Duration spikes stay extended before retracting (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float SprungDuration = 1.0f;

	/** Time interval between cycles when in periodic mode (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float PeriodicCycleInterval = 3.5f;

	/** Base damage applied to overlapping pawns upon springing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float DamageAmount = 8.0f;

	/** Upward impulse launch applied to the character on impale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float LaunchImpulseZ = 240.0f;

	/** Hidden Z position of the spikes beneath the floor plate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float SpikeRetractedZ = -30.0f;

	/** Fully extended Z position of the sharp spikes (knee/waist height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float SpikeExtendedZ = 45.0f;

	/** Depth the floor plate sinks into the ground when stepped on (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float PlateSinkDepth = 3.0f;

	/** Speed at which the floor plate sinks (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float PlateSinkSpeed = 60.0f;

	/** Speed at which the floor plate rises back to flush (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float PlateRiseSpeed = 15.0f;

	/** Speed of the rapid spring action (units per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float SpringSpeed = 2200.0f;

	/** Speed of the slow reset retraction (units per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap|Config")
	float RetractSpeed = 120.0f;

	/** Current operational state of the trap */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Trap|State")
	ESpikeTrapState TrapState = ESpikeTrapState::Dormant;

	// -------------------------------------------------------------
	// Actions & Handlers
	// -------------------------------------------------------------

	UFUNCTION()
	void OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                           bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDamageOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                          bool bFromSweep, const FHitResult& SweepResult);

	/** Triggers the trap warning sequence */
	UFUNCTION(BlueprintCallable, Category = "Trap|Action")
	void TriggerTrap();

	/** Immediately springs the spikes up */
	UFUNCTION(BlueprintCallable, Category = "Trap|Action")
	void SpringSpikes();

	/** Retracts spikes and resets the trap to dormant */
	UFUNCTION(BlueprintCallable, Category = "Trap|Action")
	void ResetTrap();

	/** Highlights trap outline (e.g., when scanned by echolocation wave) */
	UFUNCTION(BlueprintCallable, Category = "Trap|Echolocation")
	void HighlightTrap(float Duration = 3.5f);

private:
	float StateTimer = 0.0f;
	float PeriodicTimer = 0.0f;
	float CurrentSpikeZ = -30.0f;
	float CurrentPlateZ = 0.2f;
	FTimerHandle HighlightTimerHandle;

	UPROPERTY()
	TSet<AActor*> DamagedActorsThisCycle;

	void ApplyDamageToActor(AActor* TargetActor);
	void OnHighlightExpired();
};
