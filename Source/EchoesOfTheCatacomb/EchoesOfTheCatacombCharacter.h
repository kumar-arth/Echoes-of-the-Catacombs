// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"
#include "EchoesOfTheCatacombCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class USoundBase;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  Player Combat & Locomotion State Machine (Dark Souls III Inspired)
 */
UENUM(BlueprintType)
enum class EPlayerCombatState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Walking     UMETA(DisplayName = "Walking"),
	Running     UMETA(DisplayName = "Running"),
	Sprinting   UMETA(DisplayName = "Sprinting"),
	Jumping     UMETA(DisplayName = "Jumping"),
	Falling     UMETA(DisplayName = "Falling"),
	Landing     UMETA(DisplayName = "Landing"),
	Dodging     UMETA(DisplayName = "Dodging"),
	Attacking   UMETA(DisplayName = "Attacking"),
	Blocking    UMETA(DisplayName = "Blocking"),
	HitReaction UMETA(DisplayName = "Hit Reaction"),
	Staggered   UMETA(DisplayName = "Staggered"),
	GuardBroken UMETA(DisplayName = "Guard Broken"),
	Dead        UMETA(DisplayName = "Dead")
};

/**
 *  A controllable third person character with combat capabilities:
 *  - Melee attack (Left Click)
 *  - Dodge roll with i-frames (Left Shift / Alt)
 *  - Health & damage handling
 */
UCLASS(abstract)
class AEchoesOfTheCatacombCharacter : public ACharacter, public ICombatDamageable
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	/** Constructor */
	AEchoesOfTheCatacombCharacter();

	// -------------------------------------------------------------
	// Combat & Health
	// -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Stats")
	float CurrentHealth = 100.0f;

	// -------------------------------------------------------------
	// Stamina & Souls Mechanics
	// -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Stamina")
	float MaxStamina = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Stamina")
	float CurrentStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Stamina")
	float StaminaRegenRate = 45.0f;

	/** When false, character acts as stock Unreal third person character without combat abilities */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bEnableCombatAbilities = true;

	/** Toggle between Advanced Melee Combat System (Protector combo chains, buffered queuing, sweep traces) and Legacy fallback system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|System")
	bool bUseAdvancedCombatSystem = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|State")
	bool bIsInvulnerable = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|State")
	bool bIsSprinting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Stats")
	float AttackDamage = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Stats")
	float AttackRange = 240.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|State")
	bool bIsAttacking = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|State")
	bool bIsDodging = false;

	/** Shield Blocking State (Right Click Hold) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|State")
	bool bIsBlocking = false;

	/** Primary Player State Machine (Dark Souls III action commitments) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|State")
	EPlayerCombatState CombatState = EPlayerCombatState::Idle;

	UFUNCTION(BlueprintPure, Category = "Combat|State")
	EPlayerCombatState GetCombatState() const { return CombatState; }

	UFUNCTION(BlueprintCallable, Category = "Combat|State")
	bool CanEnterState(EPlayerCombatState NewState) const;

	UFUNCTION(BlueprintCallable, Category = "Combat|State")
	void SetCombatState(EPlayerCombatState NewState);

	/** Knight's Steel Longsword attached to right hand */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Weapons")
	UStaticMeshComponent* SwordMesh;

	/** Knight's Heraldic Heater Shield attached to left arm */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Weapons")
	UStaticMeshComponent* ShieldMesh;

	/** Chinese Ancient Guard modular body mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh|Guard")
	USkeletalMeshComponent* GuardBodyMesh;

	/** Chinese Ancient Guard modular head mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh|Guard")
	USkeletalMeshComponent* GuardHeadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Audio")
	USoundBase* AttackSound;

	// Combat Animations
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* Attack1Anim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* Attack2Anim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* Attack3Anim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* HeavyAttackAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* RunningAttackAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* DodgeAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* JumpAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* FallLoopAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* LandAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* HitReactAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* DeathAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Anims")
	class UAnimSequence* BlockAnim = nullptr;

	// -------------------------------------------------------------
	// Advanced Melee Combat System (Protector Sword & Shield Suite)
	// -------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorAttack1Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorAttack2Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorAttack3Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorAttack4Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorHeavy1Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorHeavy2Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorHeavy3Montage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorSprintAttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorAirAttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorDodgeMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorBlockHitMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorGuardBreakMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorHitFMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorHitBMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorHitLMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorHitRMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Montages")
	class UAnimMontage* ProtectorDieMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Anims")
	class UAnimSequence* ProtectorIdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Advanced|Anims")
	class UAnimSequence* ProtectorCommonIdleAnim = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void PerformAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void PerformRunningAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void PerformHeavyAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void PerformDodge();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void StartBlock();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void StopBlock();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractPressed();

	/** Triggers echolocation pulse to detect nearby traps and mobs through walls */
	UFUNCTION(BlueprintCallable, Category = "Echo|Action")
	void OnEcholocationPressed();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void ResetAttackState();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void ResetDodgeState();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void ResetCombo();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void ExecuteAttackHit();

	UFUNCTION(BlueprintCallable, Category = "Combat|Action")
	void OnGuardBreak();

	void ResetGuardBreak();
	void ResetHitReaction();
	void ResetLandingState();
	void UpdateLocomotionState();

	// Advanced Combat System Helpers
	void EnableComboQueueWindow();
	void TryChainQueuedAttack();
	void ExecuteAdvancedAttackHit();
	void PlayDirectionalHitReaction(const FVector& HitDirection);
	void UpdateCombatGripIdle();

	// ICombatDamageable interface
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override;
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override {}

	// Standard AActor damage override
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	                         AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	void ReloadCurrentLevel();

	FTimerHandle RespawnTimerHandle;	
	TSharedPtr<class SDungeonHUDWidget> DungeonHUDWidget;	

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Sprint & Shift Hold vs Click Inputs */
	void StartSprint();
	void StopSprint();
	void OnShiftPressed();
	void OnShiftReleased();
	void OnShiftHoldStart();

	virtual void Landed(const FHitResult& Hit) override;

	/** Stamina & Hitstop Methods */
	bool CanPerformAction(float StaminaCost) const;
	void ConsumeStamina(float Amount);
	void EnableStaminaRegen();
	void TriggerHitstop(AActor* HitTarget, float Duration = 0.06f);
	void ResetHitstop();

public:
	/** Motion Warping Component for Souls-style attack step-in and target tracking */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Warping")
	class UMotionWarpingComponent* MotionWarpingComponent;

	/** Lock On */
	UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
	void ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category = "Combat|Targeting")
	void ClearLockOn();

	void UpdateLockOn(float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	bool IsLockingOn() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Targeting")
	AActor* GetLockedTarget() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Warping")
	FVector GetTranslationWarpTarget() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Warping")
	FRotator GetRotationWarpTarget() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Warping")
	float WarpTargetDistance = 95.0f;

protected:

	FTimerHandle AttackCooldownHandle;
	FTimerHandle DodgeDurationHandle;
	FTimerHandle ComboResetHandle;
	FTimerHandle AttackHitHandle;
	FTimerHandle HitstopTimerHandle;
	FTimerHandle StaminaRegenDelayHandle;
	FTimerHandle ShiftHoldTimerHandle;
	FTimerHandle GuardBreakTimerHandle;
	FTimerHandle HitReactionTimerHandle;
	FTimerHandle LandingTimerHandle;
	FTimerHandle IFrameTimerHandle;
	FTimerHandle ComboQueueWindowHandle;
	FTimerHandle AdvancedAttackTimerHandle;
	FTimerHandle IdleMontageTimerHandle;

	TWeakObjectPtr<AActor> LastHitActor;
	bool bCanRegenStamina = true;
	bool bShiftIsHeld = false;
	float ShiftPressTime = 0.0f;

	int32 CurrentComboIndex = 0;
	int32 HeavyComboIndex = 0;
	bool bAttackQueued = false;
	bool bCanQueueAttack = false;
	bool bIsHeavyAttackActive = false;
	bool bIsGripIdleActive = false;
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;

	float PendingDamage = 35.0f;
	float PendingKnockback = 220.0f;

public:
	// Weapon attachment sockets and bone offsets (Configurable in Editor Details)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FName SwordSocketName = FName(TEXT("HandGrip_R"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FName ShieldSocketName = FName(TEXT("ShieldSocket"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FVector SwordHandLocation = FVector(0.0f, 0.0f, 3.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FRotator SwordHandRotation = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FVector ShieldForearmLocation = FVector(0.0f, -10.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FRotator ShieldForearmRotation = FRotator(0.0f, 0.0f, 90.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FVector ShieldRestLocation = FVector(0.0f, -10.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FRotator ShieldRestRotation = FRotator(0.0f, 0.0f, 90.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FVector ShieldGuardLocation = FVector(0.0f, -10.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Weapons")
	FRotator ShieldGuardRotation = FRotator(0.0f, 0.0f, 90.0f);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns CompanionInstance subobject **/
	UFUNCTION(BlueprintPure, Category="Companion")
	class ACompanionGuide* GetCompanionInstance() const { return CompanionInstance; }

	/** Starts capturing voice for companion command (Push-to-Talk pressed) */
	UFUNCTION(BlueprintCallable, Category="Companion|Voice")
	void StartCompanionListening();

	/** Stops capturing voice and sends to companion LLM (Push-to-Talk released) */
	UFUNCTION(BlueprintCallable, Category="Companion|Voice")
	void StopCompanionListening();

	/** Toggles voice listening on / off */
	UFUNCTION(BlueprintCallable, Category="Companion|Voice")
	void ToggleCompanionListening();

	/** Console command for offline / text testing: "SayCompanion <command>" */
	UFUNCTION(Exec)
	void SayCompanion(const FString& Command);

	/** Console command alias for offline / text testing: "Companion.Say <command>" */
	UFUNCTION(Exec)
	void CompanionSay(const FString& Command);

	/** Runs 10-attempt voice diagnostics table: "Companion.VoiceTest" or "CompanionVoiceTest" */
	UFUNCTION(Exec)
	void CompanionVoiceTest();

	/** Sets the Gemini API key from the in-game console: "SetGeminiKey <key>" */
	UFUNCTION(Exec)
	void SetGeminiKey(const FString& InApiKey);

	/** Sets the Gemini model name from in-game console: "SetGeminiModel <model>" */
	UFUNCTION(Exec)
	void SetGeminiModel(const FString& InModel);

	/** Opens Windows microphone privacy settings from in-game console: "OpenMicSettings" */
	UFUNCTION(Exec)
	void OpenMicSettings();

	/** Runs a quick in-game microphone diagnostics test: "TestMic" */
	UFUNCTION(Exec)
	void TestMic();

	/** Handles pressing 'F' to trigger companion echolocation pulse */
	UFUNCTION(BlueprintCallable, Category = "Companion|Echo")
	void TriggerCompanionEchoPulse();

protected:
	/** Companion class to spawn with this character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Companion")
	TSubclassOf<class ACompanionGuide> CompanionClass;

	/** Active companion instance following this character */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Companion")
	TObjectPtr<class ACompanionGuide> CompanionInstance;
};
