// Copyright Epic Games, Inc. All Rights Reserved.

#include "BossArenaGate.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Character.h"
#include "BossMinotaur.h"

ABossArenaGate::ABossArenaGate()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Arched Stone Frame
	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(RootComponent);
	FrameMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Heavy Iron Cell Door / Portcullis
	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(RootComponent);
	GateMesh->SetRelativeScale3D(FVector(1.15f, 1.25f, 1.25f));
	GateMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Trigger Box positioned slightly inside the arena to detect entry
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetRelativeLocation(FVector(0.0f, 120.0f, 100.0f));
	TriggerBox->SetBoxExtent(FVector(160.0f, 80.0f, 120.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorwayMeshFinder(
		TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Doorway.SM_Crypt_Doorway"));
	if (DoorwayMeshFinder.Succeeded())
	{
		FrameMesh->SetStaticMesh(DoorwayMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GateMeshFinder(
		TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Cell_Door.SM_Cell_Door"));
	if (GateMeshFinder.Succeeded())
	{
		GateMesh->SetStaticMesh(GateMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GateMatFinder(
		TEXT("/Game/MedievalDungeon/Materials/M_Cell_Door_Front_Inst.M_Cell_Door_Front_Inst"));
	if (GateMatFinder.Succeeded())
	{
		GateMesh->SetMaterial(0, GateMatFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> SoundFinder(
		TEXT("/Engine/VREditor/Sounds/VR_click1_Cue.VR_click1_Cue"));
	if (SoundFinder.Succeeded())
	{
		GateSound = SoundFinder.Object;
	}

	Tags.AddUnique(TEXT("BossArenaGate"));
}

void ABossArenaGate::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABossArenaGate::OnTriggerOverlapBegin);

	if (bIsEntranceGate)
	{
		// Entrance starts open so player can enter
		bIsOpen = true;
		CurrentZ = OpenZOffset;
		TargetZ = OpenZOffset;
		GateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentZ));
		GateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		// Exit gate starts closed until boss is killed
		bIsOpen = false;
		CurrentZ = 0.0f;
		TargetZ = 0.0f;
		GateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentZ));
		GateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossArenaGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (FMath::Abs(CurrentZ - TargetZ) > 1.0f)
	{
		// Fast drop or smooth lift
		const float Speed = (TargetZ < CurrentZ) ? 900.0f : 300.0f;
		CurrentZ = FMath::FInterpConstantTo(CurrentZ, TargetZ, DeltaTime, Speed);
		GateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentZ));

		if (CurrentZ <= 5.0f && TargetZ == 0.0f)
		{
			GateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

void ABossArenaGate::CloseGate()
{
	if (!bIsOpen) return;

	bIsOpen = false;
	TargetZ = 0.0f;

	if (GateSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GateSound, GetActorLocation());
	}
}

void ABossArenaGate::OpenGate()
{
	if (bIsOpen) return;

	bIsOpen = true;
	TargetZ = OpenZOffset;
	GateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GateSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GateSound, GetActorLocation());
	}
}

void ABossArenaGate::OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                          bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsEntranceGate || !bIsOpen) return;

	if (Cast<ACharacter>(OtherActor) && !OtherActor->IsA(ABossMinotaur::StaticClass()))
	{
		// Player entered the arena! Seal the entrance gate!
		CloseGate();

		// Disable trigger so it doesn't fire again
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Wake up the Minotaur!
		if (LinkedBoss.IsValid())
		{
			LinkedBoss->WakeUpAndEngage();
		}
	}
}

