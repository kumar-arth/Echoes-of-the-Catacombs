// Copyright Epic Games, Inc. All Rights Reserved.

#include "BossShadowKnight.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "BossArenaGate.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DungeonGenerator.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ABossShadowKnight::ABossShadowKnight() {
  PrimaryActorTick.bCanEverTick = true;

  // 1. Knight Capsule (generous volume ensuring player attacks always connect)
  GetCapsuleComponent()->InitCapsuleSize(65.0f, 120.0f);
  GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
  GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
  GetCapsuleComponent()->SetCollisionEnabled(
      ECollisionEnabled::QueryAndPhysics);
  GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
  GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
  GetCapsuleComponent()->SetGenerateOverlapEvents(true);
  GetCapsuleComponent()->OnComponentHit.AddDynamic(
      this, &ABossShadowKnight::OnCapsuleHit);

  // 2. Skeletal Mesh scaled 1.25x for towering, menacing boss presence
  GetMesh()->SetupAttachment(RootComponent);
  GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -118.0f));
  GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
  GetMesh()->SetRelativeScale3D(FVector(1.25f, 1.25f, 1.25f));
  GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
  GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  GetMesh()->SetCollisionResponseToAllChannels(ECR_Block);
  GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
  GetMesh()->SetGenerateOverlapEvents(true);

  AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
  AIControllerClass = AAIController::StaticClass();

  // Load Dark Knight full skeletal mesh
  static ConstructorHelpers::FObjectFinder<USkeletalMesh> KnightMeshFinder(TEXT(
      "/Game/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full.SKM_DKM_Full"));
  if (KnightMeshFinder.Succeeded()) {
    GetMesh()->SetSkeletalMesh(KnightMeshFinder.Object);
  }

	// Use direct single node animation playback (guarantees 100% visible animation without AnimBP desync)
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);

  // Dark Knight Material Instances
  static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatArmor(
      TEXT("/Game/Dark_Knight/Dark_Knight_Male/Materials/Instances/"
           "MI_DKM_Armor.MI_DKM_Armor"));
  if (MatArmor.Succeeded())
    GetMesh()->SetMaterial(0, MatArmor.Object);

  static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatHelmet(
      TEXT("/Game/Dark_Knight/Dark_Knight_Male/Materials/Instances/"
           "MI_DKM_Helmet.MI_DKM_Helmet"));
  if (MatHelmet.Succeeded())
    GetMesh()->SetMaterial(1, MatHelmet.Object);

  static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatGauntlets(
      TEXT("/Game/Dark_Knight/Dark_Knight_Male/Materials/Instances/"
           "MI_DKM_Gauntlets.MI_DKM_Gauntlets"));
  if (MatGauntlets.Succeeded())
    GetMesh()->SetMaterial(2, MatGauntlets.Object);

  static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatPants(
      TEXT("/Game/Dark_Knight/Dark_Knight_Male/Materials/Instances/"
           "MI_DKM_Pants.MI_DKM_Pants"));
  if (MatPants.Succeeded())
    GetMesh()->SetMaterial(3, MatPants.Object);

  static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatBoots(
      TEXT("/Game/Dark_Knight/Dark_Knight_Male/Materials/Instances/"
           "MI_DKM_Boots.MI_DKM_Boots"));
  if (MatBoots.Succeeded())
    GetMesh()->SetMaterial(4, MatBoots.Object);

  // 3. Colossal Dark Knight Sword attached to RightHandSocket
  AxeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeMesh"));
  AxeMesh->SetupAttachment(GetMesh(), FName(TEXT("RightHandSocket")));
  AxeMesh->SetRelativeLocation(FVector::ZeroVector);
  AxeMesh->SetRelativeRotation(FRotator::ZeroRotator);
  AxeMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.2f));
  AxeMesh->SetCollisionProfileName(TEXT("NoCollision"));
  AxeMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

  static ConstructorHelpers::FObjectFinder<UStaticMesh> SwordMeshFinder(TEXT(
      "/Game/Dark_Knight/Dark_Knight_Male/Meshes/SM_DKM_Sword.SM_DKM_Sword"));
  if (SwordMeshFinder.Succeeded()) {
    AxeMesh->SetStaticMesh(SwordMeshFinder.Object);
  }

  // 4. Defeat Pose Mesh Component
  DefeatPoseMesh =
      CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("DefeatPoseMesh"));
  DefeatPoseMesh->SetupAttachment(RootComponent);
  DefeatPoseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -118.0f));
  DefeatPoseMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
  DefeatPoseMesh->SetRelativeScale3D(FVector(1.25f, 1.25f, 1.25f));
  DefeatPoseMesh->SetVisibility(false);
  DefeatPoseMesh->SetCollisionProfileName(TEXT("NoCollision"));
  DefeatPoseMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
  if (KnightMeshFinder.Succeeded()) {
    DefeatPoseMesh->SetSkinnedAssetAndUpdate(KnightMeshFinder.Object);
  }
  if (MatArmor.Succeeded())
    DefeatPoseMesh->SetMaterial(0, MatArmor.Object);
  if (MatHelmet.Succeeded())
    DefeatPoseMesh->SetMaterial(1, MatHelmet.Object);
  if (MatGauntlets.Succeeded())
    DefeatPoseMesh->SetMaterial(2, MatGauntlets.Object);
  if (MatPants.Succeeded())
    DefeatPoseMesh->SetMaterial(3, MatPants.Object);
  if (MatBoots.Succeeded())
    DefeatPoseMesh->SetMaterial(4, MatBoots.Object);

  // 5. Movement Setup
  GetCharacterMovement()->MaxWalkSpeed = 440.0f;
  GetCharacterMovement()->bOrientRotationToMovement = true;
  GetCharacterMovement()->RotationRate = FRotator(0.0f, 400.0f, 0.0f);
  GetCharacterMovement()->GravityScale = 1.6f;
  GetCharacterMovement()->AirControl = 0.4f;

	// 6. Load Pure Animation Sequences for Direct Playback
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/AnimationSequence/Anim_DKM_Idle.Anim_DKM_Idle"));
	if (IdleFinder.Succeeded()) IdleAnim = IdleFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkFinder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/AnimationSequence/Anim_DKM_Walk_Alert_Fwd.Anim_DKM_Walk_Alert_Fwd"));
	if (WalkFinder.Succeeded()) WalkAnim = WalkFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunFinder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/AnimationSequence/Anim_DKM_Run_Alert_Fwd.Anim_DKM_Run_Alert_Fwd"));
	if (RunFinder.Succeeded()) RunAnim = RunFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Atk1Finder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/AnimationSequence/Anim_DKM_Attack_01.Anim_DKM_Attack_01"));
	if (Atk1Finder.Succeeded()) Attack1Anim = Atk1Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Atk2Finder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/AnimationSequence/Anim_DKM_Attack_02.Anim_DKM_Attack_02"));
	if (Atk2Finder.Succeeded()) Attack2Anim = Atk2Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Atk3Finder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/AnimationSequence/Anim_DKM_Attack_03.Anim_DKM_Attack_03"));
	if (Atk3Finder.Succeeded()) Attack3Anim = Atk3Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> SlamFinder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/DynamicSwordAndShieldV1/Anim_SAS_Combo5_InPlace_2.Anim_SAS_Combo5_InPlace_2"));
	if (SlamFinder.Succeeded()) SlamAnim = SlamFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> RushFinder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/SingleSword/Anim_Sword_1H_Attack_Stab3.Anim_Sword_1H_Attack_Stab3"));
	if (RushFinder.Succeeded()) RushAnim = RushFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> EvadeFinder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/DynamicSwordAndShieldV3/Anim_DSS3_Evade_B_RM.Anim_DSS3_Evade_B_RM"));
	if (EvadeFinder.Succeeded()) EvadeAnim = EvadeFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> HitReactFinder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/AnimationSequence/Anim_DKM_Hit_Alert_Fwd.Anim_DKM_Hit_Alert_Fwd"));
	if (HitReactFinder.Succeeded()) HitReactAnim = HitReactFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> DarkDeathFinder(
		TEXT("/Game/_GAME/BP/Characters/Player/DarkKnight/Animations/AnimationSequence/Anim_DKM_Death.Anim_DKM_Death"));
	if (DarkDeathFinder.Succeeded()) DeathAnim = DarkDeathFinder.Object;

  Tags.AddUnique(TEXT("Boss"));
  Tags.AddUnique(TEXT("ShadowKnight"));

	// Boss wields colossal Dark Knight sword
	WeaponState = EWeaponState::EWS_OneHandEquipped;
}

void ABossShadowKnight::SafePlayAnim(UAnimSequence* NewAnim, bool bLoop)
{
	if (!NewAnim || !GetMesh()) return;
	if (CurrentPlayingAnim == NewAnim && bLoop) return;

	CurrentPlayingAnim = NewAnim;
	GetMesh()->PlayAnimation(NewAnim, bLoop);
}

void ABossShadowKnight::BeginPlay() {
  Super::BeginPlay();

  CurrentHealth = MaxHealth;
  BossState = EShadowKnightState::Dormant;
  bHasHyperArmor = false;
	WeaponState = EWeaponState::EWS_OneHandEquipped;

	if (DefeatPoseMesh)
	{
		DefeatPoseMesh->SetVisibility(false);
	}
	if (GetMesh())
	{
		GetMesh()->SetVisibility(true);
		GetMesh()->bPauseAnims = false;
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -118.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	CurrentPlayingAnim = nullptr;
	SafePlayAnim(IdleAnim, true);

  TargetPlayer = UGameplayStatics::GetPlayerCharacter(this, 0);
}

void ABossShadowKnight::EndPlay(const EEndPlayReason::Type EndPlayReason) {
  GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
  Super::EndPlay(EndPlayReason);
}

void ABossShadowKnight::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  if (BossState == EShadowKnightState::Dead ||
      BossState == EShadowKnightState::Dormant) {
    return;
  }

  UpdateCombatAI(DeltaTime);
}

void ABossShadowKnight::WakeUpAndEngage() {
  if (BossState != EShadowKnightState::Dormant)
    return;

  BossState = EShadowKnightState::Engaged;
  AttackCooldownTimer = 0.35f;

  TargetPlayer = UGameplayStatics::GetPlayerCharacter(this, 0);
  if (TargetPlayer) {
    FVector LookDir = (TargetPlayer->GetActorLocation() - GetActorLocation())
                          .GetSafeNormal2D();
    SetActorRotation(LookDir.Rotation());
  }

	SafePlayAnim(IdleAnim, true);
  UE_LOG(LogTemp, Warning,
         TEXT("[MALIKOR] The Executioner awakens from the shadows!"));
}

void ABossShadowKnight::UpdateCombatAI(float DeltaTime) {
  if (!TargetPlayer) {
    TargetPlayer = UGameplayStatics::GetPlayerCharacter(this, 0);
    if (!TargetPlayer)
      return;
  }

  const FVector PlayerLoc = TargetPlayer->GetActorLocation();
  const FVector MyLoc = GetActorLocation();
  const float DistToPlayer = FVector::Dist(PlayerLoc, MyLoc);
  const FVector DirToPlayer = (PlayerLoc - MyLoc).GetSafeNormal2D();

	// 1. Shadow Rush (charging straight at player)
	if (bIsRushing)
	{
		SafePlayAnim(RushAnim, true);
		AttackStateTimer += DeltaTime;
		AddMovementInput(RushDirection, 1.0f);

    if (DistToPlayer < 240.0f) {
      bIsRushing = false;
      ExecuteMeleeSweep(280.0f, 8.0f, 480.0f, 90.0f);
      bHasHyperArmor = false;
      ResetToEngaged();
      return;
    }

    if (AttackStateTimer >= RushDuration) {
      bIsRushing = false;
      bHasHyperArmor = false;
      ResetToEngaged();
      return;
    }
    return;
  }

	// 2. Leap Smash in-air landing detection
	if (bIsLeaping)
	{
		SafePlayAnim(SlamAnim, false);
		AttackStateTimer += DeltaTime;
		if (AttackStateTimer > 0.40f && GetCharacterMovement()->IsMovingOnGround())
		{
			bIsLeaping = false;
			if (!bHasLandedShockwave)
			{
				bHasLandedShockwave = true;
				ExecuteRadialShockwave(420.0f, 10.0f, 520.0f);
			}
			bHasHyperArmor = false;
			ResetToEngaged();
			return;
		}
		return;
	}

  // 3. Standard Combat AI State
  if (BossState != EShadowKnightState::Engaged) {
    return;
  }

  // Face player smoothly
  if (DirToPlayer.SizeSquared() > 0.001f) {
    FRotator TargetRot = DirToPlayer.Rotation();
    FRotator NewRot =
        FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 6.0f);
    SetActorRotation(FRotator(0.0f, NewRot.Yaw, 0.0f));
  }

  AttackCooldownTimer -= DeltaTime;

  // Stalk and run toward player
  if (DistToPlayer > 190.0f) {
    GetCharacterMovement()->MaxWalkSpeed = 440.0f;
    AddMovementInput(DirToPlayer, 1.0f);
  } else {
    GetCharacterMovement()->MaxWalkSpeed = 260.0f;
    AddMovementInput(DirToPlayer, 0.4f);
  }

	// Dynamic locomotion animation
	const float CurrentSpeed = GetVelocity().Size2D();
	if (CurrentSpeed > 180.0f)
	{
		SafePlayAnim(RunAnim, true);
	}
	else if (CurrentSpeed > 20.0f)
	{
		SafePlayAnim(WalkAnim, true);
	}
	else
	{
		SafePlayAnim(IdleAnim, true);
	}

  // Decide attack when cooldown expires
  if (AttackCooldownTimer <= 0.0f) {
    if (DistToPlayer <= 260.0f) {
      // Close Range: Combo (45%), Overhead Slam (35%), Backstep (20%)
      const float Roll = FMath::FRand();
      if (Roll < 0.45f) {
        PerformExecutionerCombo();
      } else if (Roll < 0.80f) {
        PerformExecutionerSlam();
      } else {
        PerformEvadeAndCounter();
      }
    } else if (DistToPlayer <= 600.0f) {
      // Medium Range: Leap Smash (40%), Sprint Rush (35%), Stepping Overhead
      // Slam (25%)
      const float Roll = FMath::FRand();
      if (Roll < 0.40f) {
        PerformLeapSmash();
      } else if (Roll < 0.75f) {
        PerformShadowRush();
      } else {
        PerformExecutionerSlam();
      }
    } else {
      // Long Range: Sprint Charge (55%), Flying Leap Smash (45%)
      if (FMath::FRand() < 0.55f) {
        PerformShadowRush();
      } else {
        PerformLeapSmash();
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Attack 1: The Executioner Combo (Chains 1, 2, or 3 hits!)
// -----------------------------------------------------------------------------
void ABossShadowKnight::PerformExecutionerCombo() {
  BossState = EShadowKnightState::Attacking;
  bHasHyperArmor = false;
  AttackCooldownTimer = 2.8f;

  if (TargetPlayer) {
    FVector Dir = (TargetPlayer->GetActorLocation() - GetActorLocation())
                      .GetSafeNormal2D();
    SetActorRotation(Dir.Rotation());
    LaunchCharacter(Dir * 220.0f + FVector(0.0f, 0.0f, 40.0f), true, false);
  }

	// Hit 1: Cleave
	SafePlayAnim(Attack1Anim, false);

  GetWorld()->GetTimerManager().SetTimer(
      AttackTimerHandle,
      [this]() { ExecuteMeleeSweep(270.0f, 5.0f, 260.0f, 85.0f); }, 0.28f,
      false);

  // Hit 2: Reverse slash at 0.48s
  GetWorld()->GetTimerManager().SetTimer(
      ComboTimerHandle,
      [this]() {
        if (BossState != EShadowKnightState::Attacking)
          return;

        if (TargetPlayer) {
          FVector Dir = (TargetPlayer->GetActorLocation() - GetActorLocation())
                            .GetSafeNormal2D();
          SetActorRotation(Dir.Rotation());
          LaunchCharacter(Dir * 160.0f, true, false);
        }

			SafePlayAnim(Attack2Anim, false);
        ExecuteMeleeSweep(270.0f, 7.0f, 320.0f, 85.0f);
      },
      0.48f, false);

  // Hit 3: 360-degree spinning finisher at 0.95s (70% chance)
  const bool bDoFinisher = FMath::FRand() < 0.70f;
  if (bDoFinisher) {
    GetWorld()->GetTimerManager().SetTimer(
        Combo3TimerHandle,
        [this]() {
          if (BossState != EShadowKnightState::Attacking)
            return;

          bHasHyperArmor = true; // Finisher has hyper armor

          if (TargetPlayer) {
            FVector Dir =
                (TargetPlayer->GetActorLocation() - GetActorLocation())
                    .GetSafeNormal2D();
            SetActorRotation(Dir.Rotation());
            LaunchCharacter(Dir * 240.0f + FVector(0.0f, 0.0f, 60.0f), true,
                            false);
          }

				SafePlayAnim(Attack3Anim, false);

          GetWorld()->GetTimerManager().SetTimer(
              AttackTimerHandle,
              [this]() {
                ExecuteMeleeSweep(320.0f, 9.0f, 440.0f,
                                  180.0f); // 360-degree sweep
                bHasHyperArmor = false;
              },
              0.35f, false);
        },
        0.95f, false);
  }

  const float TotalDuration = bDoFinisher ? 1.6f : 1.05f;
  GetWorld()->GetTimerManager().SetTimer(
      RecoveryTimerHandle,
      [this]() {
        bHasHyperArmor = false;
        ResetToEngaged();
      },
      TotalDuration, false);
}

// -----------------------------------------------------------------------------
// Attack 2: Colossal Overhead Ground Slam with Radial Shockwave
// -----------------------------------------------------------------------------
void ABossShadowKnight::PerformExecutionerSlam() {
  BossState = EShadowKnightState::Attacking;
  bHasHyperArmor = true;
  AttackCooldownTimer = 3.0f;

  if (TargetPlayer) {
    FVector Dir = (TargetPlayer->GetActorLocation() - GetActorLocation())
                      .GetSafeNormal2D();
    SetActorRotation(Dir.Rotation());
    LaunchCharacter(Dir * 260.0f + FVector(0.0f, 0.0f, 70.0f), true, false);
  }

	SafePlayAnim(SlamAnim, false);
	const float Duration = (SlamAnim && SlamAnim->GetPlayLength() > 0.0f) ? SlamAnim->GetPlayLength() : 1.25f;

  // Blade impacts the ground at 0.55s
  GetWorld()->GetTimerManager().SetTimer(
      AttackTimerHandle,
      [this]() {
        ExecuteMeleeSweep(280.0f, 10.0f, 440.0f, 75.0f);
        ExecuteRadialShockwave(360.0f, 6.0f, 400.0f);
        bHasHyperArmor = false;
      },
      0.55f, false);

  GetWorld()->GetTimerManager().SetTimer(
      RecoveryTimerHandle,
      [this]() {
        bHasHyperArmor = false;
        ResetToEngaged();
      },
      Duration, false);
}

// -----------------------------------------------------------------------------
// Attack 3: Leaping Executioner Ground Crush & Shockwave
// -----------------------------------------------------------------------------
void ABossShadowKnight::PerformLeapSmash() {
  BossState = EShadowKnightState::Leaping;
  bHasHyperArmor = true;
  bIsLeaping = true;
  bHasLandedShockwave = false;
  AttackCooldownTimer = 3.4f;
  AttackStateTimer = 0.0f;

  FVector LeapDir = GetActorForwardVector();
  if (TargetPlayer) {
    LeapDir = (TargetPlayer->GetActorLocation() - GetActorLocation())
                  .GetSafeNormal2D();
    SetActorRotation(LeapDir.Rotation());
  }

	SafePlayAnim(SlamAnim, false);

  // Airborne leap toward player
  LaunchCharacter(LeapDir * 580.0f + FVector(0.0f, 0.0f, 460.0f), true, true);
}

// -----------------------------------------------------------------------------
// Attack 4: Shadow Rush / Sprinting Thrust Plunge
// -----------------------------------------------------------------------------
void ABossShadowKnight::PerformShadowRush() {
  BossState = EShadowKnightState::Charging;
  bIsRushing = true;
  bHasHyperArmor = true;
  AttackCooldownTimer = 2.8f;
  AttackStateTimer = 0.0f;
  RushDuration = 1.2f;

  if (TargetPlayer) {
    RushDirection = (TargetPlayer->GetActorLocation() - GetActorLocation())
                        .GetSafeNormal2D();
    SetActorRotation(RushDirection.Rotation());
  } else {
    RushDirection = GetActorForwardVector();
  }

	SafePlayAnim(RushAnim, true);

  GetCharacterMovement()->MaxWalkSpeed = 760.0f;
}

// -----------------------------------------------------------------------------
// Attack 5: Tactical Backstep Evade & Immediate Counter-Strike
// -----------------------------------------------------------------------------
void ABossShadowKnight::PerformEvadeAndCounter() {
  BossState = EShadowKnightState::Evading;
  AttackCooldownTimer = 2.2f;

  if (TargetPlayer) {
    FVector AwayDir = (GetActorLocation() - TargetPlayer->GetActorLocation())
                          .GetSafeNormal2D();
    LaunchCharacter(AwayDir * 500.0f + FVector(0.0f, 0.0f, 160.0f), true,
                    false);
  } else {
    LaunchCharacter(-GetActorForwardVector() * 500.0f +
                        FVector(0.0f, 0.0f, 160.0f),
                    true, false);
  }

	SafePlayAnim(EvadeAnim, false);

  // After backstepping (0.45s), immediately launch a counter-strike!
  GetWorld()->GetTimerManager().SetTimer(
      RecoveryTimerHandle,
      [this]() {
        if (BossState == EShadowKnightState::Dead)
          return;

        // 50% Leap Smash counter, 50% Sprint Plunge counter
        if (FMath::FRand() < 0.50f) {
          PerformLeapSmash();
        } else {
          PerformShadowRush();
        }
      },
      0.45f, false);
}

void ABossShadowKnight::ExecuteMeleeSweep(float Radius, float Damage,
                                          float KnockbackPower,
                                          float HalfAngleDegrees) {
  UWorld *World = GetWorld();
  if (!World)
    return;

  const FVector Origin = GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
  const FVector Forward = GetActorForwardVector();

  TArray<FOverlapResult> Overlaps;
  FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
  FCollisionQueryParams QueryParams;
  QueryParams.AddIgnoredActor(this);

  World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn,
                               Sphere, QueryParams);

  for (const FOverlapResult &Overlap : Overlaps) {
    AActor *HitActor = Overlap.GetActor();
    if (HitActor && HitActor != this && !HitActor->ActorHasTag(TEXT("Boss"))) {
      const FVector ToTarget =
          (HitActor->GetActorLocation() - Origin).GetSafeNormal2D();
      const float Dot = FVector::DotProduct(Forward, ToTarget);
      const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));

      if (AngleDeg <= HalfAngleDegrees) {
        FDamageEvent DmgEvent;
        HitActor->TakeDamage(Damage, DmgEvent, GetController(), this);

        if (ACharacter *Char = Cast<ACharacter>(HitActor)) {
          FVector Impulse =
              ToTarget * KnockbackPower + FVector(0.0f, 0.0f, 180.0f);
          Char->LaunchCharacter(Impulse, true, true);
        }
      }
    }
  }
}

void ABossShadowKnight::ExecuteRadialShockwave(float Radius, float Damage,
                                               float KnockbackPower) {
  UWorld *World = GetWorld();
  if (!World)
    return;

  const FVector Center = GetActorLocation() + FVector(0.0f, 0.0f, -80.0f);

  TArray<FOverlapResult> Overlaps;
  FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
  FCollisionQueryParams QueryParams;
  QueryParams.AddIgnoredActor(this);

  World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn,
                               Sphere, QueryParams);

  for (const FOverlapResult &Overlap : Overlaps) {
    AActor *HitActor = Overlap.GetActor();
    if (HitActor && HitActor != this && !HitActor->ActorHasTag(TEXT("Boss"))) {
      FVector ImpulseDir =
          (HitActor->GetActorLocation() - Center).GetSafeNormal2D();
      if (ImpulseDir.IsNearlyZero())
        ImpulseDir = GetActorForwardVector();

      FDamageEvent DmgEvent;
      HitActor->TakeDamage(Damage, DmgEvent, GetController(), this);

      if (ACharacter *Char = Cast<ACharacter>(HitActor)) {
        Char->LaunchCharacter(ImpulseDir * KnockbackPower +
                                  FVector(0.0f, 0.0f, 260.0f),
                              true, true);
      }
    }
  }
}

void ABossShadowKnight::ResetToEngaged()
{
	if (BossState != EShadowKnightState::Dead)
	{
		BossState = EShadowKnightState::Engaged;
		bHasHyperArmor = false;
		GetCharacterMovement()->MaxWalkSpeed = 440.0f;
		SafePlayAnim(IdleAnim, true);
	}
}

void ABossShadowKnight::OnCapsuleHit(UPrimitiveComponent *HitComponent,
                                     AActor *OtherActor,
                                     UPrimitiveComponent *OtherComp,
                                     FVector NormalImpulse,
                                     const FHitResult &Hit) {
  if (bIsRushing && OtherActor && !OtherActor->ActorHasTag(TEXT("Boss"))) {
    bIsRushing = false;
    bHasHyperArmor = false;
    ExecuteMeleeSweep(280.0f, 8.0f, 480.0f, 90.0f);
    ResetToEngaged();
  }
}

void ABossShadowKnight::ApplyDamage(float Damage, AActor *DamageCauser,
                                    const FVector &DamageLocation,
                                    const FVector &DamageImpulse) {
  FDamageEvent DmgEvent;
  TakeDamage(Damage, DmgEvent,
             DamageCauser ? DamageCauser->GetInstigatorController() : nullptr,
             DamageCauser);

  // Directional flinch when not in hyper armor
  if (BossState == EShadowKnightState::Engaged && !bHasHyperArmor) {
    LaunchCharacter(DamageImpulse.GetSafeNormal2D() * 120.0f, false, false);
  }
}

void ABossShadowKnight::GetHit_Implementation(const FVector &ImpactPoint,
                                              AActor *HitInstigator) {
  ApplyDamage(35.0f, HitInstigator, ImpactPoint,
              (GetActorLocation() - ImpactPoint).GetSafeNormal());
}

void ABossShadowKnight::HandleDeath() { Die(); }

float ABossShadowKnight::TakeDamage(float DamageAmount,
                                    struct FDamageEvent const &DamageEvent,
                                    class AController *EventInstigator,
                                    AActor *DamageCauser) {
  if (BossState == EShadowKnightState::Dead)
    return 0.0f;

  // Deduplicate dual calls in the same attack swing
  const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
  if (CurrentTime - LastDamageTime < 0.08f &&
      LastDamageCauser == DamageCauser) {
    return 0.0f;
  }
  LastDamageTime = CurrentTime;
  LastDamageCauser = DamageCauser;

  if (BossState == EShadowKnightState::Dormant) {
    WakeUpAndEngage();
  }

  CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

  UE_LOG(LogTemp, Warning,
         TEXT("[MALIKOR] The Executioner took %.1f damage! Remaining HP: %.1f "
              "/ %.1f"),
         DamageAmount, CurrentHealth, MaxHealth);

	// Play hit reaction animation if outside hyper-armor
	if (!bHasHyperArmor && BossState == EShadowKnightState::Engaged && HitReactAnim)
	{
		SafePlayAnim(HitReactAnim, false);
		GetWorld()->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			[this]()
			{
				if (BossState == EShadowKnightState::Engaged)
				{
					SafePlayAnim(IdleAnim, true);
				}
			},
			0.45f,
			false
		);
	}

  if (CurrentHealth <= 0.0f) {
    Die();
  }

  return DamageAmount;
}

void ABossShadowKnight::Die() {
  if (BossState == EShadowKnightState::Dead)
    return;

  BossState = EShadowKnightState::Dead;
  bHasHyperArmor = false;
  GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

  GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
  GetCharacterMovement()->StopMovementImmediately();
  GetCharacterMovement()->DisableMovement();

  GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);

	// Play death animation sequence
	if (DeathAnim)
	{
		SafePlayAnim(DeathAnim, false);
	}

  // Unseal exit gate to next sector
  if (ExitGate) {
    ExitGate->OpenGate();
  }

  // Update dungeon generator state for the room this boss was in
  TArray<AActor *> FoundGenerators;
  UGameplayStatics::GetAllActorsOfClass(this, ADungeonGenerator::StaticClass(),
                                        FoundGenerators);
  for (AActor *GenActor : FoundGenerators) {
    if (ADungeonGenerator *Gen = Cast<ADungeonGenerator>(GenActor)) {
      int32 RoomNum = Gen->GetBossRoomAtLocation(GetActorLocation());
      if (RoomNum == 0 && EntranceGate) {
        RoomNum = Gen->GetBossRoomAtLocation(EntranceGate->GetActorLocation());
      }
      if (RoomNum == 0) {
        const float X = GetActorLocation().X;
        RoomNum = (X < 5000.0f) ? 1 : ((X < 10000.0f) ? 2 : 3);
      }
      Gen->SetBossDefeated(RoomNum, true);
    }
  }

  UE_LOG(LogTemp, Warning, TEXT("[MALIKOR] The Executioner has been felled!"));
}
