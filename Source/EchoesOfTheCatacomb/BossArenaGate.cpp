// Copyright Epic Games, Inc. All Rights Reserved.

#include "BossArenaGate.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Character.h"
#include "BossMinotaur.h"
#include "BossGrux.h"
#include "BossShadowKnight.h"

ABossArenaGate::ABossArenaGate()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 1. Arched Stone Frame (centered at doorway threshold)
	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(RootComponent);
	FrameMesh->SetRelativeLocation(FVector::ZeroVector);
	FrameMesh->SetRelativeRotation(FRotator::ZeroRotator);
	FrameMesh->SetCollisionProfileName(TEXT("BlockAll"));
	FrameMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorwayMeshFinder(
		TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Doorway.SM_Crypt_Doorway"));
	if (DoorwayMeshFinder.Succeeded())
	{
		FrameMesh->SetStaticMesh(DoorwayMeshFinder.Object);
	}

	// 2. Left Door Assembly (Native Hinge at Left Jamb: X = -131, Y = 11)
	HingeLeft = CreateDefaultSubobject<USceneComponent>(TEXT("HingeLeft"));
	HingeLeft->SetupAttachment(RootComponent);
	HingeLeft->SetRelativeLocation(FVector(-131.0f, 11.0f, 0.0f));
	HingeLeft->SetRelativeRotation(FRotator::ZeroRotator);

	DoorLeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeftMesh"));
	DoorLeftMesh->SetupAttachment(HingeLeft);
	DoorLeftMesh->SetRelativeLocation(FVector::ZeroVector);
	DoorLeftMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	DoorLeftMesh->SetRelativeScale3D(FVector(1.0f, 1.003f, 1.0f));
	DoorLeftMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorLeftMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorLeftMeshFinder(
		TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_DoorWay_Large_Door_Left.SM_DoorWay_Large_Door_Left"));
	if (DoorLeftMeshFinder.Succeeded())
	{
		DoorLeftMesh->SetStaticMesh(DoorLeftMeshFinder.Object);
	}

	// 3. Right Door Assembly (Native Hinge at Right Jamb: X = +132, Y = 11)
	HingeRight = CreateDefaultSubobject<USceneComponent>(TEXT("HingeRight"));
	HingeRight->SetupAttachment(RootComponent);
	HingeRight->SetRelativeLocation(FVector(132.0f, 11.0f, 0.0f));
	HingeRight->SetRelativeRotation(FRotator::ZeroRotator);

	DoorRightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorRightMesh"));
	DoorRightMesh->SetupAttachment(HingeRight);
	DoorRightMesh->SetRelativeLocation(FVector::ZeroVector);
	DoorRightMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	DoorRightMesh->SetRelativeScale3D(FVector(1.0f, 1.003f, 1.0f));
	DoorRightMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorRightMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorRightMeshFinder(
		TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_DoorWay_Large_Door_Right.SM_DoorWay_Large_Door_Right"));
	if (DoorRightMeshFinder.Succeeded())
	{
		DoorRightMesh->SetStaticMesh(DoorRightMeshFinder.Object);
	}

	// 4. Approach Trigger Box (Corridor side Y = -160)
	ApproachTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ApproachTrigger"));
	ApproachTrigger->SetupAttachment(RootComponent);
	ApproachTrigger->SetRelativeLocation(FVector(0.0f, -160.0f, 100.0f));
	ApproachTrigger->SetBoxExtent(FVector(160.0f, 90.0f, 130.0f));
	ApproachTrigger->SetCollisionProfileName(TEXT("Trigger"));
	ApproachTrigger->SetGenerateOverlapEvents(true);

	// 5. Arena Entry Trigger Box (Inside room side Y = +200)
	ArenaEntryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ArenaEntryTrigger"));
	ArenaEntryTrigger->SetupAttachment(RootComponent);
	ArenaEntryTrigger->SetRelativeLocation(FVector(0.0f, 200.0f, 100.0f));
	ArenaEntryTrigger->SetBoxExtent(FVector(180.0f, 100.0f, 130.0f));
	ArenaEntryTrigger->SetCollisionProfileName(TEXT("Trigger"));
	ArenaEntryTrigger->SetGenerateOverlapEvents(true);

	// 6. Sound Effects
	static ConstructorHelpers::FObjectFinder<USoundBase> SlamFinder(
		TEXT("/Game/ParagonGrux/Characters/Heroes/Grux/Sounds/SoundCues/Grux_Effort_LandHeavy.Grux_Effort_LandHeavy"));
	if (SlamFinder.Succeeded())
	{
		SlamSound = SlamFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> CreakFinder(
		TEXT("/Engine/VREditor/Sounds/VR_click1_Cue.VR_click1_Cue"));
	if (CreakFinder.Succeeded())
	{
		CreakSound = CreakFinder.Object;
	}

	Tags.AddUnique(TEXT("BossArenaGate"));
}

void ABossArenaGate::BeginPlay()
{
	Super::BeginPlay();

	ApproachTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABossArenaGate::OnApproachOverlapBegin);
	ArenaEntryTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABossArenaGate::OnArenaEntryOverlapBegin);

	if (bIsEntranceGate)
	{
		// Entrance double doors start closed like house doors
		bIsOpen = false;
		CurrentLeftYaw = 0.0f;
		CurrentRightYaw = 0.0f;
		TargetLeftYaw = 0.0f;
		TargetRightYaw = 0.0f;
		HingeLeft->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		HingeRight->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

		DoorLeftMesh->SetCollisionProfileName(TEXT("BlockAll"));
		DoorRightMesh->SetCollisionProfileName(TEXT("BlockAll"));
		DoorLeftMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		DoorRightMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		// Exit gate starts closed until boss is defeated
		bIsOpen = false;
		CurrentLeftYaw = 0.0f;
		CurrentRightYaw = 0.0f;
		TargetLeftYaw = 0.0f;
		TargetRightYaw = 0.0f;
		HingeLeft->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		HingeRight->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

		DoorLeftMesh->SetCollisionProfileName(TEXT("BlockAll"));
		DoorRightMesh->SetCollisionProfileName(TEXT("BlockAll"));
		DoorLeftMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		DoorRightMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		ApproachTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArenaEntryTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossArenaGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Entrance gate failsafe: if player walks inside the arena, seal doors shut immediately!
	if (bIsEntranceGate && bIsOpen && !bHasSealed)
	{
		if (ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
		{
			const FVector LocalPos = GetTransform().InverseTransformPosition(Player->GetActorLocation());
			// If player stepped past the doorway into the room (Local Y > 140)
			if (LocalPos.Y > 140.0f && FMath::Abs(LocalPos.X) < 300.0f)
			{
				bHasSealed = true;
				CloseGate();

				ApproachTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				ArenaEntryTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

				if (LinkedBoss.IsValid()) LinkedBoss->WakeUpAndEngage();
				if (LinkedBossGrux.IsValid()) LinkedBossGrux->WakeUpAndEngage();
				if (LinkedBossShadowKnight.IsValid()) LinkedBossShadowKnight->WakeUpAndEngage();
			}
		}
	}

	const float InterpSpeed = bIsOpen ? 3.5f : 10.0f; // Rapid heavy slam on close, smooth creak on open

	if (FMath::Abs(CurrentLeftYaw - TargetLeftYaw) > 0.5f || FMath::Abs(CurrentRightYaw - TargetRightYaw) > 0.5f)
	{
		CurrentLeftYaw = FMath::FInterpTo(CurrentLeftYaw, TargetLeftYaw, DeltaTime, InterpSpeed);
		CurrentRightYaw = FMath::FInterpTo(CurrentRightYaw, TargetRightYaw, DeltaTime, InterpSpeed);

		HingeLeft->SetRelativeRotation(FRotator(0.0f, CurrentLeftYaw, 0.0f));
		HingeRight->SetRelativeRotation(FRotator(0.0f, CurrentRightYaw, 0.0f));

		if (!bIsOpen && FMath::Abs(CurrentLeftYaw) < 10.0f)
		{
			// Solid barrier when closed
			DoorLeftMesh->SetCollisionProfileName(TEXT("BlockAll"));
			DoorRightMesh->SetCollisionProfileName(TEXT("BlockAll"));
			DoorLeftMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			DoorRightMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			DoorLeftMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			DoorRightMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

void ABossArenaGate::CloseGate()
{
	if (!bIsOpen && FMath::Abs(TargetLeftYaw) < 1.0f) return;

	bIsOpen = false;
	TargetLeftYaw = 0.0f;
	TargetRightYaw = 0.0f;

	if (SlamSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SlamSound, GetActorLocation(), 1.4f, 0.85f);
	}

	UE_LOG(LogTemp, Warning, TEXT("[GATE] Heavy double doors slam shut!"));
}

void ABossArenaGate::OpenGate()
{
	// If this is an entrance gate that already sealed behind the player, NEVER reopen it!
	if (bIsEntranceGate && bHasSealed)
	{
		return;
	}

	if (bIsOpen) return;

	bIsOpen = true;
	// Doors swing open into the room (Left to +90 deg, Right to -90 deg)
	TargetLeftYaw = 90.0f;
	TargetRightYaw = -90.0f;

	// Clear collision while open so player can easily pass through
	DoorLeftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DoorRightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (CreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CreakSound, GetActorLocation(), 1.0f, 0.9f);
	}

	UE_LOG(LogTemp, Log, TEXT("[GATE] Heavy double doors swing wide open!"));
}

void ABossArenaGate::OnApproachOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                           bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsEntranceGate || bIsOpen || bHasSealed) return;

	if (Cast<ACharacter>(OtherActor) && !OtherActor->ActorHasTag(TEXT("Boss")))
	{
		// Player arrived at the entrance doors - doors swing open into the arena!
		OpenGate();
	}
}

void ABossArenaGate::OnArenaEntryOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                             UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                             bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsEntranceGate || bHasSealed) return;

	if (Cast<ACharacter>(OtherActor) && !OtherActor->ActorHasTag(TEXT("Boss")))
	{
		// Player entered the arena room! Slam the doors shut behind them!
		bHasSealed = true;
		CloseGate();

		// Disable triggers so it doesn't reopen during combat
		ApproachTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArenaEntryTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Wake up the respective boss!
		if (LinkedBoss.IsValid())
		{
			LinkedBoss->WakeUpAndEngage();
		}
		if (LinkedBossGrux.IsValid())
		{
			LinkedBossGrux->WakeUpAndEngage();
		}
		if (LinkedBossShadowKnight.IsValid())
		{
			LinkedBossShadowKnight->WakeUpAndEngage();
		}
	}
}
