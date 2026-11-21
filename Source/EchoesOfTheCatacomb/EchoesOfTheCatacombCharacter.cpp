// Copyright Epic Games, Inc. All Rights Reserved.

#include "EchoesOfTheCatacombCharacter.h"
#include "MotionWarpingComponent.h"
#include "EchoPulseComponent.h"
#include "DungeonHUDWidget.h"
#include "DungeonGenerator.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/DamageEvents.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerStart.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "EchoesOfTheCatacomb.h"
#include "UObject/UnrealType.h"
#include "Interfaces/HitInterface.h"

AEchoesOfTheCatacombCharacter::AEchoesOfTheCatacombCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	bEnableCombatAbilities = true;

	// Configure character movement (Stock Unreal Character Movement)
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->JumpZVelocity = 600.0f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->MaxAcceleration = 2048.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->BrakingDecelerationFalling = 0.0f;
	GetCharacterMovement()->GroundFriction = 8.0f;

	// Create a camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Motion Warping for Souls-style attack step-ins and target alignment
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

	AttackSound = nullptr;
	GuardBodyMesh = nullptr;
	GuardHeadMesh = nullptr;

	// Load Dark Souls III Combat Animations on SK_Mannequin
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack1Finder(
		TEXT("/Game/Anims/Animations/Sword_And_Shield_Slash_Anim.Sword_And_Shield_Slash_Anim"));
	if (Attack1Finder.Succeeded())
	{
		Attack1Anim = Attack1Finder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack2Finder(
		TEXT("/Game/Anims/Animations/Sword_And_Shield_Slash_Anim.Sword_And_Shield_Slash_Anim"));
	if (Attack2Finder.Succeeded())
	{
		Attack2Anim = Attack2Finder.Object;
	}

	// Attack 3: Overhead slash finisher
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack3Finder(
		TEXT("/Game/Anims/Animations/Sword_And_Shield_Slash_Anim.Sword_And_Shield_Slash_Anim"));
	if (Attack3Finder.Succeeded())
	{
		Attack3Anim = Attack3Finder.Object;
	}

	// Heavy Attack: Heavy charged slash
	static ConstructorHelpers::FObjectFinder<UAnimSequence> HeavyAttackFinder(
		TEXT("/Game/Anims/Animations/Sword_And_Shield_Slash_Anim.Sword_And_Shield_Slash_Anim"));
	if (HeavyAttackFinder.Succeeded())
	{
		HeavyAttackAnim = HeavyAttackFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> FallFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop.MM_Fall_Loop"));
	if (FallFinder.Succeeded())
	{
		FallLoopAnim = FallFinder.Object;
	}

	// Running Attack: Dark Souls III Lunging Sprint Slash
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunningAttackFinder(
		TEXT("/Game/Anims/Animations/Sword_And_Shield_Slash_Anim.Sword_And_Shield_Slash_Anim"));
	if (RunningAttackFinder.Succeeded())
	{
		RunningAttackAnim = RunningAttackFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> DodgeFinder(
		TEXT("/Game/Anims/Animations/Quick_Roll_To_Run_Anim.Quick_Roll_To_Run_Anim"));
	if (DodgeFinder.Succeeded())
	{
		DodgeAnim = DodgeFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> BlockFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Inplace/Protector_Defense_Loop.Protector_Defense_Loop"));
	if (BlockFinder.Succeeded())
	{
		BlockAnim = BlockFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump"));
	if (JumpFinder.Succeeded())
	{
		JumpAnim = JumpFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> LandFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Land.MM_Land"));
	if (LandFinder.Succeeded())
	{
		LandAnim = LandFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> HitReactFinder(
		TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01"));
	if (HitReactFinder.Succeeded())
	{
		HitReactAnim = HitReactFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathFinder(
		TEXT("/Game/Anims/Animations/Sword_And_Shield_Death_Anim.Sword_And_Shield_Death_Anim"));
	if (DeathFinder.Succeeded())
	{
		DeathAnim = DeathFinder.Object;
	}

	// -------------------------------------------------------------
	// Advanced Melee Combat System (Protector Suite) Montages
	// -------------------------------------------------------------
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtAtk1Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Attack_Combo01_1_Montage.Protector_Attack_Combo01_1_Montage"));
	if (ProtAtk1Finder.Succeeded()) ProtectorAttack1Montage = ProtAtk1Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtAtk2Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Attack_Combo01_2_Montage.Protector_Attack_Combo01_2_Montage"));
	if (ProtAtk2Finder.Succeeded()) ProtectorAttack2Montage = ProtAtk2Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtAtk3Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Attack_Combo01_3_Montage.Protector_Attack_Combo01_3_Montage"));
	if (ProtAtk3Finder.Succeeded()) ProtectorAttack3Montage = ProtAtk3Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtAtk4Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Attack_Combo01_4_Montage.Protector_Attack_Combo01_4_Montage"));
	if (ProtAtk4Finder.Succeeded()) ProtectorAttack4Montage = ProtAtk4Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtHvy1Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Attack_Combo04_1_Montage.Protector_Attack_Combo04_1_Montage"));
	if (ProtHvy1Finder.Succeeded()) ProtectorHeavy1Montage = ProtHvy1Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtHvy2Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Attack_Combo04_2_Montage.Protector_Attack_Combo04_2_Montage"));
	if (ProtHvy2Finder.Succeeded()) ProtectorHeavy2Montage = ProtHvy2Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtHvy3Finder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Attack_Combo04_3_Montage.Protector_Attack_Combo04_3_Montage"));
	if (ProtHvy3Finder.Succeeded()) ProtectorHeavy3Montage = ProtHvy3Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtSprintAtkFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Attack10_1_End_Montage.Protector_Attack10_1_End_Montage"));
	if (ProtSprintAtkFinder.Succeeded()) ProtectorSprintAttackMontage = ProtSprintAtkFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtAirAtkFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Protector_Air_Attack_1_Montage.Protector_Air_Attack_1_Montage"));
	if (ProtAirAtkFinder.Succeeded()) ProtectorAirAttackMontage = ProtAirAtkFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtDodgeFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Root/Dodge/Protector_Dodge_F_Montage.Protector_Dodge_F_Montage"));
	if (ProtDodgeFinder.Succeeded()) ProtectorDodgeMontage = ProtDodgeFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtBlockHitFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Root/Protector_Defense_Hit_Montage.Protector_Defense_Hit_Montage"));
	if (ProtBlockHitFinder.Succeeded()) ProtectorBlockHitMontage = ProtBlockHitFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtGuardBreakFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Root/Protector_Defense_Hit_Broken_Montage.Protector_Defense_Hit_Broken_Montage"));
	if (ProtGuardBreakFinder.Succeeded()) ProtectorGuardBreakMontage = ProtGuardBreakFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtHitFFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Root/Protector_Hit_F_Montage.Protector_Hit_F_Montage"));
	if (ProtHitFFinder.Succeeded()) ProtectorHitFMontage = ProtHitFFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtHitBFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Root/Protector_Hit_B_Montage.Protector_Hit_B_Montage"));
	if (ProtHitBFinder.Succeeded()) ProtectorHitBMontage = ProtHitBFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtHitLFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Root/Protector_Hit_L_Montage.Protector_Hit_L_Montage"));
	if (ProtHitLFinder.Succeeded()) ProtectorHitLMontage = ProtHitLFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtHitRFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Root/Protector_Hit_R_Montage.Protector_Hit_R_Montage"));
	if (ProtHitRFinder.Succeeded()) ProtectorHitRMontage = ProtHitRFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ProtDieFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Hit/Root/Protector_Die_1_Montage.Protector_Die_1_Montage"));
	if (ProtDieFinder.Succeeded()) ProtectorDieMontage = ProtDieFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> ProtIdleFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Battle/Protector_Idle.Protector_Idle"));
	if (ProtIdleFinder.Succeeded()) ProtectorIdleAnim = ProtIdleFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimSequence> ProtCommonIdleFinder(
		TEXT("/Game/Characters/Mannequin_UE4/Animations/Protector/Common/Inplace/Protector_Common_Idle.Protector_Common_Idle"));
	if (ProtCommonIdleFinder.Succeeded()) ProtectorCommonIdleAnim = ProtCommonIdleFinder.Object;

	// -------------------------------------------------------------
	// 1. Knight's Steel Longsword attached to right hand
	// -------------------------------------------------------------
	SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	SwordMesh->SetupAttachment(GetMesh(), FName(TEXT("HandGrip_R")));
	SwordMesh->SetRelativeLocation(SwordHandLocation);
	SwordMesh->SetRelativeRotation(SwordHandRotation);
	SwordMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	SwordMesh->SetCollisionProfileName(TEXT("NoCollision"));
	SwordMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	SwordMesh->SetVisibility(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SwordFinder(
		TEXT("/Game/Weapons/SM_KnightSword.SM_KnightSword"));
	if (SwordFinder.Succeeded())
	{
		SwordMesh->SetStaticMesh(SwordFinder.Object);
	}

	// -------------------------------------------------------------
	// 2. Knight's Heraldic Heater Shield attached to left forearm (lowerarm_l)
	// -------------------------------------------------------------
	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	ShieldMesh->SetupAttachment(GetMesh(), FName(TEXT("ShieldSocket")));
	ShieldMesh->SetRelativeLocation(FVector::ZeroVector);
	ShieldMesh->SetRelativeRotation(FRotator::ZeroRotator);
	ShieldMesh->SetRelativeScale3D(FVector(0.95f, 0.95f, 0.95f));
	ShieldMesh->SetCollisionProfileName(TEXT("NoCollision"));
	ShieldMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ShieldMesh->SetVisibility(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShieldFinder(
		TEXT("/Game/Weapons/SM_KnightShield.SM_KnightShield"));
	if (ShieldFinder.Succeeded())
	{
		ShieldMesh->SetStaticMesh(ShieldFinder.Object);
	}

	// Weapon Combat Stats
	AttackDamage = 45.0f;
	AttackRange = 240.0f;

	CurrentHealth = MaxHealth;
	CurrentStamina = MaxStamina;
	StaminaRegenRate = 45.0f; // DS3 base stamina recovery (45/s)
}

namespace
{
	static FTransform g_EchoesInitialSpawnTransform;
	static bool g_HasEchoesInitialSpawnTransform = false;
}

void AEchoesOfTheCatacombCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (ADungeonGenerator* Gen = Cast<ADungeonGenerator>(UGameplayStatics::GetActorOfClass(World, ADungeonGenerator::StaticClass())))
		{
			if (!Gen->PlayerSpawnLocation.IsZero())
			{
				TeleportTo(Gen->PlayerSpawnLocation + FVector(0.0f, 0.0f, 50.0f), FRotator(0.0f, 90.0f, 0.0f), false, true);
				if (AController* C = GetController())
				{
					C->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
				}
			}
		}
	}

	g_EchoesInitialSpawnTransform = GetActorTransform();
	g_HasEchoesInitialSpawnTransform = true;

	CurrentHealth = MaxHealth;
	CurrentStamina = MaxStamina;
	bIsInvulnerable = false;
	bIsSprinting = false;
	CombatState = EPlayerCombatState::Idle;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = 500.0f;
		MoveComp->MinAnalogWalkSpeed = 20.0f;
		MoveComp->MaxAcceleration = 2048.0f;
		MoveComp->BrakingDecelerationWalking = 2000.0f;
		MoveComp->GroundFriction = 8.0f;
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

		MoveComp->GravityScale = 1.0f;
		MoveComp->JumpZVelocity = 600.0f;
		MoveComp->AirControl = 0.35f;
		MoveComp->BrakingDecelerationFalling = 0.0f;
	}

	// -------------------------------------------------------------
	// Stock Unreal Character (Manny / Mannequin)
	// -------------------------------------------------------------
	USkeletalMesh* StockMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (!StockMesh)
	{
		StockMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/CR/Demo/Meshes/SKM_Manny.SKM_Manny"));
	}

	UClass* StockABP = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	if (!StockABP)
	{
		StockABP = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/CR/Demo/Animations/ABP_Manny.ABP_Manny_C"));
	}

	if (StockMesh && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(StockMesh);
		if (StockABP)
		{
			GetMesh()->SetAnimInstanceClass(StockABP);
		}
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		GetMesh()->SetVisibility(true);
	}

	// Remove any extra modular mesh attachments from previous characters
	if (GuardBodyMesh)
	{
		GuardBodyMesh->DestroyComponent();
		GuardBodyMesh = nullptr;
	}
	if (GuardHeadMesh)
	{
		GuardHeadMesh->DestroyComponent();
		GuardHeadMesh = nullptr;
	}

	// -------------------------------------------------------------
	// Weapons Attachment to Skeleton Bones/Sockets (Guaranteed against runtime mesh resets)
	// -------------------------------------------------------------
	if (SwordMesh && GetMesh())
	{
		const FName TargetSwordSocket = GetMesh()->DoesSocketExist(SwordSocketName) ? SwordSocketName : FName(TEXT("hand_r"));
		SwordMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetSwordSocket);
		if (TargetSwordSocket == FName(TEXT("HandGrip_R")))
		{
			SwordMesh->SetRelativeLocation(SwordHandLocation);
			SwordMesh->SetRelativeRotation(SwordHandRotation);
		}
		else
		{
			// Raw hand_r bone fallback: center grip into palm
			SwordMesh->SetRelativeLocation(FVector(-7.0f, 2.0f, 3.5f));
			SwordMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
		}
		SwordMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		SwordMesh->SetVisibility(true);
	}
	if (ShieldMesh && GetMesh())
	{
		const FName TargetShieldSocket = GetMesh()->DoesSocketExist(ShieldSocketName) ? ShieldSocketName : FName(TEXT("lowerarm_l"));
		ShieldMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetShieldSocket);
		if (TargetShieldSocket == FName(TEXT("ShieldSocket")))
		{
			ShieldMesh->SetRelativeLocation(FVector::ZeroVector);
			ShieldMesh->SetRelativeRotation(FRotator::ZeroRotator);
		}
		else
		{
			// Raw lowerarm_l bone fallback: outer forearm facing outwards
			ShieldMesh->SetRelativeLocation(ShieldForearmLocation);
			ShieldMesh->SetRelativeRotation(ShieldForearmRotation);
		}
		ShieldMesh->SetRelativeScale3D(FVector(0.95f, 0.95f, 0.95f));
		ShieldMesh->SetVisibility(true);
	}

	// Only spawn HUD if combat abilities are active
	if (bEnableCombatAbilities && IsLocallyControlled() && GEngine && GEngine->GameViewport)
	{
		DungeonHUDWidget = SNew(SDungeonHUDWidget).Character(this);
		GEngine->GameViewport->AddViewportWidgetContent(DungeonHUDWidget.ToSharedRef(), 100);
	}
}

void AEchoesOfTheCatacombCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateLockOn(DeltaTime);

	// Ensure Dungeon HUD is spawned as soon as character is locally controlled and viewport is ready
	if (!DungeonHUDWidget.IsValid() && IsLocallyControlled() && GEngine && GEngine->GameViewport)
	{
		DungeonHUDWidget = SNew(SDungeonHUDWidget).Character(this);
		GEngine->GameViewport->AddViewportWidgetContent(DungeonHUDWidget.ToSharedRef(), 100);
	}

	if (!bEnableCombatAbilities)
	{
		UpdateLocomotionState();
		return;
	}

	// Sprint Stamina Drain (gentle drain without abruptly canceling sprint)
	if (bIsSprinting && !bIsDodging && CurrentHealth > 0.0f)
	{
		const float Speed = GetVelocity().Size2D();
		const bool bHasMoveInput = !GetLastMovementInputVector().IsNearlyZero();
		if (Speed > 150.0f && bHasMoveInput)
		{
			ConsumeStamina(6.0f * DeltaTime);
		}
	}
	else if (bCanRegenStamina && CurrentHealth > 0.0f && !bIsDodging && !bIsAttacking)
	{
		// Dark Souls Stamina Regeneration (reduced to 25% while holding shield up!)
		const float RegenMultiplier = bIsBlocking ? 0.25f : 1.0f;
		CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + (StaminaRegenRate * RegenMultiplier * DeltaTime));
	}

	UpdateLocomotionState();
	UpdateCombatGripIdle();

	// -------------------------------------------------------------
	// Interaction & Victory Proximity Triggers
	// -------------------------------------------------------------
	if (CurrentHealth > 0.0f && !FDungeonGameState::HasPlayerWon())
	{
		UWorld* World = GetWorld();
		const FVector PlayerLoc = GetActorLocation();

		// Check for Victory Portal entry (within 170 cm of portal center)
		TArray<AActor*> FoundPortals;
		UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("VictoryPortal")), FoundPortals);
		for (AActor* Portal : FoundPortals)
		{
			if (Portal && FVector::Dist(PlayerLoc, Portal->GetActorLocation()) <= 180.0f)
			{
				FDungeonGameState::SetPlayerWon(true);
				if (GetCharacterMovement())
				{
					GetCharacterMovement()->StopMovementImmediately();
				}
				FDungeonGameState::SetPromptText(TEXT("VICTORY ACHIEVED — ESCAPED THE CATACOMBS!"));
				break;
			}
		}

		if (!FDungeonGameState::HasPlayerWon())
		{
			FString DetectedPrompt;

			// Check for nearby Boss Gem pickup (within 240 cm)
			TArray<AActor*> FoundGems;
			UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("BossGem")), FoundGems);
			for (AActor* Gem : FoundGems)
			{
				if (Gem && FVector::Dist(PlayerLoc, Gem->GetActorLocation()) <= 240.0f)
				{
					if (Gem->ActorHasTag(FName(TEXT("Gem_1"))))
					{
						DetectedPrompt = TEXT("[E] COLLECT SUN GEM (RELIC 1/3)");
					}
					else if (Gem->ActorHasTag(FName(TEXT("Gem_2"))))
					{
						DetectedPrompt = TEXT("[E] COLLECT WIND GEM (RELIC 2/3)");
					}
					else if (Gem->ActorHasTag(FName(TEXT("Gem_3"))))
					{
						DetectedPrompt = TEXT("[E] COLLECT VOID GEM (RELIC 3/3)");
					}
					else
					{
						DetectedPrompt = TEXT("[E] COLLECT BOSS GEM");
					}
					break;
				}
			}

			// Check for nearby Exit Altar (within 300 cm)
			if (DetectedPrompt.IsEmpty())
			{
				TArray<AActor*> FoundAltars;
				UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("ExitAltar")), FoundAltars);
				for (AActor* Altar : FoundAltars)
				{
					if (Altar && FVector::Dist(PlayerLoc, Altar->GetActorLocation()) <= 300.0f)
					{
						if (FDungeonGameState::IsAltarActivated())
						{
							DetectedPrompt = TEXT("PORTAL OPENED — STEP THROUGH TO ESCAPE");
						}
						else if (FDungeonGameState::GetGemCount() >= 3)
						{
							DetectedPrompt = TEXT("[E] PLACE 3 BOSS RELICS ON ALTAR");
						}
						else
						{
							DetectedPrompt = FString::Printf(TEXT("ANCIENT ALTAR: REQUIRES 3 BOSS RELICS (%d/3)"), FDungeonGameState::GetGemCount());
						}
						break;
					}
				}
			}

			FDungeonGameState::SetPromptText(DetectedPrompt);
		}
	}
}

bool AEchoesOfTheCatacombCharacter::CanEnterState(EPlayerCombatState NewState) const
{
	if (CombatState == EPlayerCombatState::Dead)
	{
		return false;
	}

	if (CombatState == EPlayerCombatState::GuardBroken)
	{
		return (NewState == EPlayerCombatState::Dead);
	}

	if (CombatState == EPlayerCombatState::Dodging)
	{
		return (NewState == EPlayerCombatState::Dead);
	}

	if (CombatState == EPlayerCombatState::Attacking)
	{
		// Dodge-cancel recovery, hit reaction, combo chaining, or death
		return (NewState == EPlayerCombatState::Dodging ||
				NewState == EPlayerCombatState::HitReaction ||
				NewState == EPlayerCombatState::Dead ||
				NewState == EPlayerCombatState::Attacking);
	}

	if (CombatState == EPlayerCombatState::HitReaction || CombatState == EPlayerCombatState::Staggered)
	{
		return (NewState == EPlayerCombatState::Dead || NewState == EPlayerCombatState::GuardBroken);
	}

	return true;
}

void AEchoesOfTheCatacombCharacter::SetCombatState(EPlayerCombatState NewState)
{
	if (CombatState == NewState) return;
	CombatState = NewState;
}

void AEchoesOfTheCatacombCharacter::UpdateLocomotionState()
{
	if (CombatState == EPlayerCombatState::Dead) return;

	if (bEnableCombatAbilities)
	{
		if (CombatState == EPlayerCombatState::Dodging ||
			CombatState == EPlayerCombatState::Attacking ||
			CombatState == EPlayerCombatState::GuardBroken ||
			CombatState == EPlayerCombatState::HitReaction ||
			CombatState == EPlayerCombatState::Landing)
		{
			return;
		}
	}

	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
	{
		if (GetVelocity().Z > 50.0f)
		{
			SetCombatState(EPlayerCombatState::Jumping);
		}
		else
		{
			SetCombatState(EPlayerCombatState::Falling);
		}
		return;
	}

	if (bEnableCombatAbilities && bIsBlocking)
	{
		SetCombatState(EPlayerCombatState::Blocking);
		return;
	}

	const float Speed2D = GetVelocity().Size2D();
	if (Speed2D < 15.0f)
	{
		SetCombatState(EPlayerCombatState::Idle);
	}
	else if (bIsSprinting)
	{
		SetCombatState(EPlayerCombatState::Sprinting);
	}
	else if (Speed2D > 250.0f)
	{
		SetCombatState(EPlayerCombatState::Running);
	}
	else
	{
		SetCombatState(EPlayerCombatState::Walking);
	}
}

bool AEchoesOfTheCatacombCharacter::CanPerformAction(float StaminaCost) const
{
	return CurrentHealth > 0.0f && CurrentStamina >= StaminaCost;
}

void AEchoesOfTheCatacombCharacter::ConsumeStamina(float Amount)
{
	CurrentStamina = FMath::Max(0.0f, CurrentStamina - Amount);
	bCanRegenStamina = false;
	GetWorldTimerManager().ClearTimer(StaminaRegenDelayHandle);
	GetWorldTimerManager().SetTimer(StaminaRegenDelayHandle, this, &AEchoesOfTheCatacombCharacter::EnableStaminaRegen, 0.45f, false);
}

void AEchoesOfTheCatacombCharacter::EnableStaminaRegen()
{
	bCanRegenStamina = true;
}

void AEchoesOfTheCatacombCharacter::StartSprint()
{
	if (!bEnableCombatAbilities)
	{
		bIsSprinting = true;
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed = 700.0f;
		}
		return;
	}

	if (CurrentHealth <= 0.0f || bIsDodging || CurrentStamina < 10.0f) return;

	bIsSprinting = true;
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = 620.0f; // DS3 Sprint speed
	}
}

void AEchoesOfTheCatacombCharacter::StopSprint()
{
	if (!bEnableCombatAbilities)
	{
		bIsSprinting = false;
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed = 500.0f;
		}
		return;
	}

	bIsSprinting = false;
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = 420.0f; // DS3 Base jog speed
	}

	// 0.30s delay before stamina begins regenerating after sprinting stops
	bCanRegenStamina = false;
	GetWorldTimerManager().ClearTimer(StaminaRegenDelayHandle);
	GetWorldTimerManager().SetTimer(StaminaRegenDelayHandle, this, &AEchoesOfTheCatacombCharacter::EnableStaminaRegen, 0.30f, false);
}

void AEchoesOfTheCatacombCharacter::OnShiftPressed()
{
	if (CurrentHealth <= 0.0f || bIsDodging) return;

	bShiftIsHeld = true;
	StartSprint();
}

void AEchoesOfTheCatacombCharacter::OnShiftHoldStart()
{
	if (bShiftIsHeld && CurrentHealth > 0.0f && !bIsDodging)
	{
		StartSprint();
	}
}

void AEchoesOfTheCatacombCharacter::OnShiftReleased()
{
	bShiftIsHeld = false;
	GetWorldTimerManager().ClearTimer(ShiftHoldTimerHandle);
	StopSprint();
}

void AEchoesOfTheCatacombCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (!bEnableCombatAbilities)
	{
		return;
	}

	if (CombatState != EPlayerCombatState::Dodging && CombatState != EPlayerCombatState::Dead)
	{
		SetCombatState(EPlayerCombatState::Landing);
		if (LandAnim)
		{
			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				AnimInst->PlaySlotAnimationAsDynamicMontage(LandAnim, FName("DefaultSlot"), 0.04f, 0.14f, 1.25f);
			}
		}
		GetWorldTimerManager().ClearTimer(LandingTimerHandle);
		GetWorldTimerManager().SetTimer(LandingTimerHandle, this, &AEchoesOfTheCatacombCharacter::ResetLandingState, 0.20f, false);
	}
}

void AEchoesOfTheCatacombCharacter::ResetLandingState()
{
	if (CombatState == EPlayerCombatState::Landing)
	{
		SetCombatState(EPlayerCombatState::Idle);
	}
}

void AEchoesOfTheCatacombCharacter::StartBlock()
{
	if (!bEnableCombatAbilities) return;
	if (CurrentHealth <= 0.0f || bIsDodging || CombatState == EPlayerCombatState::GuardBroken) return;

	bIsBlocking = true;
	if (CombatState != EPlayerCombatState::Attacking)
	{
		SetCombatState(EPlayerCombatState::Blocking);
	}
	if (bIsSprinting)
	{
		StopSprint();
	}
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = 220.0f; // Souls shield raised walk
	}

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (BlockAnim)
		{
			AnimInst->PlaySlotAnimationAsDynamicMontage(BlockAnim, FName("DefaultSlot"), 0.15f, 0.20f, 1.0f, 1000, 0.0f);
		}
	}
}

void AEchoesOfTheCatacombCharacter::StopBlock()
{
	if (!bEnableCombatAbilities) return;
	bIsBlocking = false;
	if (CombatState == EPlayerCombatState::Blocking)
	{
		SetCombatState(EPlayerCombatState::Idle);
	}
	if (GetCharacterMovement() && !bIsSprinting)
	{
		GetCharacterMovement()->MaxWalkSpeed = 420.0f; // Return to DS3 jog
	}

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (BlockAnim)
		{
			AnimInst->Montage_Stop(0.15f);
		}
	}
}

void AEchoesOfTheCatacombCharacter::OnGuardBreak()
{
	SetCombatState(EPlayerCombatState::GuardBroken);
	StopBlock();
	CurrentStamina = 0.0f;
	bCanRegenStamina = false;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
	}

	// Play heavy guard break stagger (Protector Defense Hit Broken)
	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (bUseAdvancedCombatSystem && ProtectorGuardBreakMontage)
		{
			AnimInst->Montage_Play(ProtectorGuardBreakMontage, 1.10f);
		}
		else if (HitReactAnim)
		{
			AnimInst->PlaySlotAnimationAsDynamicMontage(HitReactAnim, FName("DefaultSlot"), 0.05f, 0.20f, 0.70f);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, TEXT("GUARD BROKEN! STAGGERED (1.2s)"));
	}

	GetWorldTimerManager().ClearTimer(GuardBreakTimerHandle);
	GetWorldTimerManager().SetTimer(GuardBreakTimerHandle, this, &AEchoesOfTheCatacombCharacter::ResetGuardBreak, 1.2f, false);
}

void AEchoesOfTheCatacombCharacter::ResetGuardBreak()
{
	if (CombatState == EPlayerCombatState::GuardBroken)
	{
		SetCombatState(EPlayerCombatState::Idle);
		EnableStaminaRegen();
	}
}

void AEchoesOfTheCatacombCharacter::ResetHitReaction()
{
	if (CombatState == EPlayerCombatState::HitReaction)
	{
		SetCombatState(EPlayerCombatState::Idle);
	}
}

void AEchoesOfTheCatacombCharacter::TriggerHitstop(AActor* HitTarget, float Duration)
{
	CustomTimeDilation = 0.05f;
	if (HitTarget)
	{
		HitTarget->CustomTimeDilation = 0.05f;
		LastHitActor = HitTarget;
	}
	GetWorldTimerManager().SetTimer(HitstopTimerHandle, this, &AEchoesOfTheCatacombCharacter::ResetHitstop, Duration, false);
}

void AEchoesOfTheCatacombCharacter::ResetHitstop()
{
	CustomTimeDilation = 1.0f;
	if (LastHitActor.IsValid())
	{
		LastHitActor->CustomTimeDilation = 1.0f;
		LastHitActor.Reset();
	}
}

void AEchoesOfTheCatacombCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up Enhanced Input action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping - Spacebar ONLY JUMP!
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AEchoesOfTheCatacombCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AEchoesOfTheCatacombCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AEchoesOfTheCatacombCharacter::Look);
	}

	// Direct combat keys: Left Click for Sword Attack combo, Right Click (Hold) for Shield Guard
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::PerformAttack);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::StartBlock);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AEchoesOfTheCatacombCharacter::StopBlock);

	// SHIFT: Purely SPRINT (Hold Shift to Run)
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::OnShiftPressed);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AEchoesOfTheCatacombCharacter::OnShiftReleased);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::OnShiftPressed);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Released, this, &AEchoesOfTheCatacombCharacter::OnShiftReleased);

	// SPACEBAR: Purely JUMP
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::DoJumpStart);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AEchoesOfTheCatacombCharacter::DoJumpEnd);

	// ALT: Purely DODGE ROLL (F removed from dodge as requested)
	PlayerInputComponent->BindKey(EKeys::LeftAlt, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::PerformDodge);
	PlayerInputComponent->BindKey(EKeys::RightAlt, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::PerformDodge);

	// F: ECHOLOCATION SCAN (Detect traps & mobs through walls)
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::OnEcholocationPressed);

	// Lock-On: Middle Mouse Button OR 'Q' key (toggle on/off, auto off on death)
	PlayerInputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::ToggleLockOn);
	PlayerInputComponent->BindKey(EKeys::ThumbMouseButton, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::ToggleLockOn);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::ToggleLockOn);

	// Heavy Attack: R key
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::PerformHeavyAttack);

	// Interact: E key (Pick up Boss Gems, Activate Final Altar)
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AEchoesOfTheCatacombCharacter::OnInteractPressed);
}

void AEchoesOfTheCatacombCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AEchoesOfTheCatacombCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AEchoesOfTheCatacombCharacter::DoMove(float Right, float Forward)
{
	if (CombatState == EPlayerCombatState::Dead) return;

	if (bEnableCombatAbilities)
	{
		// Dark Souls 3 movement restrictions: cannot move if Dodging, Attacking (feet committed), GuardBroken, HitReaction
		if (CombatState == EPlayerCombatState::Dodging ||
			CombatState == EPlayerCombatState::Attacking ||
			CombatState == EPlayerCombatState::GuardBroken ||
			CombatState == EPlayerCombatState::HitReaction ||
			CombatState == EPlayerCombatState::Staggered)
		{
			return;
		}
	}

	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AEchoesOfTheCatacombCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AEchoesOfTheCatacombCharacter::DoJumpStart()
{
	if (!bEnableCombatAbilities)
	{
		Jump();
		return;
	}

	if (!CanEnterState(EPlayerCombatState::Jumping)) return;
	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling()) return;

	if (bIsBlocking)
	{
		StopBlock();
	}

	SetCombatState(EPlayerCombatState::Jumping);
	Jump();

	// If sprinting, apply forward momentum leap!
	if (bIsSprinting && GetVelocity().Size2D() > 280.0f)
	{
		LaunchCharacter(GetActorForwardVector() * 240.0f + FVector(0.0f, 0.0f, 40.0f), false, true);
	}

	// Play Jump Start animation montage
	if (JumpAnim)
	{
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->PlaySlotAnimationAsDynamicMontage(JumpAnim, FName("DefaultSlot"), 0.05f, 0.12f, 1.25f);
		}
	}
}

void AEchoesOfTheCatacombCharacter::DoJumpEnd()
{
	StopJumping();
}

void AEchoesOfTheCatacombCharacter::UpdateCombatGripIdle()
{
	if (!bEnableCombatAbilities || !bUseAdvancedCombatSystem) return;
	if (CurrentHealth <= 0.0f) return;

	UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	const float Speed2D = GetVelocity().Size2D();
	const bool bIsStationary = (Speed2D < 15.0f);
	const bool bInValidIdleState = (!bIsAttacking && !bIsDodging && !bIsBlocking && 
	                               CombatState == EPlayerCombatState::Idle);

	if (bIsStationary && bInValidIdleState)
	{
		if (!bIsGripIdleActive)
		{
			UAnimSequence* GripIdleAnim = ProtectorCommonIdleAnim ? ProtectorCommonIdleAnim : ProtectorIdleAnim;
			if (GripIdleAnim)
			{
				AnimInst->PlaySlotAnimationAsDynamicMontage(GripIdleAnim, FName("DefaultSlot"), 0.20f, 0.20f, 1.0f, 1000, 0.0f);
				bIsGripIdleActive = true;
			}
		}
	}
	else
	{
		if (bIsGripIdleActive)
		{
			AnimInst->Montage_Stop(0.18f);
			bIsGripIdleActive = false;
		}
	}
}

void AEchoesOfTheCatacombCharacter::PerformAttack()
{
	if (!bEnableCombatAbilities) return;
	if (CurrentHealth <= 0.0f || bIsDodging) return;

	// Advanced Melee Combat System combo buffering
	if (bUseAdvancedCombatSystem && bIsAttacking)
	{
		if (bCanQueueAttack)
		{
			bAttackQueued = true;
		}
		return;
	}

	if (!CanEnterState(EPlayerCombatState::Attacking)) return;

	// Motion Warping: Align with locked target and step into striking distance
	if (IsLockingOn() && MotionWarpingComponent)
	{
		MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName(TEXT("Target")),
			GetTranslationWarpTarget(),
			GetRotationWarpTarget()
		);
		MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName(TEXT("FacingTarget")),
			GetTranslationWarpTarget(),
			GetRotationWarpTarget()
		);

		AActor* Locked = GetLockedTarget();
		if (Locked && IsValid(Locked))
		{
			FVector ToLocked = (Locked->GetActorLocation() - GetActorLocation());
			ToLocked.Z = 0.0f;
			const float Dist = ToLocked.Size();
			if (Dist > WarpTargetDistance && Dist < 380.0f)
			{
				LaunchCharacter(ToLocked.GetSafeNormal() * 260.0f, false, false);
			}
		}
	}

	const float Speed = GetVelocity().Size2D();
	const bool bHasMoveInput = !GetLastMovementInputVector().IsNearlyZero();

	// Air / Plunge attack if falling
	if (bUseAdvancedCombatSystem && GetCharacterMovement() && GetCharacterMovement()->IsFalling())
	{
		if (ProtectorAirAttackMontage)
		{
			if (bIsBlocking) StopBlock();
			if (bIsGripIdleActive && GetMesh() && GetMesh()->GetAnimInstance())
			{
				GetMesh()->GetAnimInstance()->Montage_Stop(0.1f);
				bIsGripIdleActive = false;
			}
			SetCombatState(EPlayerCombatState::Attacking);
			bIsAttacking = true;
			bAttackQueued = false;
			bCanQueueAttack = false;

			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				AnimInst->Montage_Play(ProtectorAirAttackMontage, 1.20f);
				const float Duration = ProtectorAirAttackMontage->GetPlayLength() / 1.20f;
				GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &AEchoesOfTheCatacombCharacter::ResetAttackState, Duration, false);
			}
			PendingDamage = AttackDamage * 1.5f;
			PendingKnockback = 380.0f;
			HitActorsThisSwing.Empty();
			GetWorldTimerManager().SetTimer(AttackHitHandle, this, &AEchoesOfTheCatacombCharacter::ExecuteAdvancedAttackHit, 0.25f, false);
			return;
		}
	}

	// Running attack if sprinting
	if (bIsSprinting && Speed > 350.0f && bHasMoveInput)
	{
		PerformRunningAttack();
		return;
	}

	if (bIsBlocking)
	{
		StopBlock();
	}

	if (bIsGripIdleActive && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Stop(0.1f);
		bIsGripIdleActive = false;
	}

	SetCombatState(EPlayerCombatState::Attacking);
	bIsAttacking = true;
	bAttackQueued = false;
	bCanQueueAttack = false;
	bIsHeavyAttackActive = false;
	HitActorsThisSwing.Empty();

	GetCharacterMovement()->StopMovementImmediately();

	if (AController* Ctrl = GetController())
	{
		const FRotator CamRot = Ctrl->GetControlRotation();
		SetActorRotation(FRotator(0.0f, CamRot.Yaw, 0.0f));
	}

	// ADVANCED MELEE COMBAT SYSTEM (Protector 4-Hit Chain)
	if (bUseAdvancedCombatSystem)
	{
		UAnimMontage* MontageToPlay = ProtectorAttack1Montage;
		float CurrentDamage = AttackDamage;
		float KnockbackForce = 220.0f;
		float PlayRate = 1.30f;
		float QueueWindowDelay = 0.20f;

		switch (CurrentComboIndex)
		{
		case 0:
			MontageToPlay = ProtectorAttack1Montage;
			CurrentDamage = AttackDamage;
			KnockbackForce = 220.0f;
			PlayRate = 1.30f;
			QueueWindowDelay = 0.20f;
			break;
		case 1:
			MontageToPlay = ProtectorAttack2Montage ? ProtectorAttack2Montage : ProtectorAttack1Montage;
			CurrentDamage = AttackDamage * 1.15f;
			KnockbackForce = 260.0f;
			PlayRate = 1.30f;
			QueueWindowDelay = 0.18f;
			break;
		case 2:
			MontageToPlay = ProtectorAttack3Montage ? ProtectorAttack3Montage : ProtectorAttack1Montage;
			CurrentDamage = AttackDamage * 1.35f;
			KnockbackForce = 320.0f;
			PlayRate = 1.25f;
			QueueWindowDelay = 0.22f;
			break;
		case 3:
			MontageToPlay = ProtectorAttack4Montage ? ProtectorAttack4Montage : ProtectorAttack1Montage;
			CurrentDamage = AttackDamage * 1.70f;
			KnockbackForce = 480.0f;
			PlayRate = 1.20f;
			QueueWindowDelay = 0.28f;
			break;
		default:
			break;
		}

		CurrentComboIndex = (CurrentComboIndex + 1) % 4;

		GetWorldTimerManager().ClearTimer(ComboResetHandle);
		GetWorldTimerManager().SetTimer(ComboResetHandle, this, &AEchoesOfTheCatacombCharacter::ResetCombo, 0.95f, false);

		float AttackDuration = 0.40f;
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			if (MontageToPlay)
			{
				AnimInst->Montage_Play(MontageToPlay, PlayRate);
				AttackDuration = FMath::Clamp(MontageToPlay->GetPlayLength() / PlayRate, 0.32f, 1.20f);
			}
		}

		PendingDamage = CurrentDamage;
		PendingKnockback = KnockbackForce;

		GetWorldTimerManager().ClearTimer(ComboQueueWindowHandle);
		GetWorldTimerManager().SetTimer(ComboQueueWindowHandle, this, &AEchoesOfTheCatacombCharacter::EnableComboQueueWindow, QueueWindowDelay, false);

		GetWorldTimerManager().ClearTimer(AttackHitHandle);
		GetWorldTimerManager().SetTimer(AttackHitHandle, this, &AEchoesOfTheCatacombCharacter::ExecuteAdvancedAttackHit, 0.18f, false);

		GetWorldTimerManager().ClearTimer(AttackCooldownHandle);
		GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &AEchoesOfTheCatacombCharacter::ResetAttackState, AttackDuration, false);
		return;
	}

	// LEGACY FALLBACK SYSTEM (DS3 3-Hit Sequence)
	UAnimSequence* AnimToPlay = Attack1Anim;
	float CurrentDamage = AttackDamage;
	float KnockbackForce = 240.0f;
	float AttackDuration = 0.38f;
	float DamageDelay = 0.20f;
	float PlayRate = 1.30f;

	switch (CurrentComboIndex)
	{
	case 0:
		AnimToPlay = Attack1Anim;
		CurrentDamage = AttackDamage;
		KnockbackForce = 240.0f;
		DamageDelay = 0.20f;
		PlayRate = 1.30f;
		break;
	case 1:
		AnimToPlay = Attack2Anim ? Attack2Anim : Attack1Anim;
		CurrentDamage = AttackDamage * 1.25f;
		KnockbackForce = 300.0f;
		DamageDelay = 0.20f;
		PlayRate = 1.30f;
		break;
	case 2:
		AnimToPlay = Attack3Anim ? Attack3Anim : Attack1Anim;
		CurrentDamage = AttackDamage * 1.65f;
		KnockbackForce = 480.0f;
		DamageDelay = 0.24f;
		PlayRate = 1.20f;
		break;
	default:
		break;
	}

	CurrentComboIndex = (CurrentComboIndex + 1) % 3;

	GetWorldTimerManager().ClearTimer(ComboResetHandle);
	GetWorldTimerManager().SetTimer(ComboResetHandle, this, &AEchoesOfTheCatacombCharacter::ResetCombo, 0.85f, false);

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (AnimToPlay)
		{
			UAnimMontage* Montage = AnimInst->PlaySlotAnimationAsDynamicMontage(AnimToPlay, FName("DefaultSlot"), 0.06f, 0.10f, PlayRate);
			if (Montage)
			{
				Montage->bEnableRootMotionTranslation = false;
				Montage->bEnableRootMotionRotation = false;
				AttackDuration = FMath::Clamp(Montage->GetPlayLength() / PlayRate, 0.35f, 1.20f);
			}
		}
	}

	PendingDamage = CurrentDamage;
	PendingKnockback = KnockbackForce;
	GetWorldTimerManager().SetTimer(AttackHitHandle, this, &AEchoesOfTheCatacombCharacter::ExecuteAttackHit, DamageDelay, false);
	GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &AEchoesOfTheCatacombCharacter::ResetAttackState, AttackDuration, false);
}

void AEchoesOfTheCatacombCharacter::PerformRunningAttack()
{
	if (!bEnableCombatAbilities) return;
	if (!CanEnterState(EPlayerCombatState::Attacking)) return;

	// Motion Warping: Align with locked target and step into striking distance
	if (IsLockingOn() && MotionWarpingComponent)
	{
		MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName(TEXT("Target")),
			GetTranslationWarpTarget(),
			GetRotationWarpTarget()
		);
		MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName(TEXT("FacingTarget")),
			GetTranslationWarpTarget(),
			GetRotationWarpTarget()
		);

		AActor* Locked = GetLockedTarget();
		if (Locked && IsValid(Locked))
		{
			FVector ToLocked = (Locked->GetActorLocation() - GetActorLocation());
			ToLocked.Z = 0.0f;
			const float Dist = ToLocked.Size();
			if (Dist > WarpTargetDistance && Dist < 380.0f)
			{
				LaunchCharacter(ToLocked.GetSafeNormal() * 260.0f, false, false);
			}
		}
	}
	if (bIsAttacking || bIsDodging || CurrentHealth <= 0.0f) return;

	if (bIsBlocking) StopBlock();
	if (bIsGripIdleActive && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Stop(0.1f);
		bIsGripIdleActive = false;
	}

	SetCombatState(EPlayerCombatState::Attacking);
	bIsAttacking = true;
	bAttackQueued = false;
	bCanQueueAttack = false;
	bIsHeavyAttackActive = false;
	HitActorsThisSwing.Empty();
	CurrentComboIndex = 0;

	if (AController* Ctrl = GetController())
	{
		const FRotator CamRot = Ctrl->GetControlRotation();
		SetActorRotation(FRotator(0.0f, CamRot.Yaw, 0.0f));
	}

	LaunchCharacter(GetActorForwardVector() * 380.0f, false, false);

	if (bUseAdvancedCombatSystem && ProtectorSprintAttackMontage)
	{
		const float PlayRate = 1.25f;
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInst->Montage_Play(ProtectorSprintAttackMontage, PlayRate);
		}
		PendingDamage = AttackDamage * 1.45f;
		PendingKnockback = 460.0f;
		const float Duration = FMath::Clamp(ProtectorSprintAttackMontage->GetPlayLength() / PlayRate, 0.35f, 0.55f);
		GetWorldTimerManager().SetTimer(AttackHitHandle, this, &AEchoesOfTheCatacombCharacter::ExecuteAdvancedAttackHit, 0.20f, false);
		GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &AEchoesOfTheCatacombCharacter::ResetAttackState, Duration, false);
		return;
	}

	UAnimSequence* AnimToPlay = RunningAttackAnim ? RunningAttackAnim : Attack1Anim;
	float CurrentDamage = AttackDamage * 1.4f;
	float KnockbackForce = 450.0f;
	float AttackDuration = 0.45f;
	float DamageDelay = 0.24f;
	float PlayRate = 1.25f;

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (AnimToPlay)
		{
			UAnimMontage* Montage = AnimInst->PlaySlotAnimationAsDynamicMontage(AnimToPlay, FName("DefaultSlot"), 0.06f, 0.10f, PlayRate);
			if (Montage)
			{
				Montage->bEnableRootMotionTranslation = false;
				Montage->bEnableRootMotionRotation = false;
				AttackDuration = FMath::Clamp(Montage->GetPlayLength() / PlayRate, 0.35f, 0.55f);
			}
		}
	}

	PendingDamage = CurrentDamage;
	PendingKnockback = KnockbackForce;
	GetWorldTimerManager().SetTimer(AttackHitHandle, this, &AEchoesOfTheCatacombCharacter::ExecuteAttackHit, DamageDelay, false);
	GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &AEchoesOfTheCatacombCharacter::ResetAttackState, AttackDuration, false);
}

void AEchoesOfTheCatacombCharacter::PerformHeavyAttack()
{
	if (!bEnableCombatAbilities) return;
	if (CurrentHealth <= 0.0f || bIsDodging) return;

	if (bUseAdvancedCombatSystem && bIsAttacking)
	{
		if (bCanQueueAttack)
		{
			bAttackQueued = true;
		}
		return;
	}

	if (!CanEnterState(EPlayerCombatState::Attacking)) return;

	// Motion Warping: Align with locked target and step into striking distance
	if (IsLockingOn() && MotionWarpingComponent)
	{
		MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName(TEXT("Target")),
			GetTranslationWarpTarget(),
			GetRotationWarpTarget()
		);
		MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName(TEXT("FacingTarget")),
			GetTranslationWarpTarget(),
			GetRotationWarpTarget()
		);

		AActor* Locked = GetLockedTarget();
		if (Locked && IsValid(Locked))
		{
			FVector ToLocked = (Locked->GetActorLocation() - GetActorLocation());
			ToLocked.Z = 0.0f;
			const float Dist = ToLocked.Size();
			if (Dist > WarpTargetDistance && Dist < 380.0f)
			{
				LaunchCharacter(ToLocked.GetSafeNormal() * 260.0f, false, false);
			}
		}
	}

	if (bIsBlocking) StopBlock();
	if (bIsGripIdleActive && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Stop(0.1f);
		bIsGripIdleActive = false;
	}

	SetCombatState(EPlayerCombatState::Attacking);
	bIsAttacking = true;
	bAttackQueued = false;
	bCanQueueAttack = false;
	bIsHeavyAttackActive = true;
	HitActorsThisSwing.Empty();
	CurrentComboIndex = 0;

	GetCharacterMovement()->StopMovementImmediately();

	if (AController* Ctrl = GetController())
	{
		const FRotator CamRot = Ctrl->GetControlRotation();
		SetActorRotation(FRotator(0.0f, CamRot.Yaw, 0.0f));
	}

	LaunchCharacter(GetActorForwardVector() * 220.0f, false, false);

	if (bUseAdvancedCombatSystem)
	{
		UAnimMontage* HeavyMontage = ProtectorHeavy1Montage;
		float DmgMultiplier = 1.75f;
		float Knockback = 520.0f;
		float PlayRate = 1.15f;

		switch (HeavyComboIndex)
		{
		case 0:
			HeavyMontage = ProtectorHeavy1Montage;
			DmgMultiplier = 1.75f;
			Knockback = 520.0f;
			break;
		case 1:
			HeavyMontage = ProtectorHeavy2Montage ? ProtectorHeavy2Montage : ProtectorHeavy1Montage;
			DmgMultiplier = 2.05f;
			Knockback = 600.0f;
			break;
		case 2:
			HeavyMontage = ProtectorHeavy3Montage ? ProtectorHeavy3Montage : ProtectorHeavy1Montage;
			DmgMultiplier = 2.50f;
			Knockback = 750.0f;
			break;
		default:
			break;
		}

		HeavyComboIndex = (HeavyComboIndex + 1) % 3;

		float AttackDuration = 0.55f;
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			if (HeavyMontage)
			{
				AnimInst->Montage_Play(HeavyMontage, PlayRate);
				AttackDuration = FMath::Clamp(HeavyMontage->GetPlayLength() / PlayRate, 0.40f, 0.85f);
			}
		}

		PendingDamage = AttackDamage * DmgMultiplier;
		PendingKnockback = Knockback;

		GetWorldTimerManager().ClearTimer(ComboQueueWindowHandle);
		GetWorldTimerManager().SetTimer(ComboQueueWindowHandle, this, &AEchoesOfTheCatacombCharacter::EnableComboQueueWindow, 0.28f, false);

		GetWorldTimerManager().ClearTimer(AttackHitHandle);
		GetWorldTimerManager().SetTimer(AttackHitHandle, this, &AEchoesOfTheCatacombCharacter::ExecuteAdvancedAttackHit, 0.24f, false);

		GetWorldTimerManager().ClearTimer(AttackCooldownHandle);
		GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &AEchoesOfTheCatacombCharacter::ResetAttackState, AttackDuration, false);
		return;
	}

	UAnimSequence* AnimToPlay = HeavyAttackAnim ? HeavyAttackAnim : Attack1Anim;
	float CurrentDamage = AttackDamage * 1.90f;
	float KnockbackForce = 580.0f;
	float AttackDuration = 0.58f;
	float DamageDelay = 0.30f;
	float PlayRate = 1.10f;

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (AnimToPlay)
		{
			UAnimMontage* Montage = AnimInst->PlaySlotAnimationAsDynamicMontage(AnimToPlay, FName("DefaultSlot"), 0.08f, 0.12f, PlayRate);
			if (Montage)
			{
				Montage->bEnableRootMotionTranslation = false;
				Montage->bEnableRootMotionRotation = false;
				AttackDuration = FMath::Clamp(Montage->GetPlayLength() / PlayRate, 0.40f, 0.65f);
			}
		}
	}

	PendingDamage = CurrentDamage;
	PendingKnockback = KnockbackForce;
	GetWorldTimerManager().SetTimer(AttackHitHandle, this, &AEchoesOfTheCatacombCharacter::ExecuteAttackHit, DamageDelay, false);
	GetWorldTimerManager().SetTimer(AttackCooldownHandle, this, &AEchoesOfTheCatacombCharacter::ResetAttackState, AttackDuration, false);
}

void AEchoesOfTheCatacombCharacter::EnableComboQueueWindow()
{
	bCanQueueAttack = true;
}

void AEchoesOfTheCatacombCharacter::TryChainQueuedAttack()
{
	if (bAttackQueued)
	{
		bAttackQueued = false;
		bCanQueueAttack = false;
		bIsAttacking = false;
		if (bIsHeavyAttackActive)
		{
			PerformHeavyAttack();
		}
		else
		{
			PerformAttack();
		}
	}
	else
	{
		bIsAttacking = false;
		bCanQueueAttack = false;
		if (CombatState == EPlayerCombatState::Attacking)
		{
			SetCombatState(EPlayerCombatState::Idle);
		}
	}
}

void AEchoesOfTheCatacombCharacter::ExecuteAdvancedAttackHit()
{
	if (CurrentHealth <= 0.0f || !IsValid(this)) return;

	// Generous Souls forward sphere sweep starting from slightly behind character to 280cm forward
	// with a 110cm radius sphere - covers point blank, sides, and extended sword reach 100% reliably
	const FVector ForwardDir = GetActorForwardVector();
	const FVector StartTrace = GetActorLocation() - (ForwardDir * 40.0f) + FVector(0.0f, 0.0f, 20.0f);
	const FVector EndTrace = GetActorLocation() + (ForwardDir * 280.0f) + FVector(0.0f, 0.0f, 20.0f);

	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	const bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults, StartTrace, EndTrace, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(110.0f), QueryParams);

	bool bConnectedWithEnemy = false;
	AActor* FirstHitTarget = nullptr;

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && HitActor != this && !HitActorsThisSwing.Contains(HitActor))
			{
				HitActorsThisSwing.Add(HitActor);
				if (!FirstHitTarget) FirstHitTarget = HitActor;

				if (ICombatDamageable* DamageableTarget = Cast<ICombatDamageable>(HitActor))
				{
					DamageableTarget->ApplyDamage(PendingDamage, this, Hit.ImpactPoint, GetActorForwardVector() * PendingKnockback);
				}
				else
				{
					UGameplayStatics::ApplyDamage(HitActor, PendingDamage, GetController(), this, UDamageType::StaticClass());
					if (ACharacter* EnemyChar = Cast<ACharacter>(HitActor))
					{
						EnemyChar->LaunchCharacter(GetActorForwardVector() * PendingKnockback + FVector(0.0f, 0.0f, 90.0f), false, false);
					}
				}

				if (HitActor->Implements<UHitInterface>())
				{
					IHitInterface::Execute_GetHit(HitActor, Hit.ImpactPoint, this);
				}
				bConnectedWithEnemy = true;
			}
		}

		if (bConnectedWithEnemy && FirstHitTarget)
		{
			TriggerHitstop(FirstHitTarget, 0.06f);
		}
	}
}

void AEchoesOfTheCatacombCharacter::PlayDirectionalHitReaction(const FVector& HitDirection)
{
	UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst) return;

	SetCombatState(EPlayerCombatState::HitReaction);
	if (bIsGripIdleActive)
	{
		AnimInst->Montage_Stop(0.1f);
		bIsGripIdleActive = false;
	}

	UAnimMontage* MontageToPlay = ProtectorHitFMontage;

	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = GetActorRightVector().GetSafeNormal2D();
	const FVector InDir = HitDirection.GetSafeNormal2D();

	const float ForwardDot = FVector::DotProduct(Forward, InDir);
	const float RightDot = FVector::DotProduct(Right, InDir);

	if (ForwardDot >= 0.5f)
	{
		MontageToPlay = ProtectorHitFMontage ? ProtectorHitFMontage : ProtectorHitBMontage;
	}
	else if (ForwardDot <= -0.5f)
	{
		MontageToPlay = ProtectorHitBMontage ? ProtectorHitBMontage : ProtectorHitFMontage;
	}
	else if (RightDot > 0.0f)
	{
		MontageToPlay = ProtectorHitRMontage ? ProtectorHitRMontage : ProtectorHitFMontage;
	}
	else
	{
		MontageToPlay = ProtectorHitLMontage ? ProtectorHitLMontage : ProtectorHitFMontage;
	}

	if (MontageToPlay)
	{
		AnimInst->Montage_Play(MontageToPlay, 1.25f);
	}
	else if (HitReactAnim)
	{
		AnimInst->PlaySlotAnimationAsDynamicMontage(HitReactAnim, FName("DefaultSlot"), 0.05f, 0.1f, 1.5f);
	}

	GetWorldTimerManager().ClearTimer(HitReactionTimerHandle);
	GetWorldTimerManager().SetTimer(HitReactionTimerHandle, this, &AEchoesOfTheCatacombCharacter::ResetHitReaction, 0.35f, false);
}

void AEchoesOfTheCatacombCharacter::ExecuteAttackHit()
{
	if (CurrentHealth <= 0.0f || !IsValid(this)) return;

	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
	const FVector End = Start + (GetActorForwardVector() * AttackRange);

	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	const bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(75.0f), QueryParams);

	TSet<AActor*> HitActors;
	if (bHit)
	{
		bool bConnectedWithEnemy = false;
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && HitActor != this && !HitActors.Contains(HitActor))
			{
				HitActors.Add(HitActor);
				if (ICombatDamageable* DamageableTarget = Cast<ICombatDamageable>(HitActor))
				{
					DamageableTarget->ApplyDamage(PendingDamage, this, Hit.ImpactPoint, GetActorForwardVector() * PendingKnockback);
				}
				else
				{
					UGameplayStatics::ApplyDamage(HitActor, PendingDamage, GetController(), this, UDamageType::StaticClass());
					if (ACharacter* EnemyChar = Cast<ACharacter>(HitActor))
					{
						EnemyChar->LaunchCharacter(GetActorForwardVector() * PendingKnockback + FVector(0.0f, 0.0f, 90.0f), false, false);
					}
				}

				if (HitActor->Implements<UHitInterface>())
				{
					IHitInterface::Execute_GetHit(HitActor, Hit.ImpactPoint, this);
				}
				bConnectedWithEnemy = true;
			}
		}

		if (bConnectedWithEnemy && HitActors.Num() > 0)
		{
			TriggerHitstop(*HitActors.CreateIterator(), 0.05f);
		}
	}
}

void AEchoesOfTheCatacombCharacter::PerformDodge()
{
	if (!bEnableCombatAbilities) return;
	if (!CanEnterState(EPlayerCombatState::Dodging)) return;
	if (!CanPerformAction(20.0f)) return;

	if (bIsAttacking)
	{
		bIsAttacking = false;
		bAttackQueued = false;
		bCanQueueAttack = false;
		GetWorldTimerManager().ClearTimer(AttackCooldownHandle);
		GetWorldTimerManager().ClearTimer(AttackHitHandle);
		GetWorldTimerManager().ClearTimer(ComboQueueWindowHandle);
	}

	if (bIsSprinting) StopSprint();
	if (bIsBlocking) StopBlock();
	if (bIsGripIdleActive && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Stop(0.08f);
		bIsGripIdleActive = false;
	}

	SetCombatState(EPlayerCombatState::Dodging);
	bIsDodging = true;
	bIsInvulnerable = true;
	ConsumeStamina(20.0f);

	GetWorldTimerManager().ClearTimer(IFrameTimerHandle);
	GetWorldTimerManager().SetTimer(IFrameTimerHandle, [this]()
	{
		bIsInvulnerable = false;
	}, 0.30f, false);

	const float DodgeRecoveryTime = (bUseAdvancedCombatSystem) ? 0.65f : 0.80f;
	GetWorldTimerManager().ClearTimer(DodgeDurationHandle);
	GetWorldTimerManager().SetTimer(DodgeDurationHandle, this, &AEchoesOfTheCatacombCharacter::ResetDodgeState, DodgeRecoveryTime, false);

	bCanRegenStamina = false;
	GetWorldTimerManager().ClearTimer(StaminaRegenDelayHandle);
	GetWorldTimerManager().SetTimer(StaminaRegenDelayHandle, this, &AEchoesOfTheCatacombCharacter::EnableStaminaRegen, 0.35f, false);

	const FVector MoveInput = GetLastMovementInputVector();
	FVector DodgeDir;
	bool bIsBackstep = false;

	if (!MoveInput.IsNearlyZero(0.1f))
	{
		DodgeDir = MoveInput.GetSafeNormal();
		SetActorRotation(DodgeDir.Rotation());
	}
	else
	{
		bIsBackstep = true;
		if (AController* Ctrl = GetController())
		{
			const FRotator CamRot = Ctrl->GetControlRotation();
			const FRotator YawRot(0.0f, CamRot.Yaw, 0.0f);
			DodgeDir = -FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		}
		else
		{
			DodgeDir = -GetActorForwardVector();
		}
	}

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (bUseAdvancedCombatSystem && ProtectorDodgeMontage)
		{
			AnimInst->Montage_Play(ProtectorDodgeMontage, 1.25f);
		}
		else if (DodgeAnim)
		{
			AnimInst->PlaySlotAnimationAsDynamicMontage(DodgeAnim, FName("DefaultSlot"), 0.06f, 0.10f, 1.45f);
		}
	}

	const float LaunchImpulse = bIsBackstep ? 560.0f : 860.0f;
	LaunchCharacter(DodgeDir * LaunchImpulse + FVector(0.0f, 0.0f, 40.0f), true, true);
}

void AEchoesOfTheCatacombCharacter::ResetAttackState()
{
	if (bUseAdvancedCombatSystem && bAttackQueued)
	{
		TryChainQueuedAttack();
		return;
	}

	bIsAttacking = false;
	bAttackQueued = false;
	bCanQueueAttack = false;
	bIsHeavyAttackActive = false;
	if (CombatState == EPlayerCombatState::Attacking)
	{
		SetCombatState(EPlayerCombatState::Idle);
	}
}

void AEchoesOfTheCatacombCharacter::ResetDodgeState()
{
	bIsDodging = false;
	bIsInvulnerable = false;
	if (CombatState == EPlayerCombatState::Dodging)
	{
		SetCombatState(EPlayerCombatState::Idle);
	}
}

void AEchoesOfTheCatacombCharacter::ResetCombo()
{
	CurrentComboIndex = 0;
	HeavyComboIndex = 0;
	bAttackQueued = false;
	bCanQueueAttack = false;
}

void AEchoesOfTheCatacombCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	FDamageEvent DmgEvent;
	TakeDamage(Damage, DmgEvent, DamageCauser ? DamageCauser->GetInstigatorController() : nullptr, DamageCauser);
}

float AEchoesOfTheCatacombCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
                                              AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	if (bEnableCombatAbilities)
	{
		// 1. Invulnerability frames while dodge rolling! (DS3 14 i-frames)
		if (bIsDodging || bIsInvulnerable)
		{
			return 0.0f;
		}

		// 2. Shield Block Defense (Right Click Hold) - blocks 100% of any incoming attack
		if (bIsBlocking)
		{
			TriggerHitstop(DamageCauser, 0.06f);

			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				if (bUseAdvancedCombatSystem && ProtectorBlockHitMontage)
				{
					AnimInst->Montage_Play(ProtectorBlockHitMontage, 1.15f);
				}
			}

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("SHIELD BLOCKED! 0 Damage Taken"));
			}

			return 0.0f;
		}
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
	else if (!bIsAttacking && CombatState != EPlayerCombatState::GuardBroken)
	{
		if (bUseAdvancedCombatSystem)
		{
			AActor* Attacker = DamageCauser ? DamageCauser : (EventInstigator ? EventInstigator->GetPawn() : nullptr);
			const FVector HitDir = Attacker ? (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal() : -GetActorForwardVector();
			PlayDirectionalHitReaction(HitDir);
		}
		else
		{
			SetCombatState(EPlayerCombatState::HitReaction);
			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				if (HitReactAnim)
				{
					AnimInst->PlaySlotAnimationAsDynamicMontage(HitReactAnim, FName("DefaultSlot"), 0.05f, 0.1f, 1.5f);
				}
			}
			GetWorldTimerManager().ClearTimer(HitReactionTimerHandle);
			GetWorldTimerManager().SetTimer(HitReactionTimerHandle, this, &AEchoesOfTheCatacombCharacter::ResetHitReaction, 0.35f, false);
		}
	}

	return DamageAmount;
}

void AEchoesOfTheCatacombCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	if (DungeonHUDWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(DungeonHUDWidget.ToSharedRef());
		DungeonHUDWidget.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AEchoesOfTheCatacombCharacter::HandleDeath()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (bIsGripIdleActive)
		{
			AnimInst->Montage_Stop(0.1f);
			bIsGripIdleActive = false;
		}

		if (bUseAdvancedCombatSystem && ProtectorDieMontage)
		{
			AnimInst->Montage_Play(ProtectorDieMontage, 1.0f);
		}
		else if (DeathAnim)
		{
			AnimInst->PlaySlotAnimationAsDynamicMontage(DeathAnim, FName("DefaultSlot"), 0.1f, 0.2f, 1.0f);
		}
	}

	if (GEngine)
	{
	}

	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AEchoesOfTheCatacombCharacter::ReloadCurrentLevel, 1.8f, false);
}

void AEchoesOfTheCatacombCharacter::ReloadCurrentLevel()
{
	UWorld* World = GetWorld();
	if (!World) return;

	ADungeonGenerator* DungeonGen = nullptr;
	TArray<AActor*> FoundGenerators;
	UGameplayStatics::GetAllActorsOfClass(World, ADungeonGenerator::StaticClass(), FoundGenerators);
	if (FoundGenerators.Num() > 0)
	{
		DungeonGen = Cast<ADungeonGenerator>(FoundGenerators[0]);
	}

	// Determine checkpoint respawn location:
	// 1. If died before or in Boss Room 1 -> Spawn where game started
	// 2. If died before or in Boss Room 2 (Boss 1 cleared) -> Spawn at exit of Boss Room 1
	// 3. If died before or in Boss Room 3 (Boss 2 cleared) -> Spawn at exit of Boss Room 2
	FVector SpawnLoc(8020.f, 5320.f, 122.f);
	FRotator SpawnRot = FRotator::ZeroRotator;

	if (DungeonGen && DungeonGen->bBoss2Defeated)
	{
		// Checkpoint 3: Exit of Boss Room 2
		SpawnLoc = DungeonGen->Boss2ExitDoorLocation + FVector(200.0f, 0.0f, 115.0f);
		SpawnRot = FRotator(0.0f, 0.0f, 0.0f); // Face East
	}
	else if (DungeonGen && DungeonGen->bBoss1Defeated)
	{
		// Checkpoint 2: Exit of Boss Room 1
		SpawnLoc = DungeonGen->Boss1ExitDoorLocation + FVector(200.0f, 0.0f, 115.0f);
		SpawnRot = FRotator(0.0f, 0.0f, 0.0f); // Face East
	}
	else
	{
		// Checkpoint 1: Initial game start inside Sector 1 maze
		if (DungeonGen && !DungeonGen->PlayerSpawnLocation.IsZero())
		{
			SpawnLoc = DungeonGen->PlayerSpawnLocation + FVector(0.0f, 0.0f, 50.0f);
			SpawnRot = FRotator(0.0f, 90.0f, 0.0f);
		}
		else if (g_HasEchoesInitialSpawnTransform)
		{
			SpawnLoc = g_EchoesInitialSpawnTransform.GetLocation();
			SpawnRot = g_EchoesInitialSpawnTransform.GetRotation().Rotator();
		}
		else if (AActor* PlayerStart = UGameplayStatics::GetActorOfClass(World, APlayerStart::StaticClass()))
		{
			SpawnLoc = PlayerStart->GetActorLocation();
			SpawnRot = PlayerStart->GetActorRotation();
		}
	}

	SpawnLoc.Z += 5.0f;

	TeleportTo(SpawnLoc, SpawnRot, false, true);

	if (AController* C = GetController())
	{
		C->SetControlRotation(SpawnRot);
	}

	CurrentHealth = MaxHealth;
	CurrentStamina = MaxStamina;
	CombatState = EPlayerCombatState::Idle;
	ClearLockOn();

	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInst->StopAllMontages(0.1f);
	}

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		GetCharacterMovement()->StopMovementImmediately();
	}
}

void AEchoesOfTheCatacombCharacter::OnInteractPressed()
{
	if (CombatState == EPlayerCombatState::Dead || FDungeonGameState::HasPlayerWon())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector PlayerLoc = GetActorLocation();

	// 1. Check for nearby Boss Gem pickup (within 260 cm)
	TArray<AActor*> FoundGems;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("BossGem")), FoundGems);
	AActor* ClosestGem = nullptr;
	float MinDist = 260.0f;

	for (AActor* Gem : FoundGems)
	{
		if (Gem && IsValid(Gem))
		{
			const float Dist = FVector::Dist(PlayerLoc, Gem->GetActorLocation());
			if (Dist < MinDist)
			{
				MinDist = Dist;
				ClosestGem = Gem;
			}
		}
	}

	if (ClosestGem)
	{
		int32 GemId = 1;
		if (ClosestGem->ActorHasTag(FName(TEXT("Gem_1")))) GemId = 1;
		else if (ClosestGem->ActorHasTag(FName(TEXT("Gem_2")))) GemId = 2;
		else if (ClosestGem->ActorHasTag(FName(TEXT("Gem_3")))) GemId = 3;

		FDungeonGameState::CollectGem(GemId);

		// Play collection sound
		USoundBase* PickupSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SoulPickup.SoulPickup"));
		if (PickupSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, PickupSound, ClosestGem->GetActorLocation());
		}

		ClosestGem->Destroy();
		FDungeonGameState::SetPromptText(FString::Printf(TEXT("RELIC ACQUIRED (%d/3)"), FDungeonGameState::GetGemCount()));
		return;
	}

	// 2. Check for nearby Exit Altar (within 320 cm)
	TArray<AActor*> FoundAltars;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("ExitAltar")), FoundAltars);
	for (AActor* Altar : FoundAltars)
	{
		if (Altar && IsValid(Altar) && FVector::Dist(PlayerLoc, Altar->GetActorLocation()) <= 320.0f)
		{
			if (FDungeonGameState::GetGemCount() >= 3)
			{
				// Activate the altar and spawn the portal!
				TArray<AActor*> FoundGens;
				UGameplayStatics::GetAllActorsOfClass(World, ADungeonGenerator::StaticClass(), FoundGens);
				if (FoundGens.Num() > 0)
				{
					if (ADungeonGenerator* Gen = Cast<ADungeonGenerator>(FoundGens[0]))
					{
						Gen->ActivateExitAltar();
					}
				}
				FDungeonGameState::SetPromptText(TEXT("PORTAL OPENED — ENTER TO ESCAPE!"));
			}
			else
			{
				FDungeonGameState::SetPromptText(FString::Printf(TEXT("ALTAR REQUIRES ALL 3 RELICS (%d/3)"), FDungeonGameState::GetGemCount()));
			}
			return;
		}
	}
}

void AEchoesOfTheCatacombCharacter::OnEcholocationPressed()
{
	if (CombatState == EPlayerCombatState::Dead)
	{
		return;
	}

	UEchoPulseComponent* PulseComp = FindComponentByClass<UEchoPulseComponent>();
	if (PulseComp)
	{
		PulseComp->TriggerEcholocationScan();
	}
	else if (UWorld* World = GetWorld())
	{
		for (TObjectIterator<UEchoPulseComponent> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				It->TriggerEcholocationScan();
				break;
			}
		}
	}
}

void AEchoesOfTheCatacombCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	CurrentHealth = FMath::Clamp(CurrentHealth + Healing, 0.0f, MaxHealth);
}

namespace
{
	static TWeakObjectPtr<AActor> g_EchoesLockedTarget = nullptr;

	static bool IsTargetAliveEchoes(const AActor* Target)
	{
		if (!IsValid(Target) || Target->IsHidden())
		{
			return false;
		}

		static const FName DeadNames[] = { TEXT("Dead"), TEXT("bIsDead"), TEXT("bDead") };
		for (const FName& Name : DeadNames)
		{
			if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Target->GetClass()->FindPropertyByName(Name)))
			{
				if (BoolProp->GetPropertyValue_InContainer(Target))
				{
					return false;
				}
			}
		}

		static const FName HealthNames[] = { TEXT("CurrentHealth"), TEXT("Health"), TEXT("CurrentHP"), TEXT("HP") };
		for (const FName& Name : HealthNames)
		{
			if (FProperty* Prop = Target->GetClass()->FindPropertyByName(Name))
			{
				if (FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
				{
					const void* ValuePtr = NumProp->ContainerPtrToValuePtr<void>(Target);
					const double HealthVal = NumProp->GetFloatingPointPropertyValue(ValuePtr);
					if (HealthVal <= 0.0)
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	static FVector GetTargetAimLocationEchoes(const AActor* Target)
	{
		if (!IsValid(Target))
		{
			return FVector::ZeroVector;
		}

		if (const ACharacter* TargetChar = Cast<ACharacter>(Target))
		{
			if (const USkeletalMeshComponent* TargetMesh = TargetChar->GetMesh())
			{
				static const FName ChestSockets[] = {
					FName(TEXT("spine_03")), FName(TEXT("spine_02")), FName(TEXT("spine_01")),
					FName(TEXT("spine")), FName(TEXT("head")), FName(TEXT("pelvis"))
				};
				for (const FName& Socket : ChestSockets)
				{
					if (TargetMesh->DoesSocketExist(Socket))
					{
						return TargetMesh->GetSocketLocation(Socket);
					}
				}
			}
		}

		return Target->GetActorLocation() + FVector(0.f, 0.f, 45.f);
	}
}

bool AEchoesOfTheCatacombCharacter::IsLockingOn() const
{
	return g_EchoesLockedTarget.IsValid();
}

AActor* AEchoesOfTheCatacombCharacter::GetLockedTarget() const
{
	return g_EchoesLockedTarget.Get();
}

void AEchoesOfTheCatacombCharacter::ClearLockOn()
{
	g_EchoesLockedTarget = nullptr;
}

void AEchoesOfTheCatacombCharacter::ToggleLockOn()
{
	if (IsLockingOn())
	{
		ClearLockOn();
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !FollowCamera) return;

	const FVector PlayerLoc = GetActorLocation();
	const FVector CameraFwd = FollowCamera->GetForwardVector().GetSafeNormal2D();
	const FVector CameraLoc = FollowCamera->GetComponentLocation();
	const float SearchRadius = 3800.0f;

	AActor* BestTarget = nullptr;
	float BestScore = MAX_FLT;

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Char = *It;
		if (!Char || Char == this || !IsValid(Char)) continue;
		if (Char->ActorHasTag(FName(TEXT("Player")))) continue;

		// 1. Must be an enemy or boss
		const bool bIsEnemyOrBoss = Char->ActorHasTag(FName(TEXT("Boss")))
			|| Char->ActorHasTag(FName(TEXT("Enemy")))
			|| Char->GetClass()->GetName().Contains(TEXT("Boss"))
			|| Char->GetClass()->GetName().Contains(TEXT("Enemy"));
		if (!bIsEnemyOrBoss) continue;

		// 2. Alive check
		if (!IsTargetAliveEchoes(Char)) continue;

		// 3. Distance check
		const FVector ToTarget = Char->GetActorLocation() - PlayerLoc;
		const float Dist = ToTarget.Size2D();
		if (Dist > SearchRadius) continue;

		// 4. Forward cone check
		const FVector ToTargetDir = ToTarget.GetSafeNormal2D();
		const float Dot = FVector::DotProduct(CameraFwd, ToTargetDir);
		if (Dot < 0.15f) continue;

		// 5. Score candidate
		const float Score = (1.0f - Dot) * 2000.0f + Dist;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Char;
		}
	}

	if (BestTarget)
	{
		g_EchoesLockedTarget = BestTarget;
	}
}

void AEchoesOfTheCatacombCharacter::UpdateLockOn(float DeltaTime)
{
	if (!IsLockingOn()) return;

	AActor* Target = g_EchoesLockedTarget.Get();
	if (!Target || !IsValid(Target))
	{
		ClearLockOn();
		return;
	}

	// Auto lock-off if target dies
	if (!IsTargetAliveEchoes(Target) || FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > 4500.0f)
	{
		ClearLockOn();
		return;
	}

	// Smoothly orient camera and player towards target chest/torso
	const FVector PlayerLoc = GetActorLocation();
	const FVector TargetAimLoc = GetTargetAimLocationEchoes(Target);
	const FVector CamLoc = FollowCamera ? FollowCamera->GetComponentLocation() : (PlayerLoc + FVector(0.0f, 0.0f, 60.0f));
	FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(CamLoc, TargetAimLoc);
	LookAt.Pitch = FMath::Clamp(LookAt.Pitch - 8.0f, -40.0f, 20.0f);
	LookAt.Roll = 0.0f;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		const FRotator Current = PC->GetControlRotation();
		PC->SetControlRotation(FMath::RInterpTo(Current, LookAt, DeltaTime, 8.0f));
	}

	if (CombatState != EPlayerCombatState::Attacking && CombatState != EPlayerCombatState::Dodging)
	{
		FVector ToTarget = Target->GetActorLocation() - PlayerLoc;
		ToTarget.Z = 0.0f;
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator TargetRot(0.0f, ToTarget.Rotation().Yaw, 0.0f);
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), TargetRot, DeltaTime, 720.0f));
		}
	}
}

FVector AEchoesOfTheCatacombCharacter::GetTranslationWarpTarget() const
{
	AActor* Target = GetLockedTarget();
	if (!Target || !IsValid(Target)) return GetActorLocation();

	const FVector TargetLocation = Target->GetActorLocation();
	const FVector MyLocation = GetActorLocation();

	FVector TargetToMe = (MyLocation - TargetLocation);
	TargetToMe.Z = 0.0f;
	const float Dist = TargetToMe.Size();
	TargetToMe = TargetToMe.GetSafeNormal();

	const float DesiredDist = FMath::Min(Dist, WarpTargetDistance);
	return TargetLocation + (TargetToMe * DesiredDist);
}

FRotator AEchoesOfTheCatacombCharacter::GetRotationWarpTarget() const
{
	AActor* Target = GetLockedTarget();
	if (!Target || !IsValid(Target)) return GetActorRotation();

	FVector LookDir = (Target->GetActorLocation() - GetActorLocation());
	LookDir.Z = 0.0f;
	if (LookDir.IsNearlyZero()) return GetActorRotation();
	return LookDir.Rotation();
}
