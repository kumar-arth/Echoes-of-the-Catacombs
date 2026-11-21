// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"
#include "BossShadowKnight.generated.h"

class UStaticMeshComponent;
class UPoseableMeshComponent;
class ABossArenaGate;
class UAnimMontage;
class UAnimSequence;

UENUM(BlueprintType)
enum class EShadowKnightState : uint8
{
	Dormant,
	Engaged,
	Attacking,
	Leaping,
	Charging,
	Evading,
	Stunned,
	Dead
};

UCLASS()
class ECHOESOFTHECATACOMB_API ABossShadowKnight : public ABaseCharacter, public ICombatDamageable
{
	GENERATED_BODY()

public:
	ABossShadowKnight();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// Boss Identity & Stats
	// -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float MaxHealth = 850.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Stats")
	float CurrentHealth = 850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	FString BossName = TEXT("Malikor, The Executioner");

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|State")
	EShadowKnightState BossState = EShadowKnightState::Dormant;

	/** When true, incoming attacks do not flinch or interrupt the boss */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Combat")
	bool bHasHyperArmor = false;

	// -------------------------------------------------------------
	// Animations & Montages
	// -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimSequence* DeathAnim = nullptr;

	/** Combo 1: Sweeping horizontal right slash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimMontage* AttackCleaveMontage = nullptr;

	/** Combo 2: Left diagonal reverse slash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimMontage* AttackCombo2Montage = nullptr;

	/** Combo 3: 360-degree spinning executioner finisher */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimMontage* AttackCombo3Montage = nullptr;

	/** Overhead two-handed executioner ground slam with shockwave */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimMontage* AttackChargedSlamMontage = nullptr;

	/** Sprinting lunging forward plunge */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimMontage* AttackSprintRushMontage = nullptr;

	/** Hit reaction / flinch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimMontage* HitReactMontage = nullptr;

	/** Tactical backstep evade */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimMontage* EvadeBackstepMontage = nullptr;

	/** Collapse death */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Anims")
	UAnimMontage* DeathMontage = nullptr;

	// Pure animation sequences (ensures 100% visible animation without AnimBP desync)
	UAnimSequence* IdleAnim = nullptr;
	UAnimSequence* WalkAnim = nullptr;
	UAnimSequence* RunAnim = nullptr;
	UAnimSequence* Attack1Anim = nullptr;
	UAnimSequence* Attack2Anim = nullptr;
	UAnimSequence* Attack3Anim = nullptr;
	UAnimSequence* SlamAnim = nullptr;
	UAnimSequence* RushAnim = nullptr;
	UAnimSequence* EvadeAnim = nullptr;
	UAnimSequence* HitReactAnim = nullptr;

	void SafePlayAnim(UAnimSequence* NewAnim, bool bLoop);

	// -------------------------------------------------------------
	// Weapon & Components
	// -------------------------------------------------------------

	/** Heavy Dark Knight Greatsword */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	UStaticMeshComponent* AxeMesh;

	/** Posed defeated kneeling mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Visuals")
	UPoseableMeshComponent* DefeatPoseMesh;

	// -------------------------------------------------------------
	// Gates Linkage
	// -------------------------------------------------------------

	UPROPERTY()
	ABossArenaGate* EntranceGate = nullptr;

	UPROPERTY()
	ABossArenaGate* ExitGate = nullptr;

	// -------------------------------------------------------------
	// Combat Methods
	// -------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void WakeUpAndEngage();

	// ICombatDamageable interface
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override {}
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override {}

	// IHitInterface override
	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* HitInstigator) override;

	// AActor damage override
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	                        class AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "Boss|Combat")
	void Die();

protected:
	// AI Update
	void UpdateCombatAI(float DeltaTime);

	// Attack implementations (5 distinct combat moves)
	void PerformExecutionerCombo();
	void PerformExecutionerSlam();
	void PerformLeapSmash();
	void PerformShadowRush();
	void PerformEvadeAndCounter();

	// Melee hitbox checks
	void ExecuteMeleeSweep(float Radius, float Damage, float KnockbackPower, float HalfAngleDegrees = 90.0f);
	void ExecuteRadialShockwave(float Radius, float Damage, float KnockbackPower);

	void ResetToEngaged();

	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	                  UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	UPROPERTY()
	ACharacter* TargetPlayer = nullptr;

	FTimerHandle AttackTimerHandle;
	FTimerHandle ComboTimerHandle;
	FTimerHandle Combo3TimerHandle;
	FTimerHandle HyperArmorTimerHandle;
	FTimerHandle RecoveryTimerHandle;

	float AttackCooldownTimer = 1.0f;
	float DecisionTimer = 0.0f;
	float AttackStateTimer = 0.0f;
	bool bIsExecutingAttack = false;

	// Leap smash tracking
	bool bIsLeaping = false;
	bool bHasLandedShockwave = false;

	// Shadow rush tracking
	bool bIsRushing = false;
	FVector RushDirection = FVector::ZeroVector;
	float RushDuration = 0.0f;

	// Timestamp gating for damage deduplication
	float LastDamageTime = -10.0f;
	UPROPERTY()
	AActor* LastDamageCauser = nullptr;

	UAnimSequence* CurrentPlayingAnim = nullptr;
};
