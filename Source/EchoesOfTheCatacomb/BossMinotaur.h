// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"
#include "BossMinotaur.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UBoxComponent;
class UAnimSequence;
class USoundBase;
class ABossArenaGate;

UENUM(BlueprintType)
enum class EMinotaurState : uint8
{
	Dormant,
	Engaged,
	Attacking,
	Charging,
	Stunned,
	Dead
};

UCLASS()
class ECHOESOFTHECATACOMB_API ABossMinotaur : public ACharacter, public ICombatDamageable
{
	GENERATED_BODY()

public:
	ABossMinotaur();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// Components & Weapon
	// -------------------------------------------------------------

	/** Giant two-handed great-axe mounted on mino_weapon_Socket */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	UStaticMeshComponent* AxeMesh;

	/** Hitbox for weapon damage */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	UBoxComponent* AxeHitbox;

	// -------------------------------------------------------------
	// Boss Stats
	// -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float MaxHealth = 600.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Stats")
	float CurrentHealth = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float MeleeDamage = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float ChargeDamage = 14.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|State")
	EMinotaurState BossState = EMinotaurState::Dormant;

	// -------------------------------------------------------------
	// Linked Arena Gates
	// -------------------------------------------------------------

	UPROPERTY(BlueprintReadWrite, Category = "Boss|Arena")
	ABossArenaGate* EntranceGate;

	UPROPERTY(BlueprintReadWrite, Category = "Boss|Arena")
	ABossArenaGate* ExitGate;

	// -------------------------------------------------------------
	// Animations
	// -------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* IdleAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* WalkAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* RunAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* Attack1Anim; // Cleave

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* Attack2Anim; // Slam

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* Attack3Anim; // Slash

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* KickAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* StompAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* HitReactAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* DeathAnim;

	// -------------------------------------------------------------
	// Combat Actions
	// -------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Boss|Action")
	void WakeUpAndEngage();

	UFUNCTION(BlueprintCallable, Category = "Boss|Action")
	void TriggerAttack();

	UFUNCTION(BlueprintCallable, Category = "Boss|Action")
	void StartBullCharge();

	UFUNCTION(BlueprintCallable, Category = "Boss|Action")
	void RecoverFromStun();

	UFUNCTION(BlueprintCallable, Category = "Boss|Action")
	void FinishAttack();

	// ICombatDamageable interface
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override {}
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override {}

	// Standard AActor damage override
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	                         AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION()
	void OnAxeHitboxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                        bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	                  UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

protected:
	FTimerHandle AttackTimerHandle;
	FTimerHandle StunTimerHandle;
	FTimerHandle ChargeTimerHandle;
	FTimerHandle HitboxWindowHandle;
	FTimerHandle HitboxCloseHandle;
	FTimerHandle StompImpactHandle;

	float AttackCooldownTimer = 0.0f;
	float ChargeCooldownTimer = 10.0f;
	FVector ChargeTargetDirection = FVector::ZeroVector;

	int32 LastAttackIndex = -1;
	TArray<TWeakObjectPtr<AActor>> HitActorsThisAttack;

	UPROPERTY()
	UAnimSequence* CurrentPlayingAnim = nullptr;

	void SafePlayAnim(UAnimSequence* NewAnim, bool bLoop);
	void EnableAxeHitbox(bool bEnable);
	void OnHitboxOpen();
	void OnHitboxClose();
	void OnChargeTimeout();
	void TriggerStompImpact();

	// Aurelius - The Iron Bastion Components & Armor
	USkeletalMeshComponent* ChestMesh = nullptr;
	USkeletalMeshComponent* HelmetMesh = nullptr;
	USkeletalMeshComponent* GauntletMesh = nullptr;
	USkeletalMeshComponent* LegMesh = nullptr;
	UStaticMeshComponent* ShieldMesh = nullptr;

	UAnimSequence* GuardLoopAnim = nullptr;
	UAnimSequence* GuardHitAnim = nullptr;
	UAnimSequence* GuardCounterAnim = nullptr;

	bool bIsShieldGuarding = false;
	float GuardDurationTimer = 0.0f;
	int32 GuardHitCount = 0;
};

