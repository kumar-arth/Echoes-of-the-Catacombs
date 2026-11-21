// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"
#include "BossGrux.generated.h"

class UBoxComponent;
class UAnimSequence;
class ABossArenaGate;
class USkeletalMeshComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EGruxState : uint8
{
	Dormant,
	Engaged,
	Attacking,
	Stampeding,
	Stunned,
	Dead
};

UCLASS()
class ECHOESOFTHECATACOMB_API ABossGrux : public ACharacter, public ICombatDamageable
{
	GENERATED_BODY()

public:
	ABossGrux();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// Boss Stats & Identity
	// -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float MaxHealth = 700.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Stats")
	float CurrentHealth = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float CleaveDamage = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float StampedeDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Stats")
	float SlamDamage = 14.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|State")
	EGruxState BossState = EGruxState::Dormant;

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
	UAnimSequence* AttackRAAnim; // Heavy Right Cleave

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* AttackLAAnim; // Left Hook Cleave

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* FlurryAnim; // 4-Strike Flurry Combo

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* GroundSlamAnim; // Jump & Ground Smash

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* StampedeAnim; // Bull Rush / Charge

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Anim")
	UAnimSequence* RoarAnim; // Ultimate Roar / Enrage

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
	void StartStampede();

	UFUNCTION(BlueprintCallable, Category = "Boss|Action")
	void RecoverFromStun();

	UFUNCTION(BlueprintCallable, Category = "Boss|Action")
	void FinishAttack();

	// ICombatDamageable interface
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override {}
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override {}

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	                         AController* EventInstigator, AActor* DamageCauser) override;

	void SafePlayAnim(UAnimSequence* NewAnim, bool bLoop);

protected:
	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	                  UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void TriggerCleaveImpact(float Damage, float Radius, float Knockback);
	void TriggerSlamImpact();
	void TriggerRoarShockwave();

private:
	FTimerHandle AttackTimerHandle;
	FTimerHandle ImpactTimerHandle;
	FTimerHandle StampedeDurationHandle;
	FTimerHandle StunRecoveryHandle;

	float AttackCooldownTimer = 0.0f;
	float StampedeCooldownTimer = 3.0f;
	FVector StampedeTargetDirection;
	int32 LastAttackIndex = -1;

	UPROPERTY()
	UAnimSequence* CurrentPlayingAnim = nullptr;

	TSet<AActor*> HitActorsThisAttack;

	USkeletalMeshComponent* ChestMesh = nullptr;
	USkeletalMeshComponent* HelmetMesh = nullptr;
	USkeletalMeshComponent* GauntletMesh = nullptr;
	USkeletalMeshComponent* LegMesh = nullptr;
	UStaticMeshComponent* RightSwordMesh = nullptr;
	UStaticMeshComponent* LeftSwordMesh = nullptr;
};

