// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "EchoPulseComponent.generated.h"

class ADungeonGenerator;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPulseStateChanged, bool, bIsEnabled);

/** Record for a hazard or enemy waiting to be revealed in distance order as the acoustic wave expands */
USTRUCT()
struct FEchoPendingHazardReveal
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Distance = 0.0f;

	UPROPERTY()
	bool bIsPawn = false;

	UPROPERTY()
	bool bHasRevealed = false;
};

/** Record for acoustic sound wave wall reflections and secondary corner echo traces */
USTRUCT()
struct FEchoWallReflection
{
	GENERATED_BODY()

	UPROPERTY()
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY()
	FVector ImpactNormal = FVector::UpVector;

	UPROPERTY()
	FVector ReflectedDirection = FVector::ForwardVector;

	UPROPERTY()
	float DistanceFromOrigin = 0.0f;

	UPROPERTY()
	bool bHasTriggered = false;
};

/** Active highlighted pawn tracking for automatic depth-stencil and overlay expiration */
USTRUCT()
struct FEchoHighlightedPawn
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<APawn> Pawn;

	UPROPERTY()
	float TimeRemaining = 0.0f;
};

UCLASS(ClassGroup = (Echoes), meta = (BlueprintSpawnableComponent))
class ECHOESOFTHECATACOMB_API UEchoPulseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEchoPulseComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Triggers an expanding acoustic pulse from a specific world location */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void TriggerPulse(FVector Origin, float InMaxRadius = 3500.0f, float InSpeed = 1800.0f, FLinearColor InColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f));

	/** Convenience function: pulses outward from the owning player/companion location */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void TriggerPulseFromOwner();

	/** Triggers targeted tactical scan: highlights traps and non-boss mobs in radius through walls */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void TriggerEcholocationScan();

	/** Stops and resets the pulse */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void StopPulse();

	/** Check if a pulse wave is currently active */
	UFUNCTION(BlueprintPure, Category = "Echo|Pulse")
	bool IsPulseActive() const { return bIsPulseActive; }

	/** Enables or disables the ability to trigger echolocation pulses (e.g. inside boss rooms) */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void SetPulseEnabled(bool bEnabled);

	/** Check if pulse ability is currently permitted */
	UFUNCTION(BlueprintPure, Category = "Echo|Pulse")
	bool IsPulseEnabled() const { return bIsPulseEnabled; }

	/** Returns which boss room (1, 2, 3) the player is currently inside, or 0 if in the maze */
	UFUNCTION(BlueprintPure, Category = "Echo|Pulse")
	int32 GetCurrentBossRoom() const { return CurrentBossRoomIndex; }

public:
	// ========================================================================
	// PARAMETERS
	// ========================================================================

	/** Reference to the MPC_Echo asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Shader")
	UMaterialParameterCollection* EchoMPC;

	/** Wave propagation speed (cm/s). 1800 = 18 meters/second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float DefaultPulseSpeed = 1800.0f;

	/** Maximum distance the wave travels before fading out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float DefaultMaxRadius = 3500.0f;

	/** Thickness of the visual pulse ring band */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float PulseBandwidth = 180.0f;

	/** Default glowing color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	FLinearColor DefaultPulseColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);

	/** Detection radius for scanning traps and mobs (cm). 2500 = 25 meters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float ScanRadius = 2500.0f;

	/** Duration in seconds highlighted objects remain visible through walls */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float HighlightDuration = 3.5f;

	/** Noise loudness to alert AI perception (0.0 to 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Acoustics")
	float NoiseLoudness = 0.85f;

	/** Whether sound waves reflect realistically off walls */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Acoustics")
	bool bEnableWallReflections = true;

	/** Number of radial rays cast to compute wall acoustic bounces (32 = 360 deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Acoustics")
	int32 NumReflectionRays = 32;

	/** Maximum distance secondary reflected echo rays travel around corners (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Acoustics")
	float ReflectedRayMaxDistance = 1000.0f;

	/** Optional sound cue for sonar ping when a hazard is revealed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Audio")
	USoundBase* SonarPingSound;

	/** Optional sound cue for acoustic reflection off catacomb walls */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Audio")
	USoundBase* WallEchoSound;

	// ========================================================================
	// BOSS ROOM INTEGRATION
	// ========================================================================

	/** Reference to the DungeonGenerator in the level (automatically found if null) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Dungeon")
	TWeakObjectPtr<ADungeonGenerator> DungeonGenerator;

	/** If true, automatically disables pulse when stepping into any boss room, and re-enables on exit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Dungeon")
	bool bAutoManageBossRoomPulse = true;

	/** If true, exiting a boss room only re-enables pulse if that room's boss is marked defeated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Dungeon")
	bool bRequireBossDefeatToReenable = false;

	/** If true, prints on-screen debug notifications when entering/exiting boss rooms */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Debug")
	bool bShowDebugMessages = true;

	/** Broadcasts when the pulse ability is enabled or disabled */
	UPROPERTY(BlueprintAssignable, Category = "Echo|Pulse")
	FOnPulseStateChanged OnPulseStateChanged;

private:
	bool bIsPulseActive = false;
	FVector CurrentOrigin = FVector::ZeroVector;
	float CurrentRadius = 0.0f;
	float ActiveMaxRadius = 3500.0f;
	float ActiveSpeed = 1800.0f;
	FLinearColor ActiveColor;

	UPROPERTY(VisibleAnywhere, Category = "Echo|Pulse")
	bool bIsPulseEnabled = true;

	int32 CurrentBossRoomIndex = 0;
	int32 LastVisitedBossRoom = 0;
	float BossRoomCheckTimer = 0.0f;

	UPROPERTY()
	TArray<FEchoPendingHazardReveal> PendingReveals;

	UPROPERTY()
	TArray<FEchoWallReflection> WallReflections;

	UPROPERTY()
	TArray<FEchoHighlightedPawn> ActiveHighlightedPawns;

	void UpdateMaterialParameters();
	void CheckBossRoomStatus();
	void ProcessSequentialReveals(float CurrentWaveRadius);
	void ProcessWallReflections(float CurrentWaveRadius);
	void RevealTarget(FEchoPendingHazardReveal& Item);
	void TriggerWallReflection(FEchoWallReflection& Reflection);
	void UpdateHighlightedPawns(float DeltaTime);
};
