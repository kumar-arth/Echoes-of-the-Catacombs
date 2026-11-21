// Copyright Epic Games, Inc. All Rights Reserved.

#include "BossMinotaur.h"
#include "BossArenaGate.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
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

	// 1. Towering Paladin Capsule
	GetCapsuleComponent()->InitCapsuleSize(48.0f, 115.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ABossMinotaur::OnCapsuleHit);

	// 2. Base Mannequin Skeletal Mesh (Scaled 1.15x for a hulking paladin boss)
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -115.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetRelativeScale3D(FVector(1.15f, 1.15f, 1.15f));
	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMeshFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin.SK_Mannequin"));
	if (MannequinMeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannequinMeshFinder.Object);
	}

	// 3. Modular Plate Armor Components
	ChestMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PaladinChestMesh"));
	ChestMesh->SetupAttachment(GetMesh());
	ChestMesh->SetCollisionProfileName(TEXT("NoCollision"));
	ChestMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	HelmetMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PaladinHelmetMesh"));
	HelmetMesh->SetupAttachment(GetMesh());
	HelmetMesh->SetCollisionProfileName(TEXT("NoCollision"));
	HelmetMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GauntletMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PaladinGauntletMesh"));
	GauntletMesh->SetupAttachment(GetMesh());
	GauntletMesh->SetCollisionProfileName(TEXT("NoCollision"));
	GauntletMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	LegMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PaladinLegMesh"));
	LegMesh->SetupAttachment(GetMesh());
	LegMesh->SetCollisionProfileName(TEXT("NoCollision"));
	LegMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ChestFinder(
		TEXT("/Game/CombatSystem/Meshes/Armor/SK_Chest.SK_Chest"));
	if (ChestFinder.Succeeded()) ChestMesh->SetSkeletalMesh(ChestFinder.Object);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> HelmetFinder(
		TEXT("/Game/CombatSystem/Meshes/Armor/SK_Helmet.SK_Helmet"));
	if (HelmetFinder.Succeeded()) HelmetMesh->SetSkeletalMesh(HelmetFinder.Object);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> GauntletFinder(
		TEXT("/Game/CombatSystem/Meshes/Armor/SK_Guantlet.SK_Guantlet"));
	if (GauntletFinder.Succeeded()) GauntletMesh->SetSkeletalMesh(GauntletFinder.Object);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> LegFinder(
		TEXT("/Game/CombatSystem/Meshes/Armor/SK_Leg.SK_Leg"));
	if (LegFinder.Succeeded()) LegMesh->SetSkeletalMesh(LegFinder.Object);

	// 4. Right Hand: Broadsword (AxeMesh pointer used for sword to preserve reflection binding)
	AxeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	AxeMesh->SetupAttachment(GetMesh(), FName(TEXT("hand_r")));
	AxeMesh->SetRelativeLocation(FVector(-7.0f, 2.0f, 3.5f));
	AxeMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	AxeMesh->SetRelativeScale3D(FVector(1.15f, 1.15f, 1.15f));
	AxeMesh->SetCollisionProfileName(TEXT("NoCollision"));
	AxeMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SwordFinder(
		TEXT("/Game/CombatSystem/Meshes/Weapons/SM_Sword01.SM_Sword01"));
	if (SwordFinder.Succeeded())
	{
		AxeMesh->SetStaticMesh(SwordFinder.Object);
	}

	// 5. Left Hand: Heavy Tower Shield
	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaladinShieldMesh"));
	ShieldMesh->SetupAttachment(GetMesh(), FName(TEXT("hand_l")));
	ShieldMesh->SetRelativeLocation(FVector(0.0f, -5.0f, 0.0f));
	ShieldMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	ShieldMesh->SetRelativeScale3D(FVector(1.15f, 1.15f, 1.15f));
	ShieldMesh->SetCollisionProfileName(TEXT("NoCollision"));
	ShieldMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShieldFinder(
		TEXT("/Game/CombatSystem/Meshes/Weapons/SM_Shield_01.SM_Shield_01"));
	if (ShieldFinder.Succeeded())
	{
		ShieldMesh->SetStaticMesh(ShieldFinder.Object);
	}

	// 6. Sword Blade Hitbox
	AxeHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("SwordHitbox"));
	AxeHitbox->SetupAttachment(AxeMesh);
	AxeHitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	AxeHitbox->SetBoxExtent(FVector(18.0f, 18.0f, 50.0f));
	AxeHitbox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AxeHitbox->SetGenerateOverlapEvents(true);
	AxeHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AxeHitbox->OnComponentBeginOverlap.AddDynamic(this, &ABossMinotaur::OnAxeHitboxOverlap);

	// 7. Movement & Stats Configuration
	MaxHealth = 650.0f;
	CurrentHealth = 650.0f;
	MeleeDamage = 6.0f;
	ChargeDamage = 9.0f;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 320.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.0f;
	GetCharacterMovement()->GroundFriction = 4.0f;

	// 8. Load Native Protector (Sword & Shield) Animations
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Protector_Idle.Protector_Idle"));
	if (IdleFinder.Succeeded()) IdleAnim = IdleFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Movement/Inplace/Walk/Protector_Strafe_Walk_Defense_F.Protector_Strafe_Walk_Defense_F"));
	if (WalkFinder.Succeeded()) WalkAnim = WalkFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Movement/Inplace/Run/Protector_Run_F_Loop.Protector_Run_F_Loop"));
	if (RunFinder.Succeeded()) RunAnim = RunFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack1Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Inplace/Protector_Attack01.Protector_Attack01"));
	if (Attack1Finder.Succeeded()) Attack1Anim = Attack1Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack2Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Inplace/Protector_Attack02.Protector_Attack02"));
	if (Attack2Finder.Succeeded()) Attack2Anim = Attack2Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack3Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Inplace/Protector_Attack03.Protector_Attack03"));
	if (Attack3Finder.Succeeded()) Attack3Anim = Attack3Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> KickFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Inplace/Protector_Air_Attack_1.Protector_Air_Attack_1"));
	if (KickFinder.Succeeded()) KickAnim = KickFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> StompFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Inplace/Protector_FocusEnergy_Attack01.Protector_FocusEnergy_Attack01"));
	if (StompFinder.Succeeded()) StompAnim = StompFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> HitReactFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Hit_F.Protector_Hit_F"));
	if (HitReactFinder.Succeeded()) HitReactAnim = HitReactFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Die_1.Protector_Die_1"));
	if (DeathFinder.Succeeded()) DeathAnim = DeathFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> GuardLoopFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Defense1_Loop.Protector_Defense1_Loop"));
	if (GuardLoopFinder.Succeeded()) GuardLoopAnim = GuardLoopFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> GuardHitFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Defense1_Hit.Protector_Defense1_Hit"));
	if (GuardHitFinder.Succeeded()) GuardHitAnim = GuardHitFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> GuardCounterFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Defense_Counterattack01.Protector_Defense_Counterattack01"));
	if (GuardCounterFinder.Succeeded()) GuardCounterAnim = GuardCounterFinder.Object;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	Tags.AddUnique(TEXT("Boss"));
	Tags.AddUnique(TEXT("Paladin"));
	Tags.AddUnique(TEXT("Aurelius"));
	Tags.AddUnique(TEXT("Minotaur"));
}

void ABossMinotaur::SafePlayAnim(UAnimSequence* NewAnim, bool bLoop)
{
	if (!NewAnim || !GetMesh()) return;
	if (CurrentPlayingAnim == NewAnim && bLoop) return;

	CurrentPlayingAnim = NewAnim;
	GetMesh()->PlayAnimation(NewAnim, bLoop);
}

void ABossMinotaur::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	BossState = EMinotaurState::Dormant;
	bIsShieldGuarding = false;
	GuardHitCount = 0;

	// Synchronize modular plate armor to base body animation
	if (ChestMesh) ChestMesh->SetLeaderPoseComponent(GetMesh());
	if (HelmetMesh) HelmetMesh->SetLeaderPoseComponent(GetMesh());
	if (GauntletMesh) GauntletMesh->SetLeaderPoseComponent(GetMesh());
	if (LegMesh) LegMesh->SetLeaderPoseComponent(GetMesh());

	// Lazy load Guard animations if not initialized via CDO
	if (!GuardLoopAnim)
	{
		GuardLoopAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Defense1_Loop.Protector_Defense1_Loop"));
	}
	if (!GuardHitAnim)
	{
		GuardHitAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Defense1_Hit.Protector_Defense1_Hit"));
	}
	if (!GuardCounterAnim)
	{
		GuardCounterAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Defense_Counterattack01.Protector_Defense_Counterattack01"));
	}

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

	if (BossState == EMinotaurState::Stunned || BossState == EMinotaurState::Attacking)
	{
		return;
	}

	AttackCooldownTimer -= DeltaTime;

	if (BossState == EMinotaurState::Engaged)
	{
		// Always face player with smooth turning
		const FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(BossLoc, PlayerLoc);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, TargetRot.Yaw, 0.0f), DeltaTime, 5.0f));

		const float MeleeRange = 230.0f;

		// -------------------------------------------------------------
		// Shield Guard Stance active
		// -------------------------------------------------------------
		if (bIsShieldGuarding)
		{
			GuardDurationTimer -= DeltaTime;
			if (GuardDurationTimer <= 0.0f)
			{
				bIsShieldGuarding = false;
				CurrentPlayingAnim = nullptr;
			}
			else
			{
				// Advance slowly with shield raised toward player
				const FVector GuardDir = (PlayerLoc - BossLoc).GetSafeNormal2D();
				GetCharacterMovement()->MaxWalkSpeed = 180.0f;
				AddMovementInput(GuardDir, 1.0f);

				if (DistToPlayer <= MeleeRange && AttackCooldownTimer <= 0.0f)
				{
					bIsShieldGuarding = false;
					TriggerAttack();
					return;
				}

				SafePlayAnim(GuardLoopAnim ? GuardLoopAnim : WalkAnim, true);
				return;
			}
		}

		// -------------------------------------------------------------
		// Standard Locomotion & Melee Engagement
		// -------------------------------------------------------------
		if (DistToPlayer > MeleeRange)
		{
			FVector MoveDir = (PlayerLoc - BossLoc).GetSafeNormal2D();

			// Whisker sweep to avoid getting stuck behind pillars or walls
			FHitResult FrontHit;
			FCollisionQueryParams TraceParams(TEXT("PaladinNav"), false, this);
			TraceParams.AddIgnoredActor(PlayerPawn);

			const FVector TraceStart = BossLoc + FVector(0.0f, 0.0f, 40.0f);
			const FVector TraceEnd = TraceStart + (MoveDir * 140.0f);

			if (GetWorld()->SweepSingleByChannel(FrontHit, TraceStart, TraceEnd, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(50.0f), TraceParams))
			{
				const FVector RightDir = FVector::CrossProduct(MoveDir, FVector::UpVector);
				const float DeflectSign = (FVector::DotProduct(FrontHit.ImpactNormal, RightDir) >= 0.0f) ? 1.0f : -1.0f;
				MoveDir = (MoveDir + (RightDir * DeflectSign * 1.2f)).GetSafeNormal2D();
			}

			// Sprint when far, disciplined walk when close
			if (DistToPlayer > 550.0f)
			{
				GetCharacterMovement()->MaxWalkSpeed = 480.0f;
			}
			else
			{
				GetCharacterMovement()->MaxWalkSpeed = 290.0f;
			}

			AddMovementInput(MoveDir, 1.0f);
		}
		else
		{
			// Within striking distance!
			GetCharacterMovement()->StopMovementImmediately();

			if (AttackCooldownTimer <= 0.0f)
			{
				TriggerAttack();
				return;
			}
		}

		// Locomotion animation driven by actual velocity
		const float CurrentSpeed = GetVelocity().Size2D();
		if (CurrentSpeed > 25.0f)
		{
			if (GetCharacterMovement()->MaxWalkSpeed > 350.0f)
			{
				SafePlayAnim(RunAnim, true);
			}
			else
			{
				SafePlayAnim(WalkAnim, true);
			}
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
	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	SafePlayAnim(IdleAnim, true);
}

void ABossMinotaur::TriggerAttack()
{
	if (BossState != EMinotaurState::Engaged) return;

	// Cycle randomly through 5 Paladin attack variants
	int32 Roll = FMath::RandRange(0, 4);
	if (Roll == LastAttackIndex)
	{
		Roll = (Roll + FMath::RandRange(1, 4)) % 5;
	}
	LastAttackIndex = Roll;

	BossState = EMinotaurState::Attacking;
	bIsShieldGuarding = false;
	HitActorsThisAttack.Reset();

	UAnimSequence* ChosenAttack = Attack1Anim;
	float AttackDuration = 1.4f;
	float DamageDelay = 0.45f;

	switch (Roll)
	{
	case 0: // Attack 1: Heavy Right Slash (6.0 dmg)
		ChosenAttack = Attack1Anim;
		AttackDuration = 1.4f;
		DamageDelay = 0.42f;
		MeleeDamage = 6.0f;
		break;

	case 1: // Attack 2: Shield Bash & Lunging Thrust combo (5.0 + 7.0 dmg)
		ChosenAttack = Attack2Anim ? Attack2Anim : Attack1Anim;
		AttackDuration = 1.7f;
		DamageDelay = 0.38f;
		MeleeDamage = 6.5f;
		break;

	case 2: // Attack 3: Overhead Shield Slam (8.0 dmg)
		ChosenAttack = Attack3Anim ? Attack3Anim : Attack1Anim;
		AttackDuration = 1.6f;
		DamageDelay = 0.48f;
		MeleeDamage = 8.0f;
		break;

	case 3: // Attack 4: Pouncing Shield Leap / Ground Dive (9.0 dmg)
		ChosenAttack = KickAnim ? KickAnim : Attack1Anim;
		AttackDuration = 1.9f;
		DamageDelay = 0.65f;
		MeleeDamage = 9.0f;
		break;

	case 4: // Attack 5: Holy Bastion Radiant Ground Shockwave (10.0 dmg)
		ChosenAttack = StompAnim ? StompAnim : Attack1Anim;
		AttackDuration = 2.1f;
		DamageDelay = 0.75f;
		ChargeDamage = 10.0f;
		break;

	default:
		ChosenAttack = Attack1Anim;
		break;
	}

	SafePlayAnim(ChosenAttack, false);

	if (Roll == 4)
	{
		// Radial Holy Ground Shockwave
		GetWorldTimerManager().SetTimer(StompImpactHandle, this, &ABossMinotaur::TriggerStompImpact, DamageDelay, false);
	}
	else
	{
		// Standard weapon/shield strike hitbox window
		GetWorldTimerManager().SetTimer(HitboxWindowHandle, this, &ABossMinotaur::OnHitboxOpen, DamageDelay, false);
	}

	GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ABossMinotaur::FinishAttack, AttackDuration, false);
	AttackCooldownTimer = AttackDuration + FMath::FRandRange(0.4f, 0.8f);
}

void ABossMinotaur::TriggerStompImpact()
{
	if (BossState == EMinotaurState::Dead || !IsValid(this)) return;

	const FVector StompOrigin = GetActorLocation() + (GetActorForwardVector() * 70.0f);
	const float StompRadius = 380.0f;

	TArray<FOverlapResult> Overlaps;
	FCollisionShape StompSphere = FCollisionShape::MakeSphere(StompRadius);
	FCollisionQueryParams QueryParams(TEXT("PaladinStomp"), false, this);

	if (GetWorld()->OverlapMultiByChannel(Overlaps, StompOrigin, FQuat::Identity, ECC_Pawn, StompSphere, QueryParams))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (HitActor && HitActor != this)
			{
				if (ACharacter* Char = Cast<ACharacter>(HitActor))
				{
					UGameplayStatics::ApplyDamage(Char, ChargeDamage, GetController(), this, UDamageType::StaticClass());
					Char->LaunchCharacter(FVector(0.0f, 0.0f, 320.0f) + (GetActorForwardVector() * 200.0f), true, true);
				}
			}
		}
	}
}

void ABossMinotaur::OnHitboxOpen()
{
	if (BossState == EMinotaurState::Dead || !IsValid(this)) return;

	EnableAxeHitbox(true);
	GetWorldTimerManager().SetTimer(HitboxCloseHandle, this, &ABossMinotaur::OnHitboxClose, 0.40f, false);
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
	CurrentPlayingAnim = nullptr;

	// 50% chance to enter Shield Guard Stance after finishing an attack
	if (FMath::FRand() < 0.50f)
	{
		bIsShieldGuarding = true;
		GuardDurationTimer = 2.4f;
		GuardHitCount = 0;
	}
}

void ABossMinotaur::StartBullCharge()
{
	// For Aurelius, Bull Charge functions as raising the Bulwark Shield Guard
	bIsShieldGuarding = true;
	GuardDurationTimer = 3.0f;
	GuardHitCount = 0;
	BossState = EMinotaurState::Engaged;
}

void ABossMinotaur::OnChargeTimeout()
{
	bIsShieldGuarding = false;
	BossState = EMinotaurState::Engaged;
}

void ABossMinotaur::RecoverFromStun()
{
	if (BossState == EMinotaurState::Dead) return;

	BossState = EMinotaurState::Engaged;
	SafePlayAnim(IdleAnim, true);
}

void ABossMinotaur::EnableAxeHitbox(bool bEnable)
{
	if (!AxeHitbox) return;

	AxeHitbox->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (!bEnable)
	{
		HitActorsThisAttack.Reset();
	}
}

void ABossMinotaur::OnAxeHitboxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                      bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || BossState == EMinotaurState::Dead) return;
	if (HitActorsThisAttack.Contains(OtherActor)) return;

	HitActorsThisAttack.Add(OtherActor);

	if (ACharacter* TargetChar = Cast<ACharacter>(OtherActor))
	{
		UGameplayStatics::ApplyDamage(TargetChar, MeleeDamage, GetController(), this, UDamageType::StaticClass());

		const FVector KnockbackDir = (TargetChar->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		TargetChar->LaunchCharacter((KnockbackDir * 240.0f) + FVector(0.0f, 0.0f, 150.0f), true, true);
	}
}

void ABossMinotaur::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Nothing required for capsule collision
}

float ABossMinotaur::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
                               AController* EventInstigator, AActor* DamageCauser)
{
	if (BossState == EMinotaurState::Dead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	if (BossState == EMinotaurState::Dormant)
	{
		WakeUpAndEngage();
	}

	// -------------------------------------------------------------
	// Shield Guard Check (Bulwark of the Celestial Order)
	// Frontal attacks are blocked, mitigated by 85%, and trigger riposte
	// -------------------------------------------------------------
	if (bIsShieldGuarding && DamageCauser)
	{
		const FVector DirToAttacker = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const float FrontDot = FVector::DotProduct(GetActorForwardVector(), DirToAttacker);

		if (FrontDot > 0.20f) // Within forward 150° shield arc
		{
			// BLOCKED!
			GuardHitCount++;
			DamageAmount *= 0.15f; // Mitigate 85% damage

			if (GuardHitAnim)
			{
				SafePlayAnim(GuardHitAnim, false);
			}

			// After blocking 2 attacks, immediately unleash counter-riposte
			if (GuardHitCount >= 2)
			{
				bIsShieldGuarding = false;
				GuardHitCount = 0;
				BossState = EMinotaurState::Attacking;

				if (GuardCounterAnim)
				{
					SafePlayAnim(GuardCounterAnim, false);
				}

				MeleeDamage = 7.0f;
				GetWorldTimerManager().SetTimer(HitboxWindowHandle, this, &ABossMinotaur::OnHitboxOpen, 0.35f, false);
				GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ABossMinotaur::FinishAttack, 1.4f, false);
			}

			CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
			if (CurrentHealth <= 0.0f)
			{
				HandleDeath();
			}

			return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
		}
	}

	// Unblocked hit
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
	else if (BossState != EMinotaurState::Attacking && !bIsShieldGuarding)
	{
		if (HitReactAnim)
		{
			SafePlayAnim(HitReactAnim, false);
		}
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ABossMinotaur::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	FDamageEvent DmgEvent;
	AController* InstigatorCtrl = DamageCauser ? DamageCauser->GetInstigatorController() : nullptr;
	TakeDamage(Damage, DmgEvent, InstigatorCtrl, DamageCauser);
}

void ABossMinotaur::HandleDeath()
{
	if (BossState == EMinotaurState::Dead) return;

	BossState = EMinotaurState::Dead;
	bIsShieldGuarding = false;
	GetWorldTimerManager().ClearAllTimersForObject(this);
	EnableAxeHitbox(false);

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SafePlayAnim(DeathAnim, false);

	// Open exit gate
	if (ExitGate)
	{
		ExitGate->OpenGate();
	}

	// Notify DungeonGenerator that this boss is defeated
	TArray<AActor*> FoundGenerators;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADungeonGenerator::StaticClass(), FoundGenerators);
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

	UE_LOG(LogTemp, Log, TEXT("Aurelius the Iron Bastion has fallen. Exit gate opened!"));
}
