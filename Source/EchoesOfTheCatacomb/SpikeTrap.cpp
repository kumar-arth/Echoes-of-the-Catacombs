// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpikeTrap.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

ASpikeTrap::ASpikeTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 1. Root scene component
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 2. Base Plate (Compact 160x160x3 cm floor grate)
	BasePlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BasePlateMesh"));
	BasePlateMesh->SetupAttachment(RootComponent);
	BasePlateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 1.5f));
	BasePlateMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.03f));
	BasePlateMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// 3. Spikes Container (moves along Z axis)
	SpikesContainer = CreateDefaultSubobject<USceneComponent>(TEXT("SpikesContainer"));
	SpikesContainer->SetupAttachment(RootComponent);
	SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, SpikeRetractedZ));

	// 4. Instanced Spikes Mesh (3x3 grid of sharp steel cones, knee height)
	SpikesISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SpikesISM"));
	SpikesISM->SetupAttachment(SpikesContainer);
	SpikesISM->SetCollisionProfileName(TEXT("NoCollision"));
	SpikesISM->SetGenerateOverlapEvents(false);

	// 5. Trigger Box (matching the compact 160x160 plate)
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetBoxExtent(FVector(75.0f, 75.0f, 30.0f));
	TriggerBox->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);

	// 6. Damage Box (active when spikes are sprung)
	DamageBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBox"));
	DamageBox->SetupAttachment(SpikesContainer);
	DamageBox->SetBoxExtent(FVector(70.0f, 70.0f, 30.0f));
	DamageBox->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
	DamageBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DamageBox->SetGenerateOverlapEvents(true);
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Find default engine meshes & dungeon materials
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		BasePlateMesh->SetStaticMesh(CubeMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshFinder.Succeeded())
	{
		SpikesISM->SetStaticMesh(ConeMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IronMatFinder(
		TEXT("/Game/MedievalDungeon/Materials/M_Metal_Decor_Inst.M_Metal_Decor_Inst"));
	if (IronMatFinder.Succeeded())
	{
		BasePlateMesh->SetMaterial(0, IronMatFinder.Object);
		SpikesISM->SetMaterial(0, IronMatFinder.Object);
	}

	// Populate 9 spike instances (3x3 grid, spaced 42 cm apart)
	const float GridSpacing = 42.0f;
	const FVector SpikeScale(0.08f, 0.08f, 0.45f); // Sharp compact spikes
	for (int32 X = -1; X <= 1; ++X)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			FTransform SpikeTransform(FRotator::ZeroRotator, FVector(X * GridSpacing, Y * GridSpacing, 0.0f), SpikeScale);
			SpikesISM->AddInstance(SpikeTransform);
		}
	}

	CurrentSpikeZ = SpikeRetractedZ;
	bIsPeriodic = false; // Pure pressure-plate by default!
	Tags.AddUnique(TEXT("DungeonTrap"));
}

void ASpikeTrap::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASpikeTrap::OnTriggerOverlapBegin);
	DamageBox->OnComponentBeginOverlap.AddDynamic(this, &ASpikeTrap::OnDamageOverlapBegin);

	CurrentSpikeZ = SpikeRetractedZ;
	SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
	TrapState = ESpikeTrapState::Dormant;
}

void ASpikeTrap::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (TrapState)
	{
	case ESpikeTrapState::Dormant:
		if (bIsPeriodic)
		{
			PeriodicTimer += DeltaTime;
			if (PeriodicTimer >= PeriodicCycleInterval)
			{
				PeriodicTimer = 0.0f;
				TriggerTrap();
			}
		}
		break;

	case ESpikeTrapState::Warning:
		StateTimer += DeltaTime;
		// Subtle pre-spring vibration/shake
		{
			const float Jitter = FMath::Sin(StateTimer * 60.0f) * 1.5f;
			SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, SpikeRetractedZ + Jitter));
		}
		if (StateTimer >= WarningDelay)
		{
			StateTimer = 0.0f;
			SpringSpikes();
		}
		break;

	case ESpikeTrapState::Springing:
		CurrentSpikeZ = FMath::FInterpConstantTo(CurrentSpikeZ, SpikeExtendedZ, DeltaTime, SpringSpeed);
		SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
		if (FMath::IsNearlyEqual(CurrentSpikeZ, SpikeExtendedZ, 1.0f))
		{
			CurrentSpikeZ = SpikeExtendedZ;
			SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
			TrapState = ESpikeTrapState::Sprung;
			StateTimer = 0.0f;

			// Enable damage box and hurt overlapping pawns
			DamageBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			TArray<AActor*> OverlappingActors;
			DamageBox->GetOverlappingActors(OverlappingActors);
			for (AActor* Overlapped : OverlappingActors)
			{
				ApplyDamageToActor(Overlapped);
			}
		}
		break;

	case ESpikeTrapState::Sprung:
		StateTimer += DeltaTime;
		if (StateTimer >= SprungDuration)
		{
			StateTimer = 0.0f;
			TrapState = ESpikeTrapState::Retracting;
			DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DamagedActorsThisCycle.Empty();
		}
		break;

	case ESpikeTrapState::Retracting:
		CurrentSpikeZ = FMath::FInterpConstantTo(CurrentSpikeZ, SpikeRetractedZ, DeltaTime, RetractSpeed);
		SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
		if (FMath::IsNearlyEqual(CurrentSpikeZ, SpikeRetractedZ, 1.0f))
		{
			CurrentSpikeZ = SpikeRetractedZ;
			SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
			TrapState = ESpikeTrapState::Dormant;
			PeriodicTimer = 0.0f;
		}
		break;
	}
}

void ASpikeTrap::OnTriggerOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                      bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	// Only trigger when a pawn (Player or Enemy) steps on the plate
	if (Cast<APawn>(OtherActor))
	{
		if (TrapState == ESpikeTrapState::Dormant)
		{
			TriggerTrap();
		}
		else if (TrapState == ESpikeTrapState::Sprung)
		{
			ApplyDamageToActor(OtherActor);
		}
	}
}

void ASpikeTrap::OnDamageOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                     bool bFromSweep, const FHitResult& SweepResult)
{
	if (TrapState == ESpikeTrapState::Sprung)
	{
		ApplyDamageToActor(OtherActor);
	}
}

void ASpikeTrap::TriggerTrap()
{
	if (TrapState != ESpikeTrapState::Dormant) return;

	TrapState = ESpikeTrapState::Warning;
	StateTimer = 0.0f;
	DamagedActorsThisCycle.Empty();
}

void ASpikeTrap::SpringSpikes()
{
	TrapState = ESpikeTrapState::Springing;
	DamagedActorsThisCycle.Empty();
}

void ASpikeTrap::ResetTrap()
{
	TrapState = ESpikeTrapState::Dormant;
	CurrentSpikeZ = SpikeRetractedZ;
	SpikesContainer->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentSpikeZ));
	DamageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamagedActorsThisCycle.Empty();
	PeriodicTimer = 0.0f;
	StateTimer = 0.0f;
}

void ASpikeTrap::HighlightTrap(float Duration)
{
	if (BasePlateMesh)
	{
		BasePlateMesh->SetRenderCustomDepth(true);
		BasePlateMesh->SetCustomDepthStencilValue(2);
	}
	if (SpikesISM)
	{
		SpikesISM->SetRenderCustomDepth(true);
		SpikesISM->SetCustomDepthStencilValue(2);
	}

	GetWorldTimerManager().ClearTimer(HighlightTimerHandle);
	GetWorldTimerManager().SetTimer(HighlightTimerHandle, this, &ASpikeTrap::OnHighlightExpired, Duration, false);
}

void ASpikeTrap::OnHighlightExpired()
{
	if (BasePlateMesh) BasePlateMesh->SetRenderCustomDepth(false);
	if (SpikesISM) SpikesISM->SetRenderCustomDepth(false);
}

void ASpikeTrap::ApplyDamageToActor(AActor* TargetActor)
{
	if (!TargetActor || TargetActor == this || DamagedActorsThisCycle.Contains(TargetActor))
	{
		return;
	}

	DamagedActorsThisCycle.Add(TargetActor);

	// Apply gameplay damage
	UGameplayStatics::ApplyDamage(TargetActor, DamageAmount, nullptr, this, UDamageType::StaticClass());

	// If it's a character, apply physical jolt/knockback
	if (ACharacter* Char = Cast<ACharacter>(TargetActor))
	{
		Char->LaunchCharacter(FVector(0.0f, 0.0f, LaunchImpulseZ), false, true);
	}
}
