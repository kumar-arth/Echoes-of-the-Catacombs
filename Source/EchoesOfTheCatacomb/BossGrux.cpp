// Copyright Epic Games, Inc. All Rights Reserved.

#include "BossGrux.h"
#include "BossArenaGate.h"
#include "DungeonGenerator.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimSequence.h"
#include "TimerManager.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

ABossGrux::ABossGrux()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. Boss Capsule
	GetCapsuleComponent()->InitCapsuleSize(65.0f, 115.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ABossGrux::OnCapsuleHit);

	// 2. Base Mannequin Mesh scaled 1.25x for imposing presence
	GetMesh()->SetupAttachment(RootComponent);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -115.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetRelativeScale3D(FVector(1.25f, 1.25f, 1.25f));
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMeshFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin.SK_Mannequin"));
	if (MannequinMeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannequinMeshFinder.Object);
	}

	// Modular Knight Armor Components
	ChestMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMesh"));
	ChestMesh->SetupAttachment(GetMesh());
	ChestMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ChestFinder(
		TEXT("/Game/CombatSystem/Meshes/Armor/SK_Chest.SK_Chest"));
	if (ChestFinder.Succeeded()) ChestMesh->SetSkeletalMesh(ChestFinder.Object);

	HelmetMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HelmetMesh"));
	HelmetMesh->SetupAttachment(GetMesh());
	HelmetMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> HelmetFinder(
		TEXT("/Game/CombatSystem/Meshes/Armor/SK_Helmet.SK_Helmet"));
	if (HelmetFinder.Succeeded()) HelmetMesh->SetSkeletalMesh(HelmetFinder.Object);

	GauntletMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GauntletMesh"));
	GauntletMesh->SetupAttachment(GetMesh());
	GauntletMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> GauntletFinder(
		TEXT("/Game/CombatSystem/Meshes/Armor/SK_Guantlet.SK_Guantlet"));
	if (GauntletFinder.Succeeded()) GauntletMesh->SetSkeletalMesh(GauntletFinder.Object);

	LegMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegMesh"));
	LegMesh->SetupAttachment(GetMesh());
	LegMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> LegFinder(
		TEXT("/Game/CombatSystem/Meshes/Armor/SK_Leg.SK_Leg"));
	if (LegFinder.Succeeded()) LegMesh->SetSkeletalMesh(LegFinder.Object);

	// Twin Steel Knight Swords
	RightSwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightSwordMesh"));
	RightSwordMesh->SetupAttachment(GetMesh(), FName(TEXT("hand_r")));
	RightSwordMesh->SetRelativeLocation(FVector(-7.0f, 2.0f, 3.5f));
	RightSwordMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	RightSwordMesh->SetRelativeScale3D(FVector(1.15f, 1.15f, 1.15f));
	RightSwordMesh->SetCollisionProfileName(TEXT("NoCollision"));
	RightSwordMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	LeftSwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftSwordMesh"));
	LeftSwordMesh->SetupAttachment(GetMesh(), FName(TEXT("hand_l")));
	LeftSwordMesh->SetRelativeLocation(FVector(-7.0f, -2.0f, 3.5f));
	LeftSwordMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	LeftSwordMesh->SetRelativeScale3D(FVector(1.15f, 1.15f, 1.15f));
	LeftSwordMesh->SetCollisionProfileName(TEXT("NoCollision"));
	LeftSwordMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SwordFinder(
		TEXT("/Game/CombatSystem/Meshes/Weapons/SM_Sword01.SM_Sword01"));
	if (SwordFinder.Succeeded())
	{
		RightSwordMesh->SetStaticMesh(SwordFinder.Object);
		LeftSwordMesh->SetStaticMesh(SwordFinder.Object);
	}

	// 3. Movement configuration
	GetCharacterMovement()->MaxWalkSpeed = 380.0f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.0f;
	GetCharacterMovement()->GroundFriction = 4.0f;

	// 4. Load Native TwinSword Animations
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/TwinSword_Idle.TwinSword_Idle"));
	if (IdleFinder.Succeeded()) IdleAnim = IdleFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/Move/TwinSword_Walk.TwinSword_Walk"));
	if (WalkFinder.Succeeded()) WalkAnim = WalkFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/Move/TwinSword_Run.TwinSword_Run"));
	if (RunFinder.Succeeded()) RunAnim = RunFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> AttackRAFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/TwinSword_Attack01.TwinSword_Attack01"));
	if (AttackRAFinder.Succeeded()) AttackRAAnim = AttackRAFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> AttackLAFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/TwinSword_Attack02.TwinSword_Attack02"));
	if (AttackLAFinder.Succeeded()) AttackLAAnim = AttackLAFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> FlurryFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/TwinSword_Attack03.TwinSword_Attack03"));
	if (FlurryFinder.Succeeded()) FlurryAnim = FlurryFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> GroundSlamFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/TwinSword_Air_Attack_1.TwinSword_Air_Attack_1"));
	if (GroundSlamFinder.Succeeded()) GroundSlamAnim = GroundSlamFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> StampedeFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/TwinSword_Dash01.TwinSword_Dash01"));
	if (StampedeFinder.Succeeded()) StampedeAnim = StampedeFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> RoarFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Battle/Inplace/TwinSword_Attack05.TwinSword_Attack05"));
	if (RoarFinder.Succeeded()) RoarAnim = RoarFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> HitReactFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Hit/Inplace/TwinSword_Hit_Front.TwinSword_Hit_Front"));
	if (HitReactFinder.Succeeded()) HitReactAnim = HitReactFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/TwinSword/Hit/Inplace/TwinSword_Die_1.TwinSword_Die_1"));
	if (DeathFinder.Succeeded()) DeathAnim = DeathFinder.Object;

	Tags.AddUnique(TEXT("Boss"));
	Tags.AddUnique(TEXT("TwinBlade"));
	Tags.AddUnique(TEXT("Valakor"));
	Tags.AddUnique(TEXT("Grux"));
}

void ABossGrux::SafePlayAnim(UAnimSequence* NewAnim, bool bLoop)
{
	if (!NewAnim || !GetMesh()) return;
	if (CurrentPlayingAnim == NewAnim && bLoop) return;

	CurrentPlayingAnim = NewAnim;
	GetMesh()->PlayAnimation(NewAnim, bLoop);
}

void ABossGrux::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	BossState = EGruxState::Dormant;

	if (ChestMesh) ChestMesh->SetLeaderPoseComponent(GetMesh());
	if (HelmetMesh) HelmetMesh->SetLeaderPoseComponent(GetMesh());
	if (GauntletMesh) GauntletMesh->SetLeaderPoseComponent(GetMesh());
	if (LegMesh) LegMesh->SetLeaderPoseComponent(GetMesh());

	SafePlayAnim(IdleAnim, true);
}

void ABossGrux::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void ABossGrux::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (BossState == EGruxState::Dead || BossState == EGruxState::Dormant)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const FVector BossLoc = GetActorLocation();
	const float DistToPlayer = FVector::Dist2D(BossLoc, PlayerLoc);

	if (BossState == EGruxState::Stampeding)
	{
		// High speed bull rush across arena
		AddMovementInput(StampedeTargetDirection, 1.0f);
		SafePlayAnim(StampedeAnim ? StampedeAnim : RunAnim, true);
		return;
	}

	if (BossState == EGruxState::Stunned || BossState == EGruxState::Attacking)
	{
		return;
	}

	AttackCooldownTimer -= DeltaTime;
	StampedeCooldownTimer -= DeltaTime;

	// Check Stampede charge trigger (when distant > 650 cm and cooldown expired)
	if (StampedeCooldownTimer <= 0.0f && DistToPlayer > 650.0f && BossState == EGruxState::Engaged)
	{
		StartStampede();
		return;
	}

	if (BossState == EGruxState::Engaged)
	{
		// Always face player with smooth interpolation
		const FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(BossLoc, PlayerLoc);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, TargetRot.Yaw, 0.0f), DeltaTime, 6.0f));

		const float MeleeRange = 290.0f;

		if (DistToPlayer > MeleeRange)
		{
			FVector MoveDir = (PlayerLoc - BossLoc).GetSafeNormal2D();

			// Whisker sweep to avoid obstacles
			FHitResult FrontHit;
			FCollisionQueryParams TraceParams(TEXT("GruxNav"), false, this);
			TraceParams.AddIgnoredActor(PlayerPawn);

			const FVector TraceStart = BossLoc + FVector(0.0f, 0.0f, 40.0f);
			const FVector TraceEnd = TraceStart + (MoveDir * 160.0f);

			if (GetWorld()->SweepSingleByChannel(FrontHit, TraceStart, TraceEnd, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(65.0f), TraceParams))
			{
				const FVector RightDir = FVector::CrossProduct(MoveDir, FVector::UpVector);
				const float DeflectSign = (FVector::DotProduct(FrontHit.ImpactNormal, RightDir) >= 0.0f) ? 1.0f : -1.0f;
				MoveDir = (MoveDir + (RightDir * DeflectSign * 1.2f)).GetSafeNormal2D();
			}

			AddMovementInput(MoveDir, 1.0f);
		}
		else
		{
			GetCharacterMovement()->StopMovementImmediately();

			if (AttackCooldownTimer <= 0.0f)
			{
				TriggerAttack();
				return;
			}
		}

		const float CurrentSpeed = GetVelocity().Size2D();
		if (CurrentSpeed > 180.0f)
		{
			SafePlayAnim(RunAnim, true);
		}
		else if (CurrentSpeed > 25.0f)
		{
			SafePlayAnim(WalkAnim, true);
		}
		else
		{
			SafePlayAnim(IdleAnim, true);
		}
	}
}

void ABossGrux::WakeUpAndEngage()
{
	if (BossState != EGruxState::Dormant) return;

	BossState = EGruxState::Engaged;
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;

	// Initial battle twin-blade spin on awake!
	if (RoarAnim)
	{
		SafePlayAnim(RoarAnim, false);
		GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ABossGrux::FinishAttack, 1.4f, false);
	}
	else
	{
		SafePlayAnim(IdleAnim, true);
	}
}

void ABossGrux::TriggerAttack()
{
	if (BossState != EGruxState::Engaged) return;

	// Pick attack randomly from 0 to 5, preventing immediate repetition
	int32 Roll = FMath::RandRange(0, 5);
	if (Roll == LastAttackIndex)
	{
		Roll = (Roll + FMath::RandRange(1, 5)) % 6;
	}
	LastAttackIndex = Roll;

	// Attack 5: Lightning Dash Strike
	if (Roll == 5)
	{
		StartStampede();
		return;
	}

	BossState = EGruxState::Attacking;
	HitActorsThisAttack.Reset();

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		FVector FaceDir = (PlayerPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		SetActorRotation(FaceDir.Rotation());
	}

	UAnimSequence* ChosenAttack = AttackRAAnim;
	float AttackDuration = 1.0f;
	float DamageDelay = 0.28f;

	switch (Roll)
	{
	case 0: // Attack 1: Right Cross Slash
		ChosenAttack = AttackRAAnim ? AttackRAAnim : FlurryAnim;
		AttackDuration = 0.95f;
		DamageDelay = 0.28f;
		break;

	case 1: // Attack 2: Left Cross Slash
		ChosenAttack = AttackLAAnim ? AttackLAAnim : AttackRAAnim;
		AttackDuration = 0.95f;
		DamageDelay = 0.28f;
		break;

	case 2: // Attack 3: Twin Thrust Flurry
		ChosenAttack = FlurryAnim ? FlurryAnim : AttackRAAnim;
		AttackDuration = 1.35f;
		DamageDelay = 0.32f;
		break;

	case 3: // Attack 4: Pouncing Twin-Blade Ground Leap
		ChosenAttack = GroundSlamAnim ? GroundSlamAnim : AttackRAAnim;
		AttackDuration = 1.25f;
		DamageDelay = 0.45f;
		if (PlayerPawn)
		{
			FVector PounceDir = (PlayerPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			LaunchCharacter(PounceDir * 420.0f + FVector(0.0f, 0.0f, 320.0f), true, true);
		}
		break;

	case 4: // Attack 5: Whirlwind Cyclone Spin
		ChosenAttack = RoarAnim ? RoarAnim : AttackRAAnim;
		AttackDuration = 1.40f;
		DamageDelay = 0.35f;
		break;

	default:
		ChosenAttack = AttackRAAnim;
		break;
	}

	SafePlayAnim(ChosenAttack, false);

	if (Roll == 3) // Ground Leap Cross-Stab
	{
		GetWorldTimerManager().SetTimer(ImpactTimerHandle, this, &ABossGrux::TriggerSlamImpact, DamageDelay, false);
	}
	else if (Roll == 4) // Whirlwind Spin
	{
		GetWorldTimerManager().SetTimer(ImpactTimerHandle, this, &ABossGrux::TriggerRoarShockwave, DamageDelay, false);
	}
	else // Cross Slashes or Flurry
	{
		FTimerDelegate CleaveDelegate;
		CleaveDelegate.BindUObject(this, &ABossGrux::TriggerCleaveImpact, CleaveDamage, 280.0f, 260.0f);
		GetWorldTimerManager().SetTimer(ImpactTimerHandle, CleaveDelegate, DamageDelay, false);
	}

	GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ABossGrux::FinishAttack, AttackDuration, false);
	AttackCooldownTimer = AttackDuration + FMath::FRandRange(0.25f, 0.55f);
}

void ABossGrux::TriggerCleaveImpact(float Damage, float Radius, float Knockback)
{
	if (BossState == EGruxState::Dead || !IsValid(this)) return;

	const FVector Origin = GetActorLocation() + (GetActorForwardVector() * 120.0f) + FVector(0.0f, 0.0f, 15.0f);
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams QueryParams(TEXT("GruxCleave"), false, this);

	if (GetWorld()->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, Sphere, QueryParams))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (HitActor && HitActor != this && !HitActorsThisAttack.Contains(HitActor))
			{
				HitActorsThisAttack.Add(HitActor);
				if (ACharacter* Char = Cast<ACharacter>(HitActor))
				{
					UGameplayStatics::ApplyDamage(Char, Damage, GetController(), this, UDamageType::StaticClass());
					Char->LaunchCharacter(GetActorForwardVector() * Knockback + FVector(0.0f, 0.0f, 80.0f), false, false);
				}
			}
		}
	}
}

void ABossGrux::TriggerSlamImpact()
{
	if (BossState == EGruxState::Dead || !IsValid(this)) return;

	const FVector SlamOrigin = GetActorLocation() + (GetActorForwardVector() * 100.0f);
	const float SlamRadius = 380.0f;

	TArray<FOverlapResult> Overlaps;
	FCollisionShape SlamSphere = FCollisionShape::MakeSphere(SlamRadius);
	FCollisionQueryParams QueryParams(TEXT("GruxSlam"), false, this);

	if (GetWorld()->OverlapMultiByChannel(Overlaps, SlamOrigin, FQuat::Identity, ECC_Pawn, SlamSphere, QueryParams))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (HitActor && HitActor != this && !HitActorsThisAttack.Contains(HitActor))
			{
				HitActorsThisAttack.Add(HitActor);
				if (ACharacter* Char = Cast<ACharacter>(HitActor))
				{
					UGameplayStatics::ApplyDamage(Char, SlamDamage, GetController(), this, UDamageType::StaticClass());
					FVector AwayDir = (Char->GetActorLocation() - SlamOrigin).GetSafeNormal2D();
					if (AwayDir.IsNearlyZero()) AwayDir = GetActorForwardVector();
					Char->LaunchCharacter(AwayDir * 550.0f + FVector(0.0f, 0.0f, 220.0f), false, false);
				}
			}
		}
	}
}

void ABossGrux::TriggerRoarShockwave()
{
	if (BossState == EGruxState::Dead || !IsValid(this)) return;

	const FVector RoarOrigin = GetActorLocation();
	const float RoarRadius = 340.0f;

	TArray<FOverlapResult> Overlaps;
	FCollisionShape RoarSphere = FCollisionShape::MakeSphere(RoarRadius);
	FCollisionQueryParams QueryParams(TEXT("TwinWhirlwind"), false, this);

	if (GetWorld()->OverlapMultiByChannel(Overlaps, RoarOrigin, FQuat::Identity, ECC_Pawn, RoarSphere, QueryParams))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (HitActor && HitActor != this && !HitActorsThisAttack.Contains(HitActor))
			{
				HitActorsThisAttack.Add(HitActor);
				if (ACharacter* Char = Cast<ACharacter>(HitActor))
				{
					UGameplayStatics::ApplyDamage(Char, 7.0f, GetController(), this, UDamageType::StaticClass());
					FVector PushDir = (Char->GetActorLocation() - RoarOrigin).GetSafeNormal2D();
					Char->LaunchCharacter(PushDir * 400.0f + FVector(0.0f, 0.0f, 50.0f), false, false);
				}
			}
		}
	}
}

void ABossGrux::StartStampede()
{
	if (BossState != EGruxState::Engaged && BossState != EGruxState::Attacking) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;

	BossState = EGruxState::Stampeding;
	GetCharacterMovement()->MaxWalkSpeed = 620.0f;

	StampedeTargetDirection = (PlayerPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	SetActorRotation(StampedeTargetDirection.Rotation());

	SafePlayAnim(StampedeAnim ? StampedeAnim : RunAnim, true);

	// Dash lasts at most 1.2 seconds before returning to engaged
	GetWorldTimerManager().SetTimer(StampedeDurationHandle, this, &ABossGrux::RecoverFromStun, 1.2f, false);
	StampedeCooldownTimer = 5.0f;
}

void ABossGrux::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                             UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (BossState != EGruxState::Stampeding || !OtherActor) return;

	if (OtherActor->ActorHasTag(TEXT("Boss"))) return;

	if (ACharacter* PlayerChar = Cast<ACharacter>(OtherActor))
	{
		UGameplayStatics::ApplyDamage(PlayerChar, StampedeDamage, GetController(), this, UDamageType::StaticClass());
		PlayerChar->LaunchCharacter(StampedeTargetDirection * 650.0f + FVector(0.0f, 0.0f, 150.0f), true, true);
		RecoverFromStun();
		return;
	}

	// Hit wall or pillar: brief stun
	if (Hit.bBlockingHit)
	{
		BossState = EGruxState::Stunned;
		GetWorldTimerManager().ClearTimer(StampedeDurationHandle);
		GetCharacterMovement()->StopMovementImmediately();

		if (HitReactAnim) SafePlayAnim(HitReactAnim, false);

		GetWorldTimerManager().SetTimer(StunRecoveryHandle, this, &ABossGrux::RecoverFromStun, 1.4f, false);
	}
}

void ABossGrux::RecoverFromStun()
{
	if (BossState == EGruxState::Dead) return;

	BossState = EGruxState::Engaged;
	GetCharacterMovement()->MaxWalkSpeed = 340.0f;
	SafePlayAnim(IdleAnim, true);
	AttackCooldownTimer = 0.5f;
}

void ABossGrux::FinishAttack()
{
	if (BossState == EGruxState::Dead) return;

	BossState = EGruxState::Engaged;
	SafePlayAnim(IdleAnim, true);
}

void ABossGrux::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	FDamageEvent DmgEvent;
	TakeDamage(Damage, DmgEvent, nullptr, DamageCauser);
}

float ABossGrux::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
                            AController* EventInstigator, AActor* DamageCauser)
{
	if (BossState == EGruxState::Dead) return 0.0f;

	if (BossState == EGruxState::Dormant)
	{
		WakeUpAndEngage();
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
	else if (BossState == EGruxState::Engaged && HitReactAnim && FMath::FRand() < 0.25f)
	{
		SafePlayAnim(HitReactAnim, false);
		GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ABossGrux::FinishAttack, 0.6f, false);
	}

	return DamageAmount;
}

void ABossGrux::HandleDeath()
{
	BossState = EGruxState::Dead;

	GetWorldTimerManager().ClearAllTimersForObject(this);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
	if (ChestMesh) ChestMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (HelmetMesh) HelmetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (GauntletMesh) GauntletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (LegMesh) LegMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (RightSwordMesh) RightSwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (LeftSwordMesh) LeftSwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SafePlayAnim(DeathAnim, false);

	if (ExitGate)
	{
		ExitGate->OpenGate();
	}

	// Update dungeon generator state for the room this boss was in
	TArray<AActor*> FoundGenerators;
	UGameplayStatics::GetAllActorsOfClass(this, ADungeonGenerator::StaticClass(), FoundGenerators);
	for (AActor* GenActor : FoundGenerators)
	{
		if (ADungeonGenerator* Gen = Cast<ADungeonGenerator>(GenActor))
		{
			int32 RoomNum = Gen->GetBossRoomAtLocation(GetActorLocation());
			if (RoomNum == 0 && EntranceGate)
			{
				RoomNum = Gen->GetBossRoomAtLocation(EntranceGate->GetActorLocation());
			}
			if (RoomNum == 0)
			{
				const float X = GetActorLocation().X;
				RoomNum = (X < 5000.0f) ? 1 : ((X < 10000.0f) ? 2 : 3);
			}
			Gen->SetBossDefeated(RoomNum, true);
		}
	}
}

