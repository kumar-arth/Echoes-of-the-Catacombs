// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "EchoPulseComponent.generated.h"

class ADungeonGenerator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPulseStateChanged, bool, bIsEnabled);

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

	/** Triggers an expanding pulse from a specific world location */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void TriggerPulse(FVector Origin, float InMaxRadius = 3500.0f, float InSpeed = 1800.0f, FLinearColor InColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f));

	/** Convenience function: pulses outward from the owning player/companion location */
	UFUNCTION(BlueprintCallable, Category = "Echo|Pulse")
	void TriggerPulseFromOwner();

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
	float DefaultPulseSpeed = 850.0f;

	/** Maximum distance the wave travels before fading out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float DefaultMaxRadius = 4000.0f;

	/** Thickness of the visual pulse ring band */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	float PulseBandwidth = 180.0f;

	/** Default glowing color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Parameters")
	FLinearColor DefaultPulseColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);

	/** Noise loudness to alert AI perception (0.0 to 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo|Acoustics")
	float NoiseLoudness = 0.85f;

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

	void UpdateMaterialParameters();
	void CheckBossRoomStatus();
};