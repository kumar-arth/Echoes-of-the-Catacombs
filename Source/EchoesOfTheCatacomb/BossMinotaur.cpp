// Copyright Epic Games, Inc. All Rights Reserved.

#include "BossMinotaur.h"
#include "BossArenaGate.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimSequence.h"
#include "AIController.h"
#include "TimerManager.h"
#include "DungeonGenerator.h"

ABossMinotaur::ABossMinotaur()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. Large Boss Capsule
	GetCapsuleComponent()->InitCapsuleSize(95.0f, 160.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ABossMinotaur::OnCapsuleHit);

	// 2. Skeletal Mesh
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -160.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MinotaurMeshFinder(
		TEXT("/Game/minotaur1/Mesh/sk_minotaur1.sk_minotaur1"));
	if (MinotaurMeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MinotaurMeshFinder.Object);
	}

	// 3. Two-Handed Great Axe attached to hand socket
	AxeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeMesh"));
	AxeMesh->SetupAttachment(GetMesh(), FName(TEXT("mino_weapon_Socket")));
	AxeMesh->SetCollisionProfileName(TEXT("NoCollision"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> AxeMeshFinder(
		TEXT("/Game/minotaur1/Mesh/sm_minotaur1_axe.sm_minotaur1_axe"));
	if (AxeMeshFinder.Succeeded())
	{
		AxeMesh->SetStaticMesh(AxeMeshFinder.Object);
	}

	// 4. Axe Blade Hitbox
	AxeHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("AxeHitbox"));
	AxeHitbox->SetupAttachment(AxeMesh);
	AxeHitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	AxeHitbox->SetBoxExtent(FVector(30.0f, 40.0f, 60.0f));
	AxeHitbox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AxeHitbox->SetGenerateOverlapEvents(true);
	AxeHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AxeHitbox->OnComponentBeginOverlap.AddDynamic(this, &ABossMinotaur::OnAxeHitboxOverlap);

	// 5. Movement tuning
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 240.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 340.0f;

	// 6. Load Animations
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_idle.anim_minotaur1_idle"));
	if (IdleFinder.Succeeded()) IdleAnim = IdleFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkFinder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_walk_forward.anim_minotaur1_walk_forward"));
	if (WalkFinder.Succeeded()) WalkAnim = WalkFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunFinder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_run.anim_minotaur1_run"));
	if (RunFinder.Succeeded()) RunAnim = RunFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack1Finder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_attack1.anim_minotaur1_attack1"));
	if (Attack1Finder.Succeeded()) Attack1Anim = Attack1Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack2Finder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_attack2.anim_minotaur1_attack2"));
	if (Attack2Finder.Succeeded()) Attack2Anim = Attack2Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack3Finder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_attack3.anim_minotaur1_attack3"));
	if (Attack3Finder.Succeeded()) Attack3Anim = Attack3Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> KickFinder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_attack4_kick.anim_minotaur1_attack4_kick"));
	if (KickFinder.Succeeded()) KickAnim = KickFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> HitReactFinder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_hit_1.anim_minotaur1_hit_1"));
	if (HitReactFinder.Succeeded()) HitReactAnim = HitReactFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathFinder(
		TEXT("/Game/minotaur1/Animation/anim_minotaur1_death.anim_minotaur1_death"));
	if (DeathFinder.Succeeded()) DeathAnim = DeathFinder.Object;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	Tags.AddUnique(TEXT("Boss"));
	Tags.AddUnique(TEXT("Minotaur"));
}

void ABossMinotaur::SafePlayAnim(UAnimSequence* NewAnim, bool bLoop)
{
	if (!NewAnim || !GetMesh()) return;
	if (CurrentPlayingAnim == NewAnim && GetMesh()->IsPlaying()) return;

	CurrentPlayingAnim = NewAnim;
	GetMesh()->PlayAnimation(NewAnim, bLoop);
}

void ABossMinotaur::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	BossState = EMinotaurState::Dormant;

	SafePlayAnim(IdleAnim, true);
}

void ABossMinotaur::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	EnableAxeHitbox(false);

	Super::EndPlay(EndPlayReason);
}

void ABossMinotaur::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (BossState == EMinotaurState::Dead || BossState == EMinotaurState::Dormant)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const FVector BossLoc = GetActorLocation();
	const float DistToPlayer = FVector::Dist2D(BossLoc, PlayerLoc);

	if (BossState == EMinotaurState::Charging)
	{
		// High speed straight line bull rush
		AddMovementInput(ChargeTargetDirection, 1.0f);
		SafePlayAnim(RunAnim, true);
		return;
	}

	if (BossState == EMinotaurState::Stunned || BossState == EMinotaurState::Attacking)
	{
		return; // Handled by attack/stun timers and animations
	}

	// Update timers
	AttackCooldownTimer -= DeltaTime;
	ChargeCooldownTimer -= DeltaTime;

	// Check Bull Charge trigger (when player is distant > 650 cm and cooldown expired)
	if (ChargeCooldownTimer <= 0.0f && DistToPlayer > 650.0f && BossState == EMinotaurState::Engaged)
	{
		StartBullCharge();
		return;
	}

	if (BossState == EMinotaurState::Engaged)
	{
		// Always face the player with smooth interpolation
		const FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(BossLoc, PlayerLoc);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, TargetRot.Yaw, 0.0f), DeltaTime, 6.0f));

		const float MeleeRange = 280.0f;

		if (DistToPlayer > MeleeRange)
		{
			// Direct vector toward player
			FVector MoveDir = (PlayerLoc - BossLoc).GetSafeNormal2D();

			// Whisker sweep to avoid getting stuck behind pillars or walls
			FHitResult FrontHit;
			FCollisionQueryParams TraceParams(TEXT("MinotaurNav"), false, this);
			TraceParams.AddIgnoredActor(PlayerPawn);

			const FVector TraceStart = BossLoc + FVector(0.0f, 0.0f, 40.0f);
			const FVector TraceEnd = TraceStart + (MoveDir * 160.0f);

			if (GetWorld()->SweepSingleByChannel(FrontHit, TraceStart, TraceEnd, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(65.0f), TraceParams))
			{
				// Blocked! Deflect laterally around the obstacle
				const FVector RightDir = FVector::CrossProduct(MoveDir, FVector::UpVector);
				const float DeflectSign = (FVector::DotProduct(FrontHit.ImpactNormal, RightDir) >= 0.0f) ? 1.0f : -1.0f;
				MoveDir = (MoveDir + (RightDir * DeflectSign * 1.2f)).GetSafeNormal2D();
			}

			AddMovementInput(MoveDir, 1.0f);
		}
		else
		{
			// Within melee striking range! Stop moving forward
			GetCharacterMovement()->StopMovementImmediately();

			if (AttackCooldownTimer <= 0.0f)
			{
				TriggerAttack();
				return;
			}
		}

		// Locomotion Animation driven strictly by actual velocity:
		// When moving -> WalkAnim; when stopped -> IdleAnim!
		const float CurrentSpeed = GetVelocity().Size2D();
		if (CurrentSpeed > 30.0f)
		{
			SafePlayAnim(WalkAnim, true);
		}
		else
		{
			SafePlayAnim(IdleAnim, true);
		}
	}
}

void ABossMinotaur::WakeUpAndEngage()
{
	if (BossState != EMinotaurState::Dormant) return;

	BossState = EMinotaurState::Engaged;
	GetCharacterMovement()->MaxWalkSpeed = 340.0f;
	SafePlayAnim(IdleAnim, true);
}

void ABossMinotaur::TriggerAttack()
{
	if (BossState != EMinotaurState::Engaged) return;

	BossState = EMinotaurState::Attacking;
	HitActorsThisAttack.Reset();

	// Pick attack randomly
	const int32 Roll = FMath::RandRange(0, 3);
	UAnimSequence* ChosenAttack = Attack1Anim;
	float AttackDuration = 1.8f;
	float DamageDelay = 0.55f;

	if (Roll == 1 && Attack2Anim)
	{
		ChosenAttack = Attack2Anim;
		AttackDuration = 2.0f;
		DamageDelay = 0.65f;
	}
	else if (Roll == 2 && Attack3Anim)
	{
		ChosenAttack = Attack3Anim;
		AttackDuration = 1.6f;
		DamageDelay = 0.5f;
	}
	else if (Roll == 3 && KickAnim)
	{
		ChosenAttack = KickAnim;
		AttackDuration = 1.7f;
		DamageDelay = 0.6f;
	}

	SafePlayAnim(ChosenAttack, false);

	// Schedule hitbox activation at impact frame
	GetWorldTimerManager().SetTimer(HitboxWindowHandle, this, &ABossMinotaur::OnHitboxOpen, DamageDelay, false);

	// Schedule attack recovery with tight Souls-like cadence
	GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ABossMinotaur::FinishAttack, AttackDuration, false);
	AttackCooldownTimer = AttackDuration + FMath::FRandRange(0.4f, 0.9f);
}

void ABossMinotaur::OnHitboxOpen()
{
	if (BossState == EMinotaurState::Dead || !IsValid(this)) return;

	EnableAxeHitbox(true);
	GetWorldTimerManager().SetTimer(HitboxCloseHandle, this, &ABossMinotaur::OnHitboxClose, 0.45f, false);
}

void ABossMinotaur::OnHitboxClose()
{
	if (!IsValid(this)) return;

	EnableAxeHitbox(false);
}

void ABossMinotaur::FinishAttack()
{
	if (BossState == EMinotaurState::Dead) return;

	EnableAxeHitbox(false);
	BossState = EMinotaurState::Engaged;
	CurrentPlayingAnim = nullptr; // Next Tick dynamically chooses Idle or Walk based on velocity
}

void ABossMinotaur::StartBullCharge()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;

	BossState = EMinotaurState::Charging;
	ChargeCooldownTimer = 14.0f;

	ChargeTargetDirection = (PlayerPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	SetActorRotation(ChargeTargetDirection.Rotation());

	GetCharacterMovement()->MaxWalkSpeed = 820.0f;

	SafePlayAnim(RunAnim, true);

	// End charge automatically after 3.0s if no collision occurs
	GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &ABossMinotaur::OnChargeTimeout, 3.0f, false);
}

void ABossMinotaur::OnChargeTimeout()
{
	if (!IsValid(this)) return;

	if (BossState == EMinotaurState::Charging)
	{
		GetCharacterMovement()->MaxWalkSpeed = 340.0f;
		BossState = EMinotaurState::Engaged;
		CurrentPlayingAnim = nullptr;
	}
}

void ABossMinotaur::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (BossState != EMinotaurState::Charging) return;
	if (!OtherActor || OtherActor == this) return;

	// If charged into player: deal heavy damage and launch
	if (ACharacter* Char = Cast<ACharacter>(OtherActor))
	{
		UGameplayStatics::ApplyDamage(Char, ChargeDamage, GetController(), this, UDamageType::StaticClass());
		Char->LaunchCharacter(ChargeTargetDirection * 750.0f + FVector(0.0f, 0.0f, 250.0f), true, true);

		GetCharacterMovement()->MaxWalkSpeed = 340.0f;
		BossState = EMinotaurState::Engaged;
		CurrentPlayingAnim = nullptr;
		return;
	}

	// Hit wall or pillar: CRASH STUN!
	if (Hit.bBlockingHit && (OtherActor->ActorHasTag(TEXT("Pillar")) || OtherActor->ActorHasTag(TEXT("DungeonWall")) || !Cast<APawn>(OtherActor)))
	{
		BossState = EMinotaurState::Stunned;
		GetWorldTimerManager().ClearTimer(ChargeTimerHandle);

		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		GetCharacterMovement()->StopMovementImmediately();

		// Play hit react or drop to knee
		SafePlayAnim(HitReactAnim, false);

		// 4-second stunned window
		GetWorldTimerManager().SetTimer(StunTimerHandle, this, &ABossMinotaur::RecoverFromStun, 4.0f, false);
	}
}

void ABossMinotaur::RecoverFromStun()
{
	if (BossState == EMinotaurState::Dead) return;

	BossState = EMinotaurState::Engaged;
	GetCharacterMovement()->MaxWalkSpeed = 340.0f;
	CurrentPlayingAnim = nullptr;
}

void ABossMinotaur::EnableAxeHitbox(bool bEnable)
{
	HitActorsThisAttack.Reset();
	if (!AxeHitbox) return;

	if (bEnable)
	{
		AxeHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		AxeHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossMinotaur::OnAxeHitboxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                      bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || BossState == EMinotaurState::Dead) return;

	for (const TWeakObjectPtr<AActor>& HitActor : HitActorsThisAttack)
	{
		if (HitActor.Get() == OtherActor)
		{
			return;
		}
	}

	if (ACharacter* Char = Cast<ACharacter>(OtherActor))
	{
		HitActorsThisAttack.Add(Char);
		UGameplayStatics::ApplyDamage(Char, MeleeDamage, GetController(), this, UDamageType::StaticClass());

		const FVector PushDir = GetActorForwardVector();
		Char->LaunchCharacter(PushDir * 600.0f + FVector(0.0f, 0.0f, 180.0f), true, true);
	}
}

void ABossMinotaur::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	FDamageEvent DmgEvent;
	TakeDamage(Damage, DmgEvent, DamageCauser ? DamageCauser->GetInstigatorController() : nullptr, DamageCauser);
}

float ABossMinotaur::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
                               AController* EventInstigator, AActor* DamageCauser)
{
	if (BossState == EMinotaurState::Dead) return 0.0f;

	// Bonus 1.5x damage when stunned from a pillar crash!
	const float EffectiveDamage = (BossState == EMinotaurState::Stunned) ? (DamageAmount * 1.5f) : DamageAmount;

	CurrentHealth = FMath::Clamp(CurrentHealth - EffectiveDamage, 0.0f, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
	else if (BossState != EMinotaurState::Charging && BossState != EMinotaurState::Attacking)
	{
		if (BossState != EMinotaurState::Stunned)
		{
			SafePlayAnim(HitReactAnim, false);
		}
	}

	return EffectiveDamage;
}

void ABossMinotaur::HandleDeath()
{
	if (BossState == EMinotaurState::Dead) return;

	BossState = EMinotaurState::Dead;

	GetWorldTimerManager().ClearAllTimersForObject(this);
	EnableAxeHitbox(false);

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	SafePlayAnim(DeathAnim, false);

	// Unlock exit gate to Sector 2!
	if (ExitGate)
	{
		ExitGate->OpenGate();
	}

	// Update dungeon generator state
	TArray<AActor*> FoundGenerators;
	UGameplayStatics::GetAllActorsOfClass(this, ADungeonGenerator::StaticClass(), FoundGenerators);
	for (AActor* GenActor : FoundGenerators)
	{
		if (ADungeonGenerator* Gen = Cast<ADungeonGenerator>(GenActor))
		{
			Gen->SetBossDefeated(1, true);
		}
	}
}

