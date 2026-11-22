// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CompanionGuide.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ADungeonGenerator;
class UMaterialInstanceDynamic;
class UCompanionVoiceComponent;

/** State for primary companion guide behavior */
UENUM(BlueprintType)
enum class ECompanionState : uint8
{
	FollowingPlayer    UMETA(DisplayName = "Following Player"),
	GuidingToExit      UMETA(DisplayName = "Guiding To Exit"),
	WaitingForPlayer   UMETA(DisplayName = "Waiting For Player"),
	Idle               UMETA(DisplayName = "Idle")
};

/** Transient action state when executing tactical verbal commands */
UENUM(BlueprintType)
enum class ECompanionActionState : uint8
{
	None,
	Concussing,
	Scouting,
	Distracting
};

// UI / HUD / Audio event delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionSpeechRequested, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCompanionUpgradeRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionActionTriggered, FName, ActionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCompanionColorChanged, FLinearColor, NewColor, float, Duration);

/**
 * A companion guide actor represented by a glowing spherical mesh
 * that hovers, navigates, and responds to verbal/LLM combat & exploration commands.
 */
UCLASS(Blueprintable, ClassGroup = (Echoes))
class ECHOESOFTHECATACOMB_API ACompanionGuide : public AActor
{
	GENERATED_BODY()

public:
	ACompanionGuide();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------------------
	// Components
	// -------------------------------------------------------------------------

	/** Sphere collision component (used as root) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Components")
	TObjectPtr<USphereComponent> SphereCollision;

	/** Spherical static mesh representing the companion body */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Components")
	TObjectPtr<UStaticMeshComponent> SphereMesh;

	/** Point light giving the companion a gentle ethereal glow in dark catacombs */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Components")
	TObjectPtr<UPointLightComponent> CompanionLight;

	/** Voice input capture, noise gate, and Gemini LLM interface */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Components")
	TObjectPtr<UCompanionVoiceComponent> VoiceComponent;

	// -------------------------------------------------------------------------
	// State & Target Configuration
	// -------------------------------------------------------------------------

	/** Current primary state of the companion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|State")
	ECompanionState CompanionState = ECompanionState::FollowingPlayer;

	/** Active transient action (concuss, scout, distract) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companion|State")
	ECompanionActionState ActionState = ECompanionActionState::None;

	/** Actor that this companion follows */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Target")
	TObjectPtr<AActor> FollowTarget;

	/** Offset relative to character orientation (X=Back/Fwd, Y=Right/Left, Z=Up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Movement")
	FVector FollowOffset = FVector(-30.0f, 55.0f, 75.0f);

	/** Interpolation speed when following the character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Movement", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float FollowLagSpeed = 6.0f;

	/** Rotation interpolation speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Movement", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float RotationInterpSpeed = 5.0f;

	/** Amplitude of the idle hovering vertical bob in centimeters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Movement")
	float BobAmplitude = 8.0f;

	/** Frequency of the hovering vertical bob (cycles per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Movement")
	float BobFrequency = 2.5f;

	/** If distance to target exceeds this, snap immediately (e.g. after teleporting) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Movement")
	float TeleportDistanceThreshold = 1500.0f;

	/** Fixed position in world when anchored ('stay') */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companion|Movement")
	FVector AnchorLocation = FVector::ZeroVector;

	// -------------------------------------------------------------------------
	// Guiding Parameters (Used when leading player to exit or boss)
	// -------------------------------------------------------------------------

	/** Current destination world location when guiding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Guide")
	FVector GuideDestination = FVector::ZeroVector;

	/** Movement speed when leading the way */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Guide")
	float GuideMoveSpeed = 450.0f;

	// -------------------------------------------------------------------------
	// Visual Colors
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Colors")
	FLinearColor DefaultLightColor = FLinearColor(0.2f, 0.85f, 1.0f, 1.0f); // Cyan

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Colors")
	FLinearColor TrapWarningColor = FLinearColor(1.0f, 0.6f, 0.1f, 1.0f); // Amber

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Colors")
	FLinearColor DangerAlertColor = FLinearColor(1.0f, 0.15f, 0.15f, 1.0f); // Crimson

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Colors")
	FLinearColor ClearStatusColor = FLinearColor(0.15f, 0.95f, 0.35f, 1.0f); // Emerald

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Colors")
	FLinearColor UpgradeGlowColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f); // Golden

	// -------------------------------------------------------------------------
	// Combat & Action Tuning
	// -------------------------------------------------------------------------

	/** Cooldown for concuss attack in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float ConcussCooldown = 15.0f;

	/** Remaining cooldown for concuss attack */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Companion|Combat")
	float ConcussCooldownRemaining = 0.0f;

	/** Maximum search radius for finding concuss target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float ConcussRange = 1400.0f;

	/** Damage dealt by concuss shockwave */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float ConcussDamage = 1.0f;

	// -------------------------------------------------------------------------
	// Delegates (for UI / Audio / Gameplay response)
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnCompanionSpeechRequested OnSpeechRequested;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnCompanionUpgradeRequested OnUpgradeRequested;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnCompanionActionTriggered OnActionTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Events")
	FOnCompanionColorChanged OnColorChanged;

	// -------------------------------------------------------------------------
	// Step 2A Action Functions (Triggered by Verbal / LLM Commands)
	// -------------------------------------------------------------------------

	/** "concuss him" - Rushes toward hostile target and unleashes a blinding concussive wave */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool ConcussTarget(AActor* Target, FString& OutMessage);

	/** "is there any traps" - Emits an amber sonar sweep highlighting and reporting nearby traps */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool ScanForTraps(float ScanRadius, int32& OutTrapsFound, FString& OutMessage);

	/** "what's the boss health ?" - Reports current active boss health percentage and defeat statuses */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	void QueryBossHealth(FString& OutStatusMessage, float& OutHealthPercent);

	/** "upgrade items" - Enters upgrade catalyst mode and opens the forge interface */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool OpenUpgradeInterface();

	/** "pulse" / "echo" / "scan" - Triggers an echolocation wave from the companion */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool TriggerEcholocationPulse();

	/** "stay" / "wait here" / "to me" - Toggles anchor beacon mode */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	void SetAnchorMode(bool bAnchor);

	/** "run" / "fall back" / "retreat" - Retreats to a safe standoff distance behind player */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool RetreatToSafeDistance(FString& OutMessage);

	/** "scout" / "check ahead" - Scouts forward down the hallway and signals with green/red light */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool ScoutAhead(float Distance = 1400.0f);

	/** "distract" / "bait" / "lure" - Flies to a decoy position and creates noise & strobe light */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool DistractEnemies(FVector DistractLocation = FVector::ZeroVector);

	/** "danger" / "is it safe?" - Checks for hostiles within sensing range */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	void SenseDanger(bool& bThreatsDetected, int32& ThreatCount, FString& OutThreatSummary);

	/** "lead to exit" - Navigates through the catacombs toward the dungeon exit */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool GuideToExit(FString& OutMessage);

	/** "find boss" - Navigates to a specific boss arena entrance (1, 2, or 3) */
	UFUNCTION(BlueprintCallable, Category = "Companion|Actions")
	bool GuideToBoss(int32 BossIndex, FString& OutMessage);

	// -------------------------------------------------------------------------
	// Utility Helpers
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Companion")
	void SetFollowTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "Companion")
	AActor* GetFollowTarget() const { return FollowTarget; }

	UFUNCTION(BlueprintPure, Category = "Companion")
	UCompanionVoiceComponent* GetVoiceComponent() const { return VoiceComponent; }

	UFUNCTION(BlueprintCallable, Category = "Companion")
	void SetCompanionState(ECompanionState NewState);

	UFUNCTION(BlueprintPure, Category = "Companion")
	ECompanionState GetCompanionState() const { return CompanionState; }

	UFUNCTION(BlueprintCallable, Category = "Companion")
	void SetTemporaryLightColor(FLinearColor NewColor, float Duration);

	/** Starts the 3-phase physical acoustic diaphragm animation on the orb */
	UFUNCTION(BlueprintCallable, Category = "Companion|Acoustics")
	void StartSoundWaveAnimation()
	{
		bIsSoundWaveAnimating = true;
		SoundWaveAnimTimer = 0.0f;
		SetTemporaryLightColor(FLinearColor(0.15f, 0.95f, 1.0f), 0.65f);
	}

	UFUNCTION(BlueprintCallable, Category = "Companion")
	void ResetLightToDefault();

	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void StartVoiceListening();

	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void StopVoiceListening();

	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void ToggleVoiceListening();

	UFUNCTION(BlueprintCallable, Category = "Companion|Voice")
	void ProcessCommand(const FString& Command);

	UFUNCTION(BlueprintPure, Category = "Companion")
	AActor* FindClosestHostile(float SearchRadius) const;

	UFUNCTION(BlueprintPure, Category = "Companion")
	ADungeonGenerator* GetDungeonGenerator() const;

private:
	float RunningTime = 0.0f;
	float TemporaryColorTimer = 0.0f;
	float SoundWaveVibrationTimer = 0.0f;

	/** Acoustic sound wave diaphragm 3-phase physical animation */
	UPROPERTY(VisibleAnywhere, Category = "Companion|Acoustics")
	bool bIsSoundWaveAnimating = false;

	float SoundWaveAnimTimer = 0.0f;
	float SoundWaveAnimDuration = 0.65f;

	// Transient action variables
	FVector ActionStartLocation = FVector::ZeroVector;
	FVector ActionTargetLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> ActionTargetActor;
	float ActionTimer = 0.0f;
	float ActionDuration = 0.0f;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	void TickFollowing(float DeltaTime);
	void TickGuiding(float DeltaTime);
	void TickAction(float DeltaTime);
};
